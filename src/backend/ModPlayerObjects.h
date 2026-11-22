// Native HealthIF and carriable spawn paths audited against the installed EXE.
namespace PlayerObjects {
static bool god,ready;
static int godSlot=-1;
static void* godSave;
static bool spawnPending;
static Context spawnContext;
static unsigned spawnIndex,serial;
static DWORD lastSpawn;
static unsigned __int64 initialHandles[8];static void* initialActors[8];static unsigned initialCarry[8];
static unsigned __int64* spawned=initialHandles;
static void** spawnedActors=initialActors;
static unsigned* carryFixFrames=initialCarry;
static unsigned capacity=8;
static bool Grow(){
 if(capacity>0x3fffffffU)return false;unsigned next=capacity*2;
 auto handles=(unsigned __int64*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,SIZE_T(next)*sizeof(*spawned));
 auto actors=(void**)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,SIZE_T(next)*sizeof(*spawnedActors));
 auto carry=(unsigned*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,SIZE_T(next)*sizeof(*carryFixFrames));
 if(!handles||!actors||!carry){if(handles)HeapFree(GetProcessHeap(),0,handles);if(actors)HeapFree(GetProcessHeap(),0,actors);if(carry)HeapFree(GetProcessHeap(),0,carry);return false;}
 Copy(handles,spawned,SIZE_T(capacity)*sizeof(*spawned));Copy(actors,spawnedActors,SIZE_T(capacity)*sizeof(*spawnedActors));Copy(carry,carryFixFrames,SIZE_T(capacity)*sizeof(*carryFixFrames));
 if(spawned!=initialHandles){HeapFree(GetProcessHeap(),0,spawned);HeapFree(GetProcessHeap(),0,spawnedActors);HeapFree(GetProcessHeap(),0,carryFixFrames);}
 spawned=handles;spawnedActors=actors;carryFixFrames=carry;capacity=next;return true;
}
static Context objectContext;
static bool trackedContext;
static void Observe(const Context& c,bool valid){
 if(!valid)return;
 if(!trackedContext||c.slot!=objectContext.slot||c.saveData!=objectContext.saveData||c.world!=objectContext.world||c.worldManager!=objectContext.worldManager){
  // Volatile stores keep this no-CRT DLL from emitting a memset dependency.
  for(unsigned i=0;i<capacity;++i){((volatile unsigned __int64*)spawned)[i]=0;((void* volatile*)spawnedActors)[i]=0;((volatile unsigned*)carryFixFrames)[i]=0;}
  Copy(&objectContext,&c,sizeof(c));trackedContext=true;
 }
}
static bool OwnedSpawn(unsigned index,void* actor,void* player){
 return index<capacity&&actor&&actor==spawnedActors[index]&&actor!=player&&At<unsigned __int64>(actor,0x38)==spawned[index]&&At<void*>(actor,0x168)==actor;
}
static unsigned Count(){
 unsigned count=0;void* player=Player();
 for(unsigned i=0;i<capacity;++i){
  if(!spawned[i])continue;
  void* actor=Resolve(spawned[i]);
  if(OwnedSpawn(i,actor,player))++count;
  else {spawned[i]=0;spawnedActors[i]=0;carryFixFrames[i]=0;}
 }
 return count;
}
// Each model is its own archive (assets/characters/carriable/<name>.zip) loaded
// on demand, so the catalogue is the same in Toy Box and Story levels. Native
// world definitions use the same Carriable.sx / Carriable.bmx; breaking and
// explosions come from each model's own data (_BR / _EXP).
static const char* models[TS3ObjectCount]={
 "Barrel_Medium_NB","Barrel_Large_NB","Barrel_Medium_BR","Barrel_Large_BR","Crate_Large_NB","Crate_Large_BR","Crate_Huge_NB","Parcel_Large_BR","TNTBarrel_Medium_EXP",
 "Kickball_Small_NB","BatteryAA_Medium_NB","BatteryD_Large_NB","Battery9V_Large_NB","STAndysCellphone_Medium_NB","CrushedCube_Large_NB","CrushedCube_Huge_NB",
 "MoneyBag_Small_NB","MoneyBag_Medium_NB","MoneyBag_Large_NB","RedStone_Medium_NB",
 "SpaceBattery_Medium_NB","SpaceParcel1_Large_NB","SpaceParcel2_Large_NB",
 "VeggiesCarrot_Medium_NB","VeggiesCorn_Medium_NB","VeggiesPumpkin_Medium_NB","VeggiesTomato_Medium_NB"};
