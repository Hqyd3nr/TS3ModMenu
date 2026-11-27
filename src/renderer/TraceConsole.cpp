// Trace Console: a separate Windows window that shows the session log live (Ctrl+Backspace opens and closes it).
// It runs on its own thread with its own message loop, so it never touches the game's rendering or the
// menu: the menu's open state, TAB activation and input gate are unaffected. It reads the log from the
// backend (dinput8.dll) through the menu API, so it also shows everything logged before the menu loaded.
// Drawing is software only, into a 32-bit bitmap, using the menu's own font atlas (Nunito and Lilita One).
#include "../../assets/menu/Assets.generated.h"
namespace TraceConsole {
using namespace CheatMenuAssets;
static const TS3CheatMenuApi* api;
static HWND window,game;static HANDLE thread;static DWORD threadId;static volatile LONG visible;   // game: the window Ctrl+Backspace was pressed in
static BYTE* atlas;                                   // font atlas, RGBA8, white glyphs in alpha
static TS3LogLine* lines;static unsigned count,nextSequence,clearedBefore;   // copy of the log
static const unsigned maxLines=4096;
static bool autoScroll=true,allSelected;   // autoScroll: following new lines (at the bottom); allSelected: Ctrl+A
static int scroll;                                    // first visible filtered row
static char notice[160];static DWORD noticeTick;
static int width=980,height=620;static DWORD* pixels;static HBITMAP bitmap;static HDC memory;
static bool dark;static DWORD themeTick;
static int dragScroll=-1,dragStart;

// The menu DLL links only kernel32 and user32, so GDI and the registry are loaded from System32 on demand.
static HMODULE System(const WCHAR* name){WCHAR path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);lstrcatW(path,L"\\");lstrcatW(path,name);return LoadLibraryW(path);}
static struct {
 HDC(WINAPI* CreateCompatibleDC)(HDC);HBITMAP(WINAPI* CreateDIBSection)(HDC,const BITMAPINFO*,UINT,void**,HANDLE,DWORD);
 HGDIOBJ(WINAPI* SelectObject)(HDC,HGDIOBJ);BOOL(WINAPI* DeleteObject)(HGDIOBJ);BOOL(WINAPI* BitBlt)(HDC,int,int,int,int,HDC,int,int,DWORD);
 LSTATUS(WINAPI* RegGetValueW)(HKEY,LPCWSTR,LPCWSTR,DWORD,LPDWORD,PVOID,LPDWORD);
} os;
static bool LoadSystem(){
 HMODULE gdi=System(L"gdi32.dll"),reg=System(L"advapi32.dll");if(!gdi||!reg)return false;
 os.CreateCompatibleDC=(decltype(os.CreateCompatibleDC))GetProcAddress(gdi,"CreateCompatibleDC");os.CreateDIBSection=(decltype(os.CreateDIBSection))GetProcAddress(gdi,"CreateDIBSection");
 os.SelectObject=(decltype(os.SelectObject))GetProcAddress(gdi,"SelectObject");os.DeleteObject=(decltype(os.DeleteObject))GetProcAddress(gdi,"DeleteObject");
 os.BitBlt=(decltype(os.BitBlt))GetProcAddress(gdi,"BitBlt");os.RegGetValueW=(decltype(os.RegGetValueW))GetProcAddress(reg,"RegGetValueW");
 return os.CreateCompatibleDC&&os.CreateDIBSection&&os.SelectObject&&os.DeleteObject&&os.BitBlt&&os.RegGetValueW;
}

// ---------- colours (menu palette) ----------
struct Palette { DWORD bg,panel,bar,line,ink,muted,blue,chipOn,chipOff,field,stripe; DWORD level[7]; };
static Palette pal;
static DWORD Rgb(int r,int g,int b){return DWORD(r<<16|g<<8|b);}
#ifdef TRACE_CONSOLE_TEST
static int testTheme=-1;   // offline preview: 0 light, 1 dark
#endif
static void Theme(){
 // Same setting as the menu (HKCU\Software\TS3ModMenu Theme: Light, Dark or System).
 WCHAR value[16]=L"System";DWORD bytes=sizeof(value);
 os.RegGetValueW(HKEY_CURRENT_USER,L"Software\\TS3ModMenu",L"Theme",RRF_RT_REG_SZ,0,value,&bytes);
 bool d=lstrcmpiW(value,L"Dark")==0;
 if(lstrcmpiW(value,L"Light")!=0&&!d){DWORD light=1;bytes=sizeof(light);os.RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,0,&light,&bytes);d=light==0;}
 dark=d;
