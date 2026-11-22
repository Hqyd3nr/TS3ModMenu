// Native prefab loader and mount catalogue.
// Executed after actor traversal, never from the render/input thread.
namespace VehiclesWorld {
static bool pending,loading,skyPending,ready;
static unsigned choice,skyChoice,serial;
static Context requestContext,skyContext,vehicleContext;
static unsigned __int64 summoned[TS3VehicleCount];
struct SpawnRecord {unsigned __int64 handle;void* actor;};
static SpawnRecord spawnedRecords[TS3VehicleCount];
// Track every independent copy, not just the last copy of each variant.
struct OwnedActor {OwnedActor* next;SpawnRecord record;};
struct OwnedGroup {void* group;unsigned __int64 handles[3];unsigned selection;};
struct CarAttachment {CarAttachment* next;OwnedGroup owner;SpawnRecord parent,child;void* entity;bool inheritedHidden;};
static CarAttachment* carAttachments;
static void ForgetAttachments(){while(carAttachments){CarAttachment* next=carAttachments->next;HeapFree(GetProcessHeap(),0,carAttachments);carAttachments=next;}}
static OwnedActor* ownedActors;
static OwnedGroup ownedGroups[16];
static bool RecordMatches(const SpawnRecord& r,void* actor){return r.handle&&actor&&actor==r.actor&&At<unsigned __int64>(actor,0x38)==r.handle&&At<void*>(actor,0x168)==actor;}
static bool GroupMatches(const OwnedGroup& r,void* group){
 if(!group||group!=r.group)return false;
 for(unsigned j=0;j<3;++j)if(!r.handles[j]||r.handles[j]!=At<unsigned __int64>(group,8+j*8))return false;
 return true;
}
static void ForgetOwned(){
 ForgetAttachments();
 while(ownedActors){OwnedActor* next=ownedActors->next;HeapFree(GetProcessHeap(),0,ownedActors);ownedActors=next;}
 for(auto& r:ownedGroups)r={};
}
static bool TrackActor(void* actor){
 for(OwnedActor* r=ownedActors;r;r=r->next)if(RecordMatches(r->record,actor))return true;
 OwnedActor* r=(OwnedActor*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(OwnedActor));if(!r)return false;
 r->record={At<unsigned __int64>(actor,0x38),actor};r->next=ownedActors;ownedActors=r;return true;
}
static unsigned CountOwned(){
 unsigned count=0;OwnedActor** link=&ownedActors;
 while(*link){OwnedActor* r=*link;
  if(RecordMatches(r->record,Resolve(r->record.handle))){++count;link=&r->next;}
  else {*link=r->next;HeapFree(GetProcessHeap(),0,r);}
 }
 bool anyGroups=false;for(auto& r:ownedGroups)if(r.group)anyGroups=true;
 if(anyGroups){
  void* manager=*(void**)A(0x99b8b0);unsigned total=manager?At<unsigned>(manager,0x190):0;
  void** groups=manager?At<void**>(manager,0x188):0;
  for(unsigned i=0;groups&&total<=16&&i<total;++i)if(GroupMatches(ownedGroups[i],groups[i])&&At<BYTE>(groups[i],0x30))++count;
 }
 return count;
}
static bool SpawnOwned(unsigned index,void* actor){
 if(index>=TS3VehicleCount||TS3VehicleIsCar(index))return false;
 const SpawnRecord& r=spawnedRecords[index];
 return r.handle&&actor&&actor==r.actor&&At<unsigned __int64>(actor,0x38)==r.handle&&At<void*>(actor,0x168)==actor;
}
static void ObserveVehicles(const Context& c){
 if(!WorldNpcs::SameWorld(c,vehicleContext)){
  ForgetOwned();for(unsigned i=0;i<TS3VehicleCount;++i){summoned[i]=0;spawnedRecords[i]={};}
  Copy(&vehicleContext,&c,sizeof(c));
 }
}
static bool SpawnConfirmed(const Context& c,unsigned index,void* actor){
 if(index>=TS3VehicleCount||TS3VehicleIsCar(index)||!WorldNpcs::Live(actor))return false;
 ObserveVehicles(c);if(!TrackActor(actor))return false;spawnedRecords[index]={At<unsigned __int64>(actor,0x38),actor};return true;
}
// All cars, including Zurg, use the initialized native size-variant pool.
// Raw prefab duplication can register a raycast with a null chassis (0x20).
static const char* skies[]={"Default","Haunted","Enchanted","Zurg"};
static bool SupportedSkyWorld(const char* name){
 const char* names[]={"WesternTown","WTWii_Town","WT_Trendy","WT_TrendynoBLDG","WT_TrendynoAI"};
 if(name)for(unsigned i=0;i<5;++i)if(lstrcmpiA(name,names[i])==0)return true;
 return false;
}
static bool SkyAvailable(const Context& c){
 if(!ready||!c.toyBox)return false;
 return SupportedSkyWorld(((const char*(*)())A(0x5bb6a0))());
}
// Native SkyDomeManager (constructor 1fb180, singleton assignment 21de72).
// ChangeSkyDome (2094a0) selects the current-world table and its +38 sky index.
// Read the authored name rather than assuming its order matches the menu.
static int ReadSkySelection(void* manager,const char* world){
 if(!manager||!world||!At<BYTE>(manager,8))return -1;
 const unsigned __int64 count=At<unsigned __int64>(manager,0x40);
 void** worlds=At<void**>(manager,0x38);if(!worlds||count>32)return -1;
 for(unsigned i=0;i<count;++i){void* entry=worlds[i];if(!entry)continue;
  const char* name=At<const char*>(entry,0x28);if(!name||lstrcmpiA(name,world))continue;
  int selected=At<int>(entry,0x38);unsigned __int64 total=At<unsigned __int64>(entry,0x10);
  void** choices=At<void**>(entry,8);if(!choices||total>32||selected<0||(unsigned)selected>=total)return -1;
  void* item=choices[selected];const char* sky=item?At<const char*>(item,0):0;
  if(sky)for(unsigned j=0;j<4;++j)if(lstrcmpiA(sky,skies[j])==0)return int(j);
  return -1;
 }
 return -1;
}
static int ActiveSky(const Context& c){
 if(!SkyAvailable(c))return -1;
 __try {void* manager=*(void**)A(0x9ba640);
  if(!manager||At<void*>(manager,0)!=(void*)A(0x7afff0))return -1;
  return ReadSkySelection(manager,((const char*(*)())A(0x5bb6a0))());
 }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static void Install(){
 const BYTE prefab[]={0xe9,0x0b,0,0,0,0xcc,0xcc,0xcc};
 const BYTE task[]={0x4c,0x8b,0xdc,0x48,0x83,0xec,0x68};
 const BYTE sky[]={0x48,0x85,0xd2,0x0f,0x84,0x07,0x01,0,0};
 const BYTE activate[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18};
 const BYTE place[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x10};
 ready=false;
 for(unsigned i=0;i<sizeof(prefab);++i)if(*(BYTE*)A(0x5d2c80+i)!=prefab[i])return;
 for(unsigned i=0;i<sizeof(task);++i)if(*(BYTE*)A(0x5af630+i)!=task[i])return;
 for(unsigned i=0;i<sizeof(sky);++i)if(*(BYTE*)A(0x2094a0+i)!=sky[i])return;
 for(unsigned i=0;i<sizeof(activate);++i)if(*(BYTE*)A(0x1b5110+i)!=activate[i])return;
 for(unsigned i=0;i<sizeof(place);++i)if(*(BYTE*)A(0x1b4e00+i)!=place[i])return;
 ready=true;
}
static void Request(const Context& c,unsigned index,bool sky){
 if(!ready||!RegionTeleport::hookReady){Message(TS3CheatFault,"Vehicle and world controls are unavailable for this build.");return;}
 if((sky&&index>=4)||(!sky&&index>=TS3VehicleCount&&index!=0xffffffffU)){Message(TS3CheatInvalid,"Invalid selection.");return;}
 if(!sky&&TS3VehicleIsCar(index)&&!c.toyBox){Message(TS3CheatNoGame,"Toy Box only");return;}
 if(sky){
  if(!SkyAvailable(c)){Message(TS3CheatNoGame,"Sky themes are available in the main Toy Box world.");return;}
  Copy(&skyContext,&c,sizeof(c));skyChoice=index;skyPending=true;
  Message(TS3CheatOk,"Changing sky theme...");
 }else {
  if(pending||loading){Message(TS3CheatBusy,"Wait for the current vehicle or mount to finish spawning.");return;}
  Copy(&requestContext,&c,sizeof(c));choice=index;pending=true;Message(TS3CheatOk,index==0xffffffffU?"Clearing menu vehicles...":"Spawning vehicle or mount...");}
}
static void Matrix(float* matrix,const Vec& at,const Vec& direction){
 for(unsigned i=0;i<16;++i)matrix[i]=0;
 matrix[0]=direction.z;matrix[2]=-direction.x;matrix[5]=1;
 matrix[8]=direction.x;matrix[10]=direction.z;matrix[15]=1;
 matrix[12]=at.x;matrix[13]=at.y;matrix[14]=at.z;
}
static const char* BullseyeModel(unsigned index){return index==10?"Bullseye_Bizzaro":"Bullseye_Default";}
static bool MountAppearance(void* actor,unsigned index){
 if(index!=4&&index!=10)return true;
 // Rider initialization can select Bizarro automatically for a second horse.
 // After initialization, apply the user's explicit native prebuilt selection.
 const char* wanted=BullseyeModel(index);
 unsigned key=((unsigned(*)(const char*,bool))A(0x5a7e60))(wanted,true);
 if(!((void*(*)(unsigned))A(0x5a7f70))(key))return false;
 void* model=((void*(*)(void*,const char*))A(0x558fb0))(actor,"CCDB_GameModelDefinition");if(!model)return false;
 void* text=0;void* allocator=((void*(*)())A(0x2215c0))();
 ((void*(*)(void*,const char*,void*))A(0x23fb0))(&text,wanted,allocator);
 if(!text)return false;
 ((void(*)(void*,void*,bool,bool))A(0x66dd70))(model,&text,false,true);

 return At<const char*>(model,0xe0)&&lstrcmpA(At<const char*>(model,0xe0),wanted)==0;
}
// The live removal trace is 5fc160 (multiplayer population test) -> 5b7050
// (PerformanceNode_ActorTS3 unload) -> 55e160 (actor death). It trims the
// registered horse population to the player count, regardless of outfit.
// Detach only a newly created menu mount, using CCDB's native teardown order.
// Removing just its test is insufficient: another horse's test scans the group.
static bool ReleaseMountBudget(void* model,void* actor,unsigned index,void* manager,
 WorldNpcs::NpcAppearance::UnlinkFn unlink,WorldNpcs::NpcAppearance::DestroyFn destroy,void(*restore)(void*,bool)=0){
 if((index!=4&&index!=5&&index!=10)||!model||!actor||At<void*>(model,8)!=actor||!WorldNpcs::Live(actor))return false;
 void* node=At<void*>(model,0x7e8);if(!node)return true;
 if(At<ULONG_PTR>(node,0)!=A(0x7a9620)||At<unsigned __int64>(node,0xd8)!=At<unsigned __int64>(actor,0x38))return false;
 unsigned count=At<unsigned>(node,0x58);void** tests=At<void**>(node,0x50);
 if(count>64||(!tests&&count))return false;
 bool multiplayer=false;
 for(unsigned i=0;i<count;++i){
  void* test=tests[i];if(!test||At<void*>(test,8)!=node)return false;
  if(At<ULONG_PTR>(test,0)==A(0x7a9850))multiplayer=true;
 }
 if(!multiplayer)return true; // Leave unrelated performance policies intact.
 // Native 65e370 installs PerformanceNodeActivator_CCDB (59a470).
 // Restore its active model before deleting its performance node, matching
 // the native callback lifecycle. Unknown callback types remain untouched.
 void* callback=At<void*>(node,0x40);
 if(!manager||!unlink||!destroy)return false;
 if(callback){
  if(!restore||At<ULONG_PTR>(callback,0)!=A(0x7fd380)||At<unsigned __int64>(callback,0x18)!=At<unsigned __int64>(actor,0x38))return false;
  restore(callback,false);
 }
 unlink(manager,node);destroy(node,1);At<void*>(model,0x7e8)=0;
 return true;
}
static bool MountResidency(void* actor,unsigned index){
 void* model=((void*(*)(void*,const char*))A(0x558fb0))(actor,"CCDB_GameModelDefinition");
#include "ModNpcAppearanceGuards.h"
 const BYTE restoreBytes[]={0x48,0x89,0x5c,0x24,8,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x0f,0xb6,0xfa};
 for(unsigned i=0;i<sizeof(restoreBytes);++i)if(*(BYTE*)A(0x5f5580+i)!=restoreBytes[i])return false;
 if(!ReleaseMountBudget(model,actor,index,*(void**)A(0xb2ef20),
  (WorldNpcs::NpcAppearance::UnlinkFn)A(0x5da450),(WorldNpcs::NpcAppearance::DestroyFn)A(0x1c7670),(void(*)(void*,bool))A(0x5f5580)))return false;
 return true;
}
typedef void(*ModelBuildFn)(void*,bool,bool,bool);
typedef void(*PartsDirtyFn)(void*);
static bool AssembleMount(void* model,void* actor,void* parts,PartsDirtyFn dirty,ModelBuildFn build){
 if(!model||!actor||!parts||!dirty||!build||At<void*>(model,8)!=actor||At<void*>(model,0x7e8))return false;
 // Native CCDB load task 5b0810..5b0889: temporarily enable assembly,
 // dirty the part list and build. Clearing its performance node alone only
 // reveals the skeleton/eyes; it never assembles the body (flag 770 bit 1).
 BYTE old=At<BYTE>(actor,0x189);At<BYTE>(actor,0x189)=1;
 __try{dirty(parts);build(model,true,false,false);}
 __finally{At<BYTE>(actor,0x189)=old;}
 return (At<BYTE>(model,0x770)&2)!=0;
}
static bool MountBody(void* actor){
 void* model=((void*(*)(void*,const char*))A(0x558fb0))(actor,"CCDB_GameModelDefinition");if(!model)return false;
 const BYTE buildBytes[]={0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x10};
 const BYTE partsBytes[]={0xf6,0x81,0x70,7,0,0,0x10,0x48,0x8d,0x81,0xe8,0,0,0};
 const BYTE dirtyBytes[]={0x48,0x89,0x5c,0x24,8,0x57,0x48,0x83,0xec,0x20};
 for(unsigned i=0;i<sizeof(buildBytes);++i)if(*(BYTE*)A(0x672030+i)!=buildBytes[i])return false;
 for(unsigned i=0;i<sizeof(partsBytes);++i)if(*(BYTE*)A(0x655550+i)!=partsBytes[i])return false;
 for(unsigned i=0;i<sizeof(dirtyBytes);++i)if(*(BYTE*)A(0x5a6bf0+i)!=dirtyBytes[i])return false;
 void* parts=((void*(*)(void*,bool))A(0x655550))(model,false);
 if(!AssembleMount(model,actor,parts,(PartsDirtyFn)A(0x5a6bf0),(ModelBuildFn)A(0x672030)))return false;
 ((void(*)(void*,bool))A(0x55b850))(actor,false);return true;
}
struct MountJob {Context context;unsigned choice;};
static void MountComplete(void* actor,void* opaque){
 MountJob* job=(MountJob*)opaque;
 __try{
  Context c;bool valid=job&&ReadContext(Player(),c)&&WorldNpcs::SameWorld(c,job->context)&&!RegionTeleport::Active();
  bool live=valid&&WorldNpcs::Live(actor)&&WorldNpcs::NpcAppearance::Prepare(actor)&&MountResidency(actor,job->choice)&&MountAppearance(actor,job->choice)&&MountBody(actor);
  if(live)live=SpawnConfirmed(c,job->choice,actor);
  if(!live&&actor)((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);
  if(live)Log("Vehicles: mount registered and model prepared.");else Log("Vehicles: mount registration or model preparation failed.");
  Message(live?TS3CheatOk:TS3CheatFault,live?"Mount spawned. Approach it to ride.":"The mount could not be created in this area.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Mount action stopped after a game-state change.");}
 if(job)HeapFree(GetProcessHeap(),0,job);loading=pending=false;
}
static bool ContextMatches(const Context& now,const Context& expected){return now.actor==expected.actor&&now.handle==expected.handle&&WorldNpcs::SameWorld(now,expected);}
static int CarVariant(unsigned index){return index==0||index==3?1:(index==1||(index>=6&&index<=9))?0:(index==2||index==11)?2:-1;}
static const char* CarModel(unsigned index){
 const char* extra[]={"Vehicle_Rally_Black","Vehicle_Rally_BlueFlame","Vehicle_Rally_Flower","Vehicle_Rally_Red"};
 return (index==3||index==11)?"Vehicle_SportsCar_Zurg":index>=6&&index<=9?extra[index-6]:index==1?"Vehicle_Rally_BlueWhite":"Vehicle_SportsCar_White17";
}
static unsigned CarFamily(unsigned index){return (index==3||index==11)?2:index==1||(index>=6&&index<=9)?1:0;}
static bool GroupSummoned(void* group,unsigned index){
 return group&&TS3VehicleIsCar(index)&&summoned[index]&&At<BYTE>(group,0x30)!=0&&
  At<unsigned>(group,0x24)==unsigned(CarVariant(index))&&At<unsigned>(group,0x2c)==CarFamily(index)&&
  At<unsigned __int64>(group,8+8*CarVariant(index))==summoned[index];
}
static bool ChassisMatches(void* body,void* raycast){
 if(!body||!raycast||At<ULONG_PTR>(raycast,0)!=A(0x820658))return false;
 ULONG_PTR chassis=At<ULONG_PTR>(body,0x78);
 return chassis>0x10000&&At<ULONG_PTR>(raycast,0xf0)==chassis+0x20;
}
static bool CarHealthy(void* actor){
 if(!WorldNpcs::Live(actor))return false;
 void* physics=At<void*>(actor,0x368);if(!physics)return false;
 void* source=At<void*>(physics,0x28);if(!source)return false;
 if(At<ULONG_PTR>(At<void*>(source,0),8)!=A(0x7bae0))return false;
 void* body=At<void*>(source,0x28);
 auto component=(void*(*)(void*,const char*))A(0x558fb0);
 void* vehicle=component(actor,"BulletVehicle");if(!vehicle||!component(actor,"PhysicsRigidBody"))return false;
 void* wrapper=At<void*>(vehicle,0x18);if(!wrapper)return false;
 return ChassisMatches(body,At<void*>(wrapper,0x18));
}
static bool UnusedGroup(void* group){return group&&At<BYTE>(group,0x30)==0&&At<unsigned>(group,0x24)<3;}
// Mirror the visual portion of native Mini Racer purchase (ee056..ee087).
// Do NOT call 1b49d0: its tail writes persistent car records through 1b2380.
// Apply only the live model and type; leave purchased/default fields untouched.
static bool SizedPart(const char* source,unsigned size,char* out,unsigned capacity){
 if(!source||size>2||capacity<10)return false;
 unsigned n=0;while(source[n]&&source[n]!='-'){if(n+9>=capacity)return false;out[n]=source[n];++n;}
 out[n]=0;if(!n)return true;
 lstrcatA(out,size==0?"-mini":size==2?"-monster":"");return true;
}
static bool ApplySizeParts(void* group,void* source){
 // Visual operations from 1bf730, excluding its persistent-write tail.
 // The 18 native part slots use their own ref-counted strings and dirty markers.
 void* models[3];void* parts[3];char names[18][128];
 auto getParts=(void*(*)(void*,bool))A(0x655550);
 auto getPart=(void*(*)(void*,unsigned))A(0x5bba90);
 void* sourceParts=getParts(source,false);if(!sourceParts)return false;
 for(unsigned j=0;j<3;++j){
  void* actor=Resolve(At<unsigned __int64>(group,8+j*8));if(!actor)return false;
  models[j]=((void*(*)(void*,const char*,unsigned))A(0x558450))(actor,"CCDB_ModelDefinition",0);
  if(!models[j]||!(parts[j]=getParts(models[j],false)))return false;
 }
 for(unsigned i=0;i<18;++i)if(!SizedPart(At<const char*>(getPart(sourceParts,i),8),1,names[i],128))return false;
 for(unsigned j=0;j<3;++j){
  for(unsigned i=0;i<18;++i)if(names[i][0]){
   char sized[144];if(!SizedPart(names[i],j,sized,144))return false;
   void* pool=((void*(*)())A(0x5bbe60))();void* text=0;
   ((void*(*)(void*,void*,const char*))A(0x79520))(pool,&text,sized);
   if(!text)return false;
   __try{((void*(*)(void*,void*))A(0x30e10))((BYTE*)getPart(parts[j],i)+8,&text);}
   __finally{
    BYTE* header=(BYTE*)text-0x18;
    if(InterlockedDecrement((volatile LONG*)(header+0x10))<=0){
     void* allocator=At<void*>(header,0);
     ((void(*)(void*,void*))At<ULONG_PTR>(At<void*>(allocator,0),0x10))(allocator,header);
    }
   }
  }
  ((void(*)(void*))A(0x5a6bf0))(parts[j]);
  ((void(*)(void*,bool,bool,bool))A(0x672030))(models[j],true,false,true);
 }
 return true;
}
static bool CarAppearance(void* group,unsigned index){
 void* actor=Resolve(At<unsigned __int64>(group,0x10));
 void* model=actor?((void*(*)(void*,const char*,unsigned))A(0x558450))(actor,"CCDB_ModelDefinition",0):0;
 if(!model)return false;
 const char* wanted=CarModel(index);
 // Verify the requested prebuilt exists before changing any part of the group.
 unsigned key=((unsigned(*)(const char*,bool))A(0x5a7e60))(wanted,true);
 if(!((void*(*)(unsigned))A(0x5a7f70))(key))return false;
 unsigned oldSize=At<unsigned>(group,0x24);
 __try{
  At<unsigned>(group,0x24)=1;
  ((void(*)(void*,const char*,unsigned))A(0xc2500))((BYTE*)group+0x40,wanted,lstrlenA(wanted));
  void* text=(BYTE*)((void*(*)(void*))A(0x5ae80))((BYTE*)At<void*>(group,0x40)-0x18)+0x18;
  ((void(*)(void*,void*,bool,bool))A(0x66dd70))(model,&text,false,true); // consumes retained native string
  if(!ApplySizeParts(group,model))return false;
  ((void(*)(void*,unsigned,bool))A(0x1b5640))(group,CarFamily(index),true);
 }__finally{At<unsigned>(group,0x24)=oldSize;}
 return At<const char*>(model,0xe0)&&lstrcmpA(At<const char*>(model,0xe0),wanted)==0;
}
static void VehicleState(const Context& c,unsigned& present,unsigned& available,unsigned& spawned){
 present=available=spawned=0;
 if(!ready||!c.actor)return;
 __try{
  ObserveVehicles(c);
  for(unsigned i=0;i<TS3VehicleCount;++i)if(!TS3VehicleIsCar(i)){
   if(SpawnOwned(i,Resolve(spawnedRecords[i].handle)))spawned|=1u<<i;
   else spawnedRecords[i]={};
   available|=1u<<i; // Existing menu copies do not prevent another spawn.
  }
  if(!c.toyBox)return;
  void* manager=*(void**)A(0x99b8b0);if(!manager)return;
  unsigned count=At<unsigned>(manager,0x190);void** groups=At<void**>(manager,0x188);if(!groups||count>16)return;
  bool free=false;
  for(unsigned i=0;i<count;++i){void* group=groups[i];if(!group)continue;
   if(UnusedGroup(group)){
    bool healthy=true;for(unsigned j=0;j<3;++j)if(!CarHealthy(Resolve(At<unsigned __int64>(group,8+j*8))))healthy=false;
    free=free||healthy;
   }else {
    const OwnedGroup& r=ownedGroups[i];
    if(GroupMatches(r,group)&&r.selection<TS3VehicleCount&&At<unsigned>(group,0x24)==unsigned(CarVariant(r.selection))&&At<unsigned>(group,0x2c)==CarFamily(r.selection))present|=1u<<r.selection;
   }
  }
  if(free)for(unsigned i=0;i<TS3VehicleCount;++i)if(TS3VehicleIsCar(i))available|=1u<<i;
 }__except(EXCEPTION_EXECUTE_HANDLER){present=available=spawned=0;}
}
static void SummonCar(const Context& c,unsigned index,const Vec& at){
 if(!c.toyBox||CarVariant(index)<0){Message(TS3CheatNoGame,"Toy Box only");return;}
 unsigned present,available,spawned;VehicleState(c,present,available,spawned);
 // Native pool at 99b8b0; initialization 1993b0 owns three actors per slot.
 // Do not grow that pool, replace occupied cars, or write its persistent records.
 void* manager=*(void**)A(0x99b8b0);void* group=0;unsigned groupIndex=0;
 if(manager){
  unsigned count=At<unsigned>(manager,0x190);void** groups=At<void**>(manager,0x188);
  if(groups&&count<=16)for(unsigned i=0;i<count;++i){
   void* candidate=groups[i];if(!UnusedGroup(candidate))continue;
   bool healthy=true;for(unsigned j=0;j<3;++j)if(!CarHealthy(Resolve(At<unsigned __int64>(candidate,8+j*8))))healthy=false;
   if(healthy){group=candidate;groupIndex=i;break;}
  }
 }
 if(!group){Message(TS3CheatBusy,"Car slots are full. Clear Vehicles / Mounts to use them again.");return;}
 if(!CarAppearance(group,index)){Message(TS3CheatFault,"The car appearance could not be prepared. Nothing summoned.");return;}
 for(unsigned j=0;j<3;++j)if(!CarHealthy(Resolve(At<unsigned __int64>(group,8+j*8)))){
  Message(TS3CheatFault,"The native car could not be activated.");return;
 }
 unsigned __int64 expected=At<unsigned __int64>(group,8+8*CarVariant(index)),activated=0;
 ((void(*)(void*,unsigned __int64*,unsigned,bool))A(0x1b5110))(group,&activated,CarVariant(index),false);
 if(activated!=expected||!CarHealthy(Resolve(activated))){Message(TS3CheatFault,"The native car could not be activated.");return;}
 // Same group-position helper used by the native Toy Barn delivery path ef616.
 ((void(*)(void*,const Vec*,float))A(0x1b4e00))(group,&at,0.f);
 ObserveVehicles(c);summoned[index]=activated;
 ownedGroups[groupIndex].group=group;ownedGroups[groupIndex].selection=index;
 for(unsigned j=0;j<3;++j)ownedGroups[groupIndex].handles[j]=At<unsigned __int64>(group,8+j*8);
 Log("Vehicles: existing native car activated with a verified chassis and matching appearance.");
 Message(TS3CheatOk,"Car summoned. Approach it to drive.");
}
// DriverFromVehicleNode 16ae56 reads BulletVehicle+740. Rider update
// 43921c uses Rider+18 as the ridden actor handle, for either local player.
static bool Occupied(void* actor){
 if(!actor||actor==Player())return true;
 auto component=(void*(*)(void*,const char*))A(0x558fb0);
 void* vehicle=component(actor,"BulletVehicle");
 if(vehicle&&At<unsigned __int64>(vehicle,0x740))return true;
 void* players=*(void**)A(0xb2ef50);if(!players)return true;
 for(unsigned p=0;p<2;++p){void* player=At<void*>(players,0x1a0+p*0x58);if(!player)continue;
  if(player==actor)return true;
  void* rider=component(player,"Rider");
  if(rider&&At<unsigned __int64>(rider,0x18)==At<unsigned __int64>(actor,0x38))return true;
 }
 return false;
}
// Slave stores its master handle at +c0 (native 4552e0). Actor hide only
// affects its own renderer, so separately attached Zurg guns need inheritance.
static bool CarGunLink(const SpawnRecord& parent,const SpawnRecord& child,void* owner,void* actor,void* slave){
 if(!RecordMatches(parent,owner)||!RecordMatches(child,actor)||!slave||At<ULONG_PTR>(slave,0)!=A(0x7d6950)||At<void*>(slave,8)!=actor)return false;
 const char* name=At<const char*>(actor,0x148);
 return name&&lstrcmpA(name,"ZurgGun/ZurgGun")==0&&At<unsigned __int64>(slave,0xc0)==parent.handle;
}
static void* RenderEntity(void* actor){
 void* renderer=actor?At<void*>(actor,0x380):0;
 return renderer&&At<ULONG_PTR>(renderer,0)==A(0x7ef518)&&At<void*>(renderer,8)==actor?At<void*>(renderer,0x18):0;
}
static bool AttachmentVisible(const OwnedGroup& owner,void* group,unsigned __int64 parent){
 return GroupMatches(owner,group)&&At<BYTE>(group,0x30)&&At<unsigned>(group,0x2c)==2&&At<unsigned>(group,0x24)<3&&At<unsigned __int64>(group,8+8*At<unsigned>(group,0x24))==parent;
}
static void SetAttachmentHidden(void* entity,bool original,bool carVisible,void(*hide)(void*,bool,bool)){
 bool hidden=original||!carVisible;
 if(entity&&hide&&((At<unsigned>(entity,0xac)&2)!=0)!=hidden)hide(entity,hidden,true);
}
static bool AttachmentApiReady(){
 const BYTE sig[]={0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x10};
 for(unsigned i=0;i<sizeof(sig);++i)if(*(BYTE*)A(0x2b1320+i)!=sig[i])return false;
 return true;
}
static bool WatchCarAttachments(const OwnedGroup& group,void* parent){
 if(!parent||!AttachmentApiReady())return false;
 SpawnRecord owner={At<unsigned __int64>(parent,0x38),parent};unsigned visited=0;
 // Scan only at clear time. Following updates touch the few tracked links.
 for(unsigned i=0;i<256;++i)for(void* node=*(void**)A(0xb2bb90+i*8);node;node=At<void*>(node,0x18)){
  if(++visited>32768)return false;
  void* child=At<void*>(node,0);if(!child||At<ULONG_PTR>(child,0)!=A(0x7ae9e8)||At<void*>(child,0x168)!=child||At<unsigned __int64>(node,8)!=At<unsigned __int64>(child,0x38))continue;
  const char* name=At<const char*>(child,0x148);if(!name||lstrcmpA(name,"ZurgGun/ZurgGun"))continue;
  SpawnRecord record={At<unsigned __int64>(child,0x38),child};
  void* slave=((void*(*)(void*,const char*))A(0x558fb0))(child,"Slave");
  if(!CarGunLink(owner,record,parent,child,slave))continue;
  bool tracked=false;for(CarAttachment* p=carAttachments;p;p=p->next)if(RecordMatches(p->child,child)&&RecordMatches(p->parent,parent)){tracked=true;break;}
  if(tracked)continue;
  void* entity=RenderEntity(child);if(!entity)return false;
  CarAttachment* p=(CarAttachment*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(CarAttachment));if(!p)return false;
  p->owner=group;p->parent=owner;p->child=record;p->entity=entity;p->inheritedHidden=(At<unsigned>(entity,0xac)&2)!=0;p->next=carAttachments;carAttachments=p;
 }
 return true;
}
static void UpdateCarAttachments(){
 if(!carAttachments)return;
 __try{
  Context c;if(!ReadContext(Player(),c)||RegionTeleport::Active())return;
  if(!WorldNpcs::SameWorld(c,vehicleContext)){ForgetAttachments();return;}
  void* manager=c.toyBox?*(void**)A(0x99b8b0):0;
  unsigned count=manager?At<unsigned>(manager,0x190):0;void** groups=manager?At<void**>(manager,0x188):0;
  if(!groups||count>16||!AttachmentApiReady())return;
  CarAttachment** link=&carAttachments;
  while(*link){CarAttachment* p=*link;void* parent=Resolve(p->parent.handle);void* child=Resolve(p->child.handle);
   void* slave=RecordMatches(p->child,child)?((void*(*)(void*,const char*))A(0x558fb0))(child,"Slave"):0;
   void* group=0;for(unsigned i=0;i<count;++i)if(GroupMatches(p->owner,groups[i])){group=groups[i];break;}
   if(!CarGunLink(p->parent,p->child,parent,child,slave)||!group||RenderEntity(child)!=p->entity){
    // Undo only our inherited visibility on the same surviving child/model.
    // A recycled actor or replacement model must never be touched.
    if(RecordMatches(p->child,child)&&RenderEntity(child)==p->entity)
     SetAttachmentHidden(p->entity,p->inheritedHidden,true,(void(*)(void*,bool,bool))A(0x2b1320));
    *link=p->next;HeapFree(GetProcessHeap(),0,p);continue;
   }
   void* parentEntity=RenderEntity(parent);
   bool visible=AttachmentVisible(p->owner,group,p->parent.handle)&&parentEntity&&!(At<unsigned>(parentEntity,0xac)&3)&&At<unsigned>(parent,0x420)!=2;
   SetAttachmentHidden(p->entity,p->inheritedHidden,visible,(void(*)(void*,bool,bool))A(0x2b1320));
   link=&p->next;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){} // Keep records for a later valid frame.
}
static bool ReleaseCar(void* group,void* const actors[3],void* const bodies[3],void(*disable)(void*,bool),void(*hide)(void*,bool)){
 if(!group||!disable||!hide)return false;
 for(unsigned j=0;j<3;++j)if(!actors[j]||!bodies[j])return false;
 // Live-only subset of native dismissal 19a8f0. Its 1b2380 save-write is excluded.
 for(unsigned j=0;j<3;++j){disable(bodies[j],false);At<unsigned>(actors[j],0x420)=2;hide(actors[j],true);}
 At<BYTE>(group,0x30)=0;return true;
}
static void ClearVehicles(const Context& c){
 ObserveVehicles(c);unsigned removed=0,blocked=0;
 // Validate the native physics toggle before any removal.
 const BYTE sig[]={0x40,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x20};
 for(unsigned i=0;i<sizeof(sig);++i)if(*(BYTE*)A(0x536550+i)!=sig[i]){Message(TS3CheatFault,"Vehicle clearing is unavailable for this build.");return;}
 OwnedActor** link=&ownedActors;
 while(*link){OwnedActor* r=*link;void* actor=Resolve(r->record.handle);
  if(RecordMatches(r->record,actor)){
   if(Occupied(actor)){++blocked;link=&r->next;continue;}
   ((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);++removed;
   for(auto& last:spawnedRecords)if(last.actor==actor&&last.handle==r->record.handle)last={};
  }
  *link=r->next;HeapFree(GetProcessHeap(),0,r);
 }
 void* manager=c.toyBox?*(void**)A(0x99b8b0):0;
 unsigned count=manager?At<unsigned>(manager,0x190):0;void** groups=manager?At<void**>(manager,0x188):0;
 for(unsigned i=0;groups&&count<=16&&i<count;++i){OwnedGroup& r=ownedGroups[i];void* group=groups[i];
  if(!GroupMatches(r,group))continue;
  if(!At<BYTE>(group,0x30)){r={};continue;}
  void* actors[3]={},*bodies[3]={};bool safe=true;
  for(unsigned j=0;j<3;++j){actors[j]=Resolve(r.handles[j]);
   if(!CarHealthy(actors[j])||Occupied(actors[j])){safe=false;break;}
   bodies[j]=At<void*>(At<void*>(At<void*>(actors[j],0x368),0x28),0x28);
  }
  if(!safe){++blocked;continue;}
  for(unsigned j=0;j<3&&safe;++j)if(!WatchCarAttachments(r,actors[j]))safe=false;
  if(!safe){++blocked;continue;}
  if(ReleaseCar(group,actors,bodies,(void(*)(void*,bool))A(0x536550),(void(*)(void*,bool))A(0x55b850))){r={};++removed;}
 }
 UpdateCarAttachments();
 Message(blocked?TS3CheatPartial:TS3CheatOk,blocked?"Some vehicles remain in use or unavailable. Dismount, then clear again.":removed?"Vehicles / Mounts are cleared. Their car slots are available again.":"No vehicles / mounts to clear in this area.");
}
static void Frame(){
 UpdateCarAttachments();
 if((!pending||loading)&&!skyPending)return;
 __try{
  Context c;bool valid=ReadContext(Player(),c)&&!RegionTeleport::Active();
  if(skyPending){skyPending=false;
   if(valid&&ContextMatches(c,skyContext)&&SkyAvailable(c)){
    ((void(*)(void*,const char*,void*,unsigned __int64))A(0x49e160))(*(void**)A(0xb29a70),"ChangeSkyDome",(void*)skies[skyChoice],0);
    Message(TS3CheatOk,"Sky theme requested.");
   }else Message(TS3CheatStale,"Sky change cancelled because the area changed.");
  }
  if(!pending||loading)return;pending=false;
  if(!valid||!ContextMatches(c,requestContext)){Message(TS3CheatStale,"Vehicle action cancelled because gameplay changed.");return;}
  void* stream=*(void**)A(0xb2ef20);bool streaming=stream&&At<BYTE>(stream,0x40)!=0;
  if(!RegionTeleport::LoadingStateReady(((bool(*)())A(0x477410))(),streaming,streaming&&At<BYTE>(stream,0xb8)!=0,streaming?At<int>(stream,0xbc):0,streaming?At<int>(stream,0xb4):0)){
   Message(TS3CheatBusy,"Wait for the area to finish loading.");return;
  }
  if(choice==0xffffffffU){ClearVehicles(c);return;}
  Vec direction=Direction(c.actor),at=At<Vec>(c.actor,0x80);at.x+=direction.x*6;at.z+=direction.z*6;at.y+=1;
  if(!Pose(at)){Message(TS3CheatInvalid,"Cannot spawn at this position.");return;}
  const char* bank=TS3VehicleIsCar(choice)?"wt_stunt_park.fev":choice==4||choice==10?"bullseye.fev":"dragon.fev";
  if(!WorldNpcs::NpcAudio::Ensure(bank)){Message(TS3CheatFault,"Vehicle or mount sounds could not be loaded. Nothing spawned.");return;}
  if(TS3VehicleIsCar(choice)){SummonCar(c,choice,at);}
  else{
   // Exact catalogue mount templates used at ef20f/ef23b, with unique ephemeral identity.
   const char* source=(const char*)A(choice==4||choice==10?0x78f520:0x78f6f0);
   char identity[80],parameters[2048];wsprintfA(identity,"TS3ModMenuMount_%lu_%u",GetTickCount(),++serial);
   if(!WorldNpcs::Parameters(source,identity,parameters,sizeof(parameters)-32)){Message(TS3CheatFault,"Mount parameters are unavailable.");return;}
   lstrcatA(parameters,choice==4||choice==10?"name=Bullseye;":"name=Dragon;");
   void* manager=*(void**)A(0xb2ef18);MountJob* job=manager?(MountJob*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(MountJob)):0;
   if(!job){Message(TS3CheatFault,"Not enough memory to spawn a mount.");return;}
   unsigned __int64 set[3]={};((void*(*)(void*,const char*))A(0x4f1c60))(set,parameters);
   Copy(&job->context,&c,sizeof(c));job->choice=choice;loading=pending=true;
   ((void(*)(void*,void*,const Vec*,float,void(*)(void*,void*),void*))A(0x5af630))(manager,set,&at,0.f,MountComplete,job);
   ((void(*)(void*))A(0x4f2870))(set);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){pending=loading=skyPending=false;Message(TS3CheatFault,"Vehicle or world action stopped after a game-state change.");}
}
}
