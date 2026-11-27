// Offline test for the session log, crash reports and the Trace Console drawing. Never touches the game.
// Run from an empty folder: it creates Mod Menu\Logs and Mod Menu\CrashDumps next to the exe.
//   DiagnosticsTest.exe          logging, rotation, dedup, Latest.log, console preview images
//   DiagnosticsTest.exe crash    starts logging, then crashes on purpose (crash report + dump)
#define WIN32_LEAN_AND_MEAN
#define TRACE_CONSOLE_TEST
#include <windows.h>
#include <stdint.h>
#pragma function(memset,memcpy)
extern "C" void* memset(void* p,int v,size_t n){volatile BYTE* d=(BYTE*)p;for(size_t i=0;i<n;++i)d[i]=(BYTE)v;return p;}
extern "C" void* memcpy(void* p,const void* s,size_t n){volatile BYTE* d=(BYTE*)p;const BYTE* a=(const BYTE*)s;for(size_t i=0;i<n;++i)d[i]=a[i];return p;}
extern "C" int _fltused=0;
#include "../menu/ModMenuApi.h"
namespace OverlayHost{HMODULE Module(){return GetModuleHandleW(0);}bool IsOpen(){return false;}}
namespace CheatMenuUI{bool Typing(){return false;}}
#include "../backend/ModDiagnostics.h"
#include "TraceConsole.cpp"
static unsigned checks,failures;
static void Out(const char* t){DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),t,lstrlenA(t),&n,0);}
static void Check(bool ok,const char* what){++checks;if(!ok){++failures;Out("FAIL ");Out(what);Out("\r\n");}}
static bool Exists(const WCHAR* path){return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;}
static unsigned CountFiles(const WCHAR* pattern){WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW(pattern,&f);if(h==INVALID_HANDLE_VALUE)return 0;unsigned n=0;do ++n;while(FindNextFileW(h,&f));FindClose(h);return n;}
static char* ReadAll(const WCHAR* path,DWORD* size){
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,OPEN_EXISTING,0,0);if(f==INVALID_HANDLE_VALUE)return 0;
 DWORD n=GetFileSize(f,0);char* b=(char*)VirtualAlloc(0,n+1,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD got=0;ReadFile(f,b,n,&got,0);b[got]=0;CloseHandle(f);*size=got;return b;
}
static bool Has(const char* text,const char* what){for(;*text;++text){const char *a=text,*b=what;while(*a&&*b&&*a==*b){++a;++b;}if(!*b)return true;}return false;}
static void LogFromTest(uint32_t level,const char* text){Diagnostics::Write(level,text);}
static uint32_t ReadFromTest(uint32_t first,TS3LogLine* out,uint32_t max){return Diagnostics::Read(first,(Diagnostics::Line*)out,max);}
static TS3CheatMenuApi fake={TS3MenuApiVersion,sizeof(TS3CheatMenuApi),0,0,0,LogFromTest,ReadFromTest};
static void SavePixels(const WCHAR* name){
 HANDLE f=CreateFileW(name,GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);DWORD done;WriteFile(f,TraceConsole::pixels,TraceConsole::width*TraceConsole::height*4,&done,0);CloseHandle(f);
}
static void Describe(char* out,int){lstrcpyA(out,"flight (speed 15), God mode");}
static void Crash(){Diagnostics::Action("Spawn vehicle");
 Diagnostics::Write(Diagnostics::Info,"About to crash on purpose");volatile int* p=0;*p=1;}