#ifdef TRACE_CONSOLE_TEST
 if(testTheme>=0)dark=testTheme==1;
#endif
 if(!dark)pal={Rgb(255,255,255),Rgb(242,247,252),Rgb(228,239,250),Rgb(189,206,225),Rgb(27,39,66),Rgb(78,96,125),Rgb(44,107,211),Rgb(207,225,247),Rgb(237,244,252),Rgb(255,255,255),Rgb(246,249,253),
  // TRACE dark grey, DEBUG light grey, INFO blue, SUCCESS green, WARNING yellow (amber so it reads on white), ERROR red, FATAL dark red
  {Rgb(92,100,112),Rgb(128,138,150),Rgb(44,107,211),Rgb(22,128,61),Rgb(168,112,0),Rgb(206,38,38),Rgb(128,0,0)}};
 else pal={Rgb(22,34,58),Rgb(29,40,58),Rgb(27,42,70),Rgb(52,72,101),Rgb(231,237,248),Rgb(175,190,213),Rgb(131,181,255),Rgb(42,65,101),Rgb(27,42,70),Rgb(14,24,41),Rgb(25,38,63),
  {Rgb(128,138,153),Rgb(196,204,216),Rgb(120,170,255),Rgb(96,212,140),Rgb(242,201,76),Rgb(255,110,110),Rgb(232,84,100)}};
}

// ---------- drawing ----------
static void Fill(int x,int y,int w,int h,DWORD c){
 for(int j=y<0?0:y;j<y+h&&j<height;++j)for(int i=x<0?0:x;i<x+w&&i<width;++i)pixels[j*width+i]=c;
}
static void Round(int x,int y,int w,int h,DWORD c,int r){
 for(int j=0;j<h;++j)for(int i=0;i<w;++i){
  int dx=i<r?r-i:i>=w-r?i-(w-r-1):0,dy=j<r?r-j:j>=h-r?j-(h-r-1):0;if(dx*dx+dy*dy>r*r)continue;
  int px=x+i,py=y+j;if(px>=0&&py>=0&&px<width&&py<height)pixels[py*width+px]=c;
 }
}
static void Frame(int x,int y,int w,int h,DWORD c){Fill(x,y,w,1,c);Fill(x,y+h-1,w,1,c);Fill(x,y,1,h,c);Fill(x+w-1,y,1,h,c);}
static unsigned Next(const unsigned char*& p){
 unsigned c=*p++;if(c<0x80)return c;
 int extra=c>=0xf0?3:c>=0xe0?2:1;c&=0x3f>>extra;while(extra--&&(*p&0xc0)==0x80)c=c<<6|(*p++&0x3f);return c;
}
static int TextWidth(const char* text,FontIndex f){int w=0;for(const unsigned char* p=(const unsigned char*)text;*p;)w+=int(FindGlyph(f,Next(p))->advance+.5f);return w;}
// Draws text with its top at y; stops at maxX (adds an ellipsis). Returns the end x.
static int Text(int x,int y,const char* text,FontIndex f,DWORD color,int maxX=1<<30){
 if(!atlas)return x;const Font& face=kFonts[f];int r=color>>16&255,g=color>>8&255,b=color&255;
 int ellipsis=TextWidth("...",f);
 for(const unsigned char* p=(const unsigned char*)text;*p;){
  const unsigned char* at=p;const Glyph* gl=FindGlyph(f,Next(p));
  if(x+int(gl->advance)>maxX-ellipsis&&*p){if(TextWidth((const char*)at,f)+x>maxX){Text(x,y,"...",f,color);return x+ellipsis;}}
  for(int j=0;j<gl->height;++j){int py=y+face.ascent+gl->bearingY+j;if(py<0||py>=height)continue;
   for(int i=0;i<gl->width;++i){int px=x+gl->bearingX+i;if(px<0||px>=width)continue;
    int a=atlas[((gl->y+j)*kFontAtlasWidth+gl->x+i)*4+3];if(!a)continue;
    DWORD& d=pixels[py*width+px];int dr=d>>16&255,dg=d>>8&255,db=d&255;
    d=Rgb(dr+(r-dr)*a/255,dg+(g-dg)*a/255,db+(b-db)*a/255);}}
  x+=int(gl->advance+.5f);
 }
 return x;
}

