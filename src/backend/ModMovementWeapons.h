namespace MovementWeapons {
#include "ModPictomatic.h"
static bool ready,fast,jump;
static float runMultiplier=2.f,jumpMultiplier=1.8f;
static void* saveIdentity;
static int saveSlot=-1;
static void* jumpActor;static unsigned __int64 jumpHandle;static void* jumpVm;static float* jumpCell;static float jumpBase,jumpApplied;
static void (*groundOriginal)(void*,float);
static void (*scriptsOriginal)(void*);
static const char* locks[]={"UP_Primary","UP_GhostBuster","UP_Abductinator","UP_FairyWand","UP_Blaster","UP_PickAxe","UP_ToolBox","UP_Camera","UP_SlideViewer"};
static bool Session(){
 BYTE* s=*(BYTE**)A(0xb29a60);return s&&At<void*>(s,0x1a0)==saveIdentity&&At<int>(s,0x64)==saveSlot;
}
static bool Selected(void* actor){return actor&&actor==Player()&&Session();}
// Ground update reads walk/run/sprint arrays directly, bypassing the animation getters.
// Airborne movement selects 0x19c + carry-mode*4 (or the 0x24c override)
// at native 0x4bea4..0x4bed2, instead of the run table at 0x16c.
// Scale the player's native speed tables and horizontal limits only during that update.
static const unsigned speedOffsets[]={0x160,0x164,0x168,0x16c,0x170,0x174,0x178,0x17c,0x180,0x60,0x64,0x6c,0x70,0x19c,0x1a0,0x1a4,0x24c};
static void BoostedGround(void* controller,float dt){
 constexpr unsigned count=sizeof(speedOffsets)/sizeof(speedOffsets[0]);
 float before[count],applied[count];
 for(unsigned i=0;i<count;++i){float v=At<float>(controller,speedOffsets[i]);before[i]=v;applied[i]=(v>-100&&v<100)?v*runMultiplier:v;At<float>(controller,speedOffsets[i])=applied[i];}
 __try{groundOriginal(controller,dt);}
 __finally{for(unsigned i=0;i<count;++i)if(At<float>(controller,speedOffsets[i])==applied[i])At<float>(controller,speedOffsets[i])=before[i];}
}
static void Ground(void* controller,float dt){
 if(fast&&!enabled&&Selected(At<void*>(controller,8)))BoostedGround(controller,dt);else groundOriginal(controller,dt);
}
static float* JumpCell(void* actor,void** vmOut){
 void* sc=At<void*>(actor,0x3c0);void* vm=sc?At<void*>(sc,0x20):0;if(!vm)return 0;
 int index=((int(*)(void*,const char*))A(0x542310))(*(void**)vm,"jumpVelMultiplier");
 if(index<0||index>4096)return 0;
 void* vars=At<void*>(vm,8);BYTE* entries=vars?*(BYTE**)vars:0;if(!entries)return 0;
 BYTE* entry=entries+index*16;if(*(int*)entry!=1)return 0;*vmOut=vm;return (float*)(entry+8);
}
static void RestoreJump(){
 __try{if(jumpActor&&Resolve(jumpHandle)==jumpActor){void* vm=0;float* p=JumpCell(jumpActor,&vm);if(p==jumpCell&&vm==jumpVm&&*p==jumpApplied)*p=jumpBase;}}__except(EXCEPTION_EXECUTE_HANDLER){}
 jumpActor=jumpVm=0;jumpCell=0;jumpHandle=0;
}
static void ApplyJump(void* actor){
 if(!jump||enabled||!Selected(actor)){if(actor==jumpActor)RestoreJump();return;}
 __try{
  if(jumpActor&&jumpActor!=actor)RestoreJump();
  void* vm=0;float* p=JumpCell(actor,&vm);if(!p)return;
  float v=*p;if(!(v>0&&v<10))return;
  if(p!=jumpCell||vm!=jumpVm||jumpActor!=actor){jumpBase=v;jumpActor=actor;jumpHandle=At<unsigned __int64>(actor,0x38);jumpVm=vm;jumpCell=p;}
  else if(v!=jumpApplied)jumpBase=v;
  jumpApplied=jumpBase*jumpMultiplier;*p=jumpApplied;
 }__except(EXCEPTION_EXECUTE_HANDLER){jump=false;jumpActor=jumpVm=0;jumpCell=0;}
}
static void Scripts(void* actor){
 if((fast||jump)&&actor==Player()&&!Session()){fast=jump=false;RestoreJump();}
 if(jump||jumpActor)ApplyJump(actor);
 Pictomatic::Input(actor);
 scriptsOriginal(actor);
 if(jump&&actor==Player())ApplyJump(actor);
}
static void Toggle(const Context& c,bool speedToggle){
 if(!ready){Message(TS3CheatFault,"Movement cheats are unavailable for this build.");return;}
 if(speedToggle){fast=!fast;if(fast&&runMultiplier==1)runMultiplier=2.f;}else{
  if(!jump){void* vm=0;if(!JumpCell(c.actor,&vm)){Message(TS3CheatNoGame,"Super jump is unavailable for this character.");return;}}
  jump=!jump;if(jump&&jumpMultiplier==1)jumpMultiplier=1.8f;if(!jump)RestoreJump();
 }
 saveIdentity=c.saveData;saveSlot=c.slot;
 if(jump)ApplyJump(c.actor);
 Message(TS3CheatOk,speedToggle?(fast?"Super speed enabled.":"Normal running speed restored."):(jump?"Super jump enabled.":"Normal jumping restored."));
}
static void SetMultiplier(const Context& c,bool running,float value){
 if(!ready||!(value>=0.5f&&value<=(running?5.f:3.f))){Message(TS3CheatInvalid,"Choose a movement value within the slider range.");return;}
 if(!running&&value!=1){void* vm=0;if(!JumpCell(c.actor,&vm)){Message(TS3CheatNoGame,"Super jump is unavailable for this character.");return;}}
 saveIdentity=c.saveData;saveSlot=c.slot;
 if(running){runMultiplier=value;fast=value!=1;}
 else {RestoreJump();jumpMultiplier=value;jump=value!=1;if(jump)ApplyJump(c.actor);}
 Message(TS3CheatOk,value==1?(running?"Normal running speed restored.":"Normal jumping restored."):"Movement multiplier updated.");
}
static unsigned OwnedMask(const Context& c){
 if(!ready||!c.toyBox)return 0;
 __try{void* manager=*(void**)A(0xb2fde0);if(!manager||*(void**)manager!=(void*)A(0x7a2b30))return 0;
 unsigned mask=1;for(unsigned i=1;i<9;++i){void* lock=((void*(*)(void*,const char*))A(0x614c40))(manager,locks[i]);if(lock&&At<int>(lock,0xa0)==5)mask|=1u<<i;}return mask;
 }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
// Player2.sx separates gunLock[0..4] from toolLock[0..6]. Menu order is neither enum.
static unsigned NativeSelection(unsigned index){static const unsigned ids[]={0,1,2,3,4,3,4,5,6};return index<9?ids[index]:0;}
static const char* SelectionEvent(unsigned index){return index>=1&&index<=4?"SelectNewGun":"SelectNewTool";}
static void Equip(const Context& c,unsigned index,DWORD commandId=0){
 if(!c.toyBox){Message(TS3CheatNoGame,"Weapons are Toy Box only.");return;}
 if(!ready||index<1||index>=9){Message(TS3CheatInvalid,"Choose a supported weapon or tool.");return;}
 void* sc=At<void*>(c.actor,0x3c0);void* vm=sc?At<void*>(sc,0x28):0;
 if(!vm||((int(*)(void*,const char*))A(0x542310))(*(void**)vm,"currentTool")<0){Message(TS3CheatNoGame,"This character cannot switch tools right now.");return;}
 if(index==8&&!Pictomatic::PrepareGrant(c)){Message(TS3CheatBusy,"Pict-O-Matic cannot open right now.");return;}
 const bool added=!(OwnedMask(c)&(1u<<index));
 if(added){
  void* manager=*(void**)A(0xb2fde0);
  if(!manager||*(void**)manager!=(void*)A(0x7a2b30)){Message(TS3CheatFault,"Weapon inventory is unavailable.");return;}
  void* lock=((void*(*)(void*,const char*))A(0x614c40))(manager,locks[index]);
  if(!lock||At<int>(lock,0xa0)<0||At<int>(lock,0xa0)>4){Message(TS3CheatFault,"This weapon cannot be added right now.");return;}
  if(!BackupCurrency(c,commandId,2,index,locks[index])){Message(TS3CheatFault,"Save backup failed. Weapon was not added.");return;}
  ((bool(*)(void*,const char*,int))A(0x632cd0))(manager,locks[index],1);
  ((void(*)(void*,int))A(0x5f2480))(lock,5);
  if(At<int>(lock,0xa0)!=5){Message(TS3CheatFault,"Weapon unlock did not finish. Do not retry yet.");return;}
 }
 if(index==8&&!Pictomatic::EnsureDisc(c,commandId)){Message(TS3CheatFault,"Pict-O-Matic needs a picture disc; starter disc could not be added.");return;}
 // Both script handlers parse ASCII; SelectNewGun also selects tool mode 2.
 char choice[8];wsprintfA(choice,"%u",NativeSelection(index));
 ((void(*)(void*,const char*,const char*,void*,unsigned __int64))A(0x49e0b0))(*(void**)A(0xb29a70),SelectionEvent(index),choice,0,c.handle);
 if(index==8)Pictomatic::Enable(c);
 Message(TS3CheatOk,added?"Weapon added and selected. Close the menu to use it.":"Weapon selected. Close the menu to use it.");
}
static bool Cell(ULONG_PTR cell,ULONG_PTR expected,void* replacement){
 void** p=(void**)A(cell);if(*p!=(void*)A(expected))return false;DWORD old;if(!VirtualProtect(p,8,PAGE_READWRITE,&old))return false;
 bool ok=InterlockedCompareExchangePointer(p,replacement,(void*)A(expected))==(void*)A(expected);DWORD unused;VirtualProtect(p,8,old,&unused);return ok;
}
static void Install(){
 __try{
  ready=true;
#include "ModMovementWeaponsGuards.h"
  if(!ready)return;
  if(*(void**)A(0x76dda0+18*8)!=(void*)A(0x4ba10)||*(void**)A(0x7ae830+18*8)!=(void*)A(0x5618d0)){ready=false;return;}
  if(*(void**)A(0x782258+35*8)!=(void*)A(0x120800)){ready=false;return;}
  Pictomatic::entryOriginal=(void(*)(void*))A(0x120800);
  if(!Cell(0x782258+35*8,0x120800,(void*)Pictomatic::Entry)){ready=false;return;}
  groundOriginal=(void(*)(void*,float))A(0x4ba10);scriptsOriginal=(void(*)(void*))A(0x5618d0);
  ready=Cell(0x76dda0+18*8,0x4ba10,(void*)Ground)&&Cell(0x7ae830+18*8,0x5618d0,(void*)Scripts);
 }__except(EXCEPTION_EXECUTE_HANDLER){ready=false;}
}
}
