// Same-world streaming travel. Native building-lot evidence and RVAs are recorded
// in work/CompleteEdition/mod-menu/teleport-audit/native-path.txt.
// Included inside CheatMenuBridge, after Context, Message and ReadContext.
namespace RegionTeleport {
enum Phase { Idle, Requested, FadingOut, Settling, Loading };
static Phase phase;
static void (*originalFrame)(void*);
static bool hookReady,wasFlying,ownsFlight,moved,recovering,landingReady;
static void* streamManager;
static bool usesStreaming;
static unsigned fadeMask;
static void* travelActor;
static unsigned __int64 travelHandle;
static Vec origin,target;
static DWORD started,phaseTick;
static unsigned frames,stable;
static Context travelContext;
static int rememberedSlot=-1,rememberedWorld=-1;
static void* rememberedSave;
static void* rememberedWorldManager;
struct PositionSlot {Vec point;void* save;void* manager;int slot,world;bool used;int area;};
static PositionSlot positions[6];
// Authored avatar placement points from westerntown.oct (scene-root transforms,
// the same frame the native well warps use): Woody start, P2 drop-ins and car-park arrivals.
static const Vec places[TS3PlaceCount]={
 {7.059f,65.042f,-1.692f},     // Town: StartPosition_Woody
 {70.135f,62.260f,14.485f},    // Farm: StartPosition_P2_WT_Farm
 {-103.566f,74.288f,22.963f},  // Suburbs: StartPosition_P2_WT_Suburbs
 {228.302f,93.840f,88.723f},   // Bandit hideout: StartPosition_P2_WT_Hideout
 // Well exits: where the native well drops you; its exit scripts enter these areas.
 {41.863f,66.047f,-38.510f},     // Quarry: LOC_WellQuarry_TopJumpOut (TRIG_WellQuarry_TunnelOut enters WT_Farm)
 {-41.218f,64.350f,-25.764f},     // Stagecoach: LOC_WellStagePlat_TopJumpOut (enters WT_Town)
 {-26.971f,78.165f,107.256f},  // Haunted House gate: StartPosition_P2_WT_Mod_HauntedHouse
 {-98.260f,50.840f,-75.434f},  // Enchanted Glen gate: StartPosition_P2_WT_Mod_EnchantedGlen
 {16.398f,57.261f,-172.198f},  // Zurg's Spaceport gate: StartPosition_P2_WT_Mod_SpacePort
 // Car parks: native CarParksTeleport arrivals used by TRIG_StuntParkEnt / TRIG_SkateParkEnt.
 {85.947f,49.202f,98.398f},    // Stunt Park: LOC_CarTeleToStunt1
 {39.642f,49.042f,118.296f}};  // Skate Park: LOC_CarTeleToSkate1
static bool placeTravel;
// Native area entry, identical to Meridian EnterAreaNode (vtable slot 29, 69eef0):
// MapArea lookup 4fd320(registry b2ac90, type "MapArea" 75f6f0, name) and the
// "PlayerEnteredArea" message (7fd7c8, avatar handle) handled by
// PerformanceManagerTS3 (1dcfe0 -> 5d6ae0): per-area model/NPC sets, the
// four-area recent list at [manager+38]+50 and a streaming update. Walking
// through TRIG_AreaEntry_* does exactly this; teleport skips those triggers.
static const char* const areaNames[]={"WT_Town","WT_Farm","WT_Suburbs","WT_Hideout","WT_Mod_HauntedHouse","WT_Mod_EnchantedGlen","WT_Mod_SpacePort","WT_Mod_SkatePark","WT_Mod_StuntPark"};
static const int placeAreas[TS3PlaceCount]={0,1,2,3,1,0,4,5,6,8,7};
static int targetArea=-1,originArea=-1,rememberedArea=-1;
static bool areaReady; // set by Install only after the native code and strings verify
static void* MapArea(int index){
 if(!areaReady||index<0||index>=int(sizeof(areaNames)/sizeof(areaNames[0])))return 0;void* registry=*(void**)A(0xb2ac90);
 return registry?((void*(*)(void*,const char*,const char*))A(0x4fd320))(registry,(const char*)A(0x75f6f0),areaNames[index]):0;
}
// Most recently entered area of the performance manager (same object as the stream manager).
static int CurrentArea(){
 if(!areaReady)return -1;BYTE* manager=*(BYTE**)A(0xb2ef20);if(!manager||!At<BYTE>(manager,0x40))return -1;
 BYTE* recent=At<BYTE*>(manager,0x38);void* area=recent?At<void*>(recent,0x50):0;if(!area)return -1;
 for(int i=0;i<int(sizeof(areaNames)/sizeof(areaNames[0]));++i)if(MapArea(i)==area)return i;
 return -1;
}
static void EnterArea(int index){
 void* area=MapArea(index);
 if(area)((void(*)(void*,const char*,void*,unsigned __int64))A(0x49e160))(*(void**)A(0xb29a70),(const char*)A(0x7fd7c8),area,travelHandle);
}
static bool SlotMatches(const Context& c,const PositionSlot& p){return p.used&&p.save==c.saveData&&p.slot==c.slot&&p.manager==c.worldManager;}
static bool Active();
static unsigned SlotMask(const Context& c,bool available){unsigned mask=0;for(unsigned i=0;i<6;++i)if(SlotMatches(c,positions[i])&&(!available||(hookReady&&!Active()&&positions[i].world==c.world)))mask|=1u<<i;return mask;}
static const char fadeReason[]="ToyBoxCheatTeleport";
static bool Active(){return phase!=Idle;}
// Only the WesternTown hub scene: other Toy Box worlds and variants use other layouts.
static bool PlacesWorld(const Context& c){
 if(!c.toyBox)return false;const char* name=((const char*(*)())A(0x5bb6a0))();
 return name&&lstrcmpiA(name,"WesternTown")==0;
}
static bool PlacesAvailable(const Context& c){return hookReady&&!Active()&&PlacesWorld(c);}
static bool LoadingStateReady(bool transition,bool streaming,bool suspended,int reset,int pending){
 return !transition&&(!streaming||(!suspended&&reset==0&&pending==0));
}
// Live loading check for availability (Character, Zurg). StreamReady() keeps the stream state
// recorded when a teleport started; outside a teleport that is stale, and a non-streaming level
// (e.g. Haunted House or Spaceport interiors) after a hub teleport read as "never ready".
static bool AreaReady(){
 if(!*(void**)A(0xb29a70))return false;
 void* stream=*(void**)A(0xb2ef20);bool streaming=stream&&At<BYTE>(stream,0x40)!=0;
 return LoadingStateReady(((bool(*)())A(0x477410))(),streaming,streaming&&At<BYTE>(stream,0xb8)!=0,streaming?At<int>(stream,0xbc):0,streaming?At<int>(stream,0xb4):0);
}
static bool RememberedFor(const Context& c){return hasRemembered&&c.slot==rememberedSlot&&c.saveData==rememberedSave&&c.worldManager==rememberedWorldManager;}
static bool ReturnAvailable(const Context& c){return hookReady&&RememberedFor(c)&&c.world==rememberedWorld&&!Active();}
static bool Settled(){return ++frames>=2;}
static bool Loaded(int pending,int reset,DWORD elapsed){
 if(pending>0||reset!=0){stable=0;return false;}
 return ++stable>=3&&elapsed>=100;
}
static void Observe(const Context& c,bool valid){
 if(valid)for(unsigned i=0;i<6;++i)if(positions[i].used&&(c.slot!=positions[i].slot||c.saveData!=positions[i].save))positions[i].used=false;
 if(valid&&hasRemembered&&(c.slot!=rememberedSlot||c.saveData!=rememberedSave))hasRemembered=false;
}
static void Remember(const Context& c,unsigned index=0){
 if(index>=6){Message(TS3CheatInvalid,"Invalid teleport slot.");return;}
 remembered=At<Vec>(c.actor,0x80);rememberedSlot=c.slot;rememberedWorld=c.world;
 rememberedSave=c.saveData;rememberedWorldManager=c.worldManager;hasRemembered=Pose(remembered);
 rememberedArea=CurrentArea();positions[index]={remembered,c.saveData,c.worldManager,c.slot,c.world,hasRemembered,rememberedArea};
 Message(hasRemembered?TS3CheatOk:TS3CheatNoPosition,hasRemembered?"Position remembered for this world.":"Position is unavailable.");
}
static bool SameSession(){
 BYTE* save=*(BYTE**)A(0xb29a60);void* world=*(void**)A(0xb2c760);
 return save&&world==travelContext.worldManager&&*(int*)(save+0x64)==travelContext.slot&&
  *(void**)(save+0x1a0)==travelContext.saveData&&At<int>(world,0x3080)==travelContext.world;
}
static bool ActorValid(){return SameSession()&&Player()==travelActor&&Resolve(travelHandle)==travelActor;}
static void ResetCamera(){
 // Same targeted ResetCamera event as native actor warp RVA55f3d0..55f3e9.
 // Its CTetherWTVC handler validates the actor handle and resets follow filters.
 ((void(*)(void*,const char*,void*,unsigned __int64))A(0x49e160))(
  *(void**)A(0xb29a70),(const char*)A(0x7b0960),0,travelHandle);
}
static void* Fade(unsigned player){return ((void*(*)(unsigned))A(0x09f7a0))(player);}
static void FadeOut(){
 fadeMask=0;void* players=*(void**)A(0xb2ef50);
 for(unsigned p=0;p<2;++p){
  if(p&&(!players||!At<void*>(players,0x1a0+p*0x58)))continue;
  void* f=Fade(p);if(f){fadeMask|=1u<<p;((void(*)(void*,const char*,unsigned))A(0x46b9b0))(f,fadeReason,0);}
 }
}
static void FadeIn(){for(unsigned p=0;p<2;++p)if(fadeMask&(1u<<p)){void* f=Fade(p);if(f)((void(*)(void*,const char*,unsigned))A(0x471700))(f,fadeReason,0);}fadeMask=0;}
static bool Black(){if(!fadeMask)return false;for(unsigned p=0;p<2;++p)if(fadeMask&(1u<<p)){void* f=Fade(p);if(!f||!((bool(*)(void*))A(0x46f3b0))(f))return false;}return true;}
static bool StreamReady(){
 if(!*(void**)A(0xb29a70)||((bool(*)())A(0x477410))())return false;
 if(!usesStreaming)return true; // Story maps without dynamic streaming are already resident.
 return streamManager&&streamManager==*(void**)A(0xb2ef20)&&At<BYTE>(streamManager,0x40)&&
  LoadingStateReady(false,true,At<BYTE>(streamManager,0xb8)!=0,At<int>(streamManager,0xbc),At<int>(streamManager,0xb4));
}
static void Finish(bool success,const char* text){
 // Only remove our named fade request; never clear another subsystem's fade.
 FadeIn();
 if(!success&&ownsFlight&&moved&&!landingReady){
  ownsFlight=false;text="Teleport stopped. Flight stays on until you turn it off.";
 }
 if(ownsFlight){
  if(!wasFlying||!success){travelHold=true;Stop(success?"[Teleport] Area loaded; player released":"[Teleport] Cancelled; player released",success?Diagnostics::Debug:Diagnostics::Warning);travelHold=false;}
 }
 phase=Idle;ownsFlight=false;moved=false;SetTravelInput(false);
 Message(success?TS3CheatOk:TS3CheatFault,text);
 if(!InterlockedCompareExchange(&snapshotBusy,1,0)){
  published.teleportPending=0;published.result=result;lstrcpynA(published.status,status,128);
  InterlockedExchange(&snapshotBusy,0);
 }
}
static void Recover(){
 // Roll back under the same black screen and run the same stream/wait sequence.
 // If even recovery cannot finish, leave temporary flight on instead of enabling
 // gravity above missing scenery. This path is only a native-loading timeout.
 if(!recovering&&ActorValid()&&StreamReady()){
  position=origin;Place(travelActor);ResetCamera();if(originArea>=0&&targetArea>=0&&targetArea!=originArea)EnterArea(originArea);recovering=true;phase=Settling;frames=stable=0;phaseTick=GetTickCount();
  Message(TS3CheatFault,"Destination could not load. Returning to the starting position...");return;
 }
 ownsFlight=false;Finish(false,"Loading did not finish. Flight stays on so you can move safely.");
}
static void Request(const Context& c){
 if(Active()){Message(TS3CheatBusy,"Teleport is already in progress.");return;}
 placeTravel=false;
 if(!RememberedFor(c)){Message(TS3CheatNoPosition,"Remember a position first.");return;}
 if(c.world!=rememberedWorld){Message(TS3CheatNoPosition,"Return to the saved world before teleporting.");return;}
 if(!hookReady){Message(TS3CheatFault,"Region travel is unavailable for this game build.");return;}
 Copy(&travelContext,&c,sizeof(c));target=remembered;origin=At<Vec>(c.actor,0x80);targetArea=rememberedArea;originArea=CurrentArea();
 travelActor=c.actor;travelHandle=c.handle;started=GetTickCount();phase=Requested;
 ownsFlight=moved=recovering=landingReady=false;fadeMask=0;frames=stable=0;
 SetTravelInput(true);
 Message(TS3CheatOk,"Travelling to your remembered position...");
}
// Streaming needs a physics hold, not the user-facing flying animation.
static void BeginHold(){
 wasFlying=enabled;if(!enabled){travelHold=true;Start(travelActor,false);travelHold=false;}
 ownsFlight=enabled&&Same(travelActor);
}
static void Step(){
 if(!Active())return;
 SetTravelInput(true); // Independent from the renderer's open/close lease.
 if(!ActorValid()){Finish(false,"Teleport stopped because the active game changed.");return;}
 DWORD now=GetTickCount();
 if(phase==Requested){
  streamManager=*(void**)A(0xb2ef20);
  usesStreaming=streamManager&&At<BYTE>(streamManager,0x40)!=0;
  if(!StreamReady()){Finish(false,"Wait for the current loading transition to finish.");return;}
  BeginHold();
  if(!ownsFlight){Finish(false,"The active character cannot teleport right now.");return;}
  FadeOut();phase=FadingOut;phaseTick=now;return;
 }
 if(phase==FadingOut){
  if((DWORD)(now-phaseTick)>10000){Finish(false,"The screen transition did not finish. Teleport cancelled.");return;}
  if(!Black())return;
  if(!StreamReady()){Finish(false,"Another loading transition started. Teleport cancelled.");return;}
  moved=true;position=target;Place(travelActor);ResetCamera();
  // Tell the performance manager we arrived, as the area trigger would when walking in.
  if(targetArea>=0&&targetArea!=originArea)EnterArea(targetArea);
  frames=0;phase=Settling;phaseTick=now;return;
 }
 if(phase==Settling){
  if(!Settled())return;
  if(!StreamReady()){
   if((DWORD)(now-phaseTick)>10000)Recover();
   return;
  }
  if(!usesStreaming){landingReady=true;Finish(true,placeTravel?"Arrived at the selected place.":"Returned to your remembered position.");return;}
  // Native BuildingLotSelector does this after two game frames. This callback
  // runs AFTER the main-loop update, never inside an actor/component traversal.
  ((void(*)(void*,bool,bool))A(0x5fb830))(streamManager,true,true);
  phase=Loading;phaseTick=now;stable=0;return;
 }
 if(phase==Loading){
  // Preserve the engine's MaxLoadWeight and ShowMiniLoad policy. There is no
  // forced loading screen for a destination that is already fully streamed.
  if(streamManager!=*(void**)A(0xb2ef20)){Finish(false,"The streaming context changed.");return;}
  if(!StreamReady())stable=0;
  else if(Loaded(At<int>(streamManager,0xb4),At<int>(streamManager,0xbc),now-phaseTick)){
   landingReady=true;
   Finish(!recovering,recovering?"Destination could not load. Returned to the starting position.":placeTravel?"Arrived at the selected place.":"Returned to your remembered position.");return;
  }
  if((DWORD)(now-phaseTick)>60000){Recover();return;}
 }
}
static void ReturnSlot(const Context& c,unsigned index){
 if(index>=6||!SlotMatches(c,positions[index])){Message(TS3CheatNoPosition,"Remember a position in this slot first.");return;}
 const PositionSlot& p=positions[index];remembered=p.point;rememberedSlot=p.slot;rememberedWorld=p.world;rememberedSave=p.save;rememberedWorldManager=p.manager;hasRemembered=true;rememberedArea=p.area;Request(c);
}
// Same fade/stream/land path as a saved point; the place target never replaces saved slots.
static void GoPlace(const Context& c,unsigned index){
 if(index>=TS3PlaceCount){Message(TS3CheatInvalid,"Invalid place.");return;}
 if(Active()){Message(TS3CheatBusy,"Teleport is already in progress.");return;}
 if(!hookReady){Message(TS3CheatFault,"Region travel is unavailable for this game build.");return;}
 if(!PlacesWorld(c)){Message(TS3CheatNoPosition,"Places are available in the main Toy Box world.");return;}
 Copy(&travelContext,&c,sizeof(c));target=places[index];origin=At<Vec>(c.actor,0x80);targetArea=placeAreas[index];originArea=CurrentArea();
 travelActor=c.actor;travelHandle=c.handle;started=GetTickCount();phase=Requested;placeTravel=true;
 ownsFlight=moved=recovering=landingReady=false;fadeMask=0;frames=stable=0;
 SetTravelInput(true);
 Message(TS3CheatOk,"Travelling to the selected place...");
}
static void ClearSlot(unsigned index){
 if(index>=6){Message(TS3CheatInvalid,"Invalid teleport slot.");return;}
 positions[index].used=false;Message(TS3CheatOk,"Teleport slot cleared.");
}
static void Frame(void* self){
 originalFrame(self);
 ExtrasFrame();
 // A handful of identity reads only while a point exists; catch slot reloads
 // even if the overlay was hidden, without scanning actors or serializing data.
 if(hasRemembered){__try{
  BYTE* save=*(BYTE**)A(0xb29a60);
  if(save){int slot=*(int*)(save+0x64);void* data=*(void**)(save+0x1a0);
   if(data&&slot>=0&&slot<16&&(slot!=rememberedSlot||data!=rememberedSave))hasRemembered=false;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){hasRemembered=false;}}
 if(!Active())return;
 __try{Step();}__except(EXCEPTION_EXECUTE_HANDLER){
  __try{if(moved)Recover();else Finish(false,"Teleport stopped after the game state changed.");}__except(EXCEPTION_EXECUTE_HANDLER){phase=Idle;SetTravelInput(false);}
 }
}
static bool Prefix(ULONG_PTR rva,const BYTE* bytes,unsigned count){for(unsigned i=0;i<count;++i)if(*(BYTE*)A(rva+i)!=bytes[i])return false;return true;}
static void Install(){
 // Area entry is optional: travel still works without it on an unverified build.
 const BYTE lookupBytes[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83};
 const BYTE setAreaBytes[]={0x44,0x88,0x4c,0x24,0x20,0x53,0x57,0x41,0x57,0x48,0x83,0xec,0x30};
 const BYTE enteredBytes[]={0x48,0x85,0xd2,0x0f,0x84,0x48,0x02,0x00,0x00,0x41,0x54,0x41,0x56};
 __try{
  areaReady=Prefix(0x4fd320,lookupBytes,sizeof(lookupBytes))&&Prefix(0x5d6ae0,setAreaBytes,sizeof(setAreaBytes))&&
   Prefix(0x1dcfe0,enteredBytes,sizeof(enteredBytes))&&*(void**)A(0x7a9560+5*8)==(void*)A(0x1dcfe0)&&
   lstrcmpA((const char*)A(0x75f6f0),"MapArea")==0&&lstrcmpA((const char*)A(0x7fd7c8),"PlayerEnteredArea")==0;
 }__except(1){areaReady=false;}
 const BYTE frameBytes[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
 const BYTE streamBytes[]={0x40,0x55,0x57,0x41,0x57,0x48,0x83,0xec,0x30,0x80,0x79,0x40,0x00};
 const BYTE eventBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x48,0x83,0xec,0x20};
 const BYTE cameraBytes[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
 const BYTE fadeGetterBytes[]={0x48,0x89,0x5c,0x24,0x8,0x57,0x48,0x83,0xec,0x70,0x8b,0xd9,0xb8,0x4};
 const BYTE fadeOutBytes[]={0x44,0x89,0x41,0x68,0x48,0x83,0xc1,0x28,0xe9};
 const BYTE fadeInBytes[]={0x44,0x89,0x41,0x68,0x48,0x83,0xc1,0x28,0xe9};
 const BYTE fadeBlackBytes[]={0x83,0x39,0x0,0x75,0x24,0xf3,0xf,0x10,0x49,0x4,0xf3,0xf};
 __try{
  void** cell=(void**)A(0x7b2e58);
  if(*cell!=(void*)A(0x21ed70)||!Prefix(0x21ed70,frameBytes,sizeof(frameBytes))||!Prefix(0x5fb830,streamBytes,sizeof(streamBytes))||
    !Prefix(0x49e160,eventBytes,sizeof(eventBytes))||!Prefix(0x20a340,cameraBytes,sizeof(cameraBytes))||
    !Prefix(0x9f7a0,fadeGetterBytes,sizeof(fadeGetterBytes))||!Prefix(0x46b9b0,fadeOutBytes,sizeof(fadeOutBytes))||!Prefix(0x471700,fadeInBytes,sizeof(fadeInBytes))||!Prefix(0x46f3b0,fadeBlackBytes,sizeof(fadeBlackBytes)))return;
  DWORD old;if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&old))return;
  originalFrame=(void(*)(void*))*cell;
  hookReady=InterlockedCompareExchangePointer(cell,(void*)Frame,(void*)A(0x21ed70))==(void*)A(0x21ed70);
  DWORD unused;VirtualProtect(cell,sizeof(void*),old,&unused);
 }__except(EXCEPTION_EXECUTE_HANDLER){hookReady=false;}
}
}