// ---------- log ----------
static const char* const levelNames[]={"TRACE","DEBUG","INFO","SUCCESS","WARNING","ERROR","FATAL"};
static void Pull(){
 if(!api||!api->ReadLog||!lines)return;
 static TS3LogLine batch[256];
 for(;;){
  unsigned n=api->ReadLog(nextSequence,batch,256);if(!n)break;
  for(unsigned i=0;i<n;++i){
   if(count==maxLines){for(unsigned k=1;k<maxLines;++k)lines[k-1]=lines[k];--count;}   // keep the newest
   lines[count++]=batch[i];
  }
  nextSequence=batch[n-1].sequence+1;
 }
}
// Everything since the last Clear. No search or level filters: the console is for watching what happens and
// copying or exporting all of it for a bug report.
static bool Shown(const TS3LogLine& l){return l.sequence>=clearedBefore&&l.level<7;}
static unsigned Filtered(unsigned* out){unsigned n=0;for(unsigned i=0;i<count;++i)if(Shown(lines[i]))out[n++]=i;return n;}
static void Stamp(char* out,unsigned t){wsprintfA(out,"%02u:%02u:%02u.%03u",t/3600000,t/60000%60,t/1000%60,t%1000);}
// Lines since the last Clear as text: time [LEVEL] message, one per line.
static char* Collect(int* length){
 static unsigned index[maxLines];unsigned n=Filtered(index);
 char* text=(char*)VirtualAlloc(0,n*260+16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!text)return 0;
 int used=0;for(unsigned k=0;k<n;++k){const TS3LogLine& l=lines[index[k]];char t[16];Stamp(t,l.time);used+=wsprintfA(text+used,"%s [%-7s] %s\r\n",t,levelNames[l.level],l.text);}
 *length=used;return text;
}
static void Notice(const char* text){lstrcpynA(notice,text,sizeof(notice));noticeTick=GetTickCount();}
static void CopyAll(){
 int length=0;char* text=Collect(&length);if(!text)return;
 int wide=MultiByteToWideChar(CP_UTF8,0,text,length,0,0);HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,(wide+1)*sizeof(WCHAR));
 if(h){WCHAR* w=(WCHAR*)GlobalLock(h);MultiByteToWideChar(CP_UTF8,0,text,length,w,wide);w[wide]=0;GlobalUnlock(h);
  if(OpenClipboard(window)){EmptyClipboard();if(SetClipboardData(CF_UNICODETEXT,h))h=0;CloseClipboard();}
  if(h)GlobalFree(h);}
 VirtualFree(text,0,MEM_RELEASE);Notice(h?"Could not copy to the clipboard.":"Copied all messages to the clipboard.");
}
static void Clear(){clearedBefore=nextSequence;scroll=0;autoScroll=true;allSelected=false;Notice("Cleared. New messages will appear here; the log files are unchanged.");}
static void Export(){
 int length=0;char* text=Collect(&length);if(!text)return;
 WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(OverlayHost::Module(),path,MAX_PATH);while(n&&path[n-1]!=L'\\')--n;path[n]=0;
 lstrcatW(path,L"Mod Menu\\Logs\\");CreateDirectoryW(path,0);
 SYSTEMTIME t;GetLocalTime(&t);WCHAR name[64];wsprintfW(name,L"Export-%04u-%02u-%02u_%02u-%02u-%02u.log",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);lstrcatW(path,name);
 HANDLE f=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);DWORD done=0;
 bool ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,text,length,&done,0)&&done==DWORD(length);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 VirtualFree(text,0,MEM_RELEASE);
 char msg[160];if(ok){char a[64];WideCharToMultiByte(CP_UTF8,0,name,-1,a,64,0,0);wsprintfA(msg,"Exported to Mod Menu\\Logs\\%s",a);}else lstrcpyA(msg,"Export failed.");Notice(msg);
}