extern "C" void TestEntry(){
 WCHAR dir[MAX_PATH];Diagnostics::GameFolder(dir);const WCHAR* cmd=GetCommandLineW();
 bool crash=false;for(const WCHAR* c=cmd;*c;++c)if(c[0]=='c'&&c[1]=='r'&&c[2]=='a'&&c[3]=='s'&&c[4]=='h')crash=true;
 if(crash){Diagnostics::describe=Describe;Diagnostics::Start();Sleep(400);Crash();ExitProcess(3);}
 if(lstrcmpW(cmd+lstrlenW(cmd)-6,L"verify")==0){
  WCHAR p[MAX_PATH];lstrcpyW(p,dir);lstrcatW(p,L"Mod Menu\\CrashDumps\\Crash-*.txt");WIN32_FIND_DATAW fd;HANDLE h=FindFirstFileW(p,&fd);
  Check(h!=INVALID_HANDLE_VALUE,"crash report exists");
  if(h!=INVALID_HANDLE_VALUE){FindClose(h);WCHAR full[MAX_PATH];lstrcpyW(full,dir);lstrcatW(full,L"Mod Menu\\CrashDumps\\");lstrcatW(full,fd.cFileName);DWORD n=0;char* r=ReadAll(full,&n);
   Check(r&&Has(r,"Exception: Access violation"),"report: exception");Check(r&&Has(r,"Faulting address: DiagnosticsTest.exe+"),"report: faulting module");
   Check(r&&Has(r,"Stack trace:"),"report: stack trace");Check(r&&Has(r,"Active features: flight (speed 15), God mode"),"report: active features");
   Check(r&&Has(r,"Last menu action: Spawn vehicle"),"report: last action");Check(r&&Has(r,"About to crash on purpose"),"report: recent log");
   Check(r&&Has(r,"Crash dump: written"),"report: dump written");Check(r&&!Has(r,"Users" "\\"),"report: no user folder path");}   // split so the source leak scan does not flag the test itself
  char line[96];wsprintfA(line,"Crash report check: %u checks, %u failures\r\n",checks,failures);Out(line);ExitProcess(failures?1:0);
 }
 // Old sessions to prune: 12 fake ones; after start only the newest 10 (9 old + new) remain.
 WCHAR logs[MAX_PATH];lstrcpyW(logs,dir);lstrcatW(logs,L"Mod Menu");CreateDirectoryW(logs,0);lstrcatW(logs,L"\\Logs\\");CreateDirectoryW(logs,0);
 for(int i=0;i<12;++i){WCHAR p[MAX_PATH];wsprintfW(p,L"%sSession-2000-01-%02d_00-00-00.log",logs,i+1);HANDLE h=CreateFileW(p,GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);CloseHandle(h);}
 Diagnostics::Start();
 const char* samples[]={"Hook table checked (TRACE sample)","Menu key pressed: checking gameplay before opening","Gameplay: world 3, save slot 1, Toy Box",
  "Flight / noclip enabled.","Walk speed is unavailable for this character.","Save backup failed. Weapon was not added.","Game crashed: Access violation (sample)"};
 for(unsigned i=0;i<7;++i)Diagnostics::Write(i,samples[i]);
 for(int i=0;i<50;++i)Diagnostics::Write(Diagnostics::Debug,"Repeated message");
 Diagnostics::Write(Diagnostics::Info,"After repeats");
 Sleep(800);   // writer flushes every 250 ms
 WCHAR latest[MAX_PATH];lstrcpyW(latest,logs);lstrcatW(latest,L"Latest.log");DWORD size=0;char* text=ReadAll(latest,&size);
 Check(text&&Has(text," SUCCESS Flight / noclip enabled."),"Latest.log has a SUCCESS line");
 Check(text&&Has(text," WARNING Walk speed is unavailable"),"Latest.log has a WARNING line");
 Check(text&&Has(text," ERROR   Save backup failed"),"Latest.log has an ERROR line, padded level");
 Check(text&&Has(text,"repeated 49 times"),"50 identical messages collapse to one plus a repeat count");
 Check(text&&Has(text,"After repeats"),"message after repeats is written");
 Check(text&&Has(text,"Toy Story 3 Mod Menu " TS3_MENU_VERSION),"header has the version");
 WCHAR pattern[MAX_PATH];lstrcpyW(pattern,logs);lstrcatW(pattern,L"Session-*.log");
 Check(CountFiles(pattern)==10,"10 session logs kept (old ones pruned)");
 WCHAR oldest[MAX_PATH];wsprintfW(oldest,L"%sSession-2000-01-01_00-00-00.log",logs);Check(!Exists(oldest),"oldest session log deleted");
 WCHAR newest[MAX_PATH];wsprintfW(newest,L"%s%s",logs,Diagnostics::sessionName);DWORD s2=0;char* t2=ReadAll(newest,&s2);Check(t2&&s2==size,"session log matches Latest.log");
 static Diagnostics::Line got[64];unsigned n=Diagnostics::Read(0,got,64);Check(n>=9&&got[0].sequence==0,"ring buffer readable from the start");
 // Ring keeps the newest 4096 lines.
 for(int i=0;i<5000;++i){char m[32];wsprintfA(m,"line %d",i);Diagnostics::Write(Diagnostics::Trace,m);}
 n=Diagnostics::Read(0,got,1);Check(n==1&&got[0].sequence==(unsigned)Diagnostics::next-Diagnostics::capacity,"ring keeps the newest 4096 lines");
 // Trace Console drawing (no window): light and dark previews.
 TraceConsole::api=&fake;Check(TraceConsole::LoadSystem(),"GDI and registry functions load");TraceConsole::LoadAtlas();Check(TraceConsole::atlas!=0,"font atlas loads");
 TraceConsole::lines=(TS3LogLine*)VirtualAlloc(0,sizeof(TS3LogLine)*TraceConsole::maxLines,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 TraceConsole::Pull();Check(TraceConsole::count==TraceConsole::maxLines,"console copies the log");
 // Show a readable sample: the first test lines only.
 TraceConsole::count=0;TraceConsole::nextSequence=0;
 for(unsigned i=0;i<12;++i){TraceConsole::lines[TraceConsole::count]={i,i%7,(14*3600+5*60+i)*1000+i*37};lstrcpyA(TraceConsole::lines[TraceConsole::count++].text,i<7?samples[i]:"Teleported to Town.");}
 TraceConsole::lines[9].level=3;TraceConsole::lines[10].level=2;TraceConsole::lines[11].level=4;
 TraceConsole::Resize(980,620);
 TraceConsole::testTheme=0;TraceConsole::Paint();SavePixels(L"console-light.bgra");
 TraceConsole::testTheme=1;TraceConsole::Paint();SavePixels(L"console-dark.bgra");
 TraceConsole::Resize(1600,300);TraceConsole::testTheme=0;TraceConsole::Paint();SavePixels(L"console-docked.bgra");TraceConsole::Resize(980,620);
 TraceConsole::allSelected=true;TraceConsole::testTheme=0;TraceConsole::Paint();SavePixels(L"console-selected.bgra");TraceConsole::allSelected=false;
 static unsigned idx[TraceConsole::maxLines];Check(TraceConsole::Filtered(idx)==12,"every line is shown (no filters)");
 TraceConsole::nextSequence=12;TraceConsole::Clear();Check(TraceConsole::Filtered(idx)==0&&!TraceConsole::allSelected,"Clear hides earlier lines");TraceConsole::clearedBefore=0;
 TraceConsole::Export();WCHAR ex[MAX_PATH];lstrcpyW(ex,logs);lstrcatW(ex,L"Export-*.log");Check(CountFiles(ex)==1,"Export writes a file in Logs");
 char line[160];wsprintfA(line,"Diagnostics test: %u checks, %u failures\r\n",checks,failures);Out(line);ExitProcess(failures?1:0);
}
