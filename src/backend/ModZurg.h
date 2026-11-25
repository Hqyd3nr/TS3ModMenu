// Toy Box uses the world's real SwitchToZurgNode, shared with the pod.
// Story levels retain an isolated character-only switch context.
namespace ZurgTransform {
#include "ModZurgPlacement.h"
struct SwitchState { BYTE unused[0x40]; unsigned __int64 original[2],zurg[2]; };
static SwitchState state;
static void* machine;
static bool registryReady;
// The node factory is cached by the game before our initialization. Resolve
// the actual registered instance instead of intercepting that cached factory.
static void* FindRegisteredMachine(void* head){
 void* found=0;unsigned groups=0;
 for(void* group=head?At<void*>(head,0):0;group;group=At<void*>(group,0x30)){
  if(++groups>64)return 0;unsigned handlers=0;
  for(void* handler=At<void*>(group,0x10);handler;handler=At<void*>(handler,0x10)){
   if(++handlers>64)return 0;
   if(*(void**)handler!=(void*)A(0x797960))continue;
   void* node=At<void*>(handler,0x20);
   if(!node||(BYTE*)node+0x70!=handler||*(void**)node!=(void*)A(0x7979b0)||!At<void*>(node,0x28))continue;
   if(found&&found!=node)return 0;found=node;
  }
 }
 return found;
}
static SwitchState* StateFor(const Context& c){
 if(!c.toyBox)return &state;
 machine=0;if(!registryReady)return 0;
 __try{
  void* dispatcher=*(void**)A(0xb29a70);if(!dispatcher)return 0;
  void** entry=((void**(*)(void*,const char*))A(0x26a570))((BYTE*)dispatcher+0x10,"JoiningPlayer1Avatar");
  machine=FindRegisteredMachine(entry?*entry:0);
 }__except(EXCEPTION_EXECUTE_HANDLER){machine=0;}
 return (SwitchState*)machine;
}
static Context identity,request;
static bool ready,pending,tracked;
static bool SameWorld(const Context& a,const Context& b){return a.worldManager==b.worldManager&&a.world==b.world&&a.saveData==b.saveData&&a.slot==b.slot;}
static int PlayerIndex(void* manager,void* actor){
 if(!manager||!actor)return -1;
 if(At<void*>(manager,0x1a0)==actor)return 0;
 if(At<void*>(manager,0x1f8)==actor)return 1;
 return -1;
}
static bool IsZurg(void* actor){return actor&&((int(*)(void*,void*))A(0x166a30))(0,actor)==4;}
static void Reset(){for(int i=0;i<2;++i)state.original[i]=state.zurg[i]=0;tracked=false;}
// Steam achievement 39 "The Emperor's Clothes" (Transform into Emperor Zurg) is awarded by
// AmBecomeZurg@TrophyHelper: vtable slot at 0x7a6210 -> 0x1a0af0 (mov edx,39; jmp 0x27ac30, the award queue).
// Play as Zurg must not hand it out. Only that slot is replaced, and only while the player is Zurg because of
// the menu (or within 10 s of a menu switch, in case the award is queued late); the pod still awards it.
static volatile LONG menuZurg;static volatile DWORD menuSwitchTick;static bool achievementGuard;
// The original returns the queue insert's result (al=1); a skipped award reports the same success.
typedef bool(*AwardFn)(void*,void*,void*,void*);static AwardFn zurgAward;
static bool ZurgAchievement(void* a,void* b,void* c,void* d){
 if(menuZurg||(menuSwitchTick&&DWORD(GetTickCount()-menuSwitchTick)<10000))return true;
 return zurgAward(a,b,c,d);
}
static void Observe(const Context& c){if(tracked&&!SameWorld(c,identity))Reset();if(menuZurg&&!IsZurg(c.actor))InterlockedExchange(&menuZurg,0);}
static bool Active(const Context& c){
 SwitchState* shared=StateFor(c);if(!shared||(!c.toyBox&&(!tracked||!SameWorld(c,identity))))return false;
 int index=PlayerIndex(*(void**)A(0xb2ef50),c.actor);
 return index>=0&&shared->zurg[index]==c.handle&&Resolve(c.handle)==c.actor;
}
static bool Available(const Context& c){
 if(!ready||!RegionTeleport::hookReady||!c.actor||!RegionTeleport::AreaReady())return false;
 int index=PlayerIndex(*(void**)A(0xb2ef50),c.actor);if(index<0)return false;
 SwitchState* shared=StateFor(c);if(!shared)return false;
 if(Active(c))return shared->original[index]&&Resolve(shared->original[index]);
 return !IsZurg(c.actor); // Do not take over a transformation owned by a pod.
}
// Characters a menu switch leaves behind (Zurg <-> original, Woody/Buzz/Jessie) stay hidden and are reused.
// They stay where you switched. Their scenery collision (actor+28, ActorDisableCollision) is off, but the
// separate character-vs-character push collision is not: read live in game (2026-11-21), the invisible wall
// was a hidden TS3Avatar 0.4 units from the player, state 0x420 == 2, scenery collision off, and the player
// was pushing it. The native pod never shows this because you switch inside the pod.
// Every native reuse moves the character to you first (0x55f220 in SwitchToZurgNode::Switch, both branches,
// and in Player > Character), so while it is hidden (0x420 == 2, not anyone's avatar) it is parked high
// above the spot, out of reach, and re-parked if anything moves it. Scenery collision is also kept off with
// the same call Flight/noclip uses (0x4170a0 on actor+28).
namespace Retired {
static const float parkHeight=1000.f;   // well above anything you can walk or jump to
struct Entry { unsigned __int64 handle; void* actor; Vec parked; bool hasPark; };
static Entry list[8];   // 2 players x (Woody, Buzz, Jessie, Zurg)
static Context world;
static bool tracking;
static void Clear(){for(auto& e:list)e={};tracking=false;}
static void Add(const Context& c,void* actor){
 if(!actor)return;unsigned __int64 handle=At<unsigned __int64>(actor,0x38);if(!handle)return;
 if(tracking&&!SameWorld(c,world))Clear();
 Copy(&world,&c,sizeof(c));tracking=true;
 for(auto& e:list)if(e.handle==handle){e.actor=actor;e.hasPark=false;return;}
 for(auto& e:list)if(!e.handle){e.handle=handle;e.actor=actor;e.hasPark=false;return;}
}
static void Park(Entry& e,void* actor){
 Vec now=At<Vec>(actor,0x80);
 if(!e.hasPark){e.parked=now;e.parked.y+=parkHeight;e.hasPark=true;}
 float dx=now.x-e.parked.x,dy=now.y-e.parked.y,dz=now.z-e.parked.z;
 if(dx*dx+dy*dy+dz*dz<0.25f)return;   // already parked
 float yaw=((float(*)(float,float))A(0x719554))(At<float>(actor,0x70),At<float>(actor,0x78));
 ((void(*)(void*,const Vec*,float))A(0x55f220))((BYTE*)actor+0x50,&e.parked,yaw);   // native avatar placement
}
static void Frame(){
 if(!tracking)return;
 __try{
  Context c;if(!ReadContext(Player(),c))return;   // loading or no player: try again next frame
  if(!SameWorld(c,world)){Clear();return;}
  void* manager=*(void**)A(0xb2ef50);bool any=false;
  for(auto& e:list){
   if(!e.handle)continue;
   void* actor=Resolve(e.handle);
   if(actor!=e.actor||PlayerIndex(manager,actor)>=0){e={};continue;}   // gone, or playable again
   any=true;
   if(At<int>(actor,0x420)!=2)continue;                                // not in the native hidden state
   void* collision=At<void*>(actor,0x28);
   if(collision)((void(*)(void*,int))A(0x4170a0))(collision,0);
   Park(e,actor);
  }
  if(!any)tracking=false;
 }__except(EXCEPTION_EXECUTE_HANDLER){Clear();}
}
}
// Story levels switch between the characters in the player manager's list: a count at +8 and handles
// from +0x10, each in the slot given by the character's actor+0x480 (the game adds one at 0x5b4953).
// Creating Zurg adds him there too, so after transforming back the level's own switching could pick the
// hidden, parked Zurg and leave you controlling him (seen live 2026-11-25). Keep Zurg on the list only
// while you are him, exactly as the first transformation leaves it.
static void StoryTeam(void* manager,unsigned __int64 zurg,bool listed){
 void* actor=zurg?Resolve(zurg):0;if(!manager||!actor)return;
 int slot=At<int>(actor,0x480);if(slot<0||slot>15)return;
 unsigned __int64& entry=At<unsigned __int64>(manager,0x10+slot*8);int& count=At<int>(manager,8);
 if(!listed&&entry==zurg&&count>0){entry=0;--count;}
 else if(listed&&!entry&&count<16){entry=zurg;++count;}
}
static void Request(const Context& c){
 Observe(c);
 if(!Available(c)){Message(TS3CheatInvalid,"Zurg transformation is unavailable right now.");return;}
 Copy(&request,&c,sizeof(c));pending=true;
 Message(TS3CheatOk,"Transforming...");
}
static void Frame(){
 if(!pending)return;pending=false;
 __try{
  Context c;if(!ReadContext(Player(),c)||!SameWorld(c,request)||c.actor!=request.actor||c.handle!=request.handle||RegionTeleport::Active()){
   Message(TS3CheatStale,"Transformation cancelled because the active player changed.");return;
  }
  Observe(c);if(!Available(c)){Message(TS3CheatInvalid,"Zurg transformation is unavailable right now.");return;}
  void* manager=*(void**)A(0xb2ef50);int index=PlayerIndex(manager,c.actor);bool back=Active(c);SwitchState* shared=StateFor(c);
  if(back&&!Resolve(shared->original[index])){Message(TS3CheatFault,"The original character is no longer available.");return;}
  if(enabled)Stop("Flight OFF: character transformation");
  if(!back){
   // Re-record when Player > Character switched since the last transformation,
   // so Transform back returns to the character you transformed from.
   if(!shared->original[index]||!Resolve(shared->original[index])||shared->original[index]!=c.handle){
    shared->original[index]=c.handle;
    if(c.toyBox){
     int kind=((int(*)(void*,void*))A(0x166a30))(machine,c.actor);
     ((void(*)(void*,const char*,int))A(0x4a3fb0))(*(void**)A(0xb29a48),index?"OriginalAvatar1":"OriginalAvatar0",kind);
     ((void(*)(void*,const char*,unsigned __int64,void*))A(0x49e160))(*(void**)A(0xb29a70),index?"Player1PreZurg":"Player0PreZurg",c.handle,0);
    }
   }
   Copy(&identity,&c,sizeof(c));tracked=true;
   if(!shared->zurg[index]||!Resolve(shared->zurg[index])){
    // Native synchronous avatar creation, with the pod's exact class/model.
    // Validate creation BEFORE calling Switch: the game's fresh-create branch
    // otherwise disables the original avatar even when allocation fails.
    unsigned __int64 props[3]={};Vec at=At<Vec>(c.actor,0x80);
    ((void*(*)(void*,const char*))A(0x4f1c60))(props,"class=zurgAvatar; model_name=characters/zurgAvatar/zurgAvatar");
    void* avatar=((void*(*)(void*,void*,const Vec*,float,bool))A(0x5af7e0))(manager,props,&at,0.f,false);
    ((void(*)(void*))A(0x4f2870))(props);
    if(!avatar||!At<unsigned __int64>(avatar,0x38)||Resolve(At<unsigned __int64>(avatar,0x38))!=avatar){
     Message(TS3CheatFault,"The game could not create Zurg here.");return;
    }
    shared->zurg[index]=At<unsigned __int64>(avatar,0x38);
    ((void(*)(void*,const char*,void*,void*,unsigned __int64))A(0x49e0b0))(*(void**)A(0xb29a70),"RespawnAvatar",0,0,shared->zurg[index]);
   }
  }
  // Zurg's blaster sound (zurg_gun_1) is in zurg_main.fev. Toy Box always has that bank loaded; story levels
  // only load their own, so Zurg fired silently there. Load it the same way spawned characters get theirs
  // (native loader, non-persistent, so the level's audio teardown frees it). No sound is not worth failing over.
  if(!back&&!c.toyBox)WorldNpcs::NpcAudio::Ensure("zurg_main.fev");
  unsigned __int64 expected=back?shared->original[index]:shared->zurg[index];
  float yaw=((float(*)(float,float))A(0x719554))(At<float>(c.actor,0x70),At<float>(c.actor,0x78));
  menuSwitchTick=GetTickCount();   // before the switch: the award may be queued inside it
  InPlace::CameraFlags cameraFlags[16];unsigned cameraCount=c.toyBox?0:InPlace::SaveCameraFlags(cameraFlags,16);
  if(!c.toyBox)InPlace::HoldCamera();   // story levels reset the camera a frame later
  InPlace::Invoke(shared,c.actor,index,yaw,(InPlace::SwitchFn)A(0x187060));
  InPlace::RestoreCameraFlags(cameraFlags,cameraCount);   // no snap from re-attaching the camera
  bool changed=At<void*>(manager,0x1a0+index*0x58)==Resolve(expected)&&Resolve(expected)!=c.actor;
  if(changed){menuSwitchTick=GetTickCount();InterlockedExchange(&menuZurg,back?0:1);}
  // This is the native node's post-switch output. Its three authored
  // branches create/retire Zurg's mission givers and their effects. The native
  // Switch method already sends the vehicle manager's To/FromZurg messages.
  if(changed&&c.toyBox)((void(*)(void*,void*))A(0x4df9d0))(machine,0);
  if(changed)Retired::Add(c,c.actor);   // the character just left behind
  if(changed&&!c.toyBox)StoryTeam(manager,shared->zurg[index],!back);
  Message(changed?TS3CheatOk:TS3CheatFault,changed?(back?"Original character restored.":"Playing as Zurg."):"The game could not switch characters.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Transformation stopped after a game-state change.");}
}
static void Install(){
 const BYTE switchBytes[]={0x48,0x85,0xd2,0x0f,0x84,0x67,0x06,0,0,0x48,0x8b,0xc4};
 const BYTE spawnBytes[]={0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20};
 const BYTE lookupBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};
 registryReady=RegionTeleport::Prefix(0x26a570,lookupBytes,sizeof(lookupBytes));
 // Achievement guard: replace the one vtable cell only if it and its target are exactly as analysed.
 const BYTE awardBytes[]={0xba,39,0,0,0,0xe9};   // 0x1a0af7: mov edx,39; jmp award queue
 void** cell=(void**)A(0x7a6210);
 if(!achievementGuard&&*cell==(void*)A(0x1a0af0)&&RegionTeleport::Prefix(0x1a0af7,awardBytes,sizeof(awardBytes))){
  DWORD old;if(VirtualProtect(cell,8,PAGE_READWRITE,&old)){
   zurgAward=(AwardFn)A(0x1a0af0);
   achievementGuard=InterlockedCompareExchangePointer(cell,(void*)ZurgAchievement,(void*)A(0x1a0af0))==(void*)A(0x1a0af0);
   DWORD unused;VirtualProtect(cell,8,old,&unused);
  }
 }
 ready=registryReady&&RegionTeleport::Prefix(0x187060,switchBytes,sizeof(switchBytes))&&RegionTeleport::Prefix(0x5af7e0,spawnBytes,sizeof(spawnBytes))&&InPlace::Install();
}
}
