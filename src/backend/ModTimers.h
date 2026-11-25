// Targeted native challenge clocks; never changes global game time.
namespace ChallengeTimers {
static bool ready, frozen;
static DWORD activityTick;
static bool activitySeen;
static Context identity;
typedef int (*TimerFn)(void*,float,void*);
typedef int (*RaceFn)(void*,float);
typedef void (*HudFn)(void*);
static TimerFn nativeTimer;
static RaceFn nativeRace,nativeStory;
static HudFn nativeHud;
struct HudClock {void* object;DWORD seconds,anchor;bool used;};
static HudClock clocks[4];
static float TimerDelta(bool freeze,float elapsed,float delta){
 // Allow the native first update to send its one-time StartTimer message.
 return freeze&&elapsed>0.f?0.f:delta;
}
// Each hook below handles one kind of clock. While frozen it gives the game a time step of zero,
// so that clock stops and nothing else in the game slows down.
// Timed challenges:
static int Timer(void* task,float delta,void* context){
 // Only MissionTimerNode's embedded evaluator; other Eval_Timer uses pass through.
 bool owned=false;
 __try{owned=*(void**)((BYTE*)task-0x90)==(void*)A(0x8192b0);}__except(EXCEPTION_EXECUTE_HANDLER){}
 if(owned&&At<BYTE>(task,0xb8)){
  activitySeen=true;activityTick=GetTickCount();
  delta=TimerDelta(frozen,At<float>(task,0xb4),delta);
 }
 return nativeTimer(task,delta,context);
}
// Races:
static int Race(void* task,float delta){
 // This virtual method is the running race only, after the starting countdown.
 if(At<int>(task,0x294)==1){activitySeen=true;activityTick=GetTickCount();if(frozen)delta=0;}
 return nativeRace(task,delta);
}
// Story-level mission timer (vtable 0x7a8560 slot 22 -> 0x199e50, e.g. Sunnyside Daycare balloons):
// +0x368 limit, +0x36c elapsed, +0x370 running. The native adds delta to elapsed only while running and below the limit.
static int Story(void* task,float delta){
 if(At<BYTE>(task,0x370)&&At<float>(task,0x36c)<At<float>(task,0x368)){activitySeen=true;activityTick=GetTickCount();if(frozen)delta=0;}
 return nativeStory(task,delta);
}
// The on-screen clock counts from its own start time (+0x408), not from the timers above. While a
// frozen timer is running, move that start time forward by the time that passed, so the display stops too.
static void AdjustClock(HudClock& c,void* object,DWORD now,DWORD& anchor,bool freeze){
 if(c.used&&c.object==object&&c.anchor==anchor&&freeze)anchor+=now-c.seconds;
 c.object=object;c.seconds=now;c.anchor=anchor;c.used=true;
}
static void Hud(void* screen){
 DWORD now=*(DWORD*)A(0x954838)/1000;
 HudClock* c=0;
 for(unsigned i=0;i<4;++i)if(clocks[i].used&&clocks[i].object==screen){c=&clocks[i];break;}
 if(!c)for(unsigned i=0;i<4;++i)if(!clocks[i].used){c=&clocks[i];break;}
 if(c)AdjustClock(*c,screen,now,At<DWORD>(screen,0x408),frozen&&activitySeen&&DWORD(GetTickCount()-activityTick)<250&&At<BYTE>(screen,0x411));
 nativeHud(screen);
}
// Unfreeze when the level, save slot or save changes, so a freeze never carries over into another game.
static void Observe(const Context& c,bool valid){
 if(!valid)return;
 if(identity.worldManager!=c.worldManager||identity.world!=c.world||identity.slot!=c.slot||identity.saveData!=c.saveData){
  frozen=false;activitySeen=false;for(unsigned i=0;i<4;++i)clocks[i]={};identity=c;
 }
}
// Swaps a vtable entry only if it still holds the expected value, so another mod's hook is never overwritten.
static bool Swap(ULONG_PTR cell,void* expected,void* replacement){
 void** p=(void**)A(cell);DWORD old;if(!VirtualProtect(p,sizeof(void*),PAGE_READWRITE,&old))return false;
 bool ok=InterlockedCompareExchangePointer(p,replacement,expected)==expected;DWORD unused;VirtualProtect(p,sizeof(void*),old,&unused);return ok;
}
static void Install(){
 // Reject unsupported builds or another mod's hook; rollback partial installation.
 __try{
  const BYTE sig0[]={0xf3,0xf,0x58,0xb3,0xb4,0x0,0x0,0x0,0xf3,0xf,0x10,0x83,0xb0,0x0,0x0,0x0,0xf,0x2f,0xf0,0xf3,0xf,0x11,0xb3,0xb4,0x0,0x0,0x0};
  const BYTE sig1[]={0xf,0x28,0xc3,0xf3,0xf,0x58,0x82,0x88,0x0,0x0,0x0,0xf3,0xf,0x11,0x82,0x88,0x0,0x0,0x0,0xf,0x28,0xcb,0xf3,0xf,0x58,0x8a,0x8c,0x0,0x0,0x0,0xf3,0xf,0x11,0x8a,0x8c,0x0,0x0,0x0};
  const BYTE sig2[]={0xf3,0xf,0x58,0x18,0xf3,0xf,0x11,0x18};
  const BYTE sig3[]={0xb8,0xd3,0x4d,0x62,0x10,0xf7,0x25,0x12,0x3a,0x74,0x0,0x8b,0xda,0xc1,0xeb,0x6,0x2b,0x99,0x8,0x4,0x0,0x0};
  const BYTE sig4[]={0x48,0x8b,0x3,0x48,0x8b,0xcb,0xff,0x90,0xb0,0x0,0x0,0x0,0x8b,0xf8,0x85,0xc0};
  if(!RegionTeleport::Prefix(0x611d26,sig0,sizeof(sig0))||!RegionTeleport::Prefix(0x199fe6,sig1,sizeof(sig1))||!RegionTeleport::Prefix(0x19a079,sig2,sizeof(sig2))||!RegionTeleport::Prefix(0x210e1b,sig3,sizeof(sig3))||!RegionTeleport::Prefix(0x610be6,sig4,sizeof(sig4)))return;
  if(*(void**)A(0x801cf0)!=(void*)A(0x611c60)||*(void**)A(0x7a8840)!=(void*)A(0x199f60)||*(void**)A(0x788bd8)!=(void*)A(0x210e00))return;
  nativeTimer=(TimerFn)A(0x611c60);nativeRace=(RaceFn)A(0x199f60);nativeHud=(HudFn)A(0x210e00);
  if(!Swap(0x801cf0,(void*)nativeTimer,(void*)Timer))return;
  if(!Swap(0x7a8840,(void*)nativeRace,(void*)Race)){Swap(0x801cf0,(void*)Timer,(void*)nativeTimer);return;}
  if(!Swap(0x788bd8,(void*)nativeHud,(void*)Hud)){Swap(0x7a8840,(void*)Race,(void*)nativeRace);Swap(0x801cf0,(void*)Timer,(void*)nativeTimer);return;}
  ready=true;
  // Optional: story timers. Without a matching build the other timers still freeze.
  const BYTE sig5[]={0xf3,0xf,0x10,0x8b,0x68,0x3,0x0,0x0,0xf3,0xf,0x10,0x83,0x6c,0x3,0x0,0x0,0xf,0x2f,0xc8,0x76,0x5e,0x80,0xbb,0x70,0x3,0x0,0x0,0x0,0x74,0x55,0xf3,0xf,0x58,0xc2,0xf,0x2f,0xc1,0xf3,0xf,0x11,0x83,0x6c,0x3,0x0,0x0};
  if(RegionTeleport::Prefix(0x199e80,sig5,sizeof(sig5))&&*(void**)A(0x7a8610)==(void*)A(0x199e50)){nativeStory=(RaceFn)A(0x199e50);Swap(0x7a8610,(void*)nativeStory,(void*)Story);}
 }__except(EXCEPTION_EXECUTE_HANDLER){ready=false;}
}
static void Toggle(){
 if(!ready){Message(TS3CheatInvalid,"Timer controls are unavailable.");return;}
 frozen=!frozen;Message(TS3CheatOk,frozen?"Challenge timers frozen.":"Challenge timers resumed.");
}
}
