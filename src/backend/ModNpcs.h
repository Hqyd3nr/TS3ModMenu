namespace WorldNpcs {
#include "ModNpcTemplates.h"
#include "ModNpcAudio.h"
#include "ModNpcAppearance.h"
static bool pending,tracked,loading;
static unsigned requested,serial,randomState,lastTown=0xffffffffU;
static DWORD lastSpawn;
static unsigned identitySerial;
// ActorSerializer generates Townperson_N without propUniqueName. Those keys
// belong to saved residents: native spawn completion restores their location,
// model parts and death state. Keep native audio names, but isolate persistence.
static bool Parameters(const char* source,const char* identity,char* out,unsigned capacity){
 static const char suffix[]=";propLocation=False;propOrientation=False;propVisibility=False;propDeath=False;propModel=False;propCollision=False;propUniqueName=";
 unsigned needed=lstrlenA(source)+lstrlenA(suffix)+lstrlenA(identity)+2;
 if(needed>capacity)return false;
 lstrcpyA(out,source);lstrcatA(out,suffix);lstrcatA(out,identity);lstrcatA(out,";");return true;
}
static bool Live(void* actor){return actor&&At<unsigned __int64>(actor,0x38)&&Resolve(At<unsigned __int64>(actor,0x38))==actor&&At<void*>(actor,0x168)==actor;}
static Context requestContext,ownerContext;
// Records grow only after a user spawn request. No artificial NPC count limit.
struct Record {Record* next;unsigned __int64 handle;void* actor;void* movementScript;};
static Record* records;
static Record* Allocate(){Record* r=(Record*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Record));if(r){r->next=records;records=r;}return r;}
static void Forget(){while(records){Record* next=records->next;HeapFree(GetProcessHeap(),0,records);records=next;}}
static bool SameWorld(const Context& a,const Context& b){return a.slot==b.slot&&a.saveData==b.saveData&&a.world==b.world&&a.worldManager==b.worldManager;}
static void Observe(const Context& c,bool valid){
 if(!valid)return;
 if(!tracked||!SameWorld(c,ownerContext)){Forget();Copy(&ownerContext,&c,sizeof(c));tracked=true;}
}
static bool Owned(const Record* r,void* actor,void* player){return r&&r->handle&&actor&&actor!=player&&actor==r->actor&&At<unsigned __int64>(actor,0x38)==r->handle&&At<void*>(actor,0x168)==actor;}
static unsigned Count(){unsigned count=0;__try{for(Record* r=records;r;r=r->next)if(r->handle&&Owned(r,Resolve(r->handle),Player()))++count;}__except(EXCEPTION_EXECUTE_HANDLER){}return count;}
static unsigned Random(unsigned count){if(!randomState)randomState=GetTickCount()|1;randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState%count;}
static unsigned Template(unsigned selection){
 unsigned group=selection&255,variant=selection>>8;
 if(group==0){if(!variant){variant=Random(8);if(variant==lastTown)variant=(variant+1+Random(7))%8;lastTown=variant;}else --variant;}
 if(group==1){if(!variant)variant=Random(16);else --variant;}
 if(group==3){if(!variant)variant=Random(4);else --variant;}
 return npcOffsets[group]+variant;
}
static void Request(const Context& c,unsigned index){
 if(!PlayerObjects::ready||!RegionTeleport::hookReady){Message(TS3CheatFault,"NPC spawning is unavailable for this build.");return;}
 if(index!=0xffffffffU&&!TS3ValidNpcSelection(index)){Message(TS3CheatInvalid,"Choose an NPC to spawn.");return;}
 if(pending){Message(TS3CheatBusy,"Wait a moment before spawning another NPC.");return;}
 Copy(&requestContext,&c,sizeof(c));requested=index;pending=true;
 Message(TS3CheatOk,index==0xffffffffU?"Clearing spawned NPCs...":"Spawning NPC...");
}
// Customizable characters use the same resource-backed task as native Spawn Actor.
// Its completion callback runs after model/animation resources and actor initialization.
struct SpawnJob {Context context;};
// The task has already run 557ba0. 4a5a20 restores saved actor data;
// it is NOT world registration and must not be repeated here.

