// Offline tests for the dinput8 backend. Compiles the real backend with the game's functions
// replaced by stubs. Never opens or touches the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xmmintrin.h>
#include "ReleaseStrings.h"
static float Sqrt(float n){return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(n)));}
#include "../backend/FlightInput.h"
#pragma function(memset)
extern "C" void* __cdecl memset(void* p,int v,SIZE_T n){for(SIZE_T i=0;i<n;++i)((volatile BYTE*)p)[i]=(BYTE)v;return p;}
extern "C" int _fltused=0;
struct Vec{float x,y,z;};
static BYTE* module;
static bool enabled;
static float speed=15;
static Vec position;
static ULONG_PTR A(ULONG_PTR p){return (ULONG_PTR)module+p;}
template<class T>static T& At(void* p,SIZE_T o){return *(T*)((BYTE*)p+o);}
static void* Player(){return 0;}
static void* resolvedTestActor;
static bool Prefix(const char* a,const char* b){while(*b)if(*a++!=*b++)return false;return true;}
static bool Contains(const char* a,const char* b){for(;*a;++a)if(Prefix(a,b))return true;return !*b;}

static void* Resolve(unsigned __int64){return resolvedTestActor;}
static void Log(const char*,...){}
static bool requestedFlightPose;static unsigned flightStarts;
static void Start(void*,bool animate=true,bool=false){enabled=true;requestedFlightPose=animate;++flightStarts;}
static void StartFlight(void* a){Start(a);}
static void* Mount(void*){return 0;}
static void Stop(const char*){enabled=false;}
static void Place(void*){}
static bool Same(void*){return true;}
static void Commit(void*){}
static Vec Direction(void*){return {0,0,1};}
typedef void*(*RewardPropertyFn)(void*,int,const char*);
typedef const char*(*RewardValueFn)(void*,int);
static bool RewardWorldIsToyBox(void*,RewardPropertyFn,RewardValueFn){return true;}
namespace MenuInputGate{static bool external;static void SetExternalMenu(bool v){external=v;}static void InitializeExternal(){}}
#include "../backend/ModMenuBridge.cpp"
static unsigned tests,failures;
static void CheckAt(bool v,unsigned line){++tests;if(!v){++failures;char b[96];DWORD n;int len=wsprintfA(b,"FAILED line %u (check %u)\r\n",line,tests);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),b,len,&n,0);}}
#define Check(...) CheckAt((__VA_ARGS__),__LINE__)
static BYTE soundDriverTest[0x620],soundBankTest[0x40];static void* soundBankTableTest[1];
static unsigned soundLoads;static bool soundFail,soundSamplesMissing;
static bool SoundLoadMock(void* driver,const char* name,bool persistent,bool aliases,bool wait){
 ++soundLoads;Check(driver==soundDriverTest&&!persistent&&aliases&&wait);Check(lstrcmpA(name,"st_hauntedbakery.fev")==0);
 if(soundFail)return false;
 lstrcpyA((char*)soundBankTest,name);At<void*>(soundBankTest,0x20)=(void*)1;At<void*>(soundBankTest,0x28)=soundSamplesMissing?0:(void*)2;
 soundBankTableTest[0]=soundBankTest;At<void*>(driver,0x4d8)=soundBankTableTest;At<unsigned>(driver,0x4e0)=1;return true;
}
static unsigned detachStep;static void* detachManager;static void* detachNode;
static void LodUnlinkMock(void* manager,void* node){Check(detachStep==0&&manager==detachManager&&node==detachNode);detachStep=1;}
static void LodDestroyMock(void* node,unsigned flags){Check(detachStep==1&&node==detachNode&&flags==1);detachStep=2;}
static void* mountCallback;static unsigned restoreCalls;
static void MountRestoreMock(void* callback,bool unloaded){Check(callback==mountCallback&&!unloaded&&detachStep==0);++restoreCalls;}
static unsigned carDisableCalls,carHideCalls;static void* clearActors[3];static void* clearBodies[3];
static void CarDisableMock(void* body,bool enabled){Check(carDisableCalls<3&&body==clearBodies[carDisableCalls]&&!enabled);++carDisableCalls;}
static void CarHideMock(void* actor,bool hidden){Check(carHideCalls<3&&actor==clearActors[carHideCalls]&&hidden&&carDisableCalls==carHideCalls+1&&At<unsigned>(actor,0x420)==2);++carHideCalls;}
static unsigned inheritedHideCalls;
static void InheritedHideMock(void* entity,bool hidden,bool inherited){Check(inherited);++inheritedHideCalls;unsigned& flags=At<unsigned>(entity,0xac);flags=hidden?flags|2:flags&~2u;}
static unsigned modelBuildStep;static bool modelBuildFail,modelBuildThrow;static void* assemblyActor;static void* assemblyModel;static void* assemblyParts;
static void DirtyPartsMock(void* p){Check(p==assemblyParts&&modelBuildStep==0);modelBuildStep=1;}
static void AssembleMock(void* m,bool force,bool async,bool notify){
 Check(m==assemblyModel&&force&&!async&&!notify&&modelBuildStep==1&&At<BYTE>(assemblyActor,0x189)==1&&!At<void*>(m,0x7e8));modelBuildStep=2;
 if(modelBuildThrow)RaiseException(0xe0000001,0,0,0);
 if(!modelBuildFail)At<BYTE>(m,0x770)|=2;
}
static bool groundReadCorrect,groundThrow,groundChange;static float groundDt;
static void GroundMock(void* controller,float dt){
 groundDt=dt;groundReadCorrect=At<float>(controller,0x19c)==20&&At<float>(controller,0x1a0)==18&&At<float>(controller,0x1a4)==12&&At<float>(controller,0x24c)==14&&At<float>(controller,0x16c)==16&&At<float>(controller,0x178)==24&&At<float>(controller,0x60)==-30&&At<float>(controller,0x70)==30;
 if(groundChange)At<float>(controller,0x70)=17;
 if(groundThrow)RaiseException(0xe0000001,0,0,0);
}
static bool aimReject;static int aimCalls;
static void AimMock(void* driver,bool enabled){++aimCalls;if(!aimReject)At<BYTE>(driver,0x72)=enabled?1:0;}
static unsigned placementFallbacks;static bool switchThrows;
static void* PositionFallback(void*,void* out){++placementFallbacks;return out;}
static void* RotationFallback(void* ref){++placementFallbacks;return ref;}
static void* CameraFallback(void* camera){++placementFallbacks;return camera;}
static void SwitchPlacementMock(void*,void* actor,int index,bool pod){
 using namespace CheatMenuBridge::ZurgTransform::InPlace;
 float value[4]={};Check(index==1&&!pod);Check(Position((BYTE*)actor+0x1d0,value)==value);
 Check(value[0]==100&&value[1]==20&&value[2]==-30&&value[3]==0);
 Check(((float*)Rotation((BYTE*)actor+0x1d0))[5]==1.25f);
 Check(Camera(actor)==&resetProxy);((void(*)(void*))resetTable[1])(&resetProxy);
 if(switchThrows)RaiseException(0xe0000002,0,0,0);
}
static float timerTestDelta;static void* timerTestContext;static unsigned timerCalls;
static int TimerTestNative(void*,float delta,void* context){timerTestDelta=delta;timerTestContext=context;++timerCalls;return 73;}
static int RaceTestNative(void*,float delta){timerTestDelta=delta;++timerCalls;return 91;}
#include "../../work/CompleteEdition/mod-menu/zurg-healthbar/tests.h"
extern "C" void TestEntry(){
 TestZurgHud();
 using namespace CheatMenuBridge;
 BYTE aiControl[0xc0]={},aiRoot[0x80]={},aiEvaluator[0x28]={},aiVm[0x80]={};void* aiOwner=(void*)0x1234;
 At<void*>(aiControl,0)=(void*)A(0x804818);At<void*>(aiControl,8)=aiOwner;At<void*>(aiControl,0x38)=aiRoot;
 At<void*>(aiRoot,0)=(void*)A(0x800fc8);At<void*>(aiRoot,0x20)=aiControl;At<void*>(aiRoot,0x68)=aiEvaluator;
 At<void*>(aiEvaluator,0)=(void*)A(0x8010e0);At<void*>(aiEvaluator,8)=aiControl;At<void*>(aiEvaluator,0x10)=aiRoot;At<void*>(aiEvaluator,0x20)=aiVm;
 Check(NpcMovement::RootVm(aiControl,aiOwner)==aiVm);Check(!NpcMovement::RootVm(0,aiOwner));Check(!NpcMovement::RootVm(aiControl,(void*)1));
 At<void*>(aiEvaluator,0x10)=0;Check(!NpcMovement::RootVm(aiControl,aiOwner));At<void*>(aiEvaluator,0x10)=aiRoot;
 At<void*>(aiEvaluator,0)=(void*)1;Check(!NpcMovement::RootVm(aiControl,aiOwner));At<void*>(aiEvaluator,0)=(void*)A(0x8010e0);
 At<void*>(aiEvaluator,0x20)=aiVm+16;Check(NpcMovement::RootVm(aiControl,aiOwner)==aiVm+16);
 BYTE movementValues[5*16]={};At<unsigned __int64>(movementValues,4*16+8)=1;
 At<unsigned __int64>(movementValues,2*16+8)=1;Check(!NpcMovement::ApplyState(movementValues));Check(At<unsigned __int64>(movementValues,4*16+8)==1);
 At<unsigned __int64>(movementValues,2*16+8)=0;At<int>(movementValues,4*16)=1;Check(!NpcMovement::ApplyState(movementValues));
 At<int>(movementValues,4*16)=0;At<unsigned __int64>(movementValues,4*16+8)=2;Check(!NpcMovement::ApplyState(movementValues));
 At<unsigned __int64>(movementValues,4*16+8)=1;Check(NpcMovement::ApplyState(movementValues));Check(At<unsigned __int64>(movementValues,4*16+8)==0);Check(NpcMovement::ApplyState(movementValues));
 Check(!NpcMovement::ApplyState(0));
 Check(NpcMovement::Override(true,243,2434,0xda95246fU));
 Check(!NpcMovement::Override(false,243,2434,0xda95246fU));
 Check(!NpcMovement::Override(true,242,2434,0xda95246fU));
 Check(!NpcMovement::Override(true,243,2433,0xda95246fU));
 Check(!NpcMovement::Override(true,243,2434,0xda95246eU));
 Check(!WorldNpcs::NpcAudio::EnsureWith(0,SoundLoadMock));Check(!WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock)&&soundLoads==0);
 At<BYTE>(soundDriverTest,0x601)=1;soundFail=true;Check(!WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock));
 soundFail=false;soundSamplesMissing=true;Check(!WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock));
 soundSamplesMissing=false;Check(WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock));unsigned loadedCount=soundLoads;
 Check(WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock)&&soundLoads==loadedCount);
 At<unsigned>(soundDriverTest,0x4e0)=0;Check(WorldNpcs::NpcAudio::EnsureWith(soundDriverTest,SoundLoadMock)&&soundLoads==loadedCount+1);
 Check(MovementWeapons::Pictomatic::UseDown(false,false,false,true));
 Check(!MovementWeapons::Pictomatic::UseDown(false,false,false,false));
 Check(MovementWeapons::Pictomatic::UseDown(true,false,false,false));
 Check(MovementWeapons::Pictomatic::UseDown(false,true,false,false));
 Check(MovementWeapons::Pictomatic::UseDown(false,false,true,false));
 Check(!MovementWeapons::Pictomatic::DiscVisible(8,0));
 Check(MovementWeapons::Pictomatic::DiscVisible(8,5));
 Check(MovementWeapons::Pictomatic::DiscVisible(8,1));
 Check(!MovementWeapons::Pictomatic::DiscVisible(3,5));
 Check(!MovementWeapons::Pictomatic::DiscVisible(8,-1));
 Check(!MovementWeapons::Pictomatic::DiscVisible(8,6));
 // Exercise real entry ordering: the native callback must receive the repaired
 // owner, while ordinary/other-session opens and valid split-screen owners stay intact.
 BYTE viewerEntry[0x2c8]={};
 MovementWeapons::Pictomatic::entryOriginal=+[](void* screen){At<int>(screen,0x40)=At<int>(screen,0x288);At<int>(screen,0x44)++;};
 At<int>(viewerEntry,0x288)=-1;
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,0,true);
 Check(At<int>(viewerEntry,0x40)==0&&At<int>(viewerEntry,0x44)==1);
 At<int>(viewerEntry,0x288)=-1;
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,1,true);
 Check(At<int>(viewerEntry,0x40)==1&&At<int>(viewerEntry,0x44)==2);
 At<int>(viewerEntry,0x288)=1;
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,0,true);
 Check(At<int>(viewerEntry,0x40)==1);
 At<int>(viewerEntry,0x288)=-1;
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,0,false);
 Check(At<int>(viewerEntry,0x40)==-1);
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,-1,true);
 Check(At<int>(viewerEntry,0x40)==-1);
 MovementWeapons::Pictomatic::EnterForPlayer(viewerEntry,2,true);
 Check(At<int>(viewerEntry,0x40)==-1&&At<int>(viewerEntry,0x44)==6);
 bool primaryHeld=false;
 Check(MovementWeapons::Pictomatic::Press(true,true,primaryHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,true,primaryHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,false,primaryHeld));
 Check(!MovementWeapons::Pictomatic::Press(false,true,primaryHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,true,primaryHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,false,primaryHeld));
 Check(MovementWeapons::Pictomatic::Press(true,true,primaryHeld));
 bool pictoHeld=false;
 BYTE residentLod[0x40]={};detachManager=(void*)0x100;detachNode=(void*)0x200;detachStep=0;
 Check(WorldNpcs::NpcAppearance::Detach(residentLod,0,0,0));Check(detachStep==0);
 At<void*>(residentLod,0x30)=detachNode;
 Check(!WorldNpcs::NpcAppearance::Detach(residentLod,0,LodUnlinkMock,LodDestroyMock));
 Check(At<void*>(residentLod,0x30)==detachNode&&detachStep==0);
 Check(WorldNpcs::NpcAppearance::Detach(residentLod,detachManager,LodUnlinkMock,LodDestroyMock));
 Check(At<void*>(residentLod,0x30)==0&&detachStep==2);
 Check(WorldNpcs::NpcAppearance::Detach(residentLod,detachManager,LodUnlinkMock,LodDestroyMock)&&detachStep==2);
 BYTE pictoPlayers[0x260]={};void* pictoActor=(void*)0x1234;
 Check(MovementWeapons::Pictomatic::PlayerIndex(0,pictoActor)==-1);
 Check(MovementWeapons::Pictomatic::PlayerIndex(pictoPlayers,pictoActor)==-1);
 At<int>(pictoPlayers,4)=2;At<void*>(pictoPlayers,0x1a0)=pictoActor;
 Check(MovementWeapons::Pictomatic::PlayerIndex(pictoPlayers,pictoActor)==0);
 At<void*>(pictoPlayers,0x1a0)=0;At<void*>(pictoPlayers,0x1f8)=pictoActor;
 Check(MovementWeapons::Pictomatic::PlayerIndex(pictoPlayers,pictoActor)==1);
 At<int>(pictoPlayers,4)=3;
 Check(MovementWeapons::Pictomatic::PlayerIndex(pictoPlayers,pictoActor)==-1);
 Check(!MovementWeapons::Pictomatic::Press(false,true,pictoHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,true,pictoHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,false,pictoHeld));
 Check(MovementWeapons::Pictomatic::Press(true,true,pictoHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,true,pictoHeld));
 Check(!MovementWeapons::Pictomatic::Press(true,false,pictoHeld));
 Check(MovementWeapons::Pictomatic::Press(true,true,pictoHeld));
 Check(!PlayerObjects::BallNeedsCarryFix(0));Check(PlayerObjects::BallNeedsCarryFix("lightHand"));Check(!PlayerObjects::BallNeedsCarryFix("mediumHand"));Check(!PlayerObjects::BallNeedsCarryFix("mediumBoth"));
 float ax,ay;FlightStick(0,0,ax,ay);Check(ax==0&&ay==0);
 FlightStick(7849,0,ax,ay);Check(ax==0&&ay==0);
 FlightStick(7850,0,ax,ay);Check(ax>0&&ax<0.001f&&ay==0);
 FlightStick(32767,0,ax,ay);Check(ax==1&&ay==0);
 FlightStick(-32768,0,ax,ay);Check(ax==-1&&ay==0);
 FlightStick(32767,32767,ax,ay);Check(ax>0.7f&&ax<0.71f&&ay==ax&&ax*ax+ay*ay<1.0001f);
 FlightStick(0,-32768,ax,ay);Check(ax==0&&ay==-1);
 Check(FlightVerticalButtons(0)==0);Check(FlightVerticalButtons(0x1000)==1);Check(FlightVerticalButtons(0x2000)==-1);Check(FlightVerticalButtons(0x3000)==0);Check(FlightVerticalButtons(0xc300)==0);
 // Menu A/B must be released before they become vertical flight input.
 Check(FlightAvailableButtons(0x1000,true)==0);
 Check(FlightAvailableButtons(0x2000,false)==0);
 Check(FlightAvailableButtons(0x2000,false)==0);
 Check(FlightAvailableButtons(0,false)==0);
 Check(FlightAvailableButtons(0x1040,false)==0x1040);
 Check(FlightAvailableButtons(0x2000,false)==0x2000);
 Check(FlightTrigger(0)==0);Check(FlightTrigger(30)==0);
 Check(FlightTrigger(31)>0&&FlightTrigger(31)<0.005f);Check(FlightTrigger(255)==1);

 // Static Story maps need no Toy Box streamer, but actual transitions block both.
 // A normal teleport must never request the flying animation; explicit flight survives.
 enabled=false;requestedFlightPose=true;flightStarts=0;
 RegionTeleport::BeginHold();Check(enabled&&RegionTeleport::ownsFlight&&!RegionTeleport::wasFlying);
 Check(!requestedFlightPose&&flightStarts==1);
 RegionTeleport::fadeMask=0;RegionTeleport::moved=false;RegionTeleport::Finish(true,"test");Check(!enabled);
 Start(0);unsigned startsBefore=flightStarts;
 RegionTeleport::BeginHold();Check(RegionTeleport::wasFlying&&RegionTeleport::ownsFlight);
 Check(requestedFlightPose&&flightStarts==startsBefore);
 RegionTeleport::Finish(true,"test");Check(enabled&&requestedFlightPose);
 Stop("test");
 Check(RegionTeleport::LoadingStateReady(false,false,false,0,0));
 Check(RegionTeleport::LoadingStateReady(false,false,true,1,7));
 Check(!RegionTeleport::LoadingStateReady(true,false,false,0,0));
 Check(RegionTeleport::LoadingStateReady(false,true,false,0,0));
 Check(!RegionTeleport::LoadingStateReady(false,true,true,0,0));
 Check(!RegionTeleport::LoadingStateReady(false,true,false,1,0));
 Check(!RegionTeleport::LoadingStateReady(false,true,false,0,1));
 Check(!RegionTeleport::LoadingStateReady(true,true,false,0,0));
 Check(GetCheatMenuApi(1)==0);const TS3CheatMenuApi* a=GetCheatMenuApi(TS3MenuApiVersion);Check(a&&a->version==TS3MenuApiVersion&&a->bytes==sizeof(TS3CheatMenuApi));
 TS3CheatCommand cmd={1,1,TS3CheatFlightToggle,0,0};
 Check(!a->Submit(&cmd,sizeof(cmd)));a->SetOpen(2);Check(probeRequested==1&&!Open()&&!MenuInputGate::external);probeRequested=0;a->SetOpen(1);Check(Open()&&MenuInputGate::external);
 Check(!a->Submit(0,sizeof(cmd)));Check(!a->Submit(&cmd,sizeof(cmd)-1));
 cmd.id=0;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.id=1;cmd.contextId=0;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.contextId=1;
 cmd.action=99;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.action=TS3CheatFlightSpeed;cmd.value=0;Check(!a->Submit(&cmd,sizeof(cmd)));
 cmd.value=201;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.value=30;Check(a->Submit(&cmd,sizeof(cmd)));Check(queueState==2&&queued.value==30);Check(!a->Submit(&cmd,sizeof(cmd)));queueState=0;
 cmd.action=TS3CheatAddCoins;cmd.amount=0;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.amount=0x80000000U;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.amount=100;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 a->SetOpen(0);Check(!Open()&&!MenuInputGate::external);Check(!a->Submit(&cmd,sizeof(cmd)));a->SetOpen(1);openTick=(LONG)(GetTickCount()-751);Check(!Open());
 TS3CheatSnapshot snap={};Check(!a->ReadSnapshot(0,sizeof(snap)));Check(!a->ReadSnapshot(&snap,sizeof(snap)-1));
 published.version=TS3MenuApiVersion;published.tick=GetTickCount();published.gameReady=1;published.currencyReady=1;published.savedPosition=1;
 Check(a->ReadSnapshot(&snap,sizeof(snap))&&snap.gameReady==1);published.tick=GetTickCount()-2000;published.teleportPending=published.returnAvailable=1;Check(a->ReadSnapshot(&snap,sizeof(snap))&&!snap.gameReady&&!snap.currencyReady&&!snap.savedPosition&&!snap.teleportPending&&!snap.returnAvailable);
 snapshotBusy=1;Check(!a->ReadSnapshot(&snap,sizeof(snap)));snapshotBusy=0;
 SetTravelInput(true);a->SetOpen(0);Check(MenuInputGate::external);SetTravelInput(false);Check(!MenuInputGate::external);
 a->SetOpen(1);SetTravelInput(true);SetTravelInput(false);Check(MenuInputGate::external);a->SetOpen(0);
 BYTE actor[256]={};Context c={};c.actor=actor;c.handle=123;c.slot=1;c.world=2;c.toyBox=true;contextId=5;
 At<Vec>(actor,0x80)={1,2,3};cmd.action=TS3CheatRememberPosition;cmd.amount=0;Execute(cmd,c);Check(hasRemembered&&remembered.x==1&&remembered.y==2&&remembered.z==3&&RegionTeleport::rememberedSlot==1&&RegionTeleport::rememberedWorld==2);
 RegionTeleport::hookReady=true;
 // Places enter the matching native MapArea; without verified native code no area state is read.
 {const char* expect[]={"WT_Town","WT_Farm","WT_Suburbs","WT_Hideout","WT_Farm","WT_Town","WT_Mod_HauntedHouse","WT_Mod_EnchantedGlen","WT_Mod_SpacePort","WT_Mod_StuntPark","WT_Mod_SkatePark"};for(unsigned i=0;i<TS3PlaceCount;++i)Check(lstrcmpA(RegionTeleport::areaNames[RegionTeleport::placeAreas[i]],expect[i])==0);}
 Check(!RegionTeleport::areaReady&&RegionTeleport::positions[0].area==-1&&RegionTeleport::CurrentArea()==-1&&!RegionTeleport::MapArea(0));
 Check(RegionTeleport::ReturnAvailable(c));
 Context story=c;story.toyBox=false;Check(RegionTeleport::ReturnAvailable(story));RegionTeleport::Remember(story);Check(result==TS3CheatOk&&hasRemembered);
 Context other=c;other.actor=(void*)12345;other.handle=777;RegionTeleport::Observe(other,true);Check(hasRemembered&&RegionTeleport::ReturnAvailable(other));
 other.world=4;RegionTeleport::Observe(other,true);Check(hasRemembered&&!RegionTeleport::ReturnAvailable(other));
 RegionTeleport::Observe(other,false);Check(hasRemembered);
 other=c;other.slot=2;RegionTeleport::Observe(other,true);Check(!hasRemembered);
 RegionTeleport::Remember(c);other=c;other.saveData=(void*)5678;RegionTeleport::Observe(other,true);Check(!hasRemembered);
 RegionTeleport::frames=0;Check(!RegionTeleport::Settled());Check(RegionTeleport::Settled());
 RegionTeleport::stable=0;Check(!RegionTeleport::Loaded(5,0,200));Check(!RegionTeleport::Loaded(0,0,200));Check(!RegionTeleport::Loaded(0,0,200));Check(RegionTeleport::Loaded(0,0,200));
 Check(!RegionTeleport::Loaded(1,0,500));Check(RegionTeleport::stable==0);Check(!RegionTeleport::Loaded(0,1,500));
 Check(!RegionTeleport::Loaded(0,0,10));Check(!RegionTeleport::Loaded(0,0,50));Check(!RegionTeleport::Loaded(0,0,99));Check(RegionTeleport::Loaded(0,0,100));
 hasRemembered=false;cmd.action=TS3CheatReturnPosition;Execute(cmd,c);Check(result==TS3CheatNoPosition);
 cmd.action=TS3CheatFlightSpeed;cmd.value=30;Execute(cmd,c);Check(speed==15&&result==TS3CheatInvalid);enabled=true;Execute(cmd,c);Check(speed==30&&result==TS3CheatOk);cmd.value=-1;Execute(cmd,c);Check(speed==30&&result==TS3CheatInvalid);enabled=false;
 cmd.action=TS3CheatFlightToggle;RegionTeleport::phase=RegionTeleport::Requested;Execute(cmd,c);Check(!enabled&&result==TS3CheatBusy);RegionTeleport::phase=RegionTeleport::Idle;Execute(cmd,c);Check(enabled);Execute(cmd,c);Check(!enabled);
 cmd.action=TS3CheatAddCoins;c.currency=false;Execute(cmd,c);Check(result==TS3CheatNoGame);
 c.currency=true;pendingKind=1;Execute(cmd,c);Check(result==TS3CheatBusy);pendingKind=-1;c.coins=0x7fffffff;Execute(cmd,c);Check(result==TS3CheatMaxBalance);
 
 a->SetOpen(1);cmd.id=900;cmd.contextId=1;cmd.action=TS3CheatSpawnObject;
 cmd.amount=TS3ObjectCount;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.amount=TS3ObjectCount-1;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;cmd.amount=0xffffffff;Check(!a->Submit(&cmd,sizeof(cmd)));
 cmd.amount=0;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 cmd.action=TS3CheatGodToggle;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 Check(PlayerObjects::CanRefresh(false,0));Check(PlayerObjects::CanRefresh(true,0.2f));
 Check(!PlayerObjects::CanRefresh(true,0));Check(!PlayerObjects::CanRefresh(true,-1));Check(!PlayerObjects::CanRefresh(true,2));
 PlayerObjects::ready=false;PlayerObjects::Request(c,0);Check(result==TS3CheatFault&&!PlayerObjects::spawnPending);
 PlayerObjects::ready=true;Context notToy=c;notToy.toyBox=false;PlayerObjects::lastSpawn=GetTickCount();PlayerObjects::Request(notToy,0);Check(result==TS3CheatOk&&PlayerObjects::spawnPending);PlayerObjects::Request(notToy,0);Check(result==TS3CheatBusy);PlayerObjects::spawnPending=false;
 PlayerObjects::Request(c,TS3ObjectCount+1);Check(result==TS3CheatInvalid&&!PlayerObjects::spawnPending);
 PlayerObjects::god=true;PlayerObjects::godSlot=0;PlayerObjects::Protect(c,true);Check(!PlayerObjects::god);
 PlayerObjects::god=true;PlayerObjects::godSlot=c.slot;PlayerObjects::godSave=(void*)42;PlayerObjects::Protect(c,true);Check(!PlayerObjects::god);
 PlayerObjects::god=true;PlayerObjects::Protect(c,false);Check(PlayerObjects::god);PlayerObjects::Toggle(c);Check(!PlayerObjects::god);
 PlayerObjects::spawnPending=true;cmd.action=TS3CheatFlightToggle;Execute(cmd,c);Check(result==TS3CheatBusy);PlayerObjects::spawnPending=false;
 
 cmd.action=TS3CheatClearObjects;cmd.amount=0;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 PlayerObjects::Request(c,TS3ObjectCount);Check(PlayerObjects::spawnPending&&PlayerObjects::spawnIndex==TS3ObjectCount);PlayerObjects::spawnPending=false;
 PlayerObjects::trackedContext=false;PlayerObjects::Observe(c,true);PlayerObjects::spawned[0]=123;PlayerObjects::spawnedActors[0]=actor;
 PlayerObjects::Observe(c,true);Check(PlayerObjects::spawned[0]==123);Context changedWorld=c;changedWorld.world++;
 PlayerObjects::Observe(changedWorld,false);Check(PlayerObjects::spawned[0]==123);
 PlayerObjects::Observe(changedWorld,true);Check(PlayerObjects::spawned[0]==0&&PlayerObjects::spawnedActors[0]==0);
 statusTick=100;Check(StatusVisible(4099));Check(!StatusVisible(4100));statusTick=0xfffffff0;Check(StatusVisible(10));Check(!StatusVisible(5000));
 cmd.action=TS3CheatAddCoins;notToy.currency=false;Execute(cmd,notToy);Check(result==TS3CheatNoGame);
 
 BYTE prop[512]={},otherProp[512]={};PlayerObjects::spawned[0]=456;PlayerObjects::spawnedActors[0]=prop;
 At<unsigned __int64>(prop,0x38)=456;At<void*>(prop,0x168)=prop;
 Check(PlayerObjects::OwnedSpawn(0,prop,actor));Check(!PlayerObjects::OwnedSpawn(0,prop,prop));Check(!PlayerObjects::OwnedSpawn(0,otherProp,actor));
 Check(!PlayerObjects::OwnedSpawn(0,0,actor));Check(!PlayerObjects::OwnedSpawn(8,prop,actor));At<unsigned __int64>(prop,0x38)=457;Check(!PlayerObjects::OwnedSpawn(0,prop,actor));
 At<unsigned __int64>(prop,0x38)=456;At<void*>(prop,0x168)=otherProp;Check(!PlayerObjects::OwnedSpawn(0,prop,actor));
 
 At<void*>(prop,0x168)=prop;resolvedTestActor=prop;
 Check(PlayerObjects::Count()==1);resolvedTestActor=0;
 Check(PlayerObjects::Count()==0&&PlayerObjects::spawned[0]==0);Check(PlayerObjects::Count()==0);
 // More than eight props retain ownership and carry repair state across growth.
 PlayerObjects::spawned[7]=456;PlayerObjects::spawnedActors[7]=prop;PlayerObjects::carryFixFrames[7]=12;
 Check(PlayerObjects::Grow()&&PlayerObjects::capacity==16);
 Check(PlayerObjects::spawned[7]==456&&PlayerObjects::spawnedActors[7]==prop&&PlayerObjects::carryFixFrames[7]==12);
 Check(!PlayerObjects::spawned[8]&&!PlayerObjects::spawnedActors[8]&&!PlayerObjects::carryFixFrames[8]);
 PlayerObjects::spawned[7]=0;PlayerObjects::spawnedActors[7]=0;PlayerObjects::carryFixFrames[7]=0;
 PlayerObjects::spawned[15]=456;PlayerObjects::spawnedActors[15]=prop;resolvedTestActor=prop;
 Check(PlayerObjects::OwnedSpawn(15,prop,actor)&&PlayerObjects::Count()==1);
 Check(PlayerObjects::Grow()&&PlayerObjects::capacity==32&&PlayerObjects::OwnedSpawn(15,prop,actor));
 PlayerObjects::Observe(c,true);Check(PlayerObjects::Count()==0&&!PlayerObjects::spawned[15]);resolvedTestActor=0;
 cmd.action=TS3CheatEquipWeapon;cmd.amount=9;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.amount=8;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;cmd.amount=0;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 MovementWeapons::ready=false;MovementWeapons::Toggle(c,true);Check(result==TS3CheatFault&&!MovementWeapons::fast);
 MovementWeapons::Equip(notToy,0);Check(result==TS3CheatNoGame);
 BYTE ground[768]={};At<float>(ground,0x16c)=8;At<float>(ground,0x178)=12;At<float>(ground,0x60)=-15;At<float>(ground,0x70)=15;
 At<float>(ground,0x19c)=10;At<float>(ground,0x1a0)=9;At<float>(ground,0x1a4)=6;At<float>(ground,0x24c)=7;
 MovementWeapons::groundOriginal=GroundMock;MovementWeapons::BoostedGround(ground,0.02f);
 Check(groundReadCorrect&&groundDt==0.02f);Check(At<float>(ground,0x16c)==8&&At<float>(ground,0x178)==12&&At<float>(ground,0x60)==-15&&At<float>(ground,0x70)==15);
 MovementWeapons::BoostedGround(ground,0.02f);Check(groundReadCorrect&&At<float>(ground,0x16c)==8);
 groundThrow=true;__try{MovementWeapons::BoostedGround(ground,0.02f);}__except(EXCEPTION_EXECUTE_HANDLER){}Check(At<float>(ground,0x16c)==8&&At<float>(ground,0x70)==15);groundThrow=false;Check(At<float>(ground,0x19c)==10&&At<float>(ground,0x1a0)==9&&At<float>(ground,0x1a4)==6&&At<float>(ground,0x24c)==7);
 groundChange=true;MovementWeapons::BoostedGround(ground,0.02f);Check(At<float>(ground,0x16c)==8&&At<float>(ground,0x70)==17);groundChange=false;
 MovementWeapons::fast=false;MovementWeapons::Ground(ground,0.02f);Check(!groundReadCorrect&&At<float>(ground,0x16c)==8);
 MovementWeapons::Equip(c,0);Check(result==TS3CheatInvalid);
 const unsigned expectedIds[]={0,1,2,3,4,3,4,5,6};
 for(unsigned i=1;i<9;++i){Check(MovementWeapons::NativeSelection(i)==expectedIds[i]);Check(lstrcmpA(MovementWeapons::SelectionEvent(i),i<=4?"SelectNewGun":"SelectNewTool")==0);}
 a->SetOpen(1);cmd.action=TS3CheatSpawnNpc;cmd.amount=8;Check(!a->Submit(&cmd,sizeof(cmd)));cmd.amount=0;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 cmd.amount=5;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;cmd.action=TS3CheatClearNpcs;Check(a->Submit(&cmd,sizeof(cmd)));queueState=0;
 WorldNpcs::tracked=false;WorldNpcs::Observe(c,true);auto npcRecord=WorldNpcs::Allocate();Check(npcRecord!=0);npcRecord->handle=900;npcRecord->actor=prop;At<unsigned __int64>(prop,0x38)=900;At<void*>(prop,0x168)=prop;
 Check(WorldNpcs::Owned(npcRecord,prop,actor));Check(!WorldNpcs::Owned(npcRecord,prop,prop));Check(!WorldNpcs::Owned(0,prop,actor));Check(!WorldNpcs::Owned(npcRecord,otherProp,actor));Check(!WorldNpcs::Owned(npcRecord,0,actor));
 At<unsigned __int64>(prop,0x38)=901;Check(!WorldNpcs::Owned(npcRecord,prop,actor));At<unsigned __int64>(prop,0x38)=900;
 At<void*>(prop,0x168)=otherProp;Check(!WorldNpcs::Owned(npcRecord,prop,actor));At<void*>(prop,0x168)=prop;
 WorldNpcs::Observe(changedWorld,false);Check(npcRecord->handle==900);WorldNpcs::Observe(c,true);Check(npcRecord->handle==900);
 for(unsigned i=0;i<40;++i)Check(WorldNpcs::Allocate()!=0);
 unsigned records=0;for(auto r=WorldNpcs::records;r;r=r->next)++records;Check(records==41);
 WorldNpcs::Observe(changedWorld,true);Check(WorldNpcs::records==0);
 for(unsigned group=0;group<8;++group){unsigned counts[]={9,17,1,5,1,3,2,18};for(unsigned v=0;v<counts[group];++v){unsigned selection=group|(v<<8);Check(TS3ValidNpcSelection(selection));Check(WorldNpcs::Template(selection)<53);}Check(!TS3ValidNpcSelection(group|(counts[group]<<8)));}
 for(unsigned i=0;i<50;++i){unsigned pick=WorldNpcs::Template(1);Check(pick>=8&&pick<24);}
 Check(!TS3ValidNpcSelection(0xffffffffU));Check(!TS3ValidNpcSelection(0x10000));
 unsigned last=99;for(unsigned i=0;i<40;++i){unsigned pick=WorldNpcs::Template(0);Check(pick<8&&pick!=last);last=pick;}
 Check(WorldNpcs::Template(3|(2<<8))==26);Check(WorldNpcs::Template(5|(2<<8))==32);
 resolvedTestActor=prop;Check(WorldNpcs::Live(prop));Check(!WorldNpcs::Live(0));
 resolvedTestActor=0;Check(!WorldNpcs::Live(prop));
 resolvedTestActor=prop;At<void*>(prop,0x168)=otherProp;Check(!WorldNpcs::Live(prop));At<void*>(prop,0x168)=prop;
 Check(WorldNpcs::NpcAppearance::NeedsFallback("","characters/Hamm/Hamm"));
 Check(!WorldNpcs::NpcAppearance::NeedsFallback("low","high"));
 Check(!WorldNpcs::NpcAppearance::NeedsFallback("",""));
 Check(!WorldNpcs::NpcAppearance::NeedsFallback(0,"high"));
 Check(!WorldNpcs::NpcAppearance::NeedsFallback("",0));
 char npcParameters[4096];
 for(unsigned i=0;i<53;++i){
  Check(WorldNpcs::Parameters(WorldNpcs::npcTemplates[i],"TS3ModMenuNPC_test_1",npcParameters,sizeof(npcParameters)));
  Check(Contains(npcParameters,"propUniqueName=TS3ModMenuNPC_test_1;"));
  Check(Contains(npcParameters,"propModel=False;")&&Contains(npcParameters,"propLocation=False;")&&Contains(npcParameters,"propDeath=False;"));
  Check(Prefix(npcParameters,WorldNpcs::npcTemplates[i]));
  Check(!Contains(npcParameters,"AddComponent_MissionGiver=")&&!Contains(npcParameters,"AddComponent_MeridianActor=")&&!Contains(npcParameters,"_Uuid=")&&!Contains(npcParameters,"cpName="));
 }
 char tiny[8]="guard";Check(!WorldNpcs::Parameters("name=Blockhead;","TS3ModMenuNPC_test_1",tiny,sizeof(tiny)));Check(strcmp(tiny,"guard")==0);
 Check(WorldNpcs::SameWorld(c,c));Check(!WorldNpcs::SameWorld(c,changedWorld));Context npcOther=c;npcOther.saveData=(void*)42;Check(!WorldNpcs::SameWorld(c,npcOther));npcOther=c;npcOther.slot++;Check(!WorldNpcs::SameWorld(c,npcOther));
 PlayerObjects::ready=true;RegionTeleport::hookReady=true;WorldNpcs::lastSpawn=0;
 WorldNpcs::Request(c,8);Check(!WorldNpcs::pending&&result==TS3CheatInvalid);
 WorldNpcs::Request(c,0);Check(WorldNpcs::pending&&WorldNpcs::requested==0);WorldNpcs::Frame();Check(!WorldNpcs::pending&&result==TS3CheatStale);
 WorldNpcs::lastSpawn=GetTickCount();WorldNpcs::Request(c,0);Check(WorldNpcs::pending&&result==TS3CheatOk);WorldNpcs::pending=false;
 WorldNpcs::Request(c,0xffffffffU);Check(WorldNpcs::pending&&WorldNpcs::requested==0xffffffffU);WorldNpcs::pending=false;
 PlayerObjects::ready=false;WorldNpcs::Request(c,0);Check(!WorldNpcs::pending&&result==TS3CheatFault);
 // A queued resource task must not be submitted twice or lose its busy state.
 WorldNpcs::pending=WorldNpcs::loading=true;WorldNpcs::Frame();Check(WorldNpcs::pending&&WorldNpcs::loading);
 PlayerObjects::ready=true;WorldNpcs::Request(c,1);Check(result==TS3CheatBusy&&WorldNpcs::pending);
 // Cancelled/failed completion releases the queue, allowing future requests.
 WorldNpcs::SpawnComplete(0,0);Check(!WorldNpcs::pending&&!WorldNpcs::loading);Check(result==TS3CheatStale);
 WorldNpcs::lastSpawn=0;WorldNpcs::Request(c,1);Check(WorldNpcs::pending&&WorldNpcs::requested==1);WorldNpcs::pending=false;

 BYTE players[0x220]={};void* one=(void*)1;void* two=(void*)2;
 Check(ZurgTransform::PlayerIndex(0,one)==-1);Check(ZurgTransform::PlayerIndex(players,0)==-1);
 At<void*>(players,0x1a0)=one;At<void*>(players,0x1f8)=two;
 Check(ZurgTransform::PlayerIndex(players,one)==0);Check(ZurgTransform::PlayerIndex(players,two)==1);Check(ZurgTransform::PlayerIndex(players,(void*)3)==-1);
 Context z=c;Check(ZurgTransform::SameWorld(c,z));z.world++;Check(!ZurgTransform::SameWorld(c,z));z=c;z.slot++;Check(!ZurgTransform::SameWorld(c,z));z=c;z.saveData=(void*)999;Check(!ZurgTransform::SameWorld(c,z));
 ZurgTransform::state.original[0]=1;ZurgTransform::state.zurg[1]=2;ZurgTransform::tracked=true;ZurgTransform::Reset();Check(!ZurgTransform::tracked&&!ZurgTransform::state.original[0]&&!ZurgTransform::state.zurg[1]);
 ZurgTransform::ready=false;ZurgTransform::Request(c);Check(!ZurgTransform::pending&&result==TS3CheatInvalid);
 // Resolve already-existing nodes with no factory capture or heap scan.
 BYTE pod[0xc8]={},pod2[0xc8]={},head[8]={},group[0x40]={},otherHandler[0x28]={};
 At<void*>(head,0)=group;At<void*>(group,0x10)=pod+0x70;
 Check(!ZurgTransform::FindRegisteredMachine(0));Check(!ZurgTransform::FindRegisteredMachine(head));
 At<void*>(pod,0)=(void*)A(0x7979b0);At<void*>(pod,0x70)=(void*)A(0x797960);At<void*>(pod,0x90)=pod;
 Check(!ZurgTransform::FindRegisteredMachine(head));
 At<void*>(pod,0x28)=(void*)1;Check(ZurgTransform::FindRegisteredMachine(head)==pod);
 At<void*>(group,0x10)=otherHandler;At<void*>(otherHandler,0x10)=pod+0x70;
 Check(ZurgTransform::FindRegisteredMachine(head)==pod);
 At<void*>(pod,0x80)=pod+0x70;Check(!ZurgTransform::FindRegisteredMachine(head));At<void*>(pod,0x80)=0;
 At<void*>(pod2,0)=(void*)A(0x7979b0);At<void*>(pod2,0x28)=(void*)1;At<void*>(pod2,0x70)=(void*)A(0x797960);At<void*>(pod2,0x90)=pod2;
 At<void*>(pod,0x80)=pod2+0x70;Check(!ZurgTransform::FindRegisteredMachine(head));At<void*>(pod,0x80)=0;
 At<void*>(group,0x30)=group;Check(!ZurgTransform::FindRegisteredMachine(head));At<void*>(group,0x30)=0;
 auto shared=(ZurgTransform::SwitchState*)pod;shared->original[0]=123;shared->zurg[0]=456;
 ZurgTransform::Reset();Check(shared->original[0]==123&&shared->zurg[0]==456);
 At<void*>(group,0x10)=0;Check(!ZurgTransform::FindRegisteredMachine(head));
 Context zurgStory={};Check(ZurgTransform::StateFor(zurgStory)==&ZurgTransform::state);
 // Zurg uses live placement in both directions; pod calls remain untouched.
 {
  using namespace ZurgTransform::InPlace;
  BYTE actor[0x240]={};At<Vec>(actor,0x80)={100,20,-30};
  nativePosition=PositionFallback;nativeRotation=RotationFallback;nativeCamera=CameraFallback;
  float out[4]={};placementFallbacks=0;
  Check(Position(actor,out)==out);Check(Rotation(actor)==actor);Check(Camera(actor)==actor);Check(placementFallbacks==3);
  Capture(actor,1.25f);Check(ForReference(actor+0x1d0));Check(!ForReference(actor));
  Check(Position(actor,out)==out);Check(Rotation(actor)==actor);
  DWORD correctThread=thread;thread=correctThread+1;Check(!OnThread());Check(Rotation(actor+0x1d0)==actor+0x1d0);Check(Camera(actor)==actor);thread=correctThread;
  Clear();Check(!source&&!thread);
  Invoke(0,actor,1,1.25f,SwitchPlacementMock);Check(!source&&!thread);
  switchThrows=true;bool caught=false;__try{Invoke(0,actor,1,1.25f,SwitchPlacementMock);}__except(EXCEPTION_EXECUTE_HANDLER){caught=true;}
  Check(caught&&!source&&!thread);switchThrows=false;
  Check(Camera(actor)==actor);Check(Camera(0)==0);
  Capture(actor,0);At<Vec>(actor,0x80)={7,8,9};Clear();Capture(actor,-0.5f);
  Position(actor+0x1d0,out);Check(out[0]==7&&out[1]==8&&out[2]==9);Check(((float*)Rotation(actor+0x1d0))[5]==-0.5f);Clear();
 }
 {
  using namespace ChallengeTimers;
  nativeTimer=TimerTestNative;nativeRace=RaceTestNative;
  BYTE node[0x200]={};void* task=node+0x90;At<void*>(node,0)=(void*)A(0x8192b0);At<BYTE>(task,0xb8)=1;
  frozen=false;At<float>(task,0xb4)=12.f;timerCalls=0;
  Check(Timer(task,.1f,(void*)123)==73);Check(timerTestDelta==.1f&&timerTestContext==(void*)123&&timerCalls==1);
  frozen=true;Check(Timer(task,.1f,0)==73);Check(timerTestDelta==0&&activitySeen);
  At<float>(task,0xb4)=0;Timer(task,.1f,0);Check(timerTestDelta==.1f);
  At<float>(task,0xb4)=12;At<void*>(node,0)=0;Timer(task,.1f,0);Check(timerTestDelta==.1f);
  At<void*>(node,0)=(void*)A(0x8192b0);At<BYTE>(task,0xb8)=0;Timer(task,.1f,0);Check(timerTestDelta==.1f);
  BYTE race[0x300]={};At<int>(race,0x294)=1;Check(Race(race,.2f)==91);Check(timerTestDelta==0);
  for(int phase=0;phase<5;++phase){At<int>(race,0x294)=phase;Race(race,.2f);Check(timerTestDelta==(phase==1?0.f:.2f));}
  frozen=false;At<int>(race,0x294)=1;Race(race,.2f);Check(timerTestDelta==.2f);
  HudClock clock={};DWORD anchor=100;AdjustClock(clock,(void*)1,105,anchor,false);Check(anchor==100);
  AdjustClock(clock,(void*)1,106,anchor,true);Check(anchor==101);
  AdjustClock(clock,(void*)1,106,anchor,true);Check(anchor==101);
  AdjustClock(clock,(void*)1,111,anchor,true);Check(anchor==106);
  AdjustClock(clock,(void*)1,112,anchor,false);Check(anchor==106);
  AdjustClock(clock,(void*)1,113,anchor,false);Check(anchor==106);
  anchor=113;AdjustClock(clock,(void*)1,114,anchor,true);Check(anchor==113);
  AdjustClock(clock,(void*)2,115,anchor,true);Check(anchor==113);
  Context a={};a.world=1;a.slot=1;a.saveData=(void*)1;identity=a;frozen=true;Observe(a,true);Check(frozen);
  a.actor=(void*)2;Observe(a,true);Check(frozen);
  a.slot=2;Observe(a,true);Check(!frozen&&!activitySeen);
  frozen=true;Observe(a,false);Check(frozen);a.world=2;Observe(a,true);Check(!frozen);
  frozen=false;ready=false;Toggle();Check(!frozen&&result==TS3CheatInvalid);
 }
 // 1.2.0: slot isolation, validation, deferred requests and reversible multipliers.
 {
  Context c={};BYTE a[256]={};c.actor=a;c.slot=2;c.world=3;c.saveData=(void*)7;c.worldManager=(void*)9;
  RegionTeleport::phase=RegionTeleport::Idle;RegionTeleport::hookReady=true;
  for(unsigned i=0;i<6;++i){At<Vec>(a,0x80)={float(i),float(i+1),float(i+2)};RegionTeleport::Remember(c,i);Check(RegionTeleport::positions[i].point.x==float(i));}
  Check(RegionTeleport::SlotMask(c,false)==63&&RegionTeleport::SlotMask(c,true)==63);
  At<Vec>(a,0x80)={100,200,300};RegionTeleport::Remember(c,2);Check(RegionTeleport::positions[2].point.x==100&&RegionTeleport::positions[1].point.x==1);
  RegionTeleport::ClearSlot(2);Check(RegionTeleport::SlotMask(c,false)==59);
  RegionTeleport::ClearSlot(6);Check(result==TS3CheatInvalid&&RegionTeleport::SlotMask(c,false)==59);
  RegionTeleport::Remember(c,6);Check(result==TS3CheatInvalid&&RegionTeleport::SlotMask(c,false)==59);
  Context other=c;other.world=4;Check(RegionTeleport::SlotMask(other,false)==59&&RegionTeleport::SlotMask(other,true)==0);
  RegionTeleport::ReturnSlot(other,0);Check(result==TS3CheatNoPosition&&!RegionTeleport::Active());
  RegionTeleport::ReturnSlot(c,2);Check(result==TS3CheatNoPosition&&!RegionTeleport::Active());
  RegionTeleport::phase=RegionTeleport::Requested;Check(RegionTeleport::SlotMask(c,true)==0);RegionTeleport::phase=RegionTeleport::Idle;
  other=c;other.slot=3;RegionTeleport::Observe(other,true);Check(RegionTeleport::SlotMask(c,false)==0);
  RegionTeleport::Remember(c,1);other=c;other.saveData=(void*)8;RegionTeleport::Observe(other,true);Check(RegionTeleport::SlotMask(c,false)==0);
  float m[16];VehiclesWorld::Matrix(m,{7,8,9},{0,0,1});Check(m[0]==1&&m[5]==1&&m[10]==1&&m[15]==1&&m[12]==7&&m[13]==8&&m[14]==9);
  VehiclesWorld::Matrix(m,{7,8,9},{1,0,0});Check(m[0]==0&&m[2]==-1&&m[8]==1&&m[10]==0);
  { // Load-time selection, external changer, authored ordering, invalid/loading states.
   BYTE manager[0x90]={},world[0x80]={},sky0[16]={},sky1[16]={},sky2[16]={},sky3[16]={};
   void* worlds[]={world};void* skies[]={sky0,sky1,sky2,sky3};
   At<BYTE>(manager,8)=1;At<void**>(manager,0x38)=worlds;At<unsigned __int64>(manager,0x40)=1;
   At<const char*>(world,0x28)="WesternTown";At<void**>(world,8)=skies;At<unsigned __int64>(world,0x10)=4;
   At<const char*>(sky0,0)="Default";At<const char*>(sky1,0)="Enchanted";At<const char*>(sky2,0)="Haunted";At<const char*>(sky3,0)="Zurg";
   int expected[]={0,2,1,3};for(int n=0;n<4;++n){At<int>(world,0x38)=n;Check(VehiclesWorld::ReadSkySelection(manager,"WesternTown")==expected[n]);}
   Check(VehiclesWorld::ReadSkySelection(manager,"Story")==-1);Check(VehiclesWorld::ReadSkySelection(0,"WesternTown")==-1);
   At<int>(world,0x38)=-1;Check(VehiclesWorld::ReadSkySelection(manager,"WesternTown")==-1);
   At<int>(world,0x38)=4;Check(VehiclesWorld::ReadSkySelection(manager,"WesternTown")==-1);
   At<int>(world,0x38)=0;At<BYTE>(manager,8)=0;Check(VehiclesWorld::ReadSkySelection(manager,"WesternTown")==-1);
   At<BYTE>(manager,8)=1;At<unsigned __int64>(manager,0x40)=33;Check(VehiclesWorld::ReadSkySelection(manager,"WesternTown")==-1);
  }
  Check(VehiclesWorld::SupportedSkyWorld("WesternTown"));Check(VehiclesWorld::SupportedSkyWorld("wtwii_town"));Check(!VehiclesWorld::SupportedSkyWorld("ST_Train"));Check(!VehiclesWorld::SupportedSkyWorld(0));
  Check(TS3VehicleIsCar(11)&&VehiclesWorld::CarVariant(11)==2&&VehiclesWorld::CarFamily(11)==2);
  Check(lstrcmpA(VehiclesWorld::CarModel(11),"Vehicle_SportsCar_Zurg")==0);
  Check(VehiclesWorld::CarVariant(0)==1);Check(VehiclesWorld::CarVariant(1)==0);Check(VehiclesWorld::CarVariant(2)==2);Check(VehiclesWorld::CarVariant(3)==1);Check(VehiclesWorld::CarVariant(0xffffffff)==-1);
  BYTE group[0x58]={},body[0x90]={},raycast[0x100]={};
  Check(!VehiclesWorld::UnusedGroup(0));Check(VehiclesWorld::UnusedGroup(group));
  At<BYTE>(group,0x30)=1;Check(!VehiclesWorld::UnusedGroup(group));At<BYTE>(group,0x30)=0;
  At<unsigned>(group,0x24)=3;Check(!VehiclesWorld::UnusedGroup(group));At<unsigned>(group,0x24)=0xffffffff;Check(!VehiclesWorld::UnusedGroup(group));
  {using namespace VehiclesWorld;
   Check(lstrcmpA(CarModel(1),"Vehicle_Rally_BlueWhite")==0&&CarFamily(1)==1);
   Check(lstrcmpA(CarModel(0),"Vehicle_SportsCar_White17")==0&&CarFamily(0)==0);
   Check(CarFamily(2)==0&&lstrcmpA(CarModel(2),CarModel(0))==0);
   char part[64]={};Check(SizedPart("Rally_BlueWhite",0,part,64)&&lstrcmpA(part,"Rally_BlueWhite-mini")==0);
   Check(SizedPart("Rally_BlueWhite-mini",1,part,64)&&lstrcmpA(part,"Rally_BlueWhite")==0);
   Check(SizedPart("SportsCar_White17",2,part,64)&&lstrcmpA(part,"SportsCar_White17-monster")==0);
   Check(SizedPart("",0,part,64)&&!part[0]);Check(!SizedPart(0,0,part,64));Check(!SizedPart("Body",3,part,64));Check(!SizedPart("Rally_BlueWhite",0,part,10));
   ObserveVehicles(c);summoned[1]=42;At<unsigned>(group,0x24)=0;At<unsigned>(group,0x2c)=1;At<unsigned __int64>(group,8)=42;At<BYTE>(group,0x30)=1;
   Check(GroupSummoned(group,1));Check(!GroupSummoned(group,0));Check(!GroupSummoned(0,1));Check(!GroupSummoned(group,3));
   Check(CarFamily(3)==2&&lstrcmpA(CarModel(3),"Vehicle_SportsCar_Zurg")==0&&TS3VehicleIsCar(3));
   At<unsigned>(group,0x2c)=0;Check(!GroupSummoned(group,1));At<unsigned>(group,0x2c)=1;
   At<unsigned __int64>(group,8)=43;Check(!GroupSummoned(group,1));At<unsigned __int64>(group,8)=42;
   At<BYTE>(group,0x30)=0;Check(!GroupSummoned(group,1));At<BYTE>(group,0x30)=1;
   BYTE mount[0x400]={};At<unsigned __int64>(mount,0x38)=77;At<void*>(mount,0x168)=mount;
   spawnedRecords[4]={77,mount};resolvedTestActor=mount;
   {
    BYTE model[0x800]={},node[0xe0]={},populationTest[0x28]={},callback[0x28]={};void* testsForMount[]={populationTest};
    At<void*>(model,8)=mount;At<void*>(model,0x7e8)=node;
    At<ULONG_PTR>(node,0)=A(0x7a9620);At<unsigned __int64>(node,0xd8)=77;
    At<void*>(node,0x50)=testsForMount;At<unsigned>(node,0x58)=1;
    At<ULONG_PTR>(populationTest,0)=A(0x7a9850);At<void*>(populationTest,8)=node;
    detachManager=(void*)0x9876;detachNode=node;detachStep=0;
    Check(!ReleaseMountBudget(model,mount,3,detachManager,LodUnlinkMock,LodDestroyMock));
    At<void*>(model,8)=0;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<void*>(model,8)=mount;
    At<unsigned __int64>(node,0xd8)=78;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<unsigned __int64>(node,0xd8)=77;
    At<ULONG_PTR>(node,0)=0;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<ULONG_PTR>(node,0)=A(0x7a9620);
    At<void*>(node,0x40)=callback;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<void*>(node,0x40)=0;
    At<void*>(populationTest,8)=0;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<void*>(populationTest,8)=node;
    At<unsigned>(node,0x58)=65;Check(!ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock));At<unsigned>(node,0x58)=1;
    Check(!ReleaseMountBudget(model,mount,10,0,LodUnlinkMock,LodDestroyMock));
    Check(detachStep==0&&At<void*>(model,0x7e8)==node); // Invalid ownership never touches the manager.
    At<ULONG_PTR>(populationTest,0)=A(0x7fd578);
    Check(ReleaseMountBudget(model,mount,10,detachManager,LodUnlinkMock,LodDestroyMock)&&detachStep==0&&At<void*>(model,0x7e8)==node);
    At<ULONG_PTR>(populationTest,0)=A(0x7a9850);
    const unsigned mountChoices[]={4,10,5};
    for(unsigned i=0;i<3;++i){
     At<void*>(model,0x7e8)=node;detachStep=0;
     Check(ReleaseMountBudget(model,mount,mountChoices[i],detachManager,LodUnlinkMock,LodDestroyMock));
     Check(detachStep==2&&!At<void*>(model,0x7e8));
     Check(ReleaseMountBudget(model,mount,mountChoices[i],detachManager,LodUnlinkMock,LodDestroyMock)&&detachStep==2);
     Check(At<unsigned __int64>(mount,0x38)==77&&At<void*>(mount,0x168)==mount); // Actor survives native node release.
    }
   }
   // Real native CCDB callbacks are non-null. Exercise that exact lifecycle.
   {
    BYTE model[0x800]={},node[0xe0]={},test[0x28]={},callback[0x28]={};void* tests[]={test};
    At<void*>(model,8)=mount;At<ULONG_PTR>(node,0)=A(0x7a9620);At<unsigned __int64>(node,0xd8)=77;
    At<void*>(node,0x50)=tests;At<unsigned>(node,0x58)=1;At<void*>(test,8)=node;At<ULONG_PTR>(test,0)=A(0x7a9850);
    At<void*>(node,0x40)=callback;At<ULONG_PTR>(callback,0)=A(0x7fd380);At<unsigned __int64>(callback,0x18)=77;
    detachNode=node;mountCallback=callback;
    const unsigned mountIds[]={4,5,10};for(unsigned id: mountIds){
     At<void*>(model,0x7e8)=node;detachStep=restoreCalls=0;
     Check(ReleaseMountBudget(model,mount,id,detachManager,LodUnlinkMock,LodDestroyMock,MountRestoreMock));
     Check(restoreCalls==1&&detachStep==2&&!At<void*>(model,0x7e8));
    }
    At<void*>(model,0x7e8)=node;detachStep=restoreCalls=0;At<unsigned __int64>(callback,0x18)=78;
    Check(!ReleaseMountBudget(model,mount,4,detachManager,LodUnlinkMock,LodDestroyMock,MountRestoreMock)&&restoreCalls==0&&detachStep==0);
    At<unsigned __int64>(callback,0x18)=77;At<ULONG_PTR>(callback,0)=A(0x7fd3f0);
    Check(!ReleaseMountBudget(model,mount,4,detachManager,LodUnlinkMock,LodDestroyMock,MountRestoreMock)&&restoreCalls==0);
   }
   {
    BYTE carActors[3][0x430]={},carBodies[3][0x100]={},nativeGroup[0x58]={};
    for(unsigned j=0;j<3;++j){clearActors[j]=carActors[j];clearBodies[j]=carBodies[j];At<unsigned __int64>(nativeGroup,8+j*8)=100+j;}
    At<unsigned>(nativeGroup,0x20)=1;At<unsigned>(nativeGroup,0x24)=0;At<BYTE>(nativeGroup,0x30)=1;At<unsigned>(nativeGroup,0x54)=123;
    OwnedGroup own={};own.group=nativeGroup;for(unsigned j=0;j<3;++j)own.handles[j]=100+j;
    Check(GroupMatches(own,nativeGroup)&&!GroupMatches(own,group));own.handles[1]++;Check(!GroupMatches(own,nativeGroup));own.handles[1]--;
    carDisableCalls=carHideCalls=0;void* savedBody=clearBodies[1];clearBodies[1]=0;
    Check(!ReleaseCar(nativeGroup,clearActors,clearBodies,CarDisableMock,CarHideMock)&&!carDisableCalls&&At<BYTE>(nativeGroup,0x30)==1);clearBodies[1]=savedBody;
    Check(ReleaseCar(nativeGroup,clearActors,clearBodies,CarDisableMock,CarHideMock));
    Check(carDisableCalls==3&&carHideCalls==3&&UnusedGroup(nativeGroup));
    Check(At<unsigned>(nativeGroup,0x20)==1&&At<unsigned>(nativeGroup,0x24)==0&&At<unsigned>(nativeGroup,0x54)==123&&GroupMatches(own,nativeGroup));
    // A separately attached gun follows a cleared/reused slot without deleting
    // its native actor, changing its own hide bit, or resurrecting a sports gun.
    BYTE gun[0x430]={},slave[0x180]={},entity[0xc0]={};
    At<unsigned __int64>(carActors[0],0x38)=100;At<void*>(carActors[0],0x168)=carActors[0];
    At<unsigned __int64>(gun,0x38)=500;At<void*>(gun,0x168)=gun;At<const char*>(gun,0x148)="ZurgGun/ZurgGun";
    At<ULONG_PTR>(slave,0)=A(0x7d6950);At<void*>(slave,8)=gun;At<unsigned __int64>(slave,0xc0)=100;
    SpawnRecord parentRecord={100,carActors[0]},gunRecord={500,gun};
    Check(CarGunLink(parentRecord,gunRecord,carActors[0],gun,slave));
    At<unsigned __int64>(slave,0xc0)=101;Check(!CarGunLink(parentRecord,gunRecord,carActors[0],gun,slave));At<unsigned __int64>(slave,0xc0)=100;
    At<const char*>(gun,0x148)="PlayerGun";Check(!CarGunLink(parentRecord,gunRecord,carActors[0],gun,slave));At<const char*>(gun,0x148)="ZurgGun/ZurgGun";
    At<unsigned __int64>(gun,0x38)=501;Check(!CarGunLink(parentRecord,gunRecord,carActors[0],gun,slave));At<unsigned __int64>(gun,0x38)=500;
    At<unsigned>(nativeGroup,0x2c)=2;
    Check(!AttachmentVisible(own,nativeGroup,100));
    inheritedHideCalls=0;At<unsigned>(entity,0xac)=0x80;
    SetAttachmentHidden(entity,false,AttachmentVisible(own,nativeGroup,100),InheritedHideMock);
    Check(At<unsigned>(entity,0xac)==0x82&&inheritedHideCalls==1);
    SetAttachmentHidden(entity,false,false,InheritedHideMock);Check(inheritedHideCalls==1);
    At<BYTE>(nativeGroup,0x30)=1;Check(AttachmentVisible(own,nativeGroup,100));
    SetAttachmentHidden(entity,false,true,InheritedHideMock);Check(At<unsigned>(entity,0xac)==0x80&&inheritedHideCalls==2);
    At<unsigned>(entity,0xac)=0x81;SetAttachmentHidden(entity,false,false,InheritedHideMock);SetAttachmentHidden(entity,false,true,InheritedHideMock);Check(At<unsigned>(entity,0xac)==0x81);
    SetAttachmentHidden(entity,true,true,InheritedHideMock);Check(At<unsigned>(entity,0xac)==0x83);
    At<unsigned>(nativeGroup,0x2c)=0;Check(!AttachmentVisible(own,nativeGroup,100));
    At<unsigned>(nativeGroup,0x2c)=2;At<unsigned>(nativeGroup,0x24)=1;Check(!AttachmentVisible(own,nativeGroup,100));
    At<unsigned>(nativeGroup,0x24)=0;At<unsigned __int64>(nativeGroup,8)=999;Check(!AttachmentVisible(own,nativeGroup,100));At<unsigned __int64>(nativeGroup,8)=100;
    // Every copy is retained, duplicate confirmations do not duplicate ownership.
    Check(TrackActor(mount)&&TrackActor(mount)&&ownedActors&&!ownedActors->next);
    BYTE second[0x400]={};At<unsigned __int64>(second,0x38)=78;At<void*>(second,0x168)=second;
    Check(TrackActor(second)&&ownedActors->next&&!ownedActors->next->next);
    Check(RecordMatches(ownedActors->record,second)&&!RecordMatches(ownedActors->record,mount));
    At<unsigned __int64>(second,0x38)=79;Check(!RecordMatches(ownedActors->record,second));ForgetOwned();Check(!ownedActors);
   }
   {
    BYTE model[0x810]={},parts[0x40]={};At<void*>(model,8)=mount;At<BYTE>(model,0x770)=0x20;
    assemblyActor=mount;assemblyModel=model;assemblyParts=parts;modelBuildStep=0;
    At<void*>(model,0x7e8)=(void*)1;
    Check(!AssembleMount(model,mount,parts,DirtyPartsMock,AssembleMock)&&modelBuildStep==0); // Cannot run before detach.
    At<void*>(model,0x7e8)=0;At<BYTE>(mount,0x189)=0;
    Check(AssembleMount(model,mount,parts,DirtyPartsMock,AssembleMock));Check(modelBuildStep==2&&At<BYTE>(model,0x770)==0x22&&At<BYTE>(mount,0x189)==0);
    At<BYTE>(model,0x770)=0x20;modelBuildFail=true;modelBuildStep=0;
    Check(!AssembleMount(model,mount,parts,DirtyPartsMock,AssembleMock));Check(At<BYTE>(mount,0x189)==0);modelBuildFail=false;
    modelBuildThrow=true;modelBuildStep=0;bool caught=false;
    __try{AssembleMount(model,mount,parts,DirtyPartsMock,AssembleMock);}__except(EXCEPTION_EXECUTE_HANDLER){caught=true;}
    Check(caught&&At<BYTE>(mount,0x189)==0);modelBuildThrow=false;
    modelBuildStep=0;At<BYTE>(mount,0x189)=1;Check(AssembleMount(model,mount,parts,DirtyPartsMock,AssembleMock)&&At<BYTE>(mount,0x189)==1);At<BYTE>(mount,0x189)=0;
   }
   Check(SpawnOwned(4,mount));Check(!SpawnOwned(10,mount));Check(!SpawnOwned(4,0));Check(!SpawnOwned(TS3VehicleCount,mount));
   At<unsigned __int64>(mount,0x38)=78;Check(!SpawnOwned(4,mount));At<unsigned __int64>(mount,0x38)=77;
   At<void*>(mount,0x168)=0;Check(!SpawnOwned(4,mount));At<void*>(mount,0x168)=mount;
   bool oldReady=ready,oldHook=RegionTeleport::hookReady;ready=RegionTeleport::hookReady=true;pending=false;
   Request(c,4,false);Check(result==TS3CheatOk&&pending&&choice==4); // Live copy does not impose a one-per-variant limit.
   Request(c,10,false);Check(result==TS3CheatBusy&&pending&&choice==4); // In-flight request is never overwritten.
   pending=false;loading=true;Request(c,10,false);Check(result==TS3CheatBusy&&!pending&&choice==4);
   loading=false;Request(c,10,false);Check(result==TS3CheatOk&&pending&&choice==10);pending=false;
   Context vehicleView=c;vehicleView.actor=mount;vehicleView.toyBox=false;unsigned presentMask,availableMask,spawnedMask;
   VehicleState(vehicleView,presentMask,availableMask,spawnedMask);
   Check((spawnedMask&(1u<<4))&&(availableMask&(1u<<4))&&(availableMask&(1u<<10)));
   Check(!(availableMask&7u)); // Cars stay unavailable in Story while mounts remain usable.
   At<unsigned __int64>(mount,0x38)=78;VehicleState(vehicleView,presentMask,availableMask,spawnedMask);
   Check(!(spawnedMask&(1u<<4))&&(availableMask&(1u<<4))&&!spawnedRecords[4].handle);
   At<unsigned __int64>(mount,0x38)=77;spawnedRecords[4]={77,mount};
   ObserveVehicles(c);Check(summoned[1]==42&&SpawnOwned(4,mount));
   Context switched=c;switched.worldManager=(void*)123;ObserveVehicles(switched);Check(summoned[1]==0&&!SpawnOwned(4,mount));
   summoned[0]=45;switched.slot+=1;ObserveVehicles(switched);Check(summoned[0]==0);
   ready=oldReady;RegionTeleport::hookReady=oldHook;resolvedTestActor=0;
   const char* presets[]={"Vehicle_Rally_Black","Vehicle_Rally_BlueFlame","Vehicle_Rally_Flower","Vehicle_Rally_Red"};
   for(unsigned i=6;i<=9;++i){Check(CarVariant(i)==0&&CarFamily(i)==1);Check(lstrcmpA(CarModel(i),presets[i-6])==0);}
   Check(lstrcmpA(BullseyeModel(4),"Bullseye_Default")==0);Check(lstrcmpA(BullseyeModel(10),"Bullseye_Bizzaro")==0);
   ObserveVehicles(c);
  }
  Check(!VehiclesWorld::ChassisMatches(0,raycast));Check(!VehiclesWorld::ChassisMatches(body,0));
  At<ULONG_PTR>(raycast,0)=A(0x820658);At<ULONG_PTR>(raycast,0xf0)=0x20;
  Check(!VehiclesWorld::ChassisMatches(body,raycast)); // Exact crashing null-body+0x20 case.
  At<ULONG_PTR>(body,0x78)=0x100000;Check(!VehiclesWorld::ChassisMatches(body,raycast));
  At<ULONG_PTR>(raycast,0xf0)=0x100020;Check(VehiclesWorld::ChassisMatches(body,raycast));
  At<ULONG_PTR>(raycast,0)=0;Check(!VehiclesWorld::ChassisMatches(body,raycast));
  {bool old=VehiclesWorld::ready,hook=RegionTeleport::hookReady;VehiclesWorld::ready=RegionTeleport::hookReady=true;Context story=c;story.toyBox=false;
   for(unsigned i=0;i<TS3VehicleCount;++i)if(TS3VehicleIsCar(i)){VehiclesWorld::pending=false;VehiclesWorld::Request(story,i,false);Check(result==TS3CheatNoGame&&!VehiclesWorld::pending);}
   VehiclesWorld::ready=old;RegionTeleport::hookReady=hook;}
  MovementWeapons::ready=true;MovementWeapons::jumpActor=0;
  MovementWeapons::SetMultiplier(c,true,.5f);Check(MovementWeapons::fast&&MovementWeapons::runMultiplier==.5f);
  MovementWeapons::SetMultiplier(c,true,5);Check(MovementWeapons::fast&&MovementWeapons::runMultiplier==5);
  MovementWeapons::SetMultiplier(c,true,6);Check(result==TS3CheatInvalid&&MovementWeapons::runMultiplier==5);
  MovementWeapons::SetMultiplier(c,true,1);Check(!MovementWeapons::fast&&MovementWeapons::runMultiplier==1);
  MovementWeapons::Toggle(c,true);Check(MovementWeapons::fast&&MovementWeapons::runMultiplier==2);
  MovementWeapons::SetMultiplier(c,false,1);Check(!MovementWeapons::jump&&MovementWeapons::jumpMultiplier==1);
  MovementWeapons::SetMultiplier(c,false,4);Check(result==TS3CheatInvalid&&MovementWeapons::jumpMultiplier==1);
  SetOpen(1);queueState=0;TS3CheatCommand q={555,1,TS3CheatSpawnVehicle,0,0};
  for(unsigned i=0;i<TS3VehicleCount;++i){q.amount=i;Check(Submit(&q,sizeof(q)));queueState=0;}q.amount=TS3VehicleCount;Check(!Submit(&q,sizeof(q)));
  q.action=TS3CheatWorldSky;for(unsigned i=0;i<4;++i){q.amount=i;Check(Submit(&q,sizeof(q)));queueState=0;}q.amount=4;Check(!Submit(&q,sizeof(q)));
  q.action=TS3CheatTeleportPlace;for(unsigned i=0;i<TS3PlaceCount;++i){q.amount=i;Check(Submit(&q,sizeof(q)));queueState=0;}q.amount=TS3PlaceCount;Check(!Submit(&q,sizeof(q)));
  q.action=TS3CheatClearVehicles;q.amount=0;Check(Submit(&q,sizeof(q)));queueState=0;
  unsigned slotActions[]={TS3CheatRememberPosition,TS3CheatReturnPosition,TS3CheatClearPosition};for(unsigned act:slotActions){q.action=act;q.amount=5;Check(Submit(&q,sizeof(q)));queueState=0;q.amount=6;Check(!Submit(&q,sizeof(q)));}
  q.action=TS3CheatRunMultiplier;q.value=.5f;Check(Submit(&q,sizeof(q)));queueState=0;q.value=5;Check(Submit(&q,sizeof(q)));queueState=0;q.value=5.1f;Check(!Submit(&q,sizeof(q)));
  union {unsigned bits;float value;} nan;nan.bits=0x7fc00000;q.value=nan.value;Check(!Submit(&q,sizeof(q)));
  q.action=TS3CheatJumpMultiplier;q.value=3;Check(Submit(&q,sizeof(q)));queueState=0;q.value=3.1f;Check(!Submit(&q,sizeof(q)));q.value=nan.value;Check(!Submit(&q,sizeof(q)));
  MovementWeapons::fast=MovementWeapons::jump=false;MovementWeapons::runMultiplier=2;MovementWeapons::jumpMultiplier=1.8f;SetOpen(0);
 }
 char text[100];DWORD written;int n=wsprintfA(text,"Backend checks: %u; failures: %u\r\n",tests,failures);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,n,&written,0);ExitProcess(failures);
}

