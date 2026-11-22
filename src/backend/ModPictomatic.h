namespace Pictomatic {
static bool active,held;
static unsigned inputFrames,openAttempts,openFailures;
static unsigned lastGate;
static void* save;static int slot=-1;
static void* cachedVm;static int indices[6];
static bool UseDown(bool fire,bool pickup,bool camera,bool mouse){return fire||pickup||camera||mouse;}
static bool Press(bool allowed,bool down,bool& previous){
 bool pressed=allowed&&down&&!previous;previous=down;return pressed;
}
static int PlayerIndex(void* players,void* actor){
 if(!players||!actor)return -1;
 int count=At<int>(players,4);if(count<1||count>2)return -1;
 for(int i=0;i<count;++i)if(At<void*>(players,0x1a0+i*0x58)==actor)return i;
 return -1;
}
// Native first-disc pickup uses avatarAboard to choose a player (1a4460).
// A menu grant has no pickup actor: that lookup returns -1. ScreenViewmaster
// copies +288 into its input owner at entry (120825 -> 5f1df0), so the intro
// renders but cannot receive confirmation. Bind only an armed menu grant with
// a missing owner, before the original entry initializes native GUI input.
static bool entryArmed;static unsigned __int64 entryActor;
static void* entrySave;static int entrySlot=-1;
static void (*entryOriginal)(void*);
static void EnterForPlayer(void* screen,int player,bool matchingGrant){
 if(matchingGrant&&player>=0&&player<2&&At<int>(screen,0x288)==-1)
  At<int>(screen,0x288)=player;
 entryOriginal(screen);
}
static void Entry(void* screen){
 int player=-1;bool matching=false;
 __try{
  BYTE* session=*(BYTE**)A(0xb29a60);
  matching=entryArmed&&session&&At<void*>(session,0x1a0)==entrySave&&At<int>(session,0x64)==entrySlot;
  if(matching)player=PlayerIndex(*(void**)A(0xb2ef50),Resolve(entryActor));
 }__except(EXCEPTION_EXECUTE_HANDLER){matching=false;}
 entryArmed=false;
 EnterForPlayer(screen,player,matching);
}
static bool PrepareGrant(const Context& c){
 if(PlayerIndex(*(void**)A(0xb2ef50),c.actor)<0)return false;
 entryActor=c.handle;entrySave=c.saveData;entrySlot=c.slot;entryArmed=true;return true;
}
static bool Show(void* actor){
 int player=PlayerIndex(*(void**)A(0xb2ef50),actor);if(player<0)return false;
 void* manager=*(void**)A(0xb2ef10);if(!manager)return false;
 void* screen=((void*(*)(void*,const char*))A(0x4b26c0))(manager,"ScreenViewmaster");
 if(!screen||*(void**)screen!=(void*)A(0x782258))return false;
 // Exact native ShowViewMasterScreen path (0x12aaa0). Retain its pause,
 // player and GUI-stack checks; never force screen visibility or camera state.
 if(!((bool(*)(void*,int))A(0x5d6590))(screen,player))return false;
 At<int>(screen,0x288)=player;return true;
}
// The native viewer closes on entry when its disc list is empty (12084d).
// f2b60 lists type-8 unlocks with a nonzero state. Equipping UP_SlideViewer
// alone supplies no pictures, so add only a starter disc when none is present.
static bool DiscVisible(unsigned type,int state){return type==8&&state>0&&state<=5;}
static bool EnsureDisc(const Context& c,DWORD commandId){
 void* manager=*(void**)A(0xb2fde0);
 if(!manager||*(void**)manager!=(void*)A(0x7a2b30))return false;
 int count=*(int*)A(0xb2c838);void** entries=At<void**>(manager,0x10);
 if(count<1||count>10000||!entries)return false;
 for(int i=0;i<count;++i){void* item=entries[i];if(item&&DiscVisible(At<unsigned>(item,0x98),At<int>(item,0xa0))){
   const char* pictures=((const char*(*)(void*,const char*))A(0x5bda50))(item,"AssociatedMissions");
   if(pictures&&pictures[0])return true;
  }}
 void* disc=((void*(*)(void*,const char*))A(0x614c40))(manager,"VM_Disc_WT_1");
 if(!disc||At<unsigned>(disc,0x98)!=8||At<int>(disc,0xa0)!=0)return false;
 const char* pictures=((const char*(*)(void*,const char*))A(0x5bda50))(disc,"AssociatedMissions");
 if(!pictures||!pictures[0])return false;
 if(!BackupCurrency(c,commandId,2,8,"VM_Disc_WT_1"))return false;
 ((bool(*)(void*,const char*,int))A(0x632cd0))(manager,"VM_Disc_WT_1",1);
 ((void(*)(void*,int))A(0x5f2480))(disc,5);
 return DiscVisible(At<unsigned>(disc,0x98),At<int>(disc,0xa0));
}
static void Enable(const Context& c){active=true;held=false;cachedVm=0;save=c.saveData;slot=c.slot;}
static void Input(void* actor){
 if(!active||actor!=Player())return;
 ++inputFrames;lastGate=1;
 BYTE* session=*(BYTE**)A(0xb29a60);
 if(!session||At<void*>(session,0x1a0)!=save||At<int>(session,0x64)!=slot){active=false;held=false;return;}
 __try{
  void* sc=At<void*>(actor,0x3c0);void* vm=sc?At<void*>(sc,0x28):0;
  if(!vm){held=false;return;}
  lastGate=2;
  if(vm!=cachedVm){
   const char* names[]={"currentTool","carryingTool","allowToolInHand","fireWeaponButtonDown","pickupDropButtonDown","cameraButtonDown"};
   for(unsigned i=0;i<6;++i){indices[i]=((int(*)(void*,const char*))A(0x542310))(*(void**)vm,names[i]);if(indices[i]<0||indices[i]>4096){cachedVm=0;held=false;return;}}
   cachedVm=vm;
  }
  void* values=At<void*>(vm,8);BYTE* entries=values?*(BYTE**)values:0;if(!entries){held=false;return;}
  int* cells[6];for(unsigned i=0;i<6;++i){BYTE* entry=entries+indices[i]*16;if(*(int*)entry!=0){held=false;return;}cells[i]=(int*)(entry+8);}
  lastGate=3;
  bool allowed=*cells[0]==6&&*cells[1]&&*cells[2]&&!enabled&&!Open()&&!NativeMenuOpen()&&Foreground()&&!RegionTeleport::Active();
  // Script button flags can be produced and cleared inside the native update.
  // Read the physical primary mouse button as well, only acting for tool 6.
  // Track blocked presses too so a menu click cannot open the viewer on close.
  bool down=UseDown(*cells[3]!=0,*cells[4]!=0,*cells[5]!=0,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0);
  lastGate=allowed?4:3;bool pressed=Press(allowed,down,held);
  if(allowed&&down){
   // Consume this tool's use action, rather than synthesizing the camera
   // action shared with Player2's WeaponAim/airborne animation states.
   *cells[3]=0;*cells[4]=0;*cells[5]=0;
   if(pressed){++openAttempts;lastGate=5;
    if(!Show(actor)){++openFailures;lastGate=6;Message(TS3CheatBusy,"Pict-O-Matic cannot open right now.");}
    else lastGate=7;
   }
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){lastGate=8;cachedVm=0;held=false;}
}
}