static void SpawnComplete(void* actor,void* opaque){
 SpawnJob* job=(SpawnJob*)opaque;
 __try{
  Context current;
  bool valid=job&&ReadContext(Player(),current)&&SameWorld(current,job->context)&&!RegionTeleport::Active();
  if(actor&&valid){
   Observe(current,true);Record* r=Allocate();
   if(r){
    bool live=Live(actor)&&NpcAppearance::Prepare(actor);
    if(live){r->actor=actor;r->handle=At<unsigned __int64>(actor,0x38);++serial;Message(TS3CheatOk,"NPC spawned in front of you.");}
    else {((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);Message(TS3CheatFault,"This NPC could not be created in this level.");}
   }
   else {((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);Message(TS3CheatFault,"Not enough memory to track another NPC.");}
  }else{
   if(actor)((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);
   Message(valid?TS3CheatFault:TS3CheatStale,valid?"This NPC could not be created in this level.":"NPC action cancelled because gameplay changed.");
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"NPC action stopped after a game-state change.");}
 if(job)HeapFree(GetProcessHeap(),0,job);
 loading=false;pending=false;lastSpawn=GetTickCount();
}
static void Frame(){
 if(!pending||loading)return;pending=false;lastSpawn=GetTickCount();
 __try{
  Context c;if(!ReadContext(Player(),c)||c.actor!=requestContext.actor||c.handle!=requestContext.handle||!SameWorld(c,requestContext)||RegionTeleport::Active()){
   Message(TS3CheatStale,"NPC action cancelled because gameplay changed.");return;}
  Observe(c,true);
  void* stream=*(void**)A(0xb2ef20);bool streaming=stream&&At<BYTE>(stream,0x40)!=0;
  if(!RegionTeleport::LoadingStateReady(((bool(*)())A(0x477410))(),streaming,streaming&&At<BYTE>(stream,0xb8)!=0,streaming?At<int>(stream,0xbc):0,streaming?At<int>(stream,0xb4):0)){
   Message(TS3CheatBusy,"Wait for the area to finish loading.");return;}
  if(requested==0xffffffffU){unsigned count=0;
   for(Record* r=records;r;r=r->next){void* actor=r->handle?Resolve(r->handle):0;
    if(Owned(r,actor,c.actor)){((void(*)(void*,float,bool))A(0x55e160))((BYTE*)actor+0x160,0.f,false);++count;}
    if(!actor){r->handle=0;r->actor=0;}
   }
   Message(TS3CheatOk,count?"Spawned NPCs cleared.":"No spawned NPCs to clear in this area.");return;
  }
  unsigned selected=Template(requested);
  if((requested&255)==6&&!NpcAudio::Ensure()){Message(TS3CheatFault,"Muffin sounds could not be loaded. Nothing spawned.");return;}
  if((requested&255)==7&&!NpcAudio::Ensure(NpcAudio::CharacterBank(requested>>8))){Message(TS3CheatFault,"Character sounds could not be loaded. Nothing spawned.");return;}
  Vec dir=Direction(c.actor),at=At<Vec>(c.actor,0x80);at.x+=dir.x*4;at.z+=dir.z*4;at.y+=1;
  if(!Pose(at)){Message(TS3CheatInvalid,"Cannot spawn at this position.");return;}
  FILETIME now;GetSystemTimeAsFileTime(&now);char identity[96],parameters[4096];
  wsprintfA(identity,"TS3ModMenuNPC_%08lX%08lX_%08lX_%u",now.dwHighDateTime,now.dwLowDateTime,GetCurrentProcessId(),++identitySerial);
  if(!Parameters(npcTemplates[selected],identity,parameters,sizeof(parameters))){Message(TS3CheatFault,"This NPC could not be created in this level.");return;}
  unsigned __int64 set[3]={};((void*(*)(void*,const char*))A(0x4f1c60))(set,parameters);
  // New characters also use the resource-backed native task so their models,
  // motions, behavior trees and sounds are ready before reporting success.
  if((requested&255)<=1||(requested&255)>=6){
   void* manager=*(void**)A(0xb2ef18);
   SpawnJob* job=manager?(SpawnJob*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SpawnJob)):0;
   if(!job){((void(*)(void*))A(0x4f2870))(set);Message(TS3CheatFault,"This NPC could not be created in this level.");return;}
   Copy(&job->context,&c,sizeof(c));loading=true;pending=true;
   // The native task copies the parameter set and position before this call returns.
   // Do not call script registration (557ba0) again after native task completion.
   ((void(*)(void*,void*,const Vec*,float,void(*)(void*,void*),void*))A(0x5af630))(manager,set,&at,0.f,SpawnComplete,job);
   ((void(*)(void*))A(0x4f2870))(set);return;
  }
  Record* free=0;for(Record* r=records;r;r=r->next)if(!r->handle||!Owned(r,Resolve(r->handle),c.actor)){r->handle=0;r->actor=0;free=r;break;}
  if(!free)free=Allocate();
  if(!free){Message(TS3CheatFault,"Not enough memory to track another NPC.");return;}
  void* actor=((void*(*)(void*,const Vec*,float,bool,bool))A(0x554a80))(set,&at,0.f,false,false);
  // Preserve the authored entity name used by native actor audio setup.
  // A made-up CheatNpc name is unnecessary: ownership is handle + pointer + world.
  if(actor){((void(*)(void*,const char*,void*))A(0x557ba0))(actor,npcNativeNames[selected],0);free->handle=At<unsigned __int64>(actor,0x38);free->actor=actor;++serial;}
  ((void(*)(void*))A(0x4f2870))(set);
  Message(actor?TS3CheatOk:TS3CheatFault,actor?"NPC spawned in front of you.":"This NPC could not be created in this level.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"NPC action stopped after a game-state change.");}
}
}