// ---------- layout ----------
static const int headerH=56,rowH=24,statusH=28,listTop=headerH;
struct Rect{int x,y,w,h;bool In(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
static Rect copyButton,exportButton,clearButton,scrollbar;
static int hoverX=-1,hoverY=-1;
static int Rows(){return (height-listTop-statusH)/rowH;}
static void Layout(){
 int x=width-16;
 clearButton={x-84,12,84,32};x-=84+8;exportButton={x-84,12,84,32};x-=84+8;copyButton={x-96,12,96,32};
 scrollbar={width-12,listTop,10,height-listTop-statusH};
}
static void Button(const Rect& r,const char* label){
 bool hover=r.In(hoverX,hoverY);Round(r.x,r.y,r.w,r.h,hover?pal.chipOn:dark?Rgb(39,59,88):Rgb(255,255,255),7);
 Text(r.x+(r.w-TextWidth(label,Nunito18))/2,r.y+5,label,Nunito18,pal.ink);
}
static void Paint(){
 Theme();Layout();Fill(0,0,width,height,pal.bg);
 // Header
 Fill(0,0,width,headerH,pal.bar);Fill(0,headerH-1,width,1,pal.line);
 Text(16,10,"Trace Console",Lilita30,pal.ink);
 Button(copyButton,"Copy All");Button(exportButton,"Export");Button(clearButton,"Clear");
 // Log rows
 static unsigned index[maxLines];unsigned shown=Filtered(index);int rows=Rows();
 int maxScroll=int(shown)>rows?int(shown)-rows:0;if(autoScroll)scroll=maxScroll;if(scroll>maxScroll)scroll=maxScroll;if(scroll<0)scroll=0;
 int timeX=16,levelX=16+TextWidth("00:00:00.000",Nunito18)+16,textX=levelX+TextWidth("WARNING",Nunito18)+18;
 for(int r=0;r<rows&&scroll+r<int(shown);++r){
  const TS3LogLine& l=lines[index[scroll+r]];int y=listTop+r*rowH;
  if(allSelected)Fill(0,y,width-14,rowH,pal.chipOn);   // Ctrl+A: everything is selected
  else if(l.level>=5)Fill(0,y,width-14,rowH,dark?(l.level==6?Rgb(70,20,30):Rgb(55,28,36)):(l.level==6?Rgb(252,226,226):Rgb(254,240,240)));
  else if(r%2)Fill(0,y,width-14,rowH,pal.stripe);
  char t[16];Stamp(t,l.time);Text(timeX,y+1,t,Nunito18,pal.muted);
  Text(levelX,y+1,levelNames[l.level],Nunito18,pal.level[l.level]);
  Text(textX,y+1,l.text,Nunito18,pal.ink,width-20);
 }
 if(!shown)Text(16,listTop+12,count?"Cleared. New messages will appear here.":"No messages yet.",Nunito18,pal.muted);
 // Scrollbar
 if(int(shown)>rows){int track=scrollbar.h,thumb=track*rows/int(shown);if(thumb<24)thumb=24;int pos=maxScroll?(track-thumb)*scroll/maxScroll:0;
  Round(scrollbar.x,scrollbar.y+pos,8,thumb,pal.line,4);}
 // Status bar
 int sy=height-statusH;Fill(0,sy,width,statusH,pal.panel);Fill(0,sy,width,1,pal.line);
 char status[200];
 if(notice[0]&&DWORD(GetTickCount()-noticeTick)<5000)lstrcpynA(status,notice,sizeof(status));
 else wsprintfA(status,shown==1?"%u message":"%u messages",shown);
 Text(16,sy+3,status,Nunito18,pal.muted);
 // Keyboard shortcuts, while the console is the active window.
 const char* keys="C  Clear     E  Export     Ctrl+A+C  Copy all";Text(width-16-TextWidth(keys,Nunito18),sy+3,keys,Nunito18,pal.muted);
}

// ---------- window ----------
// Where the console opens, every time: centred over the game at 80% of its width and height, so the game
// stays visible around it. Nothing is remembered between openings.
static bool Place(int& x,int& y,int& w,int& h){
 RECT r;if(!game||!IsWindow(game)||!GetClientRect(game,&r)||r.right<=0||r.bottom<=0)return false;
 POINT o={0,0};ClientToScreen(game,&o);
 w=r.right*80/100;h=r.bottom*80/100;if(w<900)w=r.right<900?r.right:900;if(h<280)h=r.bottom<280?r.bottom:280;
 x=o.x+(r.right-w)/2;y=o.y+(r.bottom-h)/2;return true;
}
// Opening the console puts the cursor in its middle, so it is right there to use.
static void CentreCursor(){RECT r;if(window&&GetWindowRect(window,&r))SetCursorPos((r.left+r.right)/2,(r.top+r.bottom)/2);}
static void Resize(int w,int h){
 if(w<640)w=640;if(h<228)h=228;width=w;height=h;   // 228: header, toolbar, status bar and four rows
 if(bitmap)os.DeleteObject(bitmap);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 if(!memory)memory=os.CreateCompatibleDC(0);bitmap=os.CreateDIBSection(memory,&bi,DIB_RGB_COLORS,(void**)&pixels,0,0);os.SelectObject(memory,bitmap);
}
static void Scroll(int delta){
 static unsigned index[maxLines];int shown=int(Filtered(index)),rows=Rows(),maxScroll=shown>rows?shown-rows:0;
 scroll+=delta;if(scroll<0)scroll=0;if(scroll>maxScroll)scroll=maxScroll;autoScroll=scroll>=maxScroll;
}
static LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM w,LPARAM l){
 switch(msg){
 case WM_SIZE:if(LOWORD(l)&&HIWORD(l)){Resize(LOWORD(l),HIWORD(l));InvalidateRect(h,0,FALSE);}return 0;
 case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={900,280};return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);if(pixels){Paint();os.BitBlt(dc,0,0,width,height,memory,0,0,SRCCOPY);}EndPaint(h,&ps);return 0;}
 case WM_TIMER:{unsigned before=nextSequence;Pull();
  if(nextSequence!=before||(notice[0]&&DWORD(GetTickCount()-noticeTick)<5200))InvalidateRect(h,0,FALSE);return 0;}
 case WM_MOUSEMOVE:{hoverX=(short)LOWORD(l);hoverY=(short)HIWORD(l);
  if(dragScroll>=0&&(w&MK_LBUTTON)){static unsigned index[maxLines];int shown=int(Filtered(index)),rows=Rows();if(shown>rows){scroll=dragScroll;Scroll((hoverY-dragStart)*shown/scrollbar.h);}}
  TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);InvalidateRect(h,0,FALSE);return 0;}
 case WM_MOUSELEAVE:hoverX=hoverY=-1;InvalidateRect(h,0,FALSE);return 0;
 case WM_MOUSEWHEEL:Scroll(-(short)HIWORD(w)/40);InvalidateRect(h,0,FALSE);return 0;
 case WM_LBUTTONDOWN:{int x=(short)LOWORD(l),y=(short)HIWORD(l);SetCapture(h);
  if(copyButton.In(x,y))CopyAll();else if(exportButton.In(x,y))Export();else if(clearButton.In(x,y))Clear();
  else if(x>=scrollbar.x-4&&y>=scrollbar.y&&y<scrollbar.y+scrollbar.h){dragScroll=scroll;dragStart=y;autoScroll=false;}
  else if(y>=listTop)allSelected=false;   // clicking the list drops a Ctrl+A selection
  InvalidateRect(h,0,FALSE);return 0;}
 case WM_LBUTTONUP:dragScroll=-1;ReleaseCapture();return 0;
 case WM_KEYDOWN:{
  // C clears, E exports, Ctrl+A selects everything and Ctrl+C copies it. Holding a key down does it once.
  bool ctrl=GetKeyState(VK_CONTROL)<0,alt=GetKeyState(VK_MENU)<0,repeat=(l>>30)&1;
  if(alt||repeat){}
  else if(ctrl&&w=='A')allSelected=true;
  else if(ctrl&&w=='C')CopyAll();
  else if(!ctrl&&w=='C')Clear();
  else if(!ctrl&&w=='E')Export();
  else if(w==VK_ESCAPE)allSelected=false;
  if(w==VK_PRIOR)Scroll(-Rows());else if(w==VK_NEXT)Scroll(Rows());
  else if(w==VK_HOME){scroll=0;autoScroll=false;}else if(w==VK_END){autoScroll=true;}
  else if(w==VK_UP)Scroll(-1);else if(w==VK_DOWN)Scroll(1);
  InvalidateRect(h,0,FALSE);return 0;}
 case WM_CLOSE:ShowWindow(h,SW_HIDE);InterlockedExchange(&visible,0);return 0;   // closing hides; Ctrl+Backspace brings it back
 }
 return DefWindowProcW(h,msg,w,l);
}
static void LoadAtlas(){
 WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(OverlayHost::Module(),path,MAX_PATH);while(n&&path[n-1]!=L'\\')--n;path[n]=0;
 const WCHAR* folders[]={L"Mod Menu\\assets\\font-atlas.rgba",L"CheatMenu\\assets\\font-atlas.rgba"};
 for(const WCHAR* f:folders){
  WCHAR full[MAX_PATH];lstrcpyW(full,path);lstrcatW(full,f);
  HANDLE file=CreateFileW(full,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(file==INVALID_HANDLE_VALUE)continue;
  DWORD bytes=kFontAtlasWidth*kFontAtlasHeight*4,done=0;BYTE* data=(BYTE*)VirtualAlloc(0,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(data&&ReadFile(file,data,bytes,&done,0)&&done==bytes)atlas=data;else if(data)VirtualFree(data,0,MEM_RELEASE);
  CloseHandle(file);if(atlas)return;
 }
}
static DWORD WINAPI Run(void*){
 if(!LoadSystem())return 0;
 LoadAtlas();
 lines=(TS3LogLine*)VirtualAlloc(0,sizeof(TS3LogLine)*maxLines,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!atlas||!lines){if(api&&api->Log)api->Log(5,"Trace Console could not start: font atlas missing");return 0;}
 WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=Proc;wc.hInstance=OverlayHost::Module();wc.hCursor=LoadCursorW(0,IDC_ARROW);wc.lpszClassName=L"TS3ModMenuTraceConsole";
 RegisterClassExW(&wc);
 int x=CW_USEDEFAULT,y=CW_USEDEFAULT,w=width,h=height;Place(x,y,w,h);
 // Topmost so it stays visible over a borderless full-screen game; it can be moved to another monitor.
 window=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Trace Console",WS_OVERLAPPEDWINDOW,x,y,w,h,0,0,wc.hInstance,0);
 if(!window)return 0;
 RECT client;GetClientRect(window,&client);Resize(client.right,client.bottom);Pull();
 ShowWindow(window,SW_SHOWNOACTIVATE);   // don't take focus from the game
 CentreCursor();
 SetTimer(window,1,100,0);
 MSG msg;while(GetMessageW(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return 0;
}
static void Toggle(){
 if(!window){InterlockedExchange(&visible,1);if(!thread){thread=CreateThread(0,0,Run,0,0,&threadId);}return;}
 if(IsWindowVisible(window)){ShowWindow(window,SW_HIDE);InterlockedExchange(&visible,0);}
 else{int x,y,w,h;if(Place(x,y,w,h))SetWindowPos(window,0,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(window,SW_SHOWNOACTIVATE);CentreCursor();allSelected=false;InterlockedExchange(&visible,1);InvalidateRect(window,0,FALSE);}
}
// Ctrl+Backspace, only while the game or the console is the active window, and not while you're typing in the
// menu's search box (there it deletes a word). Polled on its own thread (cheap: 30 ms),
// so it works before the menu is ever opened and doesn't depend on the menu being open or closed.
static DWORD WINAPI Watch(void*){
 bool was=false;
 for(;;){
  Sleep(30);bool down=(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_BACK)&0x8000);
  if(down&&!was&&!(OverlayHost::IsOpen()&&CheatMenuUI::Typing())){DWORD pid=0;HWND f=GetForegroundWindow();GetWindowThreadProcessId(f,&pid);if(pid==GetCurrentProcessId()){if(f!=window)game=f;Toggle();}}
  was=down;
 }
}
static void Start(const TS3CheatMenuApi* backend){
 static bool started;if(started||!backend||!backend->ReadLog)return;started=true;api=backend;
 HANDLE t=CreateThread(0,0,Watch,0,0,0);if(t)CloseHandle(t);
}
}
