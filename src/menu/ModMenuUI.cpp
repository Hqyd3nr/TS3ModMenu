// The menu itself: pages, the settings drawer, themes and saved preferences. Drawn every frame
// while open; actions go to the game through OverlayHost::Backend().
#include "OverlayHost.h"
#include "Localization.generated.h"
#include "FallbackFont.generated.h"
#include "MenuVersion.h"
#include "../../assets/menu/Assets.generated.h"

#ifdef UI_SELF_TEST
namespace NativeUI {void TestPointer(ImVec2,bool,bool);}
#endif
namespace CheatMenuUI {
using namespace CheatMenuAssets;
static float Min(float a,float b){return a<b?a:b;}
static bool BetaBuild(){for(const char* v=TS3_MENU_VERSION;*v;++v)if(v[0]=='B'&&v[1]=='e'&&v[2]=='t'&&v[3]=='a')return true;return false;}
static float Max(float a,float b){return a>b?a:b;}
static OverlayHost::Texture font,icons[kIconCount];
static bool assetsAttempted=false,assetsReady=false;
static int page=0,amountIndex=1;
static float pageScroll[8]={};
static bool settingsOpen=false,drawingSettings=false,languageOpen=false,drawingLanguage=false;
static int capture=0; // Settings > Controls: 1 waiting for a key, 2 waiting for a controller button.
static float settingsScroll=0;
#ifdef UI_SELF_TEST
static float testRenderedScroll,testScrollLimit;
static const char* testFooter="";
#endif
static float settingsSlide=0,settingsFrom=0;
static DWORD settingsTick=0;
static void SetSettings(bool open){if(open==settingsOpen)return;capture=0;NativeUI::PadCapture(false);settingsFrom=settingsSlide;settingsTick=GetTickCount();settingsOpen=open;languageOpen=false;}
// CSS ease-out: cubic-bezier(0,0,.58,1), matching Companion sheet-in.
static float SettingsEase(float t){if(t>=1)return 1;if(t<=0)return 0;float lo=0,hi=1;for(int i=0;i<16;++i){float u=(lo+hi)*.5f;float x=1.74f*(1-u)*u*u+u*u*u;if(x<t)lo=u;else hi=u;}float u=(lo+hi)*.5f;return 3*(1-u)*u*u+u*u*u;}
static void UpdateSettings(){float t=Min(1.f,float(DWORD(GetTickCount()-settingsTick))/220.f);settingsSlide=settingsOpen?settingsFrom+(1.f-settingsFrom)*SettingsEase(t):settingsFrom*SettingsEase(1.f-t);}


static constexpr bool shading=true;
static bool prefsLoaded=false,dark=false,themeSeen=false;
static ImVec2 themeOrigin;static bool themeOriginSet=false;
static int theme=2; // 0 Light, 1 Dark, 2 System; System is the default.
static DWORD themePoll=0;static bool systemDark=false;
// Settings > Controls: key and controller button that open/close the menu (saved in the registry).
// Keys/buttons used for movement, flight, the menu itself or Windows are refused so they never do two things.
static int openKey=VK_TAB;static WORD openButton=0x0001;static bool captureHeld[256];static const char* captureNote;static DWORD captureNoteTick;
// A and B select and go back in the menu; View opens missions and Menu pauses the game.
static const WORD padBits[]={0x0001,0x0002,0x0004,0x0008,0x0040,0x0080,0x0100,0x0200,0x0400,0x0800,0x4000,0x8000};
static const char* padNames[]={"D-pad Up","D-pad Down","D-pad Left","D-pad Right","Left stick","Right stick","LB","RB","LT","RT","X","Y"};
static const IconIndex padArt[]={PadUp,PadDown,PadLeft,PadRight,PadLeftStick,PadRightStick,PadLB,PadRB,PadLT,PadRT,PadX,PadY};
static int PadIndex(WORD bit){for(int i=0;i<12;++i)if(padBits[i]==bit)return i;return -1;}
// The game's key numbers (Button Map table at RVA 0x9504f0) as Windows virtual keys; 92-96 are mouse.
static const BYTE engineVk[103]={'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
 '0','1','2','3','4','5','6','7','8','9',VK_SPACE,VK_OEM_3,VK_OEM_MINUS,VK_OEM_PLUS,VK_OEM_4,VK_OEM_6,VK_OEM_2,VK_OEM_1,VK_OEM_7,VK_OEM_PERIOD,VK_OEM_COMMA,VK_OEM_5,
 VK_ESCAPE,VK_BACK,VK_TAB,VK_RETURN,VK_INSERT,VK_DELETE,VK_HOME,VK_END,VK_PRIOR,VK_NEXT,VK_UP,VK_DOWN,VK_LEFT,VK_CLEAR,VK_RIGHT,VK_MULTIPLY,VK_ADD,VK_SUBTRACT,VK_DIVIDE,
 VK_NUMPAD0,VK_NUMPAD1,VK_NUMPAD2,VK_NUMPAD3,VK_NUMPAD4,VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD7,VK_NUMPAD8,VK_NUMPAD9,VK_DECIMAL,VK_RETURN,VK_NUMLOCK,
 VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,0,0,0,0,0,VK_LSHIFT,VK_LCONTROL,VK_LMENU,VK_RSHIFT,VK_RCONTROL,VK_RMENU};
// Movement and jump keys from the keyboard driver defaults (0x27f100: PC_MOVE_*, PC_JUMP).
static const BYTE gameVk[]={'W','A','S','D',VK_SPACE};
static bool GameKey(int vk){
 for(BYTE g:gameVk)if(g==vk)return true;
 // Remapped movement/jump: live table at RVA 0xb2eb08 ({key,flag} words; 0-3 move, 4 jump).
 // Read only inside the supported game build (same fingerprint the menu checks before opening).
 static int supported=-1;if(supported<0)supported=OverlayHost::GameBuildMatches()?1:0;
 const BYTE* game=(const BYTE*)GetModuleHandleW(nullptr);
 if(game&&supported==1)for(int i=0;i<5;++i){WORD code=*(const WORD*)(game+0xb2eb08+4*i);if(code<103&&engineVk[code]==vk)return true;}
 return false;
}
// Flight: WASD, Space/E up, Ctrl/Q down, Shift boost. Esc closes the menu.
static bool FlightKey(int vk){return vk=='W'||vk=='A'||vk=='S'||vk=='D'||vk==VK_SPACE||vk=='E'||vk=='Q'||(vk>=VK_LSHIFT&&vk<=VK_RCONTROL);}
static bool MenuKey(int vk){return vk==VK_ESCAPE;}
static bool SystemKey(int vk){return vk<0x08||vk>0xFE||(vk>=0x10&&vk<=0x12)||(vk>=0x15&&vk<=0x1F&&vk!=VK_ESCAPE)||vk==VK_LWIN||vk==VK_RWIN||vk==VK_APPS||(vk>=0xA4&&vk<=0xB7)||vk==0xE5||vk==VK_F12||vk==VK_NUMLOCK||vk==VK_CAPITAL||vk==VK_SCROLL||vk==VK_SNAPSHOT;}
// nullptr when the key is free; otherwise why it was refused.
static const char* KeyConflict(int vk){
 return GameKey(vk)||FlightKey(vk)||MenuKey(vk)||SystemKey(vk)?"This key can't be used.":nullptr;
}
static void KeyName(int vk,char* out,int size){
 UINT code=MapVirtualKeyW(UINT(vk),0);
 switch(vk){case VK_LEFT:case VK_UP:case VK_RIGHT:case VK_DOWN:case VK_PRIOR:case VK_NEXT:case VK_END:case VK_HOME:case VK_INSERT:case VK_DELETE:case VK_DIVIDE:case VK_NUMLOCK:case VK_RCONTROL:case VK_RMENU:code|=0x100;}
 WCHAR name[48];int n=code?GetKeyNameTextW(LONG(code<<16),name,48):0;
 if(n<=0||!WideCharToMultiByte(CP_UTF8,0,name,-1,out,size,nullptr,nullptr))wsprintfA(out,"Key %d",vk);
}
static bool prefsStore=true; // false in self-tests: never touch the player's saved settings
static uint32_t sequence=0,pending=0,pendingAction=0;
static bool closeAfterPictomaticAdd=false;
static int appliedSky=-1,requestedSky=-1;
static TS3CheatSnapshot state{};
static float scale=1,ox=0,oy=0,slider=15;
static char localMessage[128]{};
static DWORD localMessageTick;
static bool placed=false,titleDragging=false;
static float windowX,windowY,grabX,grabY;
static ImDrawList* draw;
static ImU32 ink=IM_COL32(27,39,66,255),muted=IM_COL32(78,96,125,255);
static ImU32 blue=IM_COL32(44,107,211,255),paper=IM_COL32(255,255,255,255);
static ImU32 line=IM_COL32(189,206,225,255),gold=IM_COL32(240,176,0,255);
// Dark Toy Box palette.
static ImU32 Surface(ImU32 c){
 if(!dark)return c;
 switch(c){
 case IM_COL32(242,247,252,255):return IM_COL32(27,42,70,255);
 case IM_COL32(228,239,250,255):return IM_COL32(20,34,57,255);
 case IM_COL32(201,227,246,255):return IM_COL32(14,24,41,255);
 case IM_COL32(230,236,243,255):case IM_COL32(224,231,239,255):return IM_COL32(29,40,58,255);
 case IM_COL32(222,236,250,255):case IM_COL32(218,234,250,255):return IM_COL32(39,59,88,255);
 case IM_COL32(207,225,247,255):return IM_COL32(42,65,101,255);
 case IM_COL32(237,244,252,255):case IM_COL32(224,235,247,255):return IM_COL32(27,42,70,255);
 case IM_COL32(205,217,231,255):case IM_COL32(217,228,240,255):return IM_COL32(52,72,101,255);
 default:return c;
 }
}
static void UpdateTheme(){
 if(theme==2&&(!themePoll||DWORD(GetTickCount()-themePoll)>=1000)){
  themePoll=GetTickCount();
  using ReadFn=LSTATUS(WINAPI*)(HKEY,LPCWSTR,LPCWSTR,DWORD,LPDWORD,PVOID,LPDWORD);
  static HMODULE registry=LoadLibraryExW(L"advapi32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  static auto read=registry?(ReadFn)GetProcAddress(registry,"RegGetValueW"):nullptr;
  DWORD light=1,bytes=sizeof(light);
  if(read&&read(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes)==ERROR_SUCCESS)systemDark=light==0;
 }
 bool nextDark=theme==1||(theme==2&&systemDark);
 if(themeSeen&&nextDark!=dark){ImVec2 origin=themeOriginSet?themeOrigin:ImVec2(ox+990*scale,oy+34*scale);NativeUI::BeginReveal(origin,ImVec2(ox,oy),ImVec2(ox+1040*scale,oy+680*scale));}
 dark=nextDark;themeSeen=true;themeOriginSet=false;
 ink=dark?IM_COL32(231,237,248,255):IM_COL32(27,39,66,255);
 muted=dark?IM_COL32(175,190,213,255):IM_COL32(78,96,125,255);
 paper=dark?IM_COL32(22,34,58,255):IM_COL32(255,255,255,255);
 line=dark?IM_COL32(52,72,101,255):IM_COL32(189,206,225,255);
 blue=dark?IM_COL32(131,181,255,255):IM_COL32(44,107,211,255);
 gold=dark?IM_COL32(243,191,46,255):IM_COL32(240,176,0,255);
}
static ImVec2 P(float x,float y){return ImVec2(float(int(ox+x*scale+0.5f)),float(int(oy+y*scale+0.5f)));}
static void Box(float x,float y,float w,float h,ImU32 color,float radius=8){draw->AddRectFilled(P(x,y),P(x+w,y+h),Surface(color),radius*scale);}
static void Border(float x,float y,float w,float h){draw->AddRect(P(x,y),P(x+w,y+h),line,8*scale,0,scale);}
static OverlayHost::Texture Load(OverlayHost::Runtime* runtime,const char* name,int w,int h,bool mipmaps=false){
 wchar_t path[1024];DWORD n=GetModuleFileNameW(OverlayHost::Module(),path,1024);
 if(!n||n>=1024)return {};
 while(n&&path[n-1]!=L'\\')--n;path[n]=0;
 wchar_t file[128];if(!MultiByteToWideChar(CP_UTF8,0,name,-1,file,128))return {};
 if(n+32+lstrlenW(file)>=1024)return {};
 lstrcatW(path,L"Mod Menu\\assets\\");lstrcatW(path,file);
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)return {};
 const DWORD bytes=DWORD(w*h*4);DWORD got=0;BYTE* data=(BYTE*)VirtualAlloc(0,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!data){CloseHandle(f);return {};}
 bool ok=GetFileSize(f,0)==bytes&&ReadFile(f,data,bytes,&got,0)&&got==bytes;CloseHandle(f);
 auto texture=ok?OverlayHost::CreateTextureRGBA(runtime,w,h,data,mipmaps):OverlayHost::Texture{};VirtualFree(data,0,MEM_RELEASE);return texture;
}
static OverlayHost::Texture BuiltinFont(OverlayHost::Runtime* runtime){
 static_assert(FallbackFont::width==kFontAtlasWidth&&FallbackFont::height==kFontAtlasHeight,"Regenerate the embedded font when the atlas changes");
 const unsigned count=FallbackFont::width*FallbackFont::height;
 auto* pixels=(ImU32*)VirtualAlloc(nullptr,count*4,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!pixels)return {};
 unsigned at=0;bool ok=true;
 for(unsigned i=0;i<sizeof(FallbackFont::runs);i+=3){unsigned n=FallbackFont::runs[i]|unsigned(FallbackFont::runs[i+1])<<8;
  if(!n||n>count-at){ok=false;break;}ImU32 color=0xffffff|unsigned(FallbackFont::runs[i+2])<<24;
  for(unsigned j=0;j<n;++j)pixels[at++]=color;
 }
 auto texture=ok&&at==count?OverlayHost::CreateTextureRGBA(runtime,kFontAtlasWidth,kFontAtlasHeight,pixels):OverlayHost::Texture{};
 VirtualFree(pixels,0,MEM_RELEASE);return texture;
}
static void Assets(OverlayHost::Runtime* runtime){
 if(assetsAttempted)return;assetsAttempted=true;
 OverlayHost::BeginTextureBatch(runtime);
 font=Load(runtime,kFontAtlasFile,kFontAtlasWidth,kFontAtlasHeight);assetsReady=bool(font);
 // Keep the embedded atlas in the normal font descriptor slot. A fallback
 // font does not make missing installation files count as a healthy menu.
 if(!font)font=BuiltinFont(runtime);
 for(int i=0;i<kIconCount;++i){icons[i]=Load(runtime,kIcons[i].file,kIcons[i].width,kIcons[i].height,true);assetsReady=assetsReady&&bool(icons[i]);}
 assetsReady=OverlayHost::EndTextureBatch(runtime)&&assetsReady;
}
static float Width(const char* text,FontIndex f=Nunito18){text=Localization::Get(text);float w=0;for(const unsigned char* p=(const unsigned char*)text;*p;)w+=FindGlyph(f,Localization::Next(p))->advance;return w;}
static void Text(float x,float y,const char* text,FontIndex f=Nunito18,ImU32 color=ink,float maxWidth=0){
 text=Localization::Get(text);const Font& face=kFonts[f];float start=x;
 float ratio=maxWidth>0?Min(1.f,maxWidth/Max(1.f,Width(text,f))):1.f;
 for(const unsigned char* p=(const unsigned char*)text;*p;){
  unsigned code=Localization::Next(p);if(code=='\n'){x=start;y+=face.lineHeight;continue;}
  const Glyph* g=FindGlyph(f,code);
  if(g->width&&g->height){ImVec2 a=P(x+g->bearingX*ratio,y+(face.ascent+g->bearingY)*ratio);
   draw->AddImage(font.ImGuiRef(),a,P(x+(g->bearingX+g->width)*ratio,y+(face.ascent+g->bearingY+g->height)*ratio),
    ImVec2(float(g->x)/kFontAtlasWidth,float(g->y)/kFontAtlasHeight),
    ImVec2(float(g->x+g->width)/kFontAtlasWidth,float(g->y+g->height)/kFontAtlasHeight),color);}
  x+=g->advance*ratio;
 }
}
static float Paragraph(float x,float y,const char* text,float width,bool paint=true){
 text=Localization::Get(text);char row[1024];int used=0;float at=y;
 for(const char* p=text;*p;){const char* end=p;while(*end&&*end!=' ')++end;int n=int(end-p);
  if(used+n+2>=1024)break;
  int old=used;if(used)row[used++]=' ';for(int i=0;i<n;++i)row[used++]=p[i];row[used]=0;
  if(old&&Width(row)>width){row[old]=0;if(paint)Text(x,at,row,Nunito18,muted);at+=28;used=0;for(int i=0;i<n;++i)row[used++]=p[i];row[used]=0;}
  p=end;if(*p==' ')++p;
 }
 if(used){if(paint)Text(x,at,row,Nunito18,muted);at+=28;}return at;
}
static void Icon(IconIndex i,float x,float y,float size,ImU32 tint=IM_COL32(255,255,255,255)){float w=size,h=size;if(kIcons[i].width!=kIcons[i].height){ // aspect-fit non-square art (logo, wide signs)
float f=Min(size/kIcons[i].width,size/kIcons[i].height);w=kIcons[i].width*f;h=kIcons[i].height*f;x+=(size-w)/2;y+=(size-h)/2;}if(icons[i])draw->AddImage(icons[i].ImGuiRef(),P(x,y),P(x+w,y+h),ImVec2(0,0),ImVec2(1,1),tint);}
static bool Hit(const char* id,float x,float y,float w,float h,bool enabled=true){
 bool blocked=((settingsOpen||settingsSlide>0)&&!drawingSettings)||(languageOpen&&drawingSettings&&!drawingLanguage)||(drawingSettings&&!settingsOpen);
 NativeUI::NavRegister(id,P(x,y),ImVec2(w*scale,h*scale),enabled&&!blocked);
 ImGui::SetCursorScreenPos(blocked?ImVec2(-10000,-10000):P(x,y));bool clicked=ImGui::InvisibleButton(id,ImVec2(w*scale,h*scale));
 if(enabled&&ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
 return enabled&&!blocked&&clicked;
}
static ImU32 Mix(ImU32 a,ImU32 b,float t){ImU32 result=0;for(int shift=0;shift<32;shift+=8){float x=float((a>>shift)&255),y=float((b>>shift)&255);result|=ImU32(x+(y-x)*t+0.5f)<<shift;}return result;}
// Every Button uses three channels. Visiting all pages in one opening needs
// roughly 300 slots, so keep generous headroom; a full table snaps instead.
constexpr unsigned kMotionSlots=512;
struct Motion{unsigned id;float value;DWORD tick;};static Motion motions[kMotionSlots];
static float Animate(const char* id,unsigned channel,float target){
 unsigned hash=2166136261u;for(const char* p=id;*p;++p)hash=(hash^(unsigned char)*p)*16777619u;hash^=channel*0x9e3779b9u;if(!hash)hash=1;
 unsigned slot=hash%kMotionSlots;for(unsigned i=0;i<kMotionSlots;++i,slot=(slot+1)%kMotionSlots){auto& m=motions[slot];if(!m.id){m={hash,target,GetTickCount()};return target;}if(m.id==hash){DWORD now=GetTickCount();float step=Min(1.f,float(DWORD(now-m.tick))/150.f);m.tick=now;m.value+=(target-m.value)*step;if(m.value-target<.002f&&target-m.value<.002f)m.value=target;return m.value;}}
 return target;
}
static bool Button(const char* id,const char* label,float x,float y,float w,float h,bool enabled=true,bool primary=false){
 bool click=Hit(id,x,y,w,h,enabled),hover=enabled&&ImGui::IsItemHovered();
 float glow=Animate(id,1,hover?1.f:0.f),pressed=Animate(id,2,enabled&&ImGui::IsItemActive()?1.f:0.f),selected=Animate(id,3,primary?1.f:0.f);
 y+=pressed*1.5f;
 ImU32 base=Mix(Surface(IM_COL32(242,247,252,255)),gold,selected);
 ImU32 over=Mix(Surface(IM_COL32(222,236,250,255)),IM_COL32(255,194,28,255),selected);
 Box(x,y,w,h,enabled?Mix(base,over,glow):Surface(IM_COL32(230,236,243,255)),10);
 if(shading&&enabled){const int highlight=dark?int(10+32*selected+0.5f):42;draw->PushClipRect(P(x+1,y+1),P(x+w-1,y+h-1),true);draw->AddRectFilledMultiColor(P(x+1,y+1),P(x+w-1,y+h-1),IM_COL32(255,255,255,highlight),IM_COL32(255,255,255,highlight),IM_COL32(30,60,95,10),IM_COL32(30,60,95,10));draw->PopClipRect();}
 if(selected<.99f)draw->AddRect(P(x,y),P(x+w,y+h),Mix(line,gold,selected),10*scale,0,scale);
 float labelWidth=Min(w-16,Width(label));Text(x+(w-labelWidth)/2,y+(h-26)/2,label,Nunito18,enabled?Mix(ink,IM_COL32(27,39,66,255),selected):muted,w-16);return click;
}

// A short command acknowledgement must not flash every control disabled or
// remove the controller's selected item. Send still serializes requests.
// Zurg cannot be played in Toy Box levels outside the main world (interiors, parks).
static bool ZurgHint(){return state.gameReady&&state.toyBox&&!state.placesReady&&!state.zurgActive&&!state.teleportPending;}
static bool Ready(){return state.gameReady&&!state.teleportPending&&!state.spawnPending&&!state.npcPending&&!state.vehiclePending&&!state.skyPending;}
static void Send(TS3CheatAction action,uint32_t amount=0,float value=0){
 const TS3CheatMenuApi* api=OverlayHost::Backend();if(!api||!Ready()||pending)return;
 sequence=(sequence>state.lastCommandId?sequence:state.lastCommandId)+1;if(!sequence)sequence=1;
 TS3CheatCommand cmd{sequence,state.contextId,(uint32_t)action,amount,value};
 if(api->Submit(&cmd,sizeof(cmd))){pending=sequence;pendingAction=action;closeAfterPictomaticAdd=action==TS3CheatEquipWeapon&&amount==8&&!(state.weaponMask&(1u<<8));localMessage[0]=0;}
 else {lstrcpynA(localMessage,"Another action is finishing. Try again in a moment.",128);localMessageTick=GetTickCount();}
}
static void Panel(float x,float y,float w,float h,const char* title){if(shading){Box(x+1,y+4,w,h,IM_COL32(28,64,103,18));Box(x,y+2,w,h,IM_COL32(28,64,103,12));}Box(x,y,w,h,paper);Border(x,y,w,h);Text(x+20,y+16,title,Nunito18,muted);}
static void Rule(float x,float y,float w){draw->AddLine(P(x,y),P(x+w,y),line,scale);}
static void Heading(const char* title,const char* description){Text(240,90,title,Lilita30,ink,768);Text(240,134,description,Nunito18,muted,768);}
static void Switch(const char* id,float x,float y,bool on,TS3CheatAction action,bool available=true){
 bool clicked=Hit(id,x,y,48,26,Ready()&&available);
 float position=Animate(id,4,on?1.f:0.f);Box(x,y,48,26,Mix(Surface(IM_COL32(205,217,231,255)),blue,position),13);draw->AddCircleFilled(P(x+13+22*position,y+13),10*scale,IM_COL32(255,255,255,255));
 if(clicked)Send(action);
}
static void MovementSlider(bool running,float x,float y){
 const char* id=running?"run-slider":"jump-slider";const char* reset=running?"run-reset":"jump-reset";
 static float values[2]={1,1};static bool dragging[2]={};unsigned i=running?0:1;
 float max=running?5.f:3.f;bool controls=Ready()&&state.movementReady;
 if(!dragging[i]&&!pending)values[i]=Max(.5f,Min(max,running?state.runMultiplier:state.jumpMultiplier));
 float& value=values[i];Hit(id,x,y+25,334,30,controls);bool active=controls&&ImGui::IsItemActive();
 if(active){value=.5f+(max-.5f)*Max(0.f,Min(1.f,(ImGui::GetIO().MousePos.x-(ox+x*scale))/(334*scale)));value=float(int(value*10+.5f))*.1f;}
 int step=controls?NativeUI::NavAdjust(id):0;if(step){float next=Max(.5f,Min(max,float(int(value*10+.5f)+step)*.1f));if(next!=value){value=next;Send(running?TS3CheatRunMultiplier:TS3CheatJumpMultiplier,0,value);}}
 if(dragging[i]&&!active&&controls)Send(running?TS3CheatRunMultiplier:TS3CheatJumpMultiplier,0,value);dragging[i]=active;
 const bool visualReady=controls||(state.movementReady&&state.teleportPending);
 ImU32 tint=visualReady?blue:IM_COL32(173,186,202,255);
 Icon(running?Speed:Jump,x,y-2,28);Text(x+40,y,running?"Walk speed":"Jump height",Nunito18,visualReady?ink:muted,230);
 float fraction=(value-.5f)/(max-.5f);Box(x,y+37,334,4,IM_COL32(217,228,240,255),2);Box(x,y+37,334*fraction,4,tint,2);draw->AddCircleFilled(P(x+334*fraction,y+39),7*scale,tint);
 char label[32];int n=int(value*10+.5f);wsprintfA(label,"%d.%dx",n/10,n%10);Text(x+334-Width(label),y,label,Nunito18,visualReady?blue:muted);
 Text(x,y+66,running?"0.5 - 5.0x":"0.5 - 3.0x",Nunito18,muted);
 if(n!=10&&Button(reset,"Reset",x+200,y+62,134,34,controls)){value=1;n=10;Send(running?TS3CheatRunMultiplier:TS3CheatJumpMultiplier,0,1);}
 if(controls){
  const char* next=running?"jump-slider":"god-mode";
  NativeUI::NavLink(id,2,n!=10?reset:next);
  if(n!=10){NativeUI::NavLink(reset,1,id);NativeUI::NavLink(reset,2,next);}
  else NativeUI::NavRedirect(reset,id);
  if(!running)NativeUI::NavLink(id,1,int(values[0]*10+.5f)!=10?"run-reset":"run-slider");
 }else {NativeUI::NavRedirect(id,"Player");NativeUI::NavRedirect(reset,"Player");}
}
static void PlayerPage(){
 constexpr bool showFreezeTimers=true;
 Heading("Player","Movement, protection and flight controls, etc.");
 Panel(634,184,374,566,"Abilities");
 MovementSlider(true,654,230);Rule(654,338,334);
 MovementSlider(false,654,352);Rule(654,460,334);
 // Buzz's laser: his own wrist laser, forced the way the game-within-a-game level does it.
 const char* names[]={"God mode","Freeze timers","Buzz's laser"};
 const char* descriptions[]={"Protection from normal damage","Pause race and challenge timers",state.buzzLaserReady?"It's not a laser! It's a... It's a little light bulb that blinks":state.toyBox?"Switch to Buzz to use this":"Play as Buzz in Toy Box"};
 const char* ids[]={"god-mode","freeze-timers","buzz-laser"};const IconIndex art[]={Shield,Timer,BuzzLaser};
 const TS3CheatAction actions[]={TS3CheatGodToggle,TS3CheatFreezeTimers,TS3CheatBuzzLaser};
 const bool on[]={state.godEnabled!=0,state.timersFrozen!=0,state.buzzLaserEnabled!=0};
 const bool available[]={true,state.timersReady!=0,state.buzzLaserReady!=0};
 const int abilityCount=3;static_assert(showFreezeTimers,"laser row follows Freeze timers");
 for(int i=0;i<abilityCount;++i){float y=474+86.f*i;
  // Buzz's laser is locked like a Toy Box-only category when you are not Buzz in Toy Box.
  const bool locked=i==2&&!available[i];const ImU32 lockedInk=IM_COL32(133,146,162,255);
  if(locked)Icon(Lock,650,y-4,36);else Icon(art[i],654,y,28);Text(694,y,names[i],Nunito18,locked?lockedInk:ink,230);
  if(i==2)Paragraph(654,y+33,descriptions[i],334);else Text(654,y+33,descriptions[i],Nunito18,muted,334);
  Switch(ids[i],940,y+1,on[i],actions[i],available[i]);if(i<abilityCount-1)Rule(654,y+70,334);}
 NativeUI::NavLink("god-mode",1,int(state.jumpMultiplier*10+.5f)!=10?"jump-reset":"jump-slider");
 Panel(634,770,374,ZurgHint()?152.f:100.f,"Play as Zurg");Icon(Zurg,654,814,36);
 if(Button("zurg-transform",state.zurgActive?"Transform back":"Transform",710,814,278,38,Ready()&&state.zurgReady))Send(TS3CheatZurgTransform);
 if(ZurgHint())Paragraph(654,864,"Zurg is only available in the main Toy Box world and Story Mode.",334);
 Panel(240,184,374,454,"Flight");
 Icon(Fly,260,238,28);Text(300,238,"Flight / noclip",Nunito18,ink,230);Switch("flight",546,239,state.flightEnabled,TS3CheatFlightToggle);
 Text(260,274,"Fly freely through scenery",Nunito18,muted,334);Rule(260,310,334);
 const bool controls=Ready()&&state.flightEnabled;static bool dragging=false;
 if(!dragging&&!pending)slider=state.flightSpeed>=1?state.flightSpeed:15;
 Text(260,318,"Speed",Nunito18,state.flightEnabled?ink:muted);
 Hit("speed-slider",260,343,334,30,controls);bool active=controls&&ImGui::IsItemActive();
 if(active){slider=1+199*Max(0.f,Min(1.f,(ImGui::GetIO().MousePos.x-(ox+260*scale))/(334*scale)));slider=float(int(slider+0.5f));}
 int step=controls?NativeUI::NavAdjust("speed-slider"):0;if(step){float next=Max(1.f,Min(200.f,slider+step));if(next!=slider){slider=next;Send(TS3CheatFlightSpeed,0,slider);}}
 if(dragging&&!active&&controls)Send(TS3CheatFlightSpeed,0,slider);dragging=active;
 char speedText[32];wsprintfA(speedText,"%d",int(slider));Text(594-Width(speedText),318,speedText,Nunito18,state.flightEnabled?blue:muted);
 ImU32 tint=state.flightEnabled?blue:IM_COL32(173,186,202,255);
 Box(260,355,334,4,IM_COL32(217,228,240,255),2);Box(260,355,334*(slider-1)/199,4,tint,2);draw->AddCircleFilled(P(260+334*(slider-1)/199,357),7*scale,tint);
 if(int(slider+0.5f)!=15&&Button("reset-speed","Reset",460,385,134,34,controls)){slider=15;Send(TS3CheatFlightSpeed,0,15);}
 if(controls){
  NativeUI::NavLink("flight",2,"speed-slider");NativeUI::NavLink("speed-slider",1,"flight");
  if(int(slider+0.5f)!=15){NativeUI::NavLink("speed-slider",2,"reset-speed");NativeUI::NavLink("reset-speed",1,"speed-slider");}
  else NativeUI::NavRedirect("reset-speed","speed-slider");
 }else {NativeUI::NavRedirect("speed-slider","flight");NativeUI::NavRedirect("reset-speed","flight");}
 Text(260,389,state.flightEnabled?"1 - 200":"Flight is off",Nunito18,muted);Rule(260,430,334);
 Text(260,442,"Keyboard",Nunito18,blue);
 Text(260,466,"WASD: move   /   Shift: boost",Nunito18,muted,334);
 Text(260,490,"Space / E: up   Ctrl / Q: down",Nunito18,muted,334);
 Text(260,519,"Controller",Nunito18,blue);
 Text(260,542,"Move / look",Nunito18,muted,128);Icon(PadLeftStick,414,538,28);Text(462,542,"/",Nunito18,muted);Icon(PadRightStick,494,538,28);
 Text(260,570,"Up / down",Nunito18,muted,128);Icon(PadA,414,566,28);Text(462,570,"/",Nunito18,muted);Icon(PadB,494,566,28);
 Text(260,598,"Boost",Nunito18,muted);Icon(PadLeftStick,414,594,28);
 // Character: switch between Woody, Buzz and Jessie in place (Toy Box), shown
 // with the game's own face portraits. The current character is highlighted.
 Panel(240,658,374,212,"Character");
 // Tiles left to right: Buzz, Woody, Jessie. kind[] is the backend index (0 Woody, 1 Buzz, 2 Jessie).
 {const char* who[]={"Buzz","Woody","Jessie"};const char* ids[]={"character-buzz","character-woody","character-jessie"};const IconIndex face[]={Player,Woody,Jessie};const int kind[]={1,0,2};
  const bool can=Ready()&&state.characterReady!=0;
  for(int k=0;k<3;++k){const float x=260+115.f*k;const bool current=state.character==kind[k],lit=can||current;
   if(Button(ids[k],"",x,708,104,116,can,current)&&!current)Send(TS3CheatSwitchCharacter,kind[k]);
   Icon(face[k],x+22,716,60,lit?IM_COL32(255,255,255,255):IM_COL32(255,255,255,110));
   const float w=Min(96.f,Width(who[k]));Text(x+(104-w)/2,784,who[k],Nunito18,lit?ink:muted,96);}
  Text(260,836,state.characterReady||state.toyBox?"Switch without leaving Toy Box":"Play in Toy Box to switch",Nunito18,muted,334);}

}
static void CurrencyPage(){
 Heading("Currency","Add coins or Pixar Balls to your Toy Box.");
 const uint32_t amounts[]={100,1000,10000,100000};const char* labels[]={"+100","+1,000","+10,000","+100,000"};
 for(int i=0;i<4;++i)if(Button(labels[i],labels[i],240+196.f*i,184,180,38,true,i==amountIndex))amountIndex=i;
 const bool currencyQueued=pending&&(pendingAction==TS3CheatAddCoins||pendingAction==TS3CheatAddPixar);
 const bool allowed=Ready()&&state.currencyReady&&!state.currencyPending&&!currencyQueued;
 for(int i=0;i<2;++i){float x=240+394.f*i;Panel(x,246,374,180,i?"Pixar Balls":"Coins");Icon(i?PixarBalls:Coins,x+294,266,52);
  char balance[32];if(state.currencyReady)wsprintfA(balance,"%d",i?state.pixar:state.coins);else lstrcpynA(balance,"--",32);
  Text(x+20,310,balance,Nunito24);int current=i?state.pixar:state.coins;
  bool adding=state.currencyPending==uint32_t(i+1)||(pending&&pendingAction==uint32_t(i?TS3CheatAddPixar:TS3CheatAddCoins));
  const char* label=adding?(i?"Adding Pixar Balls":"Adding coins"):(i?"Add Pixar Balls":"Add coins");
  if(Button(i?"add-pixar":"add-coins",label,x+20,360,334,42,allowed&&current<2147483647,true))Send(i?TS3CheatAddPixar:TS3CheatAddCoins,amounts[amountIndex]);
 }
 Icon(Info,242,458,22);Text(278,456,state.currencyPending?"Close the menu and collect your currency pickup.":"Collect your currency, then save normally to keep it.",Nunito18,muted,730);
}
static void TeleportPage(){
 Heading("Teleport","Jump to a Toy Box place, or keep up to six return points.");
 // Places: native avatar start/drop-in points in the main Toy Box world.
 const char* places[]={"Town","Farm","Suburbs","Bandit hideout","Quarry","Stagecoach","Sid's Haunted House","Enchanted Glen","Zurg's Spaceport","Stunt Park","Skate Park"};
 // The game's own well-tunnel area signs (cut from their boards) and car-park logos.
 const IconIndex placeArt[]={PlaceTown,PlaceFarm,PlaceSuburbs,PlaceBandit,PlaceQuarry,PlaceStagecoach,PlaceHaunted,PlaceEnchanted,PlaceSpaceport,PlaceStuntPark,PlaceSkatePark};
 static_assert(sizeof(places)/sizeof(places[0])==TS3PlaceCount,"place names must match the backend table");
 const bool placesOn=Ready()&&state.placesReady;
 Panel(240,184,768,416,"Places");
 for(unsigned i=0;i<TS3PlaceCount;++i){float x=260+374.f*(i%2),y=226+58.f*(i/2);char id[32];wsprintfA(id,"place-%u",i);
  // Wide park logos get more width than the square area signs.
  if(kIcons[placeArt[i]].width>=kIcons[placeArt[i]].height*3/2)Icon(placeArt[i],x-7,y-2,48);else Icon(placeArt[i],x-2,y+4,36);Text(x+44,y+10,places[i],Nunito18,placesOn?ink:muted,222);
  if(Button(id,"Go",x+274,y+6,70,34,placesOn))Send(TS3CheatTeleportPlace,i);
  if(i/2<(TS3PlaceCount-1)/2){Rule(x,y+52,344);}
 }
 Text(260,566,state.placesReady||!Ready()?"Places use the game's own arrival points.":"Places are available in the main Toy Box world.",Nunito18,muted,728);
 const char* names[]={"Slot 1","Slot 2","Slot 3","Slot 4","Slot 5","Slot 6"};
 Text(240,624,"SAVED POINTS",Lilita30,ink,768);
 for(unsigned i=0;i<6;++i){float x=240+394.f*(i%2),y=670+158.f*(i/2);bool saved=(state.positionMask&(1u<<i))!=0,available=(state.returnMask&(1u<<i))!=0;
  Panel(x,y,374,142,names[i]);Icon(Position,x+316,y+12,36);
  Text(x+20,y+50,saved?(available?"Return point ready":"Saved in another world"):"Empty slot",Nunito18,available?blue:muted,334);
  char remember[32],back[32],clear[32];wsprintfA(remember,"remember-%u",i);wsprintfA(back,"return-%u",i);wsprintfA(clear,"forget-%u",i);
  if(Button(remember,saved?"Replace":"Remember",x+20,y+94,126,34,Ready()))Send(TS3CheatRememberPosition,i);
  if(Button(back,"Teleport",x+154,y+94,116,34,Ready()&&available,true))Send(TS3CheatReturnPosition,i);
  if(Button(clear,"Clear",x+278,y+94,76,34,Ready()&&saved))Send(TS3CheatClearPosition,i);
  if(i<2){char place[32];wsprintfA(place,"place-%u",i==0?TS3PlaceCount-1-((TS3PlaceCount-1)%2):TS3PlaceCount-1-(TS3PlaceCount%2));NativeUI::NavLink(place,2,remember);NativeUI::NavLink(remember,1,place);}
  NativeUI::NavLink(remember,4,available?back:clear);NativeUI::NavLink(back,3,remember);NativeUI::NavLink(back,4,clear);NativeUI::NavLink(clear,3,available?back:remember);
 }
 Text(240,1144,"Return within the saved world. Slots clear when you change saves.",Nunito18,muted,768);
}
static void VehiclesPage(){
 Heading("Vehicles","Choose a vehicle, mount or variant.");
 static unsigned variant[5]={};
 const char* names[]={"Sports car","Teeny Racer","Bullseye","Zurg's car","Dragon"};
 const unsigned counts[]={2,5,2,2,1};
 const unsigned ids[5][5]={{0,2},{1,6,7,8,9},{4,10},{3,11},{5}};
 const char* sports[]={"Standard","Monster"};
 const char* mini[]={"Blue & white","Black","Blue flames","Flowers","Red"};
 const char* horse[]={"Default","Bizarro"};
 for(unsigned i=0;i<5;++i){
  unsigned selected=ids[i][variant[i]];bool car=TS3VehicleIsCar(selected);
  float x=240+394.f*(i%2),y=184+116.f*(i/2),height=104.f;
  Box(x,y,374,height,paper);Border(x,y,374,height);
  Icon(i==0?(variant[i]?VehicleMonster:VehicleSports):i==1?VehicleMini:i==2?Bullseye:i==3?VehicleZurg:Dragon,x+10,y+4,52);
  Text(x+70,y+17,names[i],Nunito22,ink,166);
  bool available=(state.vehicleAvailableMask&(1u<<selected))!=0;
  const char* label=car?(!state.toyBox?"Summon":available?"Summon":(state.vehicleSummonedMask&(1u<<selected))?"Summoned":"Slots full"):"Spawn";
  // Same 94x32 button as NPCs; only grows left for a longer translated label.
  const float bw=Max(94.f,Width(label)+24);
  if(Button(names[i],label,x+356-bw,y+12,bw,32,Ready()&&state.vehiclesReady&&!state.vehiclePending&&available&&(!car||state.toyBox)))Send(TS3CheatSpawnVehicle,selected);
  if(counts[i]>1){
   char prev[32],next[32];wsprintfA(prev,"vehicle-prev-%u",i);wsprintfA(next,"vehicle-next-%u",i);
   if(Button(prev,"<",x+16,y+58,32,32,true))variant[i]=(variant[i]+counts[i]-1)%counts[i];
   if(Button(next,">",x+326,y+58,32,32,true))variant[i]=(variant[i]+1)%counts[i];
   NativeUI::NavLink(prev,4,next);NativeUI::NavLink(next,3,prev);
   NativeUI::NavLink(prev,1,names[i]);NativeUI::NavLink(next,1,names[i]);NativeUI::NavLink(names[i],2,next);
   Text(x+62,y+63,(i==0||i==3)?sports[variant[i]]:i==1?mini[variant[i]]:horse[variant[i]],Nunito18,muted,252);
  }else Text(x+20,y+66,i==3?"The Evil Emperor's battle car":"A fire-breathing mount",Nunito18,muted,334);
 }
 if(Button("clear-vehicles","Clear Vehicles / Mounts",240,536,296,40,Ready()&&state.vehiclesReady&&!state.vehiclePending&&state.spawnedVehicles>0))Send(TS3CheatClearVehicles);
 {char count[128];wsprintfA(count,Localization::Get("%u spawned vehicles / mounts"),state.spawnedVehicles);Text(556,544,count,Nunito18,muted,452);}
 Text(240,586,state.toyBox?((state.vehicleAvailableMask&15u)?"Cars use native slots. Clear Vehicles / Mounts to use them again.":"Car slots are full. Clear Vehicles / Mounts to use them again."):"Cars are Toy Box only.",Nunito18,muted,768);
}
static void WorldPage(){
 Heading("World","Choose one of Toy Box's original sky themes.");
 const char* names[]={"Western","Sid's Haunted House","Enchanted Glen","Zurg's Spaceport"};
 const IconIndex art[]={WorldWestern,WorldHaunted,WorldEnchanted,WorldZurg};
 for(unsigned i=0;i<4;++i){float x=240+394.f*(i%2),y=184+100.f*(i/2);bool applied=appliedSky==int(i);
  Box(x,y,374,84,paper);Border(x,y,374,84);Icon(art[i],x+12,y+16,52);Text(x+76,y+14,names[i],Nunito18,ink,278);
  if(Button(names[i],applied?"Applied":"Apply",x+238,y+44,122,30,Ready()&&state.skyReady&&!applied,!applied)){requestedSky=int(i);Send(TS3CheatWorldSky,i);}
 }
 Text(240,402,state.skyReady?"Western restores the default Toy Box sky.":"Sky themes are available in the main Toy Box world.",Nunito18,muted,768);
}
// Objects: the game's own carriable models (order matches the backend table).
// No icons until real game artwork exists for each prop.
static const char* const objectNames[TS3ObjectCount]={
 "Medium barrel","Large barrel","Breakable barrel","Large breakable barrel","Wooden crate","Breakable crate","Huge crate","Parcel","TNT barrel",
 "Ball","AA battery","D battery","9-volt battery","Andy's phone","Crushed cube","Huge crushed cube",
 "Small money bag","Money bag","Large money bag","Red stone",
 "Space battery","Space parcel A","Space parcel B",
 "Carrot","Corn","Pumpkin","Tomato"};
static const unsigned char objectCategories[TS3ObjectCount]={1,1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2, 3,3,3,3, 4,4,4, 5,5,5,5};
static const char* const objectCategoryNames[]={"All","Crates & barrels","Toys","Treasure","Space","Food"};
static char objectSearch[41];static int objectCategory;static float objectsBottom=600;static bool objectSearchActive=true;
// Fold letters to lowercase ASCII so "carre" finds "CarriÃ¨re" and "TNT" finds "tnt".
static unsigned Fold(unsigned c){
 if(c>='A'&&c<='Z')return c+32;
 if(c>=0xc0&&c<=0xff){
  static const char map[]="aaaaaaaceeeeiiiidnooooo/ouuuuyps";unsigned folded=(unsigned char)map[(c-0xc0)&31];
  return folded=='/'?c:folded;
 }
 return c;
}
static bool ObjectMatches(unsigned i){
 if(objectCategory&&objectCategories[i]!=objectCategory)return false;
 if(!objectSearch[0]||!objectSearchActive)return true;
 unsigned name[64];int n=0;for(const unsigned char* p=(const unsigned char*)Localization::Get(objectNames[i]);*p&&n<64;)name[n++]=Fold(Localization::Next(p));
 int q=lstrlenA(objectSearch);
 for(int start=0;start+q<=n;++start){int k=0;while(k<q&&name[start+k]==(unsigned char)objectSearch[k])++k;if(k==q)return true;}
 return false;
}
// Typed search text. Polled like Tab/Escape because games often skip WM_CHAR.
// Behaves like a normal search box: clicking into it selects everything (so Backspace
// or typing replaces it), a second click places the caret, double-click selects all,
// arrows/Home/End move, Delete, Ctrl+Backspace/Ctrl+arrows by word, Ctrl+V pastes, X clears.
static bool searchFocused,searchSelected;static DWORD searchBlink,searchClickTick;static int searchCaret;
static void FocusSearch(bool on){searchFocused=on;searchSelected=on&&objectSearch[0];searchCaret=lstrlenA(objectSearch);searchBlink=GetTickCount();}
static char SearchChar(int c){return c>='A'&&c<='Z'?char(c+32):(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='\''||c=='-'?char(c):0;}
static void SearchErase(int from,int to){int length=lstrlenA(objectSearch);for(int i=to;i<=length;++i)objectSearch[from+i-to]=objectSearch[i];searchCaret=from;}
static bool SearchInsert(char c){int length=lstrlenA(objectSearch);if(length>=40)return false;for(int i=length;i>=searchCaret;--i)objectSearch[i+1]=objectSearch[i];objectSearch[searchCaret++]=c;return true;}
static bool SearchClearSelection(){if(!searchSelected)return false;searchSelected=false;bool had=objectSearch[0]!=0;objectSearch[0]=0;searchCaret=0;return had;}
static int SearchWord(int from,int step){
 int length=lstrlenA(objectSearch),i=from;
 if(step<0){while(i>0&&objectSearch[i-1]==' ')--i;while(i>0&&objectSearch[i-1]!=' ')--i;}
 else {while(i<length&&objectSearch[i]!=' ')++i;while(i<length&&objectSearch[i]==' ')++i;}
 return i;
}
static bool SearchPaste(){
 bool changed=false;if(!OpenClipboard(nullptr))return false;
 if(HANDLE data=GetClipboardData(CF_TEXT))if(const char* text=(const char*)GlobalLock(data)){
  changed=SearchClearSelection();
  for(const char* q=text;*q&&*q!='\r'&&*q!='\n';++q)if(char c=SearchChar((unsigned char)*q))changed|=SearchInsert(c);
  GlobalUnlock(data);}
 CloseClipboard();return changed;
}
// One key press (or repeat). Returns true when the text changed.
static bool SearchKey(int vk,bool ctrl){
 int length=lstrlenA(objectSearch);if(searchCaret>length)searchCaret=length;if(searchCaret<0)searchCaret=0;
 if(vk==VK_LEFT||vk==VK_RIGHT||vk==VK_HOME||vk==VK_END){
  const bool back=vk==VK_LEFT||vk==VK_HOME;
  if(searchSelected){searchSelected=false;searchCaret=back?0:length;return false;}
  searchCaret=vk==VK_HOME?0:vk==VK_END?length:ctrl?SearchWord(searchCaret,back?-1:1):back?(searchCaret>0?searchCaret-1:0):(searchCaret<length?searchCaret+1:length);
  return false;
 }
 if(ctrl&&vk=='A'){if(length){searchSelected=true;searchCaret=length;}return false;}
 if(ctrl&&vk=='V')return SearchPaste();
 if(vk==VK_BACK||vk==VK_DELETE){
  if(searchSelected)return SearchClearSelection();
  if(vk==VK_BACK){if(!searchCaret)return false;SearchErase(ctrl?SearchWord(searchCaret,-1):searchCaret-1,searchCaret);return true;}
  if(searchCaret>=length)return false;SearchErase(searchCaret,ctrl?SearchWord(searchCaret,1):searchCaret+1);return true;
 }
 if(ctrl)return false;
 char c=vk==VK_SPACE?' ':vk==VK_OEM_7?'\'':vk==VK_OEM_MINUS?'-':SearchChar(vk);
 if(!c)return false;
 bool cleared=SearchClearSelection();return SearchInsert(c)||cleared;
}
// Holding a key repeats it (400 ms delay, then every 50 ms), like Backspace.
static void PollSearch(bool resync){
 static bool down[256];static int repeatVk;static DWORD repeatAt;
 const int extra[]={VK_SPACE,VK_OEM_7,VK_OEM_MINUS,VK_BACK,VK_DELETE,VK_LEFT,VK_RIGHT,VK_HOME,VK_END};
 bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,alt=(GetAsyncKeyState(VK_MENU)&0x8000)!=0,live=searchFocused&&!resync&&!alt,changed=false;DWORD now=GetTickCount();
 auto poll=[&](int vk){bool held=(GetAsyncKeyState(vk)&0x8000)!=0,edge=held&&!down[vk];down[vk]=held;
  // Ctrl+A and Ctrl+V act once; everything else (including Ctrl+Backspace/arrows) repeats.
  bool once=ctrl&&(vk=='A'||vk=='V');
  if(!held&&vk==repeatVk)repeatVk=0;
  if(!live)return;
  if(edge){changed|=SearchKey(vk,ctrl);repeatVk=once?0:vk;repeatAt=now+400;searchBlink=now;}
  else if(held&&vk==repeatVk&&LONG(now-repeatAt)>=0){changed|=SearchKey(vk,ctrl);repeatAt=now+50;searchBlink=now;}
 };
 for(int vk='A';vk<='Z';++vk)poll(vk);
 for(int vk='0';vk<='9';++vk)poll(vk);
 for(int vk:extra)poll(vk);
 if(!live)repeatVk=0;
 if(changed)pageScroll[3]=0;
}
static float SearchWidth(int count){float w=0;for(int i=0;i<count&&objectSearch[i];++i)w+=FindGlyph(Nunito18,(unsigned char)objectSearch[i])->advance;return w;}
// Click to focus; a click anywhere else unfocuses. Hidden while a controller is
// in use (no keyboard), and the categories and list move up into its place.
static void SearchBox(bool blocked){
 Hit("object-search",240,184,768,42,false); // mouse only: not a controller target
 const bool pressed=!blocked&&ImGui::IsItemActivated(),empty=!objectSearch[0];
 const float mouse=(ImGui::GetIO().MousePos.x-ox)/scale;
 const bool overClear=!blocked&&mouse>=966&&ImGui::IsItemHovered();
 if(pressed&&!empty&&mouse>=966){objectSearch[0]=0;searchSelected=false;searchCaret=0;searchFocused=true;searchBlink=GetTickCount();pageScroll[3]=0;}
 else if(pressed){DWORD now=GetTickCount();
  if(!searchFocused)FocusSearch(true);
  else if(DWORD(now-searchClickTick)<GetDoubleClickTime()){searchSelected=!empty;searchCaret=lstrlenA(objectSearch);}
  else {searchSelected=false;int length=lstrlenA(objectSearch),best=length;float bestGap=1e9f;
   for(int i=0;i<=length;++i){float gap=258+SearchWidth(i)-mouse;if(gap<0)gap=-gap;if(gap<bestGap){bestGap=gap;best=i;}}
   searchCaret=best;searchBlink=now;}
  searchClickTick=now;}
 else if(searchFocused&&ImGui::IsMouseClicked(0))FocusSearch(false);
 Box(240,184,768,42,paper,10);
 if(searchFocused)draw->AddRect(P(240,184),P(1008,226),blue,8*scale,0,2*scale);else Border(240,184,768,42);
 const bool none=!objectSearch[0];float textWidth=none?0:Min(700.f,SearchWidth(40));
 if(searchSelected&&!none)draw->AddRectFilled(P(256,191),P(260+textWidth,219),(blue&0x00ffffffu)|0x55000000u,3*scale);
 Text(258,192,none?(searchFocused?"":"Click to search objects"):objectSearch,Nunito18,none?muted:ink,700);
 const float caret=258+Min(700.f,SearchWidth(searchCaret))+1;
 if(searchFocused&&!searchSelected&&(DWORD(GetTickCount()-searchBlink)/530)%2==0)draw->AddLine(P(caret,192),P(caret,218),ink,2*scale);
 // Clear (X) appears once there is text, like a browser search field.
 // Hover: same light-blue square as the settings cog, and the X darkens.
 if(!none){const ImU32 cross=overClear?ink:muted;if(overClear)Box(971,189,32,32,IM_COL32(207,225,247,255),6);
  draw->AddLine(P(980,198),P(994,212),cross,2*scale);draw->AddLine(P(994,198),P(980,212),cross,2*scale);}
}
static void ObjectsPage(){
 Heading("Objects","Spawn a carryable prop in front of your character.");
 static DWORD lastSeen;DWORD now=GetTickCount();bool resync=DWORD(now-lastSeen)>250;lastSeen=now;
 const bool keyboard=!NativeUI::ControllerActive(),blocked=settingsOpen||settingsSlide>0;
 if(resync||!keyboard||blocked){searchFocused=searchSelected=false;}
 PollSearch(resync||blocked);
 const float shift=keyboard?0.f:-56.f;
 if(keyboard)SearchBox(blocked);
 float x=240;
 for(int k=0;k<6;++k){char id[24];wsprintfA(id,"object-category-%d",k);float w=Width(objectCategoryNames[k])+36;
  if(Button(id,objectCategoryNames[k],x,240+shift,w,36,true,objectCategory==k)&&objectCategory!=k){objectCategory=k;pageScroll[3]=0;}
  x+=w+10;
 }
 objectSearchActive=keyboard;
 const bool can=Ready()&&state.objectsReady&&!state.spawnPending;unsigned matches=0;
 for(unsigned i=0;i<TS3ObjectCount;++i)matches+=ObjectMatches(i);
 // One panel with dividers (like Teleport places): rounded cards per row cost
 // too many vertices once 27 objects scroll past.
 const float listTop=288+shift;
 if(matches)Panel(240,listTop,768,24+52.f*((matches+1)/2),"");
 unsigned shown=0;const float top=pageScroll[3]+68,bottom=pageScroll[3]+630;
 for(unsigned i=0;i<TS3ObjectCount;++i){if(!ObjectMatches(i))continue;
  float ox=260+374.f*(shown%2),oy=listTop+12+52.f*(shown/2);++shown;
  // Off-screen rows keep their controller target but draw nothing.
  if(oy+44<top||oy>bottom){if(Hit(objectNames[i],ox+250,oy+5,94,34,can))Send(TS3CheatSpawnObject,i);continue;}
  Text(ox,oy+10,objectNames[i],Nunito18,ink,236);
  if(Button(objectNames[i],"Spawn",ox+250,oy+5,94,34,can))Send(TS3CheatSpawnObject,i);
  if((shown-1)/2<(matches-1)/2)Rule(ox,oy+46,344); // no divider under the last row
 }
 float end=listTop+24+52.f*((shown+1)/2);
 if(!shown){Text(240,listTop+16,"No objects match your search.",Nunito18,muted,768);end=listTop+56;}
 if(Button("clear-objects","Clear spawned objects",240,end+8,256,40,Ready()&&state.objectsReady&&!state.spawnPending&&state.spawnedObjects>0))Send(TS3CheatClearObjects);
 {char count[128];wsprintfA(count,Localization::Get("%u spawned objects"),state.spawnedObjects);Text(520,end+16,count,Nunito18,muted,488);}
 objectsBottom=end+8+40+24;
}
static void WeaponsPage(){
 Heading("Weapons & tools","Add and equip an item from the Toy Box weapon cycle.");
 const char* names[]={"Lightning Rod","UFO Gun","Fairy Wand","Blaster","Pickaxe","Toolbox","Camera","Pict-O-Matic"};
 const IconIndex art[]={Lightning,UfoGun,Wand,Weapons,Pickaxe,Tools,Camera,Pictomatic};
 for(int i=0;i<8;++i){float x=240+394.f*(i%2),y=184+100.f*(i/2);bool owned=(state.weaponMask&(1u<<(i+1)))!=0;
  Box(x,y,374,84,paper);Border(x,y,374,84);Icon(art[i],x+14,y+22,40);Text(x+66,y+15,names[i],Nunito18,ink,164);Text(x+66,y+45,owned?"Available":"Not added",Nunito18,muted);
  if(Button(names[i],owned?"Equip":"Add & equip",x+238,y+26,122,32,Ready()&&state.toyBox&&state.movementReady))Send(TS3CheatEquipWeapon,i+1);
 }
 Text(240,592,"Added weapons stay unlocked when you save.",Nunito18,muted);
}
static void NpcsPage(){
 Heading("NPCs","Choose a character, outfit or enemy variant.");
 static unsigned variant[8]={0,0,0,0,0,0,0,5}; // Hamm is the initial Characters choice.
 const char* names[]={"Townsperson","Alien","Fairy","Bandit","Gargoyle","Zurg robot","Muffin","Characters"};
 const char* town[]={"Random outfit","Pioneer","Miner","City slicker","Farmer","Western man","Western woman","Dance man","Dance woman"};
 const char* alien[]={"Random outfit","Default","Commando","Ogre","Star Command 1","Star Command 2","Zombie","Mummy","Renegade","Deputy","Cowboy","Blacksmith","Mechanic","Tourist","Zurg fan","Disco","Strawberry"};
 const char* bandit[]={"Random bandit","Standard","Red outlaw","Green outlaw","Orange outlaw"};
 const char* robot[]={"Toy Box robot","Story robot","Captain robot"};
 const char* muffin[]={"Normal muffin","Giant muffin"};
 const char* characters[]={"Stinky Pete","Wheezy","Bo Peep","Mr. Pricklepants","Zurg","Hamm","Slinky","Rex","Lotso","Big Baby","Buttercup","Chunk","Sarge","Sparks","Stretch","Trixie","Twitch","Dolly Witch"};
 const IconIndex characterArt[]={NpcPete,NpcWheezy,NpcBoPeep,NpcPricklepants,NpcZurg,NpcHamm,NpcSlinky,NpcRex,NpcLotso,NpcBigBaby,NpcButtercup,NpcChunk,NpcSarge,NpcSparks,NpcStretch,NpcTrixie,NpcTwitch,NpcDollyWitch};
 const unsigned counts[]={9,17,1,5,1,3,2,18};
 const IconIndex banditArt[]={NpcBandit,NpcBandit,NpcRed,NpcGreen,NpcPurple};
 const IconIndex townArt[]={NpcTown,NpcPioneer,NpcMiner,NpcTown,NpcFarmer,NpcWestern,NpcTown,NpcDanceMan,NpcDanceWoman};
 const IconIndex alienArt[]={NpcAlien,NpcAlienDefault,NpcAlienCommando,NpcAlienOgre,NpcAlienStarCom1,NpcAlienStarCom2,NpcAlienZombie,NpcAlienMummy,NpcAlienRenegade,NpcAlienDeputy,NpcAlienCowboy,NpcAlienBlacksmith,NpcAlienMechanic,NpcAlienTourist,NpcAlienZurgFan,NpcAlienDisco,NpcAlienStrawberry};
 for(unsigned i=0;i<8;++i){float x=240+394.f*(i%2),y=184+116.f*(i/2);
  Box(x,y,374,104,paper);Border(x,y,374,104);
  Icon(i==0?townArt[variant[i]]:i==1?alienArt[variant[i]]:i==2?NpcFairy:i==3?banditArt[variant[i]]:i==4?NpcGargoyle:i==5?(variant[5]==2?NpcCaptain:NpcRobot):i==6?NpcMuffin:characterArt[variant[i]],x+14,y+13,34);
  Text(x+58,y+16,names[i],Nunito22,ink,195);
  if(Button(names[i],"Spawn",x+262,y+12,94,32,Ready()&&state.npcsReady&&!state.npcPending))Send(TS3CheatSpawnNpc,i|(variant[i]<<8));
  if(counts[i]>1){
   char prev[32],next[32];wsprintfA(prev,"npc-prev-%u",i);wsprintfA(next,"npc-next-%u",i);
   if(Button(prev,"<",x+16,y+58,32,32,true))variant[i]=(variant[i]+counts[i]-1)%counts[i];
   if(Button(next,">",x+326,y+58,32,32,true))variant[i]=(variant[i]+1)%counts[i];
   NativeUI::NavLink(prev,4,next);NativeUI::NavLink(next,3,prev);
   NativeUI::NavLink(prev,1,names[i]);NativeUI::NavLink(next,1,names[i]);NativeUI::NavLink(names[i],2,next);
   Text(x+62,y+63,i==0?town[variant[i]]:i==1?alien[variant[i]]:i==3?bandit[variant[i]]:i==5?robot[variant[i]]:i==6?muffin[variant[i]]:characters[variant[i]],Nunito18,muted,252);
  }else Text(x+18,y+64,i==1?"A little green visitor":i==2?"An enchanted companion":"An enemy from Sid's Haunted House",Nunito18,muted);
 }
 if(Button("clear-npcs","Clear spawned NPCs",240,666,256,42,Ready()&&state.npcsReady&&!state.npcPending&&state.spawnedNpcs>0))Send(TS3CheatClearNpcs);
 char count[128];wsprintfA(count,Localization::Get("%u spawned NPCs"),state.spawnedNpcs);Text(526,675,count,Nunito18,muted);
 Text(240,710,"Clear removes only NPCs spawned through this menu.",Nunito18,muted,768);
}
// Settings live in the registry (HKCU\Software\TS3ModMenu); no settings file. advapi32 is loaded
// at runtime like the System-theme check above.
static const WCHAR prefsKey[]=L"Software\\TS3ModMenu";
using RegGetFn=LSTATUS(WINAPI*)(HKEY,LPCWSTR,LPCWSTR,DWORD,LPDWORD,PVOID,LPDWORD);
using RegSetFn=LSTATUS(WINAPI*)(HKEY,LPCWSTR,LPCWSTR,DWORD,LPCVOID,DWORD);
static HMODULE Advapi(){static HMODULE m=LoadLibraryExW(L"advapi32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);return m;}
static RegGetFn RegGet(){static RegGetFn f=Advapi()?(RegGetFn)GetProcAddress(Advapi(),"RegGetValueW"):nullptr;return f;}
static RegSetFn RegSet(){static RegSetFn f=Advapi()?(RegSetFn)GetProcAddress(Advapi(),"RegSetKeyValueW"):nullptr;return f;}
static bool PrefText(const WCHAR* name,WCHAR* out,DWORD chars){RegGetFn f=RegGet();WCHAR value[32];DWORD bytes=sizeof(value);if(!f||f(HKEY_CURRENT_USER,prefsKey,name,RRF_RT_REG_SZ,nullptr,value,&bytes)!=ERROR_SUCCESS||lstrlenW(value)>=int(chars))return false;lstrcpyW(out,value);return true;}
static bool PrefNumber(const WCHAR* name,DWORD& out){RegGetFn f=RegGet();DWORD value=0,bytes=sizeof(value);if(!f||f(HKEY_CURRENT_USER,prefsKey,name,RRF_RT_REG_DWORD,nullptr,&value,&bytes)!=ERROR_SUCCESS)return false;out=value;return true;}
static bool SetPrefText(const WCHAR* name,const WCHAR* value){RegSetFn f=RegSet();return f&&f(HKEY_CURRENT_USER,prefsKey,name,REG_SZ,value,DWORD((lstrlenW(value)+1)*sizeof(WCHAR)))==ERROR_SUCCESS;}
static bool SetPrefNumber(const WCHAR* name,DWORD value){RegSetFn f=RegSet();return f&&f(HKEY_CURRENT_USER,prefsKey,name,REG_DWORD,&value,sizeof(value))==ERROR_SUCCESS;}
// A settings file only exists as a fallback, and only ever as ModMenu.ini (never CheatMenu.ini),
// in %APPDATA%\Disney\Toy Story 3 Complete Edition next to the game's own save folder.
static bool SettingsFile(WCHAR* path,const WCHAR* name){DWORD n=GetEnvironmentVariableW(L"APPDATA",path,MAX_PATH);if(!n||n>=MAX_PATH-90)return false;lstrcatW(path,L"\\Disney\\Toy Story 3 Complete Edition\\");lstrcatW(path,name);return true;}
static bool FileExists(const WCHAR* path){return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;}
// Always written fresh: only theme, language and menu key/button, nothing left over from old versions.
static void WriteSettingsFile(const WCHAR* path,const WCHAR* theme,const WCHAR* lang,int key,int button){
 DeleteFileW(path);WCHAR number[16];
 WritePrivateProfileStringW(L"Menu",L"Theme",theme,path);WritePrivateProfileStringW(L"Menu",L"Language",lang,path);
 wsprintfW(number,L"%d",key);WritePrivateProfileStringW(L"Menu",L"OpenKey",number,path);wsprintfW(number,L"%d",button);WritePrivateProfileStringW(L"Menu",L"OpenButton",number,path);
}
static void SavePreferences(){if(!prefsStore)return;
 const WCHAR* value=theme==1?L"Dark":theme==2?L"System":L"Light";const WCHAR* lang=Localization::codes[Localization::language];
 bool saved=SetPrefText(L"Theme",value);saved=SetPrefText(L"Language",lang)&&saved;saved=SetPrefNumber(L"OpenKey",DWORD(openKey))&&saved;saved=SetPrefNumber(L"OpenButton",DWORD(openButton))&&saved;
 WCHAR path[MAX_PATH];if(!SettingsFile(path,L"ModMenu.ini"))return;
 if(saved){if(FileExists(path))DeleteFileW(path);return;}   // registry holds everything: no file
 WriteSettingsFile(path,value,lang,openKey,openButton);     // registry unavailable: keep them in ModMenu.ini
}
// Versions up to 1.2.0 kept settings in CheatMenu.ini (older ones also left unused Clouds/Shading lines).
// Only theme, language and menu key/button are carried over: into the registry, then the old file is
// deleted (only after every value was written). If the registry can't be written, a fresh ModMenu.ini
// with just those settings replaces CheatMenu.ini.
static void MigrateSettingsFiles(){
 WCHAR path[MAX_PATH],old[MAX_PATH];if(!SettingsFile(path,L"ModMenu.ini")||!SettingsFile(old,L"CheatMenu.ini"))return;
 const WCHAR* from=FileExists(path)?path:FileExists(old)?old:nullptr;if(!from)return;   // ModMenu.ini is newer than any CheatMenu.ini
 WCHAR value[16],lang[16];GetPrivateProfileStringW(L"Menu",L"Theme",L"System",value,16,from);GetPrivateProfileStringW(L"Menu",L"Language",L"en",lang,16,from);
 UINT key=GetPrivateProfileIntW(L"Menu",L"OpenKey",VK_TAB,from),button=GetPrivateProfileIntW(L"Menu",L"OpenButton",1,from);
 if(SetPrefText(L"Theme",value)&&SetPrefText(L"Language",lang)&&SetPrefNumber(L"OpenKey",key)&&SetPrefNumber(L"OpenButton",button))DeleteFileW(path);
 else WriteSettingsFile(path,value,lang,int(key),int(button));
 DeleteFileW(old);
}
static void SettingsDrawer(){
 if(settingsSlide<=0)return;
 drawingSettings=true;
 const float x=620+40*(1-settingsSlide);
 const float opacity=settingsSlide;
 draw->PushClipRect(P(0,0),P(1040,680),true);
 Box(0,0,1040,680,IM_COL32(8,16,32,int(107*opacity+.5f)),12);
 if(Hit("settings-dismiss",0,0,x,680))SetSettings(false);
 unsigned firstDrawerVertex=NativeUI::vertexCount;
 Box(x-6,0,426,680,IM_COL32(8,16,32,24),12);
 Box(x,0,420,680,IM_COL32(242,247,252,255),12);
 Box(x,0,12,680,IM_COL32(242,247,252,255),0);
 Box(x,0,420,78,IM_COL32(201,227,246,255),12);
 Box(x,66,420,12,IM_COL32(201,227,246,255),0);
 Box(x,0,12,78,IM_COL32(201,227,246,255),0);
 Text(x+24,22,"Settings",Lilita36);
 if(Hit("settings-close",x+360,18,44,44))SetSettings(false);
 Box(x+360,21,44,44,IM_COL32(8,16,32,20),12);
 Box(x+360,18,44,44,paper,12);
 draw->AddRect(P(x+360,18),P(x+404,62),ImGui::IsItemHovered()?muted:line,12*scale,0,scale);
 // Companion SVG: 22px viewBox 24, M6 6 L18 18 / M18 6 L6 18, 2.6px clean stroke; no separate endpoint circles.
 const float half=5.5f,thickness=2.6f*22.f/24.f;
 draw->AddLine(P(x+382-half,40-half),P(x+382+half,40+half),ink,thickness*scale);
 draw->AddLine(P(x+382+half,40-half),P(x+382-half,40+half),ink,thickness*scale);
 // Scroll the body; keep its title and close button fixed.
 float baseOy=oy;ImVec2 mouse=ImGui::GetIO().MousePos;
 // Controls card: 6 controller rows or 5 keyboard rows; everything below follows it.
 const bool usingPad=NativeUI::ControllerActive()||capture==2;const float cardH=usingPad?344.f:312.f,shift=cardH-344.f;
 // Keep in step with the drawing below: Team (two members), then Credits: Menu artwork and three one-line credits.
 float creditEnd=Paragraph(x+36,973+shift,"Creator, feature direction and interface design.",338,false)+12;
 creditEnd=Paragraph(x+36,creditEnd+30,"Community support and beta testing.",338,false)+18+36+30;
 creditEnd=Paragraph(x+36,creditEnd,"Robot, Fairy and Gargoyle portraits, cut out from game scenes.",338,false)+18+76+76+58;
 const float maxScroll=Max(0.f,creditEnd+24-672);
 settingsScroll=Max(0.f,Min(settingsScroll,maxScroll));
 if(!languageOpen){float wheel=NativeUI::ControllerScroll();if(mouse.x>=P(x,0).x&&mouse.y>=P(0,84).y&&mouse.y<P(0,680).y)wheel+=ImGui::GetIO().MouseWheel;settingsScroll=Max(0.f,Min(maxScroll,settingsScroll-wheel*48));}
#ifdef UI_SELF_TEST
 testRenderedScroll=settingsScroll;testScrollLimit=maxScroll;
#endif
 draw->PushClipRect(P(x+12,84),P(x+406,672),true);oy-=settingsScroll*scale;
 Text(x+24,106,"APPEARANCE",Lilita30,ink,362);
 Box(x+20,151,380,180,paper,10);Border(x+20,151,380,180);
 Text(x+36,165,"Language",Nunito18,muted);
 drawingLanguage=true;
 if(Button("language-select",Localization::names[Localization::language],x+32,194,356,40))languageOpen=!languageOpen;
 draw->AddTriangleFilled(P(x+370,210),P(x+380,210),P(x+375,216),muted);
 drawingLanguage=false;
 Text(x+36,249,"Theme",Nunito18,muted);
 const char* labels[]={"System","Light","Dark"};const int values[]={2,0,1};float reveal[3];bool changing=NativeUI::Reveal(reveal);
 for(int i=0;i<3;++i)if(Button(labels[i],labels[i],x+32+118.f*i,280,112,40,true,theme==values[i])&&!changing){theme=values[i];themePoll=0;themeOrigin=P(x+88+118.f*i,300);themeOriginSet=true;SavePreferences();}

 // Controls: the key (mouse/keyboard) or controller button that opens and closes the menu.
 if(capture==1){
  for(int vk=1;vk<256;++vk){bool down=(GetAsyncKeyState(vk)&0x8000)!=0;
   if(down&&!captureHeld[vk]&&vk>=0x08&&!(vk>=0x10&&vk<=0x12)){
    if(vk==VK_ESCAPE){capture=0;break;}
    // A key event between Windows down and up cancels the Start menu (same dummy-key method as PowerToys).
    if(vk==VK_LWIN||vk==VK_RWIN){INPUT dummy[2]={};dummy[0].type=dummy[1].type=INPUT_KEYBOARD;dummy[0].ki.wVk=dummy[1].ki.wVk=0xFF;dummy[1].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(2,dummy,sizeof(INPUT));}
    const char* why=vk==openKey?nullptr:KeyConflict(vk);
    if(why){captureNote=why;captureNoteTick=GetTickCount();}else{openKey=vk;capture=0;SavePreferences();break;}}
   captureHeld[vk]=down;}
 }else if(capture==2){
  WORD pressed=NativeUI::PadPressed();
  if(pressed&0x2000)capture=0;
  else if(pressed&0x1030){captureNote="This button can't be used.";captureNoteTick=GetTickCount();}
  else for(int i=0;i<12;++i)if(pressed&padBits[i]){openButton=padBits[i];capture=0;SavePreferences();break;}
  if(!capture)NativeUI::PadCapture(false);
 }
 if(!capture)captureNote=nullptr;
 Text(x+24,355,"CONTROLS",Lilita30,ink,362);
 Box(x+20,400,380,cardH,paper,10);Border(x+20,400,380,cardH);
 Text(x+36,414,usingPad?"Menu button":"Menu key",Nunito18,muted,348);
 char keyLabel[64],keyName[64];const int padIndex=PadIndex(openButton);KeyName(openKey,keyName,64);
 if(capture==1)lstrcpynA(keyLabel,Localization::Get("Press a key..."),64);
 else if(capture==2)lstrcpynA(keyLabel,Localization::Get("Press a button..."),64);
 else if(usingPad)lstrcpynA(keyLabel,Localization::Get(padIndex>=0?padNames[padIndex]:"D-pad Up"),64);
 else lstrcpynA(keyLabel,keyName,64);
 const bool showIcon=usingPad&&!capture&&padIndex>=0;
 if(Button("open-binding",showIcon?"":keyLabel,x+32,443,356,40,true,capture!=0)){
  if(capture){capture=0;NativeUI::PadCapture(false);}
  else{capture=usingPad?2:1;captureNote=nullptr;for(int vk=0;vk<256;++vk)captureHeld[vk]=(GetAsyncKeyState(vk)&0x8000)!=0;NativeUI::PadCapture(capture==2);}
 }
 if(showIcon){float w=28+8+Min(300.f,Width(keyLabel)),left=x+32+(356-w)/2;Icon(padArt[padIndex],left,449,28);Text(left+36,450,keyLabel,Nunito18,ink,300);}
 const bool noted=capture&&captureNote&&DWORD(GetTickCount()-captureNoteTick)<2500;
 Text(x+36,493,noted?captureNote:capture==1?"Press Esc to cancel.":capture==2?"Press B to cancel.":usingPad?"Press A, then the new button.":"Click, then press the new key.",Nunito18,noted?IM_COL32(196,92,0,255):muted,348);
 // Reference list for the input in use (keyboard and mouse, or controller).
 draw->AddLine(P(x+36,529),P(x+384,529),line,scale);
 if(usingPad){
  const IconIndex art[]={padIndex>=0?padArt[padIndex]:PadUp,PadA,PadB,PadDpad,PadLeftStick,PadRightStick};
  const char* what[]={"Open / close menu","Select","Back / close","Move around","Adjust slider","Scroll"};
  for(int i=0;i<6;++i){Icon(art[i],x+36,541+32.f*i,26);Text(x+76,543+32.f*i,what[i],Nunito18,ink,308);}
 }else{
  char escName[32];KeyName(VK_ESCAPE,escName,32);
  const char* keys[]={keyName,escName,"Left click","Drag","Mouse wheel"};
  const char* what[]={"Open / close menu","Close menu","Select","Adjust slider","Scroll"};
  for(int i=0;i<5;++i){Text(x+36,543+32.f*i,keys[i],Nunito18,muted,124);Text(x+168,543+32.f*i,what[i],Nunito18,ink,216);}
 }
 Text(x+24,766+shift,"ABOUT",Lilita30,ink,362);
 Box(x+20,811+shift,380,creditEnd-811-shift,paper,10);Border(x+20,811+shift,380,creditEnd-811-shift);
 Text(x+36,829+shift,"Toy Story 3 Mod Menu",Nunito22,ink,344);
 char version[64];wsprintfA(version,Localization::Get("Version %s"),TS3_MENU_VERSION);Text(x+36,861+shift,version,Nunito18,muted);
 Text(x+36,907+shift,"Team",Nunito22,ink,344);
 Text(x+36,943+shift,"Hqyd3nr",Nunito18,ink);float cy=Paragraph(x+36,973+shift,"Creator, feature direction and interface design.",338)+12;
 Text(x+36,cy,"RichiePlayz2593",Nunito18,ink);cy=Paragraph(x+36,cy+30,"Community support and beta testing.",338)+18;
 Text(x+36,cy,"Credits",Nunito22,ink,344);cy+=36;
 Text(x+36,cy,"Menu artwork",Nunito18,ink);cy=Paragraph(x+36,cy+30,"Robot, Fairy and Gargoyle portraits, cut out from game scenes.",338)+18;
 Text(x+36,cy,"Game artwork",Nunito18,ink);Text(x+36,cy+30,"Disney / Pixar & Avalanche",Nunito18,muted);cy+=76;
 Text(x+36,cy,"Interface inspiration",Nunito18,ink);Text(x+36,cy+30,"Toy Story 3 Companion",Nunito18,muted);cy+=76;
 Text(x+36,cy,"Fonts",Nunito18,ink);Text(x+36,cy+30,"Lilita One & Nunito (SIL OFL)",Nunito18,muted);
 oy=baseOy;draw->PopClipRect();

 // Drag handle also allows scrolling without a wheel.
 float track=568,thumb=Max(70.f,track*588/(588+maxScroll)),top=94+(track-thumb)*(settingsScroll/Max(1.f,maxScroll));
 Box(x+409,94,5,track,line,2);
 if(Hit("settings-scroll",x+403,94,16,track)&&maxScroll>0){}
 if(ImGui::IsItemActive())settingsScroll=Max(0.f,Min(maxScroll,(mouse.y/scale-oy/scale-94-thumb*.5f)/(track-thumb)*maxScroll));
 Box(x+407,top,8,thumb,muted,4);
 if(languageOpen){
  drawingLanguage=true;float dy=234-settingsScroll;
  // The dropdown owns pointer input until a choice or dismissal.
  if(Hit("language-dismiss",x,84,420,596))languageOpen=false;
  Box(x+32,dy,356,200,paper,8);Border(x+32,dy,356,200);
  for(int i=0;i<Localization::count;++i){char id[32];wsprintfA(id,"language-%d",i);
   if(Button(id,Localization::names[i],x+36,dy+4+38.f*i,348,36,true,Localization::language==i)){Localization::language=i;languageOpen=false;SavePreferences();}}
  drawingLanguage=false;
 }
 for(unsigned i=firstDrawerVertex;i<NativeUI::vertexCount;++i){auto& c=NativeUI::vertices[i].color;c=(c&0xffffff)|(unsigned(float(c>>24)*opacity+.5f)<<24);}
 draw->PopClipRect();drawingSettings=false;
}
static void Star(float x,float y,float size){
 const float points[10][2]={{0,-1},{.235f,-.324f},{.951f,-.309f},{.38f,.124f},{.588f,.809f},{0,.4f},{-.588f,.809f},{-.38f,.124f},{-.951f,-.309f},{-.235f,-.324f}};
 for(int i=0;i<10;++i){int j=(i+1)%10;draw->AddTriangleFilled(P(x,y),P(x+points[i][0]*size,y+points[i][1]*size),P(x+points[j][0]*size,y+points[j][1]*size),IM_COL32(243,191,46,110));}
}
static void Gear(float x,float y){
 for(int i=0;i<4;++i){const float dx[]={1,0,.707107f,.707107f},dy[]={0,1,.707107f,-.707107f};draw->AddLine(P(x-dx[i]*12,y-dy[i]*12),P(x+dx[i]*12,y+dy[i]*12),muted,5*scale);}
 draw->AddCircleFilled(P(x,y),9*scale,muted);draw->AddCircleFilled(P(x,y),4*scale,paper);
}
void Preload(OverlayHost::Runtime* runtime){Assets(runtime);}
// When the game last reported gameplay. A report that is only late because of one long frame (the game
// was busy, not in a loading screen) keeps the last good state for up to 3 s instead of closing the menu.
static DWORD readyTick;
void Opened(OverlayHost::Runtime*){readyTick=GetTickCount();capture=0;NativeUI::PadCapture(false);themeSeen=false;for(auto& m:motions)m={};pending=0;closeAfterPictomaticAdd=false;localMessage[0]=0;titleDragging=false;}
void Closed(OverlayHost::Runtime*){titleDragging=false;capture=0;NativeUI::PadCapture(false);}
static void LoadPreferences(){
 if(prefsLoaded)return;prefsLoaded=true;if(!prefsStore)return;
 WCHAR value[16]=L"System",lang[16]=L"en";DWORD key=VK_TAB,button=1;
 bool registry=PrefText(L"Theme",value,16);if(!registry){MigrateSettingsFiles();registry=PrefText(L"Theme",value,16);}
 WCHAR path[MAX_PATH];
 if(registry){PrefText(L"Language",lang,16);PrefNumber(L"OpenKey",key);PrefNumber(L"OpenButton",button);}
 else if(SettingsFile(path,L"ModMenu.ini")&&FileExists(path)){   // fallback file when the registry can't be used
  GetPrivateProfileStringW(L"Menu",L"Theme",L"System",value,16,path);GetPrivateProfileStringW(L"Menu",L"Language",L"en",lang,16,path);
  key=GetPrivateProfileIntW(L"Menu",L"OpenKey",VK_TAB,path);button=GetPrivateProfileIntW(L"Menu",L"OpenButton",1,path);}
 theme=lstrcmpiW(value,L"Dark")==0?1:lstrcmpiW(value,L"System")==0?2:0;
 for(int i=0;i<Localization::count;++i)if(lstrcmpiW(lang,Localization::codes[i])==0)Localization::language=i;
 openKey=key==VK_TAB||(key<256&&!KeyConflict(int(key)))?int(key):VK_TAB;
 openButton=button>0&&button<0x10000&&PadIndex(WORD(button))>=0?WORD(button):WORD(1);
}
int OpenKey(){LoadPreferences();return openKey;}
// The open key or button by name, in English, for the session log ("Tab key", "D-pad Up").
void OpenControlName(bool pad,char* out,int size){
 LoadPreferences();if(pad){int i=PadIndex(openButton);lstrcpynA(out,i>=0?padNames[i]:"controller button",size);return;}
 char key[48];KeyName(openKey,key,48);wsprintfA(out,"%.40s key",key);
}
bool Typing(){return searchFocused&&page==3&&!settingsOpen&&!NativeUI::ControllerActive();}
WORD OpenButton(){LoadPreferences();return openButton;}
bool Capturing(){return capture!=0;}
void DrawAssetWarning(OverlayHost::Runtime* runtime){
 // The build notice can appear before the menu ever opened: load its font and language.
 LoadPreferences();Assets(runtime);
 // Passive notice: no controls or cursor capture, and no external artwork.
 draw=ImGui::GetForegroundDrawList();auto viewport=ImGui::GetIO().DisplaySize;
 float oldScale=scale,oldX=ox,oldY=oy;
 scale=Min(1.f,Min((viewport.x-32)/728,(viewport.y-32)/264));
 ox=(viewport.x-728*scale)/2;oy=Min(48.f,(viewport.y-264*scale)/2);
 draw->AddRectFilled(P(3,6),P(731,270),IM_COL32(10,24,44,80),12*scale);
 draw->AddRectFilled(P(0,0),P(728,264),IM_COL32(245,249,254,255),12*scale);
 draw->AddRect(P(0,0),P(728,264),IM_COL32(189,206,225,255),12*scale,0,scale);
 draw->AddRectFilled(P(0,0),P(728,7),IM_COL32(240,176,0,255),3*scale);
 draw->AddTriangleFilled(P(42,25),P(22,61),P(62,61),IM_COL32(240,176,0,255));
 draw->AddRectFilled(P(40,36),P(44,48),IM_COL32(27,39,66,255),1*scale);
 draw->AddCircleFilled(P(42,54),2*scale,IM_COL32(27,39,66,255));
 if(font){
  const bool build=OverlayHost::BuildWarning();
  Text(82,25,build?"UNSUPPORTED GAME VERSION":"MOD MENU UNAVAILABLE",Lilita30,IM_COL32(27,39,66,255),618);
  Text(28,83,build?"This Mod Menu does not support this game version.":"Some menu files are missing or damaged.",Nunito22,IM_COL32(27,39,66,255),672);
  Text(28,139,build?"Check the mod page for a Mod Menu version that matches your game.":"Restore the Mod Menu folder beside the game executable, then restart the game.",Nunito18,IM_COL32(78,96,125,255),672);
  draw->AddLine(P(28,199),P(700,199),IM_COL32(189,206,225,255),scale);
  Text(28,213,"Gameplay controls are available. This notice closes automatically.",Nunito18,IM_COL32(78,96,125,255),672);
 }
 scale=oldScale;ox=oldX;oy=oldY;
}
void Reset(OverlayHost::Runtime* runtime){
 OverlayHost::WaitForTextures(runtime);OverlayHost::DestroyTexture(runtime,font);
 for(auto& icon:icons)OverlayHost::DestroyTexture(runtime,icon);assetsAttempted=assetsReady=false;
}
void Draw(OverlayHost::Runtime* runtime){
 LoadPreferences();
 UpdateTheme();UpdateSettings();
 Assets(runtime);
 const TS3CheatMenuApi* api=OverlayHost::Backend();TS3CheatSnapshot next{};
 bool got=api&&api->ReadSnapshot(&next,sizeof(next));
 bool late=got&&!next.gameReady&&DWORD(GetTickCount()-next.tick)>1500&&DWORD(GetTickCount()-readyTick)<3000;
 if(got&&!late){
  state=next;
  appliedSky=state.skyReady&&state.activeSky>=0&&state.activeSky<4?state.activeSky:-1;
  if(pending&&state.lastCommandId>=pending){
   // Close once for this accepted travel command, never for a stale snapshot
   // seen when the user opens the menu again after a transition.
   // Zurg and Player > Character switch in place: the menu stays open for them.
   if(state.teleportPending&&pendingAction!=TS3CheatZurgTransform&&pendingAction!=TS3CheatSwitchCharacter)OverlayHost::Close("teleporting");
   bool closePictomatic=closeAfterPictomaticAdd&&state.lastCommandId==pending&&state.result==TS3CheatOk;
   pending=0;closeAfterPictomaticAdd=false;
   if(closePictomatic){OverlayHost::Close("Pict-O-Matic added");return;}
  }
 }
 else if(!got)state.gameReady=state.currencyReady=state.savedPosition=0;
 if(state.gameReady)readyTick=GetTickCount();
 if(!state.gameReady){OverlayHost::Close("gameplay ended (loading, cutscene or a game menu)");return;}
 if(!assetsReady){OverlayHost::ShowAssetWarning();DrawAssetWarning(runtime);return;}
 // Desktop layout uses full-size controls rather than shrinking the old cards.
 const ImVec2 viewport=ImGui::GetIO().DisplaySize;
 // Never grow past 1x: on large displays the menu stays compact instead of filling the screen.
 scale=Min(1.f,Min((viewport.x-48)/1040,Min((viewport.y-48)/680,viewport.y*0.90f/680)));
 if(scale<0.25f)return;
 if(!placed){windowX=(viewport.x-1040*scale)/2;windowY=(viewport.y-680*scale)/2;placed=true;}
 if(titleDragging){if(!ImGui::IsMouseDown(0))titleDragging=false;else{windowX=ImGui::GetIO().MousePos.x-grabX;windowY=ImGui::GetIO().MousePos.y-grabY;}}
 windowX=Max(16.f,Min(windowX,viewport.x-1040*scale-16));windowY=Max(16.f,Min(windowY,viewport.y-680*scale-16));ox=windowX;oy=windowY;
 ImGui::SetNextWindowPos(P(0,0),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(1040*scale,680*scale),ImGuiCond_Always);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
 ImGui::Begin("##TS3Cheats",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoNav);
 if(NativeUI::BackPressed()){if(languageOpen)languageOpen=false;else if(settingsOpen)SetSettings(false);else {OverlayHost::Close("Back pressed");return;}}
 NativeUI::NavScope(languageOpen?2:settingsOpen?1:0);
 draw=ImGui::GetWindowDrawList();Hit("title-drag",0,0,960,68);
 if(ImGui::IsItemHovered()||titleDragging)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
 if(ImGui::IsItemActivated()){titleDragging=true;grabX=ImGui::GetIO().MousePos.x-ox;grabY=ImGui::GetIO().MousePos.y-oy;}
 if(shading){Box(3,7,1040,680,IM_COL32(10,29,58,48),14);Box(1,3,1040,680,IM_COL32(10,29,58,30),13);}
 Box(0,0,1040,680,IM_COL32(242,247,252,255),12);Box(0,0,208,680,IM_COL32(228,239,250,255),12);Box(196,0,12,680,IM_COL32(228,239,250,255),0);
 // Companion styles.css: two offset cloud sizes in a 480 x 420 repeating tile.
 Box(208,68,831,562,IM_COL32(201,227,246,255),0);
 draw->PushClipRect(P(209,68),P(1039,630),true);
 if(shading)draw->AddRectFilledMultiColor(P(209,68),P(1039,630),IM_COL32(255,255,255,dark?5:28),IM_COL32(255,255,255,dark?5:28),IM_COL32(38,83,135,10),IM_COL32(38,83,135,10));
 if(dark){for(int row=0;row<5;++row)for(int col=0;col<7;++col)Star(238+128.f*col+(row%2?40.f:0.f),97+126.f*row,9.f+float((row+col)%3)*3.f);}
 else for(int row=-1;row<2;++row)for(int col=-1;col<3;++col){float x=208+480.f*col,y=69+420.f*row;ImU32 cloud=IM_COL32(234,245,253,255);
  draw->AddCircleFilled(P(x+70,y+92),30*scale,cloud);draw->AddCircleFilled(P(x+112,y+70),42*scale,cloud);draw->AddCircleFilled(P(x+160,y+88),34*scale,cloud);
  draw->AddEllipseFilled(P(x+115,y+112),ImVec2(92*scale,26*scale),cloud);
  draw->AddCircleFilled(P(x+352,y+318),24*scale,cloud);draw->AddCircleFilled(P(x+386,y+300),34*scale,cloud);draw->AddCircleFilled(P(x+424,y+316),26*scale,cloud);
  draw->AddEllipseFilled(P(x+388,y+336),ImVec2(72*scale,20*scale),cloud);
 }
 draw->PopClipRect();
 Border(0,0,1040,680);Rule(208,68,832);draw->AddLine(P(208,0),P(208,680),line,scale);
 // Beta builds (MenuVersion.h says "Beta") show a small Beta label above the title; releases don't.
 Icon(GameLogo,18,10,48);if(BetaBuild()){Text(79,4,"Beta",Lilita20,IM_COL32(229,165,10,255));Text(78,25,"Mods",Lilita30);}else Text(78,17,"Mods",Lilita30);
 const char* tabs[]={"Player","Currency","Teleport","Objects","Weapons","NPCs","Vehicles","World"};const IconIndex art[]={PlayerActions,Coins,Teleport,Tools,Weapons,NpcTown,VehicleMini,WorldCategory};
 if((page==1||page==4||page==7)&&!state.toyBox)page=0;
 Text(240,23,"Mod menu",Nunito18,muted,154);Text(402,23,"/",Nunito18,muted);Text(424,23,tabs[page],Nunito18,ink,370);
 Box(814,20,128,30,dark?IM_COL32(46,65,94,255):IM_COL32(224,235,247,255),6);const char* mode=state.gameReady?(state.toyBox?"Toy Box":"Story Mode"):"No game";Text(822,22,mode,Nunito18,muted,112);
 if(Hit("settings-cog",966,14,48,40)){SetSettings(true);titleDragging=false;localMessage[0]=0;}if(settingsOpen||ImGui::IsItemHovered())Box(966,14,48,40,IM_COL32(207,225,247,255),8);Gear(990,34);
 Text(20,90,"CATEGORIES",Nunito18,muted);
 for(int i=0;i<8;++i){float y=126+60.f*i;bool available=(i!=1&&i!=4&&i!=7)||state.toyBox;bool click=Hit(tabs[i],12,y,184,52,available);
  if(!available)Box(12,y,184,52,IM_COL32(224,231,239,255),6);
  else if(i==page||ImGui::IsItemHovered())Box(12,y,184,52,i==page?IM_COL32(207,225,247,255):IM_COL32(218,234,250,255),6);
  if(available&&i==page)Box(12,y+12,3,28,blue,1);
  if(available){if(i==6&&icons[VehicleMini])draw->AddImage(icons[VehicleMini].ImGuiRef(),P(24,y+26-9.7895f),P(60,y+26+9.7895f),ImVec2(0.058594f,0.246094f),ImVec2(0.949219f,0.730469f));
  // World planet art carries a wide glow; crop to it and draw at the car's width so it reads the same size.
  else if(i==7&&icons[WorldCategory])draw->AddImage(icons[WorldCategory].ImGuiRef(),P(24,y+26-14.069f),P(58,y+26+14.069f),ImVec2(0.046875f,0.125f),ImVec2(0.953125f,0.875f));else Icon(art[i],24,y+12,28);}else Icon(Lock,20,y+8,36);Text(64,y+(available?13:2),tabs[i],Nunito18,!available?IM_COL32(133,146,162,255):i==page?blue:ink,128);
  if(!available)Text(64,y+27,"Toy Box only",Nunito18,IM_COL32(133,146,162,255),128);if(click){page=i;localMessage[0]=0;}
 }
 NativeUI::NavHome(tabs[page]);
 const float contentBottom[]={886,500,1182,600,630,746,620,440};float maxScroll=Max(0.f,(page==3?objectsBottom:contentBottom[page]+(page==0&&ZurgHint()?52.f:0.f))-630);
 float wheel=0;ImVec2 pointer=ImGui::GetIO().MousePos;
 if(!settingsOpen&&settingsSlide==0){wheel=NativeUI::ControllerScroll();if(pointer.x>=P(208,0).x&&pointer.x<P(1040,0).x&&pointer.y>=P(0,68).y&&pointer.y<P(0,630).y)wheel+=ImGui::GetIO().MouseWheel;}
 pageScroll[page]=Max(0.f,Min(maxScroll,pageScroll[page]-wheel*48));
 draw->PushClipRect(P(209,68),P(1039,630),true);float contentOy=oy;oy-=pageScroll[page]*scale;
 if(page==0)PlayerPage();else if(page==1)CurrencyPage();else if(page==2)TeleportPage();else if(page==3)ObjectsPage();else if(page==4)WeaponsPage();else if(page==5)NpcsPage();else if(page==6)VehiclesPage();else if(page==7)WorldPage();
 oy=contentOy;draw->PopClipRect();
 if(maxScroll>0){
  // Match the Settings track, rounded handle and pointer dragging.
  float track=542,thumb=Max(70.f,track*562/(562+maxScroll)),top=78+(track-thumb)*pageScroll[page]/maxScroll;
  Box(1029,78,5,track,line,2);
  Hit("page-scroll",1023,78,16,track,!settingsOpen&&settingsSlide==0);
  if(ImGui::IsItemActive())pageScroll[page]=Max(0.f,Min(maxScroll,(pointer.y/scale-oy/scale-78-thumb*.5f)/(track-thumb)*maxScroll));
  Box(1027,top,8,thumb,muted,4);
 }
 Rule(208,630,832);
 if(localMessage[0]&&DWORD(GetTickCount()-localMessageTick)>=4000)localMessage[0]=0;
 char keyText[48],unavailable[160];KeyName(openKey,keyText,48);wsprintfA(unavailable,Localization::Get("Mod controls are unavailable. Press %s to close."),keyText);
 const char* message=localMessage[0]?localMessage:pending?"Applying...":!api?unavailable:state.teleportPending?"Travelling to your remembered position...":!state.gameReady?"":state.status[0]?state.status:"Ready. Changes apply to your current character.";
 // Keep a long status inside the footer instead of drawing over the frame.
 bool warning=localMessage[0]||!api||(!pending&&!state.teleportPending&&state.gameReady&&state.status[0]&&state.result!=TS3CheatNone&&state.result!=TS3CheatOk&&state.result!=TS3CheatBusy);
 if(warning)Icon(Warning,240,643,24);
 // Report success only once the game's own sky selection reads back as the request.
 if(lstrcmpA(message,"Sky theme requested.")==0&&requestedSky>=0&&appliedSky==requestedSky)message="Sky theme applied.";
 Text(warning?274:240,646,message,Nunito18,muted,warning?726:760);
#ifdef UI_SELF_TEST
 testFooter=message;
#endif
 SettingsDrawer();
 ImGui::End();ImGui::PopStyleVar(2);
 // Original game cursor artwork; do not draw the ImGui cursor on top.
 NativeUI::NavFinish();NativeUI::MarkContentEnd();
 IconIndex cursor=GameCursor;
 ImVec2 mouse=ImGui::GetIO().MousePos;
 ImGui::GetIO().MouseDrawCursor=false;ImGui::SetMouseCursor(ImGuiMouseCursor_None);
 if(!NativeUI::ControllerActive()&&mouse.x>=0&&mouse.y>=0){float hx=2.f,hy=2.f;ImVec2 top(mouse.x-hx,mouse.y-hy);ImGui::GetForegroundDrawList()->AddImage(icons[cursor].ImGuiRef(),top,ImVec2(top.x+32,top.y+32));}
}
#ifdef UI_SELF_TEST
float TestPageScroll(){return pageScroll[page];}
void TestObjectFilter(const char* text,int category){lstrcpynA(objectSearch,text,41);searchCaret=lstrlenA(objectSearch);objectCategory=category;pageScroll[3]=0;}
bool TestSearchFocused(){return searchFocused;}
void TestPointAt(float x,float y){NativeUI::TestHover(x<0?ImVec2(-1000,-1000):P(x,y));}
bool TestSearchEditing(){
 auto set=[](const char* t){lstrcpynA(objectSearch,t,41);searchSelected=false;searchCaret=lstrlenA(objectSearch);};
 // Ctrl+A then Backspace clears; Ctrl+A then a letter replaces; repeats append.
 set("tnt");SearchKey('A',true);if(!searchSelected)return false;
 if(!SearchKey(VK_BACK,false)||objectSearch[0]||searchSelected)return false;
 set("tnt");SearchKey('A',true);SearchKey('C',false);if(lstrcmpA(objectSearch,"c"))return false;
 SearchKey('A',false);SearchKey('A',false);if(lstrcmpA(objectSearch,"caa"))return false;
 SearchKey(VK_BACK,false);if(lstrcmpA(objectSearch,"ca"))return false;
 objectSearch[0]=0;searchCaret=0;SearchKey('A',true);if(searchSelected)return false; // nothing to select
 // Caret editing: arrows, Home/End, Delete, insert in the middle.
 set("crate");SearchKey(VK_LEFT,false);SearchKey(VK_LEFT,false);SearchKey('X',false);if(lstrcmpA(objectSearch,"craxte"))return false;
 SearchKey(VK_HOME,false);SearchKey(VK_DELETE,false);if(lstrcmpA(objectSearch,"raxte")||searchCaret)return false;
 SearchKey(VK_END,false);SearchKey(VK_DELETE,false);if(lstrcmpA(objectSearch,"raxte")||searchCaret!=5)return false;
 // Selection: Left/Right collapse to the start/end; Delete clears it.
 set("ball");SearchKey('A',true);SearchKey(VK_LEFT,false);if(searchSelected||searchCaret)return false;
 SearchKey('A',true);SearchKey(VK_DELETE,false);if(objectSearch[0])return false;
 // Ctrl+Backspace / Ctrl+Delete / Ctrl+arrows work by word.
 set("large money bag");SearchKey(VK_BACK,true);if(lstrcmpA(objectSearch,"large money "))return false;
 SearchKey(VK_BACK,true);if(lstrcmpA(objectSearch,"large "))return false;
 set("large money bag");SearchKey(VK_HOME,false);SearchKey(VK_RIGHT,true);if(searchCaret!=6)return false;
 SearchKey(VK_DELETE,true);if(lstrcmpA(objectSearch,"large bag"))return false;
 // Length stays capped at 40.
 set("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");if(SearchKey('B',false)||lstrlenA(objectSearch)!=40)return false;
 set("");return true;
}
bool TestSearchClicks(){
 searchFocused=searchSelected=false;
 NativeUI::TestPointer(P(400,205),true,false);SearchBox(false);if(!searchFocused)return false;
 NativeUI::TestPointer(P(400,205),false,true);SearchBox(false);if(!searchFocused)return false;
 NativeUI::TestPointer(P(600,500),true,false);SearchBox(false);if(searchFocused)return false;
 NativeUI::TestPointer(P(600,500),false,true);SearchBox(false);
 NativeUI::TestPointer(P(400,205),true,false);SearchBox(true);if(searchFocused)return false; // Settings drawer covers it
 NativeUI::TestPointer(P(400,205),false,true);SearchBox(true);
 // With text: the first click selects all (Backspace clears it), a later click places the caret.
 lstrcpynA(objectSearch,"tnt barrel",41);searchClickTick=GetTickCount()-5000;
 NativeUI::TestPointer(P(400,205),true,false);SearchBox(false);NativeUI::TestPointer(P(400,205),false,true);SearchBox(false);
 if(!searchFocused||!searchSelected)return false;
 searchClickTick=GetTickCount()-5000;
 NativeUI::TestPointer(P(259,205),true,false);SearchBox(false);NativeUI::TestPointer(P(259,205),false,true);SearchBox(false);
 if(searchSelected||searchCaret!=0)return false;
 // Double-click selects all again.
 NativeUI::TestPointer(P(259,205),true,false);SearchBox(false);NativeUI::TestPointer(P(259,205),false,true);SearchBox(false);
 if(!searchSelected)return false;
 // The X clears the text and keeps the box focused.
 NativeUI::TestPointer(P(987,205),true,false);SearchBox(false);NativeUI::TestPointer(P(987,205),false,true);SearchBox(false);
 if(objectSearch[0]||!searchFocused)return false;
 searchFocused=searchSelected=false;
 NativeUI::TestPointer(ImVec2(-1000,-1000),false,false);return true;
}
float TestScale(){return scale;}
int TestAppliedSky(){return appliedSky;}
bool TestWarningFont(){return font.resource&&font.resource->GetDesc().MipLevels==1;}
bool TestIconTextures(){
 if(!font.resource||font.resource->GetDesc().MipLevels!=1)return false;
 for(int i=0;i<kIconCount;++i){unsigned levels=1,w=kIcons[i].width,h=kIcons[i].height;
  while(w>1||h>1){w=w>1?w/2:1;h=h>1?h/2:1;++levels;}
  if(!icons[i].resource||icons[i].resource->GetDesc().MipLevels!=levels)return false;
 }return true;
}
bool TestDrawer(){
 settingsOpen=false;settingsSlide=0;SetSettings(true);settingsTick-=110;UpdateSettings();
 if(!(settingsSlide>0&&settingsSlide<1))return false;
 settingsTick-=220;UpdateSettings();if(settingsSlide!=1)return false;
 SetSettings(false);settingsTick-=110;UpdateSettings();if(!(settingsSlide>0&&settingsSlide<1))return false;
 settingsTick-=220;UpdateSettings();if(settingsSlide!=0)return false;
 settingsOpen=true;settingsSlide=settingsFrom=1;settingsTick=GetTickCount();
 // Underlying controls cannot activate while the drawer is visible.
 NativeUI::TestPointer(P(250,500),true,false);Hit("underlying-test",240,480,100,40);
 NativeUI::TestPointer(P(250,500),false,true);if(Hit("underlying-test",240,480,100,40))return false;
 // A click in gameplay outside the menu must leave Settings alone.
 NativeUI::TestPointer(P(-10,500),true,false);SettingsDrawer();
 NativeUI::TestPointer(P(-10,500),false,true);SettingsDrawer();if(!settingsOpen)return false;
 NativeUI::TestPointer(P(250,500),true,false);SettingsDrawer();
 NativeUI::TestPointer(P(250,500),false,true);SettingsDrawer();if(settingsOpen)return false;
 settingsOpen=true;settingsSlide=settingsFrom=1;
 NativeUI::TestPointer(P(1000,38),true,false);SettingsDrawer();
 NativeUI::TestPointer(P(1000,38),false,true);SettingsDrawer();if(settingsOpen)return false;
 NativeUI::TestPointer(ImVec2(-1000,-1000),false,false);
 settingsOpen=true;settingsSlide=settingsFrom=1;settingsTick=GetTickCount();return true;
}
bool TestScrollEdges(){
 settingsOpen=true;settingsSlide=settingsFrom=1;languageOpen=false;
 for(int lang=0;lang<5;++lang){Localization::language=lang;settingsScroll=10000;
  NativeUI::TestPointer(P(800,500),false,false);
  for(int i=0;i<12;++i){NativeUI::vertexCount=NativeUI::batchCount=0;ImGui::GetIO().MouseWheel=-4;SettingsDrawer();if(testRenderedScroll!=testScrollLimit||settingsScroll!=testScrollLimit)return false;}
  settingsScroll=0;
  for(int i=0;i<12;++i){NativeUI::vertexCount=NativeUI::batchCount=0;ImGui::GetIO().MouseWheel=4;SettingsDrawer();if(testRenderedScroll!=0||settingsScroll!=0)return false;}
 }
 ImGui::GetIO().MouseWheel=0;Localization::language=0;NativeUI::TestPointer(ImVec2(-1000,-1000),false,false);return true;
}
void TestLanguage(int value){Localization::language=value;prefsLoaded=true;prefsStore=false;settingsScroll=0;languageOpen=false;}
void TestCreditsScroll(){settingsScroll=300;}
void TestLanguageDropdown(){languageOpen=true;settingsScroll=0;}
bool TestLocalization(){
 for(int lang=0;lang<Localization::count;++lang){Localization::language=lang;
  for(const auto& row:Localization::entries){if(lstrcmpA(Localization::Get(row[0]),row[lang]))return false;
   for(const unsigned char* p=(const unsigned char*)row[lang];*p;){unsigned c=Localization::Next(p);for(int f=0;f<5;++f)if(FindGlyph((FontIndex)f,c)->codepoint!=c)return false;}}
 }
 for(const char* name:Localization::names){for(const unsigned char* p=(const unsigned char*)name;*p;){unsigned c=Localization::Next(p);if(FindGlyph(Nunito18,c)->codepoint!=c)return false;}}
 Localization::language=0;settingsScroll=0;languageOpen=false;prefsStore=false;
 settingsOpen=true;settingsSlide=1;
 NativeUI::TestPointer(P(720,210),true,false);SettingsDrawer();NativeUI::TestPointer(P(720,210),false,true);SettingsDrawer();if(!languageOpen)return false;
 NativeUI::TestPointer(P(720,292),true,false);SettingsDrawer();NativeUI::TestPointer(P(720,292),false,true);SettingsDrawer();if(languageOpen||Localization::language!=1)return false;
 Localization::language=0;NativeUI::TestPointer(ImVec2(-1000,-1000),false,false);return true;
}
void TestSelectPage(int value){page=value==6?0:value==7?6:value==8?7:value;settingsOpen=value==6;settingsSlide=settingsFrom=settingsOpen?1.f:0.f;settingsTick=GetTickCount();}
const char* TestKeyConflict(int vk){return KeyConflict(vk);}
void TestTheme(int value){prefsLoaded=true;theme=value;themePoll=0;themeSeen=false;NativeUI::EndReveal();}
void TestAnimatedTheme(int value){theme=value;themeOrigin=P(870,300);themeOriginSet=true;}
bool TestDark(){return dark;}
bool TestSettingsOpen(){return settingsOpen;}
bool TestLanguageOpen(){return languageOpen;}
int TestChosenLanguage(){return Localization::language;}
void TestSettleSettings(){settingsTick-=500;UpdateSettings();}
const char* TestFooter(){return testFooter;}
bool TestMotionCapacity(){
 // Every page's buttons in one opening must keep animating.
 for(auto& m:motions)m={};char id[32];
 for(int i=0;i<400;++i){wsprintfA(id,"capacity-%d",i);Animate(id,1,0);}
 for(auto& m:motions)if(m.id)m.tick-=75;
 float value=Animate("capacity-399",1,1);for(auto& m:motions)m={};return value>0&&value<1;
}
bool TestMotion(){
 Animate("animation-test",4,0);for(auto& m:motions)if(m.id)m.tick-=75;
 float rising=Animate("animation-test",4,1);for(auto& m:motions)if(m.id)m.tick-=75;
 float falling=Animate("animation-test",4,0);return rising>0&&rising<1&&falling>0&&falling<rising;
}
#endif

}

