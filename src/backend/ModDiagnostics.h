// Session logging and crash reports for the Mod Menu. Lives in dinput8.dll so it starts with the game,
// before the menu DLL loads or the menu is ever opened. Everything stays on this PC:
//   Mod Menu\Logs\Session-<date>.log, Mod Menu\Logs\Latest.log      (newest 10 sessions kept)
//   Mod Menu\CrashDumps\Crash-<date>.txt and .dmp                    (newest 5 crashes kept)
// Messages go into a ring buffer (the menu DLL reads it through the API) and a writer thread appends
// them to the files a few times a second, so logging never waits on the disk. Errors are written at once.
// No C runtime: Win32 only. Self-contained: it reads the game image itself and needs nothing from the host.
#include "../menu/MenuVersion.h"
namespace Diagnostics {
enum Level : unsigned { Trace, Debug, Info, Success, Warning, Error, Fatal };
static const char* const levelNames[]={"TRACE","DEBUG","INFO","SUCCESS","WARNING","ERROR","FATAL"};
struct Line { unsigned sequence, level, time; char text[232]; };   // time: ms since local midnight
static const unsigned capacity=4096;
static Line* ring;                       // capacity lines, allocated on first use
static volatile LONG next;               // sequence of the next line (also the total written)
static SRWLOCK lock=SRWLOCK_INIT;
static HANDLE wake,sessionFile=INVALID_HANDLE_VALUE,latestFile=INVALID_HANDLE_VALUE;
static unsigned written;                 // lines already in the files
static WCHAR folder[MAX_PATH],sessionName[64];
static char lastAction[128]="none";
static DWORD lastActionTime;
static void (*describe)(char* out,int size);   // set by the menu backend: active features for crash reports
static volatile LONG menuAttached,menuOpened,menuOpen;
static volatile LONG crashing;

// Repeated identical messages are counted instead of written again.
static unsigned repeatLevel;static char repeatText[232];static unsigned repeatCount;static DWORD repeatTick;

static unsigned Now(){SYSTEMTIME t;GetLocalTime(&t);return ((t.wHour*60+t.wMinute)*60+t.wSecond)*1000+t.wMilliseconds;}
// wsprintf has no 64-bit format: print the two halves.
static const char* Hex(ULONG_PTR v,char* out){wsprintfA(out,"0x%08X%08X",DWORD(v>>32),DWORD(v));return out;}
static bool Same(const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
static void Append(unsigned level,const char* text){
 if(!ring){ring=(Line*)VirtualAlloc(0,sizeof(Line)*capacity,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!ring)return;}
 Line& l=ring[next%capacity];l.sequence=next;l.level=level;l.time=Now();lstrcpynA(l.text,text,sizeof(l.text));
 InterlockedIncrement(&next);
}
static void FlushRepeat(){
 if(repeatCount<2)return;
 char text[96];wsprintfA(text,"(previous message repeated %u times)",repeatCount-1);Append(repeatLevel,text);repeatCount=0;
}
// Thread safe; text is copied. Long text is cut to 231 characters.
static void Write(unsigned level,const char* text){
 if(!text||level>Fatal)return;
 if(crashing){Append(level,text);return;}
 AcquireSRWLockExclusive(&lock);
 DWORD tick=GetTickCount();
 if(repeatCount&&repeatLevel==level&&Same(repeatText,text)&&DWORD(tick-repeatTick)<5000){++repeatCount;repeatTick=tick;ReleaseSRWLockExclusive(&lock);return;}
 FlushRepeat();
 Append(level,text);
 repeatLevel=level;lstrcpynA(repeatText,text,sizeof(repeatText));repeatCount=1;repeatTick=tick;
 ReleaseSRWLockExclusive(&lock);
 if(wake&&level>=Error)SetEvent(wake);
}
static void Writef(unsigned level,const char* format,...){
 char text[512];va_list args;va_start(args,format);wvsprintfA(text,format,args);va_end(args);Write(level,text);
}
// Copies lines with sequence >= first into out (oldest still in the ring if first is older). Returns the count.
static unsigned Read(unsigned first,Line* out,unsigned max){
 if(!ring||!out)return 0;
 AcquireSRWLockShared(&lock);
 unsigned end=(unsigned)next,start=end>capacity?end-capacity:0;if(first<start)first=start;
 unsigned n=0;for(unsigned s=first;s<end&&n<max;++s)out[n++]=ring[s%capacity];
 ReleaseSRWLockShared(&lock);return n;
}
static void Action(const char* text){lstrcpynA(lastAction,text,sizeof(lastAction));lastActionTime=GetTickCount();}

// ---------- files ----------
static bool GameFolder(WCHAR* out){
 DWORD n=GetModuleFileNameW(0,out,MAX_PATH);if(!n||n>=MAX_PATH-40)return false;
 while(n&&out[n-1]!=L'\\')--n;out[n]=0;return true;
}
static char today[16];   // yyyy-mm-dd of the session start, set when the files open
static void Format(char* out,const Line& l){
 unsigned t=l.time;
 wsprintfA(out,"%s %02u:%02u:%02u.%03u %-7s %s\r\n",today,t/3600000,t/60000%60,t/1000%60,t%1000,levelNames[l.level],l.text);
}
static void WriteBoth(const char* text,DWORD length){
 DWORD done;
 if(sessionFile!=INVALID_HANDLE_VALUE)WriteFile(sessionFile,text,length,&done,0);
 if(latestFile!=INVALID_HANDLE_VALUE)WriteFile(latestFile,text,length,&done,0);
}
static void FlushFiles(){
 if(sessionFile==INVALID_HANDLE_VALUE&&latestFile==INVALID_HANDLE_VALUE)return;
 static Line batch[256];
 for(;;){
  unsigned n=Read(written,batch,256);if(!n)break;
  static char text[256*300];int used=0;
  for(unsigned i=0;i<n;++i){if(batch[i].sequence<written)continue;Format(text+used,batch[i]);used+=lstrlenA(text+used);}
  written=batch[n-1].sequence+1;WriteBoth(text,used);
 }
}
// Keeps the newest `keep` files matching pattern (names sort by date) and deletes older ones.
static void Prune(const WCHAR* dir,const WCHAR* pattern,unsigned keep){
 WCHAR search[MAX_PATH];lstrcpyW(search,dir);lstrcatW(search,pattern);
 WCHAR names[64][64];unsigned count=0;WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW(search,&f);
 if(h==INVALID_HANDLE_VALUE)return;
 do{if(count<64&&lstrlenW(f.cFileName)<64)lstrcpyW(names[count++],f.cFileName);}while(FindNextFileW(h,&f));
 FindClose(h);
 for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j)if(lstrcmpW(names[j],names[i])>0){WCHAR t[64];lstrcpyW(t,names[i]);lstrcpyW(names[i],names[j]);lstrcpyW(names[j],t);}
 for(unsigned i=keep;i<count;++i){WCHAR p[MAX_PATH];lstrcpyW(p,dir);lstrcatW(p,names[i]);DeleteFileW(p);}
}
static void Stamp(WCHAR* out){SYSTEMTIME t;GetLocalTime(&t);wsprintfW(out,L"%04u-%02u-%02u_%02u-%02u-%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);}
static void OpenFiles(){
 WCHAR dir[MAX_PATH];if(!GameFolder(dir))return;
 lstrcpyW(folder,dir);lstrcatW(dir,L"Mod Menu");CreateDirectoryW(dir,0);lstrcatW(dir,L"\\Logs\\");CreateDirectoryW(dir,0);
 WCHAR stamp[32];Stamp(stamp);wsprintfW(sessionName,L"Session-%s.log",stamp);
 Prune(dir,L"Session-*.log",9);   // with the new one: 10 sessions
 WCHAR path[MAX_PATH];lstrcpyW(path,dir);lstrcatW(path,sessionName);
 sessionFile=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 lstrcpyW(path,dir);lstrcatW(path,L"Latest.log");
 latestFile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 char header[160];SYSTEMTIME t;GetLocalTime(&t);wsprintfA(today,"%04u-%02u-%02u",t.wYear,t.wMonth,t.wDay);
 int n=wsprintfA(header,"# Toy Story 3 Mod Menu %s session log, started %s %02u:%02u:%02u\r\n",TS3_MENU_VERSION,today,t.wHour,t.wMinute,t.wSecond);WriteBoth(header,n);
}

// ---------- crash reports ----------
static LPTOP_LEVEL_EXCEPTION_FILTER chained;
static const char* ExceptionName(DWORD code){
 switch(code){
 case EXCEPTION_ACCESS_VIOLATION:return "Access violation";case EXCEPTION_STACK_OVERFLOW:return "Stack overflow";
 case EXCEPTION_ILLEGAL_INSTRUCTION:return "Illegal instruction";case EXCEPTION_INT_DIVIDE_BY_ZERO:return "Integer divide by zero";
 case EXCEPTION_PRIV_INSTRUCTION:return "Privileged instruction";case EXCEPTION_IN_PAGE_ERROR:return "In-page error";
 case EXCEPTION_BREAKPOINT:return "Breakpoint";case 0xC0000409:return "Stack buffer overrun / fail fast";
 case 0xE06D7363:return "C++ exception";default:return "Unknown exception";
 }
}
// Module file name (no folder, so no user name or path ends up in a report) and offset of an address.
static void Where(ULONG_PTR address,char* out){
 HMODULE m=0;char name[MAX_PATH];name[0]='?';name[1]=0;
 if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)address,&m)&&m){
  char full[MAX_PATH];if(GetModuleFileNameA(m,full,MAX_PATH)){const char* s=full;for(const char* p=full;*p;++p)if(*p=='\\'||*p=='/')s=p+1;lstrcpynA(name,s,MAX_PATH);}
  wsprintfA(out,"%s+0x%X",name,DWORD(address-(ULONG_PTR)m));
 }else{char h[24];wsprintfA(out,"%s (no module)",Hex(address,h));}
}
static HANDLE report;
static void Put(const char* text){DWORD done;if(report!=INVALID_HANDLE_VALUE)WriteFile(report,text,lstrlenA(text),&done,0);}
static void Putf(const char* format,...){char text[1024];va_list args;va_start(args,format);wvsprintfA(text,format,args);va_end(args);Put(text);}
static void GameVersion(char* out){
 lstrcpyA(out,"unknown");
 __try{
  BYTE* base=(BYTE*)GetModuleHandleW(0);auto nt=(IMAGE_NT_HEADERS64*)(base+((IMAGE_DOS_HEADER*)base)->e_lfanew);
  WCHAR exe[MAX_PATH];GetModuleFileNameW(0,exe,MAX_PATH);
  WCHAR sys[MAX_PATH];GetSystemDirectoryW(sys,MAX_PATH);lstrcatW(sys,L"\\version.dll");HMODULE v=LoadLibraryW(sys);
  typedef DWORD(WINAPI* SizeFn)(LPCWSTR,LPDWORD);typedef BOOL(WINAPI* InfoFn)(LPCWSTR,DWORD,DWORD,LPVOID);typedef BOOL(WINAPI* QueryFn)(LPCVOID,LPCWSTR,LPVOID*,PUINT);
  SizeFn size=v?(SizeFn)GetProcAddress(v,"GetFileVersionInfoSizeW"):0;InfoFn info=v?(InfoFn)GetProcAddress(v,"GetFileVersionInfoW"):0;QueryFn query=v?(QueryFn)GetProcAddress(v,"VerQueryValueW"):0;
  static BYTE data[8192];DWORD unused,n=size?size(exe,&unused):0;VS_FIXEDFILEINFO* f=0;UINT len=0;
  if(n&&n<=sizeof(data)&&info(exe,0,n,data)&&query(data,L"\\",(LPVOID*)&f,&len)&&f)
   wsprintfA(out,"%u.%u.%u.%u, image timestamp 0x%08X",HIWORD(f->dwFileVersionMS),LOWORD(f->dwFileVersionMS),HIWORD(f->dwFileVersionLS),LOWORD(f->dwFileVersionLS),nt->FileHeader.TimeDateStamp);
  else wsprintfA(out,"image timestamp 0x%08X",nt->FileHeader.TimeDateStamp);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
struct DumpJob { EXCEPTION_POINTERS* info; DWORD thread; WCHAR path[MAX_PATH]; BOOL ok; };
// Written from a separate thread: the crashed thread may have no stack left.
static DWORD WINAPI WriteDump(void* p){
 DumpJob* job=(DumpJob*)p;
 WCHAR dll[MAX_PATH];GetSystemDirectoryW(dll,MAX_PATH);lstrcatW(dll,L"\\dbghelp.dll");HMODULE h=LoadLibraryW(dll);if(!h)return 0;
 #pragma pack(push,4)   // MINIDUMP_EXCEPTION_INFORMATION is 4-byte packed
 struct ExceptionInfo{DWORD ThreadId;EXCEPTION_POINTERS* ExceptionPointers;BOOL ClientPointers;};
 #pragma pack(pop)
 typedef BOOL(WINAPI* DumpFn)(HANDLE,DWORD,HANDLE,int,ExceptionInfo*,void*,void*);
 DumpFn dump=(DumpFn)GetProcAddress(h,"MiniDumpWriteDump");if(!dump)return 0;
 HANDLE file=CreateFileW(job->path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(file==INVALID_HANDLE_VALUE)return 0;
 ExceptionInfo e={job->thread,job->info,FALSE};
 // MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs: small but useful.
 job->ok=dump(GetCurrentProcess(),GetCurrentProcessId(),file,0x1000|0x40|0x1,&e,0,0);CloseHandle(file);return 0;
}
static LONG WINAPI Crashed(EXCEPTION_POINTERS* info){
 if(InterlockedExchange(&crashing,1))return chained?chained(info):EXCEPTION_CONTINUE_SEARCH;
 __try{
  WCHAR dir[MAX_PATH];if(GameFolder(dir)){
  lstrcatW(dir,L"Mod Menu");CreateDirectoryW(dir,0);lstrcatW(dir,L"\\CrashDumps\\");CreateDirectoryW(dir,0);
  Prune(dir,L"Crash-*.txt",4);Prune(dir,L"Crash-*.dmp",4);   // with this one: 5 crashes
  WCHAR stamp[32];Stamp(stamp);WCHAR txt[MAX_PATH],dmp[MAX_PATH];wsprintfW(txt,L"%sCrash-%s.txt",dir,stamp);wsprintfW(dmp,L"%sCrash-%s.dmp",dir,stamp);
  report=CreateFileW(txt,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  EXCEPTION_RECORD* r=info->ExceptionRecord;CONTEXT* c=info->ContextRecord;SYSTEMTIME t;GetLocalTime(&t);char where[300],game[128];
  GameVersion(game);
  Putf("Toy Story 3 Mod Menu crash report\r\n\r\nMod Menu version: %s\r\nGame version: %s\r\nCrash time: %04u-%02u-%02u %02u:%02u:%02u (local)\r\n\r\n",TS3_MENU_VERSION,game,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
  Where((ULONG_PTR)r->ExceptionAddress,where);char fault[300];lstrcpyA(fault,where);
  Putf("Exception: %s (0x%08X), flags 0x%X\r\nFaulting address: %s\r\n",ExceptionName(r->ExceptionCode),r->ExceptionCode,r->ExceptionFlags,where);
  if(r->ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&r->NumberParameters>=2)
  {char h[24];Putf("Access: %s at %s\r\n",r->ExceptionInformation[0]==0?"read":r->ExceptionInformation[0]==1?"write":"execute",Hex(r->ExceptionInformation[1],h));}
  {char a[24],b[24],d[24];Putf("Thread: %u\r\nRegisters: RIP=%s RSP=%s RBP=%s\r\n\r\nStack trace:\r\n",GetCurrentThreadId(),Hex(c->Rip,a),Hex(c->Rsp,b),Hex(c->Rbp,d));}
  // Walk the stack with the x64 unwind tables (no symbols needed): module+offset for each frame.
  CONTEXT walk=*c;
  for(int i=0;i<48&&walk.Rip;++i){
   Where(walk.Rip,where);Putf("  %2d  %s\r\n",i,where);
   DWORD64 image=0;PRUNTIME_FUNCTION f=RtlLookupFunctionEntry(walk.Rip,&image,0);
   if(!f){walk.Rip=*(DWORD64*)walk.Rsp;walk.Rsp+=8;continue;}   // leaf function: return address on top
   void* data=0;DWORD64 frame=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,walk.Rip,f,&walk,&data,&frame,0);
  }
  Put("\r\nMod Menu status:\r\n");
  Putf("  Menu DLL connected: %s\r\n  Menu opened this session: %s\r\n  Menu open at crash: %s\r\n",menuAttached?"yes":"no",menuOpened?"yes":"no",menuOpen?"yes":"no");
  Putf("  Last menu action: %s",lastAction);
  if(lastActionTime)Putf(" (%u s before the crash)",(GetTickCount()-lastActionTime)/1000);
  Put("\r\n  Active features: ");
  char features[512];lstrcpyA(features,"(unavailable)");   // no initializer: that would need memset (no C runtime)
  if(describe){__try{describe(features,sizeof(features));}__except(EXCEPTION_EXECUTE_HANDLER){}}
  Put(features);Put("\r\n\r\nRecent log messages:\r\n");
  static Line recent[80];unsigned end=(unsigned)next,first=end>80?end-80:0,n=0;
  if(ring)for(unsigned s=first;s<end;++s)recent[n++]=ring[s%capacity];   // no lock: the crashed thread may hold it
  for(unsigned i=0;i<n;++i){char line[300];Format(line,recent[i]);Put("  ");Put(line);}
  // Dump file.
  static DumpJob job;job.info=info;job.thread=GetCurrentThreadId();lstrcpyW(job.path,dmp);job.ok=FALSE;
  HANDLE worker=CreateThread(0,0,WriteDump,&job,0,0);if(worker){WaitForSingleObject(worker,20000);CloseHandle(worker);}
  Putf("\r\nCrash dump: %s\r\n",job.ok?"written":"could not be written");
  if(report!=INVALID_HANDLE_VALUE)CloseHandle(report);report=INVALID_HANDLE_VALUE;
  char fatal[200];wsprintfA(fatal,"[Crash] Game crashed: %s at %s. A crash report was saved in Mod Menu\\CrashDumps.",ExceptionName(r->ExceptionCode),fault);
  Append(Fatal,fatal);FlushFiles();
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 return chained?chained(info):EXCEPTION_CONTINUE_SEARCH;
}
// Other code (the game, Steam) may install its own handler later. Re-assert ours and chain to theirs.
static void KeepCrashHandler(){
 LPTOP_LEVEL_EXCEPTION_FILTER previous=SetUnhandledExceptionFilter(Crashed);
 if(previous&&previous!=Crashed)chained=previous;
}

// ---------- startup ----------
// Startup summary: the Windows version, and any other DLL mods in the game folder (file names only).
// Another mod hooking the same game is a common cause of crashes, so this shows it without asking.
static void LogSystem(){
 typedef LONG(WINAPI* GetVersionFn)(OSVERSIONINFOW*);   // RtlGetVersion reports the real version
 HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");GetVersionFn version=ntdll?(GetVersionFn)GetProcAddress(ntdll,"RtlGetVersion"):0;
 OSVERSIONINFOW v;v.dwOSVersionInfoSize=sizeof(v);
 if(version&&version(&v)==0)Writef(Info,"[Startup] Windows %s, build %u",v.dwMajorVersion==10&&v.dwBuildNumber>=22000?"11":v.dwMajorVersion==10?"10":"(older)",v.dwBuildNumber);
 // The game's own DLLs and the mod's two are left out.
 static const WCHAR* const known[]={L"dinput8.dll",L"TS3ModMenu.dll",L"bink2w64.dll",L"fmod.dll",L"fmodL.dll",L"fmodstudio.dll",L"steam_api64.dll"};
 WCHAR dir[MAX_PATH];if(!GameFolder(dir))return;
 char list[200];int used=0,count=0;list[0]=0;bool more=false;
 static const WCHAR* const patterns[2]={L"*.dll",L"*.asi"};
 for(const WCHAR* pattern:patterns){
  WCHAR search[MAX_PATH];lstrcpyW(search,dir);lstrcatW(search,pattern);
  WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW(search,&f);if(h==INVALID_HANDLE_VALUE)continue;
  do{
   if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
   bool skip=false;for(const WCHAR* k:known)if(lstrcmpiW(f.cFileName,k)==0)skip=true;
   if(skip)continue;
   char name[80];if(!WideCharToMultiByte(CP_UTF8,0,f.cFileName,-1,name,80,0,0))continue;
   int n=lstrlenA(name);if(used+n+2>=(int)sizeof(list)-8){more=true;continue;}
   if(count++){list[used++]=',';list[used++]=' ';}
   lstrcpyA(list+used,name);used+=n;
  }while(FindNextFileW(h,&f));
  FindClose(h);
 }
 if(more)lstrcpyA(list+used,", ...");
 if(count)Writef(Info,"[Startup] Other DLL files in the game folder: %s",list);
 else Write(Info,"[Startup] No other DLL mods in the game folder");
}
static DWORD WINAPI Writer(void*){
 OpenFiles();
 LogSystem();
 for(unsigned tick=0;;++tick){
  WaitForSingleObject(wake,250);
  AcquireSRWLockExclusive(&lock);if(repeatCount>1&&DWORD(GetTickCount()-repeatTick)>=5000){FlushRepeat();repeatCount=0;}ReleaseSRWLockExclusive(&lock);
  FlushFiles();
  if(tick%8==0)KeepCrashHandler();   // about every 2 seconds
 }
}
// Called first thing in DllMain: crash handler now, files and writer on their own thread.
static void Start(){
 KeepCrashHandler();
 wake=CreateEventW(0,FALSE,FALSE,0);
 Writef(Info,"[Startup] Toy Story 3 Mod Menu %s loading",TS3_MENU_VERSION);
 HANDLE thread=CreateThread(0,0,Writer,0,0,0);if(thread)CloseHandle(thread);
}
}
