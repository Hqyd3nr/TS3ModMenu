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
static void Observe(const Context& c){if(tracked&&!SameWorld(c,identity))Reset();}
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
  unsigned __int64 expected=back?shared->original[index]:shared->zurg[index];
  float yaw=((float(*)(float,float))A(0x719554))(At<float>(c.actor,0x70),At<float>(c.actor,0x78));
  InPlace::Invoke(shared,c.actor,index,yaw,(InPlace::SwitchFn)A(0x187060));
  bool changed=At<void*>(manager,0x1a0+index*0x58)==Resolve(expected)&&Resolve(expected)!=c.actor;
  // This is the native node's post-switch output. Its three authored
  // branches create/retire Zurg's mission givers and their effects. The native
  // Switch method already sends the vehicle manager's To/FromZurg messages.
  if(changed&&c.toyBox)((void(*)(void*,void*))A(0x4df9d0))(machine,0);
  Message(changed?TS3CheatOk:TS3CheatFault,changed?(back?"Original character restored.":"Playing as Zurg."):"The game could not switch characters.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Transformation stopped after a game-state change.");}
}
static void Install(){
 const BYTE switchBytes[]={0x48,0x85,0xd2,0x0f,0x84,0x67,0x06,0,0,0x48,0x8b,0xc4};
 const BYTE spawnBytes[]={0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20};
 const BYTE lookupBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};
 registryReady=RegionTeleport::Prefix(0x26a570,lookupBytes,sizeof(lookupBytes));
 ready=registryReady&&RegionTeleport::Prefix(0x187060,switchBytes,sizeof(switchBytes))&&RegionTeleport::Prefix(0x5af7e0,spawnBytes,sizeof(spawnBytes))&&InPlace::Install();
}
}
