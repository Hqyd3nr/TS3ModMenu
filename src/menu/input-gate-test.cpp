#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD now=1000;static bool held,mouseHeld;
static DWORD FakeTick(){return now;}static DWORD FakePid(){return 321;}
static SHORT FakeKeys(int key){return (held&&key=='W')||(mouseHeld&&key==VK_LBUTTON)?(SHORT)0x8000:0;}
static void Log(const char*,void* =0){}
#define GetTickCount FakeTick
#define GetCurrentProcessId FakePid
#define GetAsyncKeyState FakeKeys
#include "../backend/MenuInputGate.h"
#undef GetTickCount
#undef GetCurrentProcessId
#undef GetAsyncKeyState
static int checks,reads,drains,cursorCalls,clipCalls,keyCalls,dispatchCalls;
static void Check(bool ok){++checks;if(!ok){char b[64];DWORD n=wsprintfA(b,"FAILED gate check %d\n",checks),written;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),b,n,&written,0);ExitProcess(checks);}}
static HRESULT WINAPI FakeState(void*,DWORD count,void* p){++reads;for(DWORD i=0;i<count;++i)((volatile BYTE*)p)[i]=0xaa;return S_OK;}
static HRESULT WINAPI FakeData(void*,DWORD,void* p,DWORD* count,DWORD flags){++reads;if(!p&&!flags)++drains;*count=3;return S_OK;}
static HRESULT WINAPI FakeProperty(void*,const GUID&,void* p){LONG* r=(LONG*)p;r[4]=0;r[5]=65534;return S_OK;}
static void* fakeDeviceObject;
static HRESULT WINAPI FakeCreate(void*,const GUID&,void** out,void*){*out=fakeDeviceObject;return S_OK;}
static MenuInputGate::Pad pad;
static DWORD WINAPI FakePad(DWORD,MenuInputGate::Pad* p){*p=pad;return 0;}
static BOOL WINAPI FakeCursor(int,int){++cursorCalls;return TRUE;}
static BOOL WINAPI FakeClip(const RECT*){++clipCalls;return TRUE;}
static SHORT WINAPI FakeKey(int){++keyCalls;return (SHORT)0x8000;}
static LRESULT WINAPI FakeDispatch(const MSG*){++dispatchCalls;return 123;}
static MenuInputGate::Lease lease;
static void Renew(bool open){lease.sequence=2;lease.magic=0x554e4c32;lease.version=2;lease.game=321;lease.owner=654;lease.tick=now;lease.open=open?1:0;lstrcpyA(lease.session,"0123456789abcdef0123456789abcdef");}
extern "C" void TestEntry(){
 using namespace MenuInputGate;
 lstrcpyA(expectedSession,"0123456789abcdef0123456789abcdef");MenuInputGate::lease=&::lease;Renew(true);
 Check(sizeof(Lease)==68);Check(ValidLease(::lease,now,321,expectedSession));
 Check(!ValidLease(::lease,now+750,321,expectedSession));Check(!ValidLease(::lease,now-1,321,expectedSession));
 Check(!ValidLease(::lease,now,322,expectedSession));Check(!ValidLease(::lease,now,321,"1123456789abcdef0123456789abcdef"));
 ::lease.owner=0;Check(!ValidLease(::lease,now,321,expectedSession));Renew(true);
 Check(Blocked());::lease.sequence=3;now+=700;Check(Blocked());now+=51;Check(!Blocked());
 Renew(true);Check(Blocked());Renew(false);held=true;Check(!Blocked());held=false;Check(!Blocked());
 static void* table[32];void** object=table;table[5]=(void*)FakeProperty;table[9]=(void*)FakeState;table[10]=(void*)FakeData;
 static void* rootTable[8];void** rootObject=rootTable;rootTable[3]=(void*)FakeCreate;fakeDeviceObject=&object;
 AttachDirectInput(&rootObject);Check(rootTable[3]==(void*)CreateDevice&&rootReady);GUID guid={};void* created=0;
 Check(SUCCEEDED(CreateDevice(&rootObject,guid,&created,0))&&created==&object);Check(table[9]==(void*)DeviceState&&table[10]==(void*)DeviceData&&deviceReady);
 BYTE keyboard[256],mouse[20];Renew(false);Check(SUCCEEDED(DeviceState(&object,256,keyboard))&&keyboard[17]==0xaa);
 Renew(true);Check(SUCCEEDED(DeviceState(&object,256,keyboard))&&keyboard[17]==0&&keyboard[255]==0);
 Check(SUCCEEDED(DeviceState(&object,20,mouse))&&mouse[0]==0&&mouse[19]==0);
 Check(!Ready());hooksReady=true;Check(Ready());hooksReady=false;
 LONG joystick[68];Check(SUCCEEDED(DeviceState(&object,272,joystick))&&joystick[0]==32767&&joystick[7]==32767&&joystick[8]==-1&&joystick[11]==-1&&joystick[12]==0);
 DWORD events=8;Check(SUCCEEDED(DeviceData(&object,24,keyboard,&events,1))&&events==0&&drains==1);
 Renew(false);Check(SUCCEEDED(DeviceState(&object,256,keyboard))&&keyboard[17]==0xaa);
 padOriginal=FakePad;pad.packet=25;pad.buttons=0x1000;pad.lx=20000;Pad output;
 Renew(true);Check(PadState(0,&output)==0&&output.buttons==0&&output.lx==0&&output.packet==(25^0x80000000));
 Renew(false);Check(PadState(0,&output)==0&&output.buttons==0&&output.lx==20000);
 pad.buttons=0;pad.lx=0;Check(PadState(0,&output)==0&&output.packet==25&&!padRelease[0]);
 pad.buttons=0x1000;Check(PadState(0,&output)==0&&output.buttons==0x1000);
 cursorOriginal=FakeCursor;clipOriginal=FakeClip;keyOriginal=FakeKey;dispatchOriginal=FakeDispatch;
 Renew(true);Check(Cursor(10,20)&&cursorCalls==0);RECT area={};Check(Clip(&area)&&clipCalls==1);Check(Key('W')==0&&keyCalls==0);
 MSG message={};message.message=WM_LBUTTONDOWN;Check(Dispatch(&message)==0&&dispatchCalls==0);message.message=WM_ACTIVATEAPP;Check(Dispatch(&message)==123&&dispatchCalls==1);
 Renew(false);Check(Cursor(10,20)&&cursorCalls==1);Check(Key('W')==(SHORT)0x8000&&keyCalls==1);
 Check(!InputMessage(WM_PAINT)&&InputMessage(WM_KEYDOWN)&&InputMessage(WM_MOUSEWHEEL)&&InputMessage(WM_INPUT));
 // Closing while walking must not wait for W or the mouse to be released.
 Renew(true);Check(Blocked());held=mouseHeld=true;Renew(false);Check(!Blocked());
 Check(Key('W')==(SHORT)0x8000&&Key(VK_LBUTTON)==0);
 Check(SUCCEEDED(DeviceState(&object,256,keyboard))&&keyboard[17]==0xaa);
 Check(SUCCEEDED(DeviceState(&object,20,mouse))&&mouse[12]==0&&mouse[0]==0xaa);
 mouseHeld=false;Check(!Suppressed(VK_LBUTTON));Check(Key(VK_LBUTTON)==(SHORT)0x8000);held=false;
 pad.buttons=0x1000;pad.lx=21000;pad.rightTrigger=200;Renew(true);Check(PadState(0,&output)==0&&output.lx==0);
 Renew(false);Check(PadState(0,&output)==0&&output.lx==21000&&output.buttons==0&&output.rightTrigger==0);
 pad.buttons=0;pad.rightTrigger=0;Check(PadState(0,&output)==0&&output.lx==21000&&!padRelease[0]);
 pad.buttons=0x1000;Check(PadState(0,&output)==0&&output.buttons==0x1000&&output.lx==21000);
 // While flying the game's copy loses A/B and the left stick; right stick (camera) and other buttons stay.
 pad.buttons=0x1000|0x4000;pad.ly=-9000;pad.rx=5000;flightOwnsFaceButtons=true;
 Check(PadState(0,&output)==0&&output.buttons==0x4000&&output.lx==0&&output.ly==0&&output.rx==5000);
 flightOwnsFaceButtons=false;Check(PadState(0,&output)==0&&output.lx==21000&&output.ly==-9000);pad.buttons=0;pad.ly=pad.rx=0;

 MenuInputGate::lease=0;Renew(false);held=false;wasOpen=releaseKeys=0;
 Check(!Blocked());SetExternalMenu(true);Check(ExternalOpen()&&Blocked());
 Check(SUCCEEDED(DeviceState(&object,256,keyboard))&&keyboard[17]==0);
 message.message=WM_MOUSEMOVE;int previousDispatch=dispatchCalls;
 Check(Dispatch(&message)==123&&dispatchCalls==previousDispatch+1);
 now+=749;Check(ExternalOpen());now+=1;Check(!ExternalOpen()&&!Blocked());
 SetExternalMenu(true);Check(Blocked());SetExternalMenu(false);held=true;Check(!Blocked());held=false;Check(!Blocked());
 MenuInputGate::lease=&::lease;Renew(true);Check(Blocked());SetExternalMenu(false);Check(Blocked());Renew(false);Check(!Blocked());
 char result[160];DWORD count=wsprintfA(result,"PASSED %d input-gate assertions; fake devices/lease only, no game or system input changes\n",checks),written;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),result,count,&written,0);ExitProcess(0);
}
