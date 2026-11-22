#define UI_SELF_TEST
#include "../menu/NativeUI.cpp"
#pragma function(memset)
extern "C" void* memset(void* p,int v,size_t n){volatile BYTE* d=(BYTE*)p;for(size_t i=0;i<n;++i)d[i]=(BYTE)v;return p;}
extern "C" int _fltused=0;
static unsigned checks,failures;
static void Check(bool v){++checks;if(!v){++failures;char b[80];DWORD n;int len=wsprintfA(b,"Failed check %u\r\n",checks);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),b,len,&n,0);}}
static NativeUI::PadState mockPad;static DWORD mockSlot;static bool mockConnected=true;
static DWORD WINAPI MockPad(DWORD slot,NativeUI::PadState* p){if(!mockConnected||slot!=mockSlot)return ERROR_DEVICE_NOT_CONNECTED;*p=mockPad;return 0;}
static void ControllerChecks(){
 using namespace NativeUI;padLoaded=true;readPad=MockPad;probeAt=0;padSlot=4;padConnected=false;
 mockPad.buttons=1;Check(!PollController(true,false)); // connecting while held does not open
 mockPad.buttons=0;PollController(true,false);mockPad.buttons=1;Check(PollController(true,false));
 Check(!PollController(true,true)&&navDirection==0); // opening press is consumed
 mockPad.buttons=0;PollController(true,true);mockPad.buttons=2;PollController(true,true);Check(navDirection==2);
 PollController(true,true);Check(navDirection==0);repeatAt=GetTickCount();PollController(true,true);Check(navDirection==2);
 mockPad.buttons=0;PollController(true,true);mockPad.buttons=0x1000;PollController(true,true);Check(navAccept);
 PollController(true,true);Check(!navAccept);mockPad.buttons=0x2000;PollController(true,true);Check(navBack);
 mockPad.buttons=0;mockPad.lx=9000;PollController(true,true);Check(navStep==0);
 mockPad.lx=20000;stickAt=GetTickCount();PollController(true,true);Check(navStep==1);
 mockPad.lx=-20000;stickAt=GetTickCount();PollController(true,true);Check(navStep==-1);
 mockPad.lx=0;PollController(true,true);Check(navStep==0);
 mockPad.ry=8000;PollController(true,true);Check(ControllerScroll()==0);
 mockPad.ry=-25000;scrollTick=GetTickCount()-20;PollController(true,true);Check(ControllerScroll()<0&&manualScroll&&navMode&&navStep==0);
 mockPad.ry=25000;scrollTick=GetTickCount()-20;PollController(true,true);Check(ControllerScroll()>0);
 mockPad.ry=0;PollController(true,true);Check(ControllerScroll()==0&&manualScroll);
 mockPad.buttons=2;PollController(true,true);Check(!manualScroll);mockPad.buttons=0;
 mockPad.buttons=1;Check(!PollController(false,false));Check(!PollController(true,false)); // refocus held
 mockConnected=false;PollController(true,true);Check(!navMode&&!navAccept&&!navBack);
 mockConnected=true;mockSlot=2;probeAt=GetTickCount();mockPad.buttons=0;PollController(true,false);Check(padSlot==2);
 mockPad.buttons=1;Check(PollController(true,false));
 // Settings > Controls: chosen open button (Y), triggers, closing with a non-D-pad button, capture mode.
 mockPad.buttons=0;PollController(true,false,0x8000);mockPad.buttons=1;Check(!PollController(true,false,0x8000)); // D-pad Up no longer opens
 mockPad.buttons=0;PollController(true,false,0x8000);mockPad.buttons=0x8000;Check(PollController(true,false,0x8000));
 mockPad.buttons=0;PollController(true,true,0x8000);mockPad.buttons=0x8000;Check(PollController(true,true,0x8000)); // Y closes too
 mockPad.buttons=0;PollController(true,true,1);mockPad.buttons=1;Check(!PollController(true,true,1)&&navDirection==1); // D-pad Up still navigates while open
 mockPad.buttons=0;PollController(true,false,0x0400);mockPad.lt=200;Check(PollController(true,false,0x0400));mockPad.lt=0; // LT
 mockPad.buttons=0;PollController(true,true,1);PadCapture(true);mockPad.buttons=0x2000;Check(!PollController(true,true,0x2000|1)&&!navBack&&PadPressed()==0x2000);
 mockPad.buttons=0;PollController(true,true,1);mockPad.buttons=0x0020;Check(!PollController(true,true,0x0020)&&PadPressed()==0x0020); // capturing: chosen button never closes
 PadCapture(false);mockPad.buttons=0;PollController(true,true,1);
 // A D-pad press during a change is reported only, and still held afterwards it does not move the focus.
 PadCapture(true);mockPad.buttons=1;Check(!PollController(true,true,0x20)&&navDirection==0&&PadPressed()==1);
 PadCapture(false);PollController(true,true,0x20);Check(navDirection==0);mockPad.buttons=0;PollController(true,true,0x20);
 down=clicked=released=false;mouseKnown=false;io.MouseWheel=0;io.MousePos=ImVec2();navMode=true;navFocus=navCount=0;
 clips[0]={0,0,1000,1000};clipTop=0;
 auto items=[](){NavRegister("top-left",ImVec2(10,10),ImVec2(100,30),true);NavRegister("top-right",ImVec2(210,10),ImVec2(100,30),true);NavRegister("bottom-left",ImVec2(10,110),ImVec2(100,30),true);NavRegister("bottom-right",ImVec2(210,110),ImVec2(100,30),true);};
 items();navFocus=Hash("top-left");navDirection=4;NavFrame();Check(NavSelected("top-right"));items();navDirection=2;NavFrame();Check(NavSelected("bottom-right"));items();navDirection=3;NavFrame();Check(NavSelected("bottom-left"));items();navDirection=1;NavFrame();Check(NavSelected("top-left"));
 navDirection=0;navAccept=true;NavRegister("top-left",ImVec2(10,10),ImVec2(100,30),false);SetCursorScreenPos(ImVec2(10,10));Check(!InvisibleButton("top-left",ImVec2(100,30)));
 NavRegister("top-left",ImVec2(10,10),ImVec2(100,30),true);Check(InvisibleButton("top-left",ImVec2(100,30)));Check(!InvisibleButton("top-left",ImVec2(100,30)));
 unsigned count=navCount;NavRegister("settings-dismiss",ImVec2(),ImVec2(900,900),true);Check(navCount==count);
 navCount=0;navFocus=Hash("speed-slider");navStep=1;NavRegister("speed-slider",ImVec2(10,10),ImVec2(200,30),true);Check(NavAdjust("speed-slider")==1);
 navCount=0;NavRegister("other",ImVec2(10,10),ImVec2(200,30),true);io.MousePos=physicalMouse;NavFrame();Check(navFocus==Hash("speed-slider"));
 clips[0]={0,80,400,300};navCount=0;navFocus=Hash("offscreen");NavRegister("offscreen",ImVec2(10,600),ImVec2(200,30),true);navAccept=true;io.MousePos=physicalMouse;NavFrame();Check(io.MouseWheel==-1&&!navAccept&&navFocus==Hash("offscreen"));
 navAccept=true;NavRegister("offscreen",ImVec2(10,600),ImVec2(200,30),true);SetCursorScreenPos(ImVec2(10,600));Check(!InvisibleButton("offscreen",ImVec2(200,30)));
 navCount=0;navFocus=Hash("spawn-npc");navAccept=true;NavRegister("spawn-npc",ImVec2(10,10),ImVec2(100,30),false);SetCursorScreenPos(ImVec2(10,10));Check(!InvisibleButton("spawn-npc",ImVec2(100,30)));io.MousePos=physicalMouse;NavFrame();Check(navFocus==Hash("spawn-npc"));
 navCount=0;navFocus=Hash("spawn-npc");navAccept=false;NavRegister("spawn-npc",ImVec2(10,110),ImVec2(100,30),true);NavRegister("settings-cog",ImVec2(210,10),ImVec2(40,40),true);io.MousePos=physicalMouse;navDirection=4;NavFrame();Check(navFocus==Hash("spawn-npc"));NavRegister("spawn-npc",ImVec2(200,110),ImVec2(100,30),true);NavRegister("settings-cog",ImVec2(210,10),ImVec2(40,40),true);io.MousePos=physicalMouse;navDirection=1;NavFrame();Check(NavSelected("settings-cog"));navDirection=0;
 NavScope(2);Check(navCount==0&&navFocus==0&&!navAccept);navFocus=Hash("spawn-npc");ClearInput();Check(!mouseKnown&&navCount==0&&navFocus==Hash("spawn-npc"));
 // NPC arrows must navigate across their row instead of diagonally to Spawn.
 clips[0]={0,0,1000,1000};navCount=0;navFocus=Hash("next");navMode=true;navAccept=false;navDirection=0;manualScroll=false;
 auto npc=[](){NavRegister("spawn",ImVec2(502,196),ImVec2(94,32),true);NavRegister("prev",ImVec2(256,242),ImVec2(32,32),true);NavRegister("next",ImVec2(566,242),ImVec2(32,32),true);NavLink("prev",4,"next");NavLink("next",3,"prev");NavLink("prev",1,"spawn");NavLink("next",1,"spawn");NavLink("spawn",2,"next");};
 npc();Check(NextItem(2,3)==1);Check(NextItem(1,4)==2);Check(NextItem(1,1)==0);Check(NextItem(2,1)==0);Check(NextItem(0,2)==2);
 // Same-row preference also works without card-specific links.
 navItems[2].links[2]=0;Check(NextItem(2,3)==1);
 navCount=0;navFocus=Hash("offscreen");clips[0]={0,80,400,300};NavRegister("offscreen",ImVec2(10,600),ImVec2(100,30),true);manualScroll=true;navScroll=0;navDirection=0;io.MousePos=physicalMouse;io.MouseWheel=0;NavFrame();Check(io.MouseWheel==0);manualScroll=false;navCount=0;navFocus=0;clips[0]={0,0,1000,1000};
 ClearInput();io.MousePos=ImVec2(500,500);io.MouseWheel=0;navMode=true;NavFrame();Check(navMode); // recentring preserves controller focus
 io.MousePos=ImVec2(600,500);NavFrame();Check(!navMode); // real mouse takes over
 navMode=true;navScope=0;navFocus=navCount=0;NavRegister("settings-cog",ImVec2(210,10),ImVec2(40,40),true);NavRegister("Player",ImVec2(10,110),ImVec2(100,30),true);Check(NavSelected("Player"));Check(ControllerActive());navMode=false;Check(!ControllerActive());
 navMode=false;navCount=navFocus=navEligible=0;navStep=navDirection=0;navAccept=navBack=false;clips[0]={0,0,2560,1440};
}
extern "C" void TestEntry(){
 using namespace NativeUI;clips[0]={0,0,2560,1440};io.MousePos=ImVec2(15,15);SetCursorScreenPos(ImVec2(10,10));down=true;clicked=true;released=false;
 Check(!InvisibleButton("button",ImVec2(100,30)));Check(IsItemHovered());Check(IsItemActivated());Check(IsItemActive());
 clicked=false;Check(!InvisibleButton("button",ImVec2(100,30)));Check(!IsItemActivated());Check(IsItemActive());
 down=false;released=true;Check(InvisibleButton("button",ImVec2(100,30)));Check(!IsItemActive());Check(!InvisibleButton("button",ImVec2(100,30)));
 down=true;clicked=true;released=false;InvisibleButton("drag",ImVec2(100,30));io.MousePos=ImVec2(500,500);clicked=false;InvisibleButton("drag",ImVec2(100,30));Check(IsItemActive());Check(!IsItemHovered());
 down=false;released=true;Check(!InvisibleButton("drag",ImVec2(100,30)));Check(active==0);
 io.MousePos=ImVec2(15,15);down=true;clicked=true;released=false;InvisibleButton("first",ImVec2(100,30));SetCursorScreenPos(ImVec2(200,200));InvisibleButton("second",ImVec2(100,30));Check(!IsItemActive());Check(!IsItemActivated());
 vertices=(NativeVertex*)VirtualAlloc(0,sizeof(NativeVertex)*120000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);batches=(NativeBatch*)VirtualAlloc(0,sizeof(NativeBatch)*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);clips[0]={0,0,2560,1440};
 list.AddImage(2,ImVec2(10,10),ImVec2(40,40));list.AddImage(2,ImVec2(40,10),ImVec2(70,40));Check(vertexCount==12&&batchCount==1&&batches[0].count==12);
 list.PushClipRect(ImVec2(20,20),ImVec2(60,60),true);list.AddImage(2,ImVec2(),ImVec2(100,100));Check(batchCount==2&&batches[1].clip.left==20&&batches[1].clip.right==60);list.PopClipRect();
 list.AddRectFilled(ImVec2(100,100),ImVec2(200,200),0xffffffff,12);list.AddCircleFilled(ImVec2(100,100),10,0xffffffff);Check(!Overflowed());
 ControllerChecks();
 vertexCount=119999;list.AddImage(2,ImVec2(),ImVec2(30,30));Check(Overflowed()&&vertexCount==119999);Shutdown();
 char text[100];int n=wsprintfA(text,"Native input/drawing: %u checks, %u failures\r\n",checks,failures);DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,n,&done,0);ExitProcess(failures?1:0);
}