static constexpr unsigned kickball=9,clearObjects=TS3ObjectCount;
static bool CanRefresh(bool protectedNow,float remaining){return !protectedNow||(remaining>0&&remaining<0.35f);}
static void* Health(void* actor){
 int id=*(int*)A(0xb2a464);if(id<=0||!actor)return 0;
 void** vt=*(void***)actor;
 if(vt[9]!=(void*)A(0x5598e0))return 0;
 void* h=((void*(*)(void*,int))vt[9])(actor,id);
 if(!h)return 0;
 void* table=*(void**)h;
 if(table!=(void*)A(0x7692b0)&&table!=(void*)A(0x7d9660))return 0;
 if(*(void**)((BYTE*)h-0x10)!=actor)return 0;
 return h;
}
static void Protect(const Context& c,bool valid){
 if(!god)return;
 if(valid&&(c.slot!=godSlot||c.saveData!=godSave)){god=false;return;}
 if(!valid)return;
 __try{void* h=Health(c.actor);if(!h)return;
  if(CanRefresh(At<BYTE>(h,0x24)!=0,At<float>(h,0x28)))
   ((void(*)(void*,bool,float,int,const char*))A(0x459770))(h,true,0.5f,0,0);
 }__except(EXCEPTION_EXECUTE_HANDLER){god=false;}
}
static void Toggle(const Context& c){
 if(god){god=false;Message(TS3CheatOk,"God mode off. Damage protection ends shortly.");return;}
 if(!ready||!Health(c.actor)){Message(TS3CheatFault,"God mode is unavailable for this character.");return;}
 god=true;godSlot=c.slot;godSave=c.saveData;Protect(c,true);
 Message(TS3CheatOk,"God mode enabled.");
}
static void Request(const Context& c,unsigned index){
 if(!ready||!RegionTeleport::hookReady){Message(TS3CheatFault,"Object spawning is unavailable for this build.");return;}
 if(index>clearObjects){Message(TS3CheatInvalid,"Choose an object to spawn.");return;}
 if(spawnPending){Message(TS3CheatBusy,"Wait a moment before spawning another object.");return;}
 Copy(&spawnContext,&c,sizeof(c));spawnIndex=index;spawnPending=true;
 Message(TS3CheatOk,index==clearObjects?"Clearing spawned objects...":"Spawning object...");
}
// The lightHand player throw path switches to the inventory-ball action.
// A spawned physical kickball must use the normal carried-object throw instead.
static bool BallNeedsCarryFix(const char* style){return style&&lstrcmpA(style,"lightHand")==0;}
static void RepairBallCarry(){
 for(unsigned i=0;i<capacity;++i){if(!carryFixFrames[i])continue;--carryFixFrames[i];
  __try{
   void* actor=spawned[i]?Resolve(spawned[i]):0;
   if(!OwnedSpawn(i,actor,Player())){carryFixFrames[i]=0;continue;}
   void* property=((void*(*)(void*,const char*))A(0x55fd40))(actor,"CarryStyle");
   if(!property||At<BYTE>(property,0x2a)!=7||!BallNeedsCarryFix(At<const char*>(property,8)))continue;
   void* pool=*(void**)A(0x9c2700);if(!pool)continue;
   const char* value=((const char*(*)(void*,const char*))A(0x24f860))(pool,"mediumHand");
   if(value){At<const char*>(property,8)=value;carryFixFrames[i]=0;}
  }__except(EXCEPTION_EXECUTE_HANDLER){carryFixFrames[i]=0;}
 }
}
static void Frame(){
 RepairBallCarry();
 if(!spawnPending)return;
 spawnPending=false;lastSpawn=GetTickCount();
 __try{
  Context c;if(!ReadContext(Player(),c)||c.actor!=spawnContext.actor||c.handle!=spawnContext.handle||c.slot!=spawnContext.slot||c.saveData!=spawnContext.saveData||c.world!=spawnContext.world||c.worldManager!=spawnContext.worldManager||RegionTeleport::Active()){
   Message(TS3CheatStale,"Object spawn cancelled because gameplay changed.");return;}
  void* stream=*(void**)A(0xb2ef20);
  bool streaming=stream&&At<BYTE>(stream,0x40)!=0;
  if(!RegionTeleport::LoadingStateReady(((bool(*)())A(0x477410))(),streaming,streaming&&At<BYTE>(stream,0xb8)!=0,streaming?At<int>(stream,0xbc):0,streaming?At<int>(stream,0xb4):0)){Message(TS3CheatBusy,"Wait for the area to finish loading.");return;}
  if(spawnIndex==clearObjects){
   unsigned count=0;
   for(unsigned i=0;i<capacity;++i){
    void* actor=spawned[i]?Resolve(spawned[i]):0;
    if(OwnedSpawn(i,actor,c.actor)){
     // ActorMarkForDeath uses the embedded lifecycle component at +160.
     // Defer deletion through the engine, including carried-object cleanup.
     ((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);++count;
    }
    // Retain live handles until native removal completes.
    if(!actor){spawned[i]=0;spawnedActors[i]=0;carryFixFrames[i]=0;}
   }
   Message(TS3CheatOk,count?"Spawned objects cleared.":"No spawned objects to clear in this area.");return;
  }
  int free=-1;for(unsigned i=0;i<capacity;++i)if(!spawned[i]||!OwnedSpawn(i,Resolve(spawned[i]),c.actor)){free=int(i);break;}
  if(free<0){unsigned old=capacity;if(!Grow()){Message(TS3CheatFault,"Not enough memory to track another object.");return;}free=int(old);}
  Vec dir=Direction(c.actor),at=At<Vec>(c.actor,0x80);at.x+=dir.x*3;at.z+=dir.z*3;at.y+=1;
  if(!Pose(at)){Message(TS3CheatInvalid,"Cannot spawn at this position.");return;}
  char params[768],name[64];wsprintfA(name,"CheatProp_%lu_%u",GetTickCount(),++serial);
  wsprintfA(params,"class=compactor;destroyWhenDead=True;propTargetable=True;propCarryable=True;voiceset=Pickup;model_name=characters/Carriable/%s;anim1=Characters/Carriable.sx;behaviorMapFile=Characters/Carriable.bmx;AddComponent_AiBehaviorControl=True;enableShutdown=True;shutdownDistance=30;",models[spawnIndex]);
  // Exactly the parameter-constructor / create / name-setup / free sequence
  // used by native script callbacks. Runs after the actor traversal.
  unsigned __int64 set[3]={};
  ((void*(*)(void*,const char*))A(0x4f1c60))(set,params);
  void* actor=((void*(*)(void*,const Vec*,float,bool,bool))A(0x554a80))(set,&at,0.f,false,false);
  if(actor){((void(*)(void*,const char*,void*))A(0x557ba0))(actor,name,0);spawned[free]=At<unsigned __int64>(actor,0x38);spawnedActors[free]=actor;carryFixFrames[free]=spawnIndex==kickball?120:0;}
  ((void(*)(void*))A(0x4f2870))(set);
  Message(actor?TS3CheatOk:TS3CheatFault,actor?"Object spawned in front of you.":"The game could not create this object here.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Object creation stopped after a game-state change. Do not repeat immediately.");}
}
static void Install(){
 __try{
  ready=true;
#include "ModPlayerObjectsGuards.h"
 }__except(EXCEPTION_EXECUTE_HANDLER){ready=false;}
}
}
