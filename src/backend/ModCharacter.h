// Player > Character: switch between Woody, Buzz and Jessie in place (Toy Box).
//
// Creation uses the co-op join-in screen's own character strings (0x104517) and
// the same synchronous avatar factory the Zurg transformation uses (0x5af7e0).
// The hand-over repeats the native avatar switch's existing-avatar branch
// (0x187364 .. 0x187502 in SwitchToZurgNode::Switch) step for step, minus its
// Zurg-only SwitchedToZurg message, then PlayerManager::SetAvatar (0x5f1030).
// A character you leave stays hidden and is reused, like the original character
// during a Zurg transformation. No save data, unlocks or level settings change.
namespace CharacterSwitch {
static const char* const classes[3]={
 "class=woody; model_name=characters/Woody/Woody",
 "class=buzz; model_name=characters/BuzzLightYear/BuzzLightYear",
 "class=jessie; model_name=characters/Jessie/Jessie"};
static unsigned __int64 cached[2][3];
static Context identity,request;
static bool pending,tracked;
static unsigned target;
// 0x166a30 (also used for Zurg): 1 Buzz (0xcc), 2 Woody (0xd4), 3 Jessie (0xcf),
// 4 Zurg. Menu order is Woody, Buzz, Jessie; -1 for anything else.
static int Kind(void* actor){
 if(!actor)return -1;
 __try{int k=((int(*)(void*,void*))A(0x166a30))(0,actor);return k==2?0:k==1?1:k==3?2:-1;}
 __except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static void Observe(const Context& c){
 if(tracked&&ZurgTransform::SameWorld(c,identity))return;
 for(int p=0;p<2;++p)for(int k=0;k<3;++k)cached[p][k]=0;
 Copy(&identity,&c,sizeof(c));tracked=true;
}
static int Current(const Context& c){return c.toyBox?Kind(c.actor):-1;}
static bool Available(const Context& c){
 if(!c.toyBox||!ZurgTransform::ready||!RegionTeleport::hookReady||!c.actor||!RegionTeleport::AreaReady())return false;
 return ZurgTransform::PlayerIndex(*(void**)A(0xb2ef50),c.actor)>=0;
}
// Refusals shared by the request and the deferred switch. Returns false after a message.
static bool Allowed(const Context& c,unsigned kind){
 if(!c.toyBox){Message(TS3CheatNoGame,"Switching characters is Toy Box only.");return false;}
 if(!Available(c)){Message(TS3CheatInvalid,"Character switching is unavailable right now.");return false;}
 if(ZurgTransform::Active(c)||Kind(c.actor)<0){Message(TS3CheatInvalid,"Transform back to switch first.");return false;}
 if(BuzzLaser::Active(c)){Message(TS3CheatInvalid,"Disable Buzz's laser to switch first.");return false;}
 if(Kind(c.actor)==int(kind)){Message(TS3CheatInvalid,"You are already playing as this character.");return false;}
 void* manager=*(void**)A(0xb2ef50);int index=ZurgTransform::PlayerIndex(manager,c.actor);
 if(Kind(At<void*>(manager,0x1a0+(1-index)*0x58))==int(kind)){Message(TS3CheatInvalid,"The other player is already this character.");return false;}
 return true;
}
static void Request(const Context& c,unsigned kind){
 if(kind>2){Message(TS3CheatInvalid,"Unsupported command.");return;}
 if(!Allowed(c,kind))return;
 Copy(&request,&c,sizeof(c));target=kind;pending=true;
 Message(TS3CheatOk,"Switching character...");
}
static void* Sym(int id){return ((void*(*)(int))A(0x4fdbb0))(id);}
static void Frame(){
 if(!pending)return;pending=false;
 __try{
  Context c;if(!ReadContext(Player(),c)||!ZurgTransform::SameWorld(c,request)||c.actor!=request.actor||c.handle!=request.handle||RegionTeleport::Active()){
   Message(TS3CheatStale,"Character switch cancelled because the active player changed.");return;
  }
  Observe(c);if(!Allowed(c,target))return;
  void* manager=*(void**)A(0xb2ef50);const int index=ZurgTransform::PlayerIndex(manager,c.actor);
  void* source=c.actor;cached[index][Kind(source)]=c.handle;
  Vec at=At<Vec>(source,0x80);
  float yaw=((float(*)(float,float))A(0x719554))(At<float>(source,0x70),At<float>(source,0x78));
  void* other=At<void*>(manager,0x1a0+(1-index)*0x58);
  void* avatar=cached[index][target]?Resolve(cached[index][target]):0;
  if(avatar&&(avatar==source||avatar==other||Kind(avatar)!=int(target)))avatar=0;
  if(enabled)Stop("Flight OFF: character switch");
  if(!avatar){
   // Synchronous native creation; validated before the current character is touched.
   unsigned __int64 props[3]={};
   ((void*(*)(void*,const char*))A(0x4f1c60))(props,classes[target]);
   avatar=((void*(*)(void*,void*,const Vec*,float,bool))A(0x5af7e0))(manager,props,&at,0.f,false);
   ((void(*)(void*))A(0x4f2870))(props);
   if(!avatar||!At<unsigned __int64>(avatar,0x38)||Resolve(At<unsigned __int64>(avatar,0x38))!=avatar||Kind(avatar)!=int(target)){
    Message(TS3CheatFault,"The game could not create this character here.");return;
   }
   cached[index][target]=At<unsigned __int64>(avatar,0x38);
   ((void(*)(void*,const char*,void*,void*,unsigned __int64))A(0x49e0b0))(*(void**)A(0xb29a70),"RespawnAvatar",0,0,cached[index][target]);
  }
  void* actors=*(void**)A(0xb2bb70);
  ZurgTransform::InPlace::Capture(source,yaw);
  __try{
   // Bring the new character in (native existing-avatar branch).
   ((void(*)(void*,int))A(0x4170a0))(At<void*>(avatar,0x28),1);
   ((void(*)(void*,void*))A(0x550770))(avatar,Sym(2));
   ((void(*)(void*,bool))A(0x55b850))(avatar,false);
   if((!At<BYTE>(avatar,0x188)||At<BYTE>(avatar,0x189))&&At<float>(avatar,0x180)==0.f)At<int>(avatar,0x420)=0;
   ((void(*)(void*,void*))A(0x5503e0))(actors,avatar);
   ((void(*)(void*,const Vec*,float))A(0x55f220))((BYTE*)avatar+0x50,&at,yaw);
   // Retire the current character (native common tail).
   ((void(*)(void*,bool))A(0x55b850))(source,true);
   ((void(*)(void*,void*))A(0x5606c0))(source,Sym(2));
   ((void(*)(void*,int))A(0x4170a0))(At<void*>(source,0x28),0);
   if(!*(int*)A(0xb2a44c)){int serial=++*(int*)A(0xb2a448);*(int*)A(0xb2a44c)=serial;}
   void* machine=((void*(*)(void*))(*(void***)source)[0x48/8])(source);
   if(machine){
    typedef void(*Post)(void*,const char*,void*);
    ((Post)(*(void***)machine)[1])(machine,"Respawn",0);
    ((Post)(*(void***)machine)[1])(machine,"NoToolAction2",0);
   }
   At<int>(source,0x420)=2;
   ((void(*)(void*,void*))A(0x555be0))(actors,source);
   ((void(*)(void*,int,void*))A(0x5f1030))(manager,index,avatar);
  }__finally{ZurgTransform::InPlace::Clear();}
  const bool changed=At<void*>(manager,0x1a0+index*0x58)==avatar;
  const char* done[3]={"Playing as Woody.","Playing as Buzz.","Playing as Jessie."};
  Message(changed?TS3CheatOk:TS3CheatFault,changed?done[target]:"The game could not switch characters.");
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Character switch stopped after a game-state change.");}
}
}
