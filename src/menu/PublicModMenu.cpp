#include "ReleaseStrings.h"
// dinput8.dll for the Toy Story 3 Mod Menu.
// The game loads dinput8.dll from its own folder first, so this proxy runs inside the game.
// It passes DirectInput8Create on to the real system DLL, hooks a few game update functions
// for flight, and hosts the menu backend that TS3ModMenu.dll talks to.
// Toy Story 3 Complete Edition, Steam build 25668953, x64.
// RVAs and object layouts traced from SHA256 f87941b418a56b935a5e8372c11720304410d204a8cabd1d9970becfceff9722.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xmmintrin.h>
extern "C" int _fltused=0;
struct Vec {float x,y,z;};
typedef void (*UpdateFn)(void*);

static BYTE* module;
static HMODULE realInput;
static WCHAR logPath[MAX_PATH];
static void* flightActor;
static unsigned __int64 flightHandle;
static bool enabled,oldCollision,flightPoseActive,flightFacing;
static bool travelHold;   // teleport is using flight to hold the player while the new area loads
static float flightYaw;
static Vec position;
static float speed=15.0f,fastMultiplier=3.0f;
static UpdateFn actorBegin,actorEnd,groundBegin,groundUpdate,driverUpdate;
static DWORD lastLog;
static ULONG_PTR A(ULONG_PTR rva){return (ULONG_PTR)module+rva;}
template<class T> static T& At(void* p,SIZE_T offset){return *(T*)((BYTE*)p+offset);}
#include "ModDiagnostics.h"
// Messages from the shared menu code go to the session log (Mod Menu\Logs) with a [Category] tag.
static void Log(const char* message,void* actor=0){
 (void)actor;if(!message)return;
 auto starts=[&](const char* p){const char* m=message;while(*p&&*m==*p){++m;++p;}return !*p;};
 char text[256];
 if(starts("Vehicles: ")){wsprintfA(text,"[Vehicles] %s",message+10);Diagnostics::Write(Diagnostics::Debug,text);}
 else if(starts("Unsupported")){wsprintfA(text,"[Startup] %s",message);Diagnostics::Write(Diagnostics::Error,text);}
 else Diagnostics::Write(Diagnostics::Info,message);
}
// Global player manager. Player one's actor is at +0x1a0, player two's at +0x1f8.
static void* Players(){return *(void**)A(0xb2ef50);}
static void* Player(){void* p=Players();return p?At<void*>(p,0x1a0):0;}
// Looks up an actor from its handle (stored at actor+0x38). Flight keeps the handle as well
// as the pointer, so it notices when the actor it was moving has been replaced.
static void* Resolve(unsigned __int64 handle){return ((void*(*)(unsigned __int64))A(0x558400))(handle);}
// Riding Bullseye: the game gives the rider a Slave component (master handle at +0xc0, as read by 4552e0)
// that carries the rider with the horse; the horse's own BullseyeGroundController walks and jumps.
static void* Mount(void* rider){
 if(!rider)return 0;
 auto component=(void*(*)(void*,const char*))A(0x558fb0);
 void* slave=component(rider,"Slave");
 if(!slave||At<ULONG_PTR>(slave,0)!=A(0x7d6950)||At<void*>(slave,8)!=rider)return 0;
 void* horse=Resolve(At<unsigned __int64>(slave,0xc0));if(!horse||horse==rider)return 0;
 void* ground=component(horse,"BullseyeGroundController");
 return ground&&At<ULONG_PTR>(ground,0)==A(0x76c858)&&At<void*>(ground,8)==horse?horse:0;
}
// Driving a car. Seen live (2026-11-26) in a Toy Box car: the game attaches the player to the car with a
// Slave component whose +0xc0 is the car's handle (the same link Mount uses for Bullseye), and removes the
// Slave again when you get out. Bullseye and the dragon are attached the same way, so only a vehicle with an
// engine (VehiclePowertrain) counts as a car. Some cars also fill the Rider component's +0x18 (rider update
// 0x43921c) with the car's handle; that path is kept as well.
static bool Driving(void* player){
 if(!player)return false;
 auto component=(void*(*)(void*,const char*))A(0x558fb0);
 void* slave=component(player,"Slave");
 if(slave&&At<ULONG_PTR>(slave,0)==A(0x7d6950)&&At<void*>(slave,8)==player){
  void* car=Resolve(At<unsigned __int64>(slave,0xc0));
  if(car&&car!=player&&component(car,"VehiclePowertrain"))return true;
 }
 void* rider=component(player,"Rider");if(!rider||At<void*>(rider,8)!=player)return false;
 unsigned __int64 handle=At<unsigned __int64>(rider,0x18);void* car=handle?Resolve(handle):0;
 return car&&car!=player&&component(car,"BulletVehicle")!=0;
}
static bool flightMount;   // flightActor is the horse the player is riding
// True while p is still the actor flight started on, and still the one the player controls or rides.
static bool Same(void* p){return p && p==flightActor && (flightMount?p==Mount(Player()):p==Player()) && At<unsigned __int64>(p,0x38)==flightHandle;}
static bool Owner(void* component){return enabled && Same(At<void*>(component,8));}
// Turns the actor's collision on or off. Flight turns it off so you can fly through walls.
static void Collision(void* actor,bool on){
 void* instance=At<void*>(actor,0x28);
 if(instance)((void(*)(void*,int))A(0x4170a0))(instance,on?1:0);
}
// Applies the edited transform (position at +0x80, rotation at +0x90) so the game uses it.
static void Commit(void* actor){((void(*)(void*))A(0x56dab0))((BYTE*)actor+0x50);}
// Holds the actor at the flight position: writes it, clears the velocity at +0xc0 so gravity
// can't build up, and lets the actor refresh itself.
static void Place(void* actor){
 At<Vec>(actor,0x80)=position;
 Vec zero={0,0,0};At<Vec>(actor,0xc0)=zero;
 // Facing: same yaw-only rotation the native avatar switch writes (0x55f220 -> 0x24b9c0, half-angle radians) into the transform quaternion.
 if(flightFacing){float euler[3]={0,flightYaw,0},q[4];((float*(*)(float*,float*))A(0x24b9c0))(euler,q);for(int i=0;i<4;++i)At<float>(actor,0x90+4*i)=q[i];}
 Commit(actor);
 void** vt=*(void***)actor;
 ((void(*)(void*,int))vt[21])(actor,0);
 ((void(*)(void*))vt[23])(actor);
}
#include "FlightPose.h"
// Ends flight: restores the collision state saved by Start and the normal animation.
static void Stop(const char* reason,unsigned level=Diagnostics::Info){
 void* previous=flightHandle?Resolve(flightHandle):0;
 if(enabled && previous==flightActor && previous){
  Vec zero={0,0,0};At<Vec>(previous,0xc0)=zero;Commit(previous);Collision(previous,oldCollision);FlightLanding::Begin(previous,flightPoseActive);
 }
 enabled=false;flightPoseActive=false;flightFacing=false;flightMount=false;flightActor=0;flightHandle=0;Diagnostics::Write(level,reason);
}
// Starts flight on actor. Refuses actors with no collision or a stale handle, saves the current
// collision state for Stop, then turns collision off.
static void Start(void* actor,bool animate=true,bool mount=false){
 FlightLanding::Cancel();
 void* instance=At<void*>(actor,0x28);if(!instance){Diagnostics::Write(Diagnostics::Warning,"[Flight] Could not start: this character has no collision object");return;}
 flightActor=actor;flightHandle=At<unsigned __int64>(actor,0x38);
 if(!flightHandle || Resolve(flightHandle)!=actor){flightActor=0;flightHandle=0;Diagnostics::Write(Diagnostics::Warning,"[Flight] Could not start: the character is no longer valid");return;}
 oldCollision=(At<DWORD>(instance,0x48)&1)!=0;
 position=At<Vec>(actor,0x80);flightFacing=false;enabled=true;flightMount=mount;flightPoseActive=animate;Collision(actor,false);Place(actor);if(flightPoseActive)FlightPose::Apply(actor,true);
 Diagnostics::Write(travelHold?Diagnostics::Debug:Diagnostics::Info,travelHold?"[Teleport] Holding the player in place while the area loads":"[Flight] Turned on");
}
// While riding, fly the horse (no flight pose: that is the rider's animation); the rider stays on it.
static void StartFlight(void* player){void* horse=Mount(player);Start(horse?horse:player,!horse,horse!=0);}
static bool Key(int key){return (GetAsyncKeyState(key)&0x8000)!=0;}
static bool ModMenuInputBlocked();
static float Sqrt(float n){return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(n)));}
static float Yaw(float x,float z){return ((float(*)(float,float))A(0x719554))(x,z);}
// Camera heading on the ground plane. Same camera fields as the native query 0x79350, but that query
// swaps to the camera's up vector beyond 45 degrees of pitch, which points backwards when looking up
// (flight reversed/sideways near walls). Use the forward vector unless the view is almost vertical.
static Vec Direction(void* actor){
 static Vec last={0,0,1};
 void* players=Players();int index=players?((int(*)(void*,unsigned __int64))A(0x5bcd10))(players,At<unsigned __int64>(actor,0x38)):-1;
 BYTE* camera=index!=-1?((BYTE*(*)(void*,int))A(0x5bb040))(players,index):0;
 if(!camera)return last;
 Vec f=At<Vec>(camera,0x1a8),u=At<Vec>(camera,0x1b4);
 float x=f.x,z=f.z,n=Sqrt(x*x+z*z);
 if(n<0.25f){float side=f.y>0?-1.f:1.f;x=u.x*side;z=u.z*side;n=Sqrt(x*x+z*z);}
 if(!(n>0.001f))return last;
 last.x=x/n;last.y=0;last.z=z/n;return last;
}
struct FlightPad {DWORD packet;WORD buttons;BYTE lt,rt;SHORT lx,ly,rx,ry;};
// First connected XInput controller, read straight from xinput1_4.dll.
static bool ReadFlightPad(FlightPad& pad){
 typedef DWORD(WINAPI* ReadFn)(DWORD,FlightPad*);
 static ReadFn read;static bool attempted;
 if(!attempted){attempted=true;HMODULE x=GetModuleHandleW(L"XINPUT1_4.dll");if(x)read=(ReadFn)GetProcAddress(x,"XInputGetState");}
 if(!read)return false;
 for(DWORD i=0;i<4;++i)if(read(i,&pad)==0)return true;
 return false;
}
#include "FlightInput.h"
// Runs every frame for the player (called from Begin). Reads the keyboard and controller, moves the
// flight position relative to the camera and turns the character towards where it's going.
static void Tick(void* player){
 void* actor=Mount(player);if(!actor)actor=player;   // the flown actor: the ridden horse, or the player
 if(enabled && !Same(actor))Stop("[Flight] Turned off: the player character changed");
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
 bool blocked=foreground!=GetCurrentProcessId()
    ||ModMenuInputBlocked()
 ;
 FlightPad pad={};bool hasPad=enabled&&ReadFlightPad(pad);
 pad.buttons=FlightAvailableButtons(pad.buttons,blocked);
 if(blocked)return;
 if(!enabled)return;
 // The game's frame time. Odd values (pauses, loading hitches) are skipped so you don't jump.
 float dt=*(float*)A(0x95483c);if(!(dt>0 && dt<0.25f))return;
 Vec forward=Direction(player);
 float x=(Key('D')?1.0f:0)-(Key('A')?1.0f:0);
 float z=(Key('W')?1.0f:0)-(Key('S')?1.0f:0);
 float y=(Key(VK_SPACE)||Key('E')?1.0f:0)-(Key(VK_CONTROL)||Key('Q')?1.0f:0);
 bool boost=Key(VK_SHIFT);
 if(hasPad){float px,pz;FlightStick(pad.lx,pad.ly,px,pz);x+=px;z+=pz;y+=FlightVerticalButtons(pad.buttons);boost=boost||(pad.buttons&0x40)!=0;}
 float n=Sqrt(x*x+y*y+z*z);if(n>1){x/=n;y/=n;z/=n;}
 // Turn to face away from the camera while moving, so W shows the character's back.
 if(x*x+z*z>0.0001f){
  if(!flightFacing){flightYaw=Yaw(At<float>(actor,0x70),At<float>(actor,0x78));flightFacing=true;}
  const float pi=3.14159265f;float turn=Yaw(forward.x,forward.z)-flightYaw;
  while(turn>pi)turn-=2*pi;while(turn<-pi)turn+=2*pi;
  float most=dt*12;flightYaw+=turn>most?most:turn<-most?-most:turn;
 }
 float step=speed*dt*(boost?fastMultiplier:1.0f);
 position.x+=(forward.x*z+forward.z*x)*step;position.y+=y*step;
 position.z+=(forward.z*z-forward.x*x)*step;Collision(actor,false);Place(actor);
}


// Shared input gate only: no desktop viewer bridge or F10 shortcut.
#include "MenuInputGate.h"
static bool ModMenuInputBlocked(){MenuInputGate::flightOwnsFaceButtons=enabled;return MenuInputGate::Blocked();}
#include "RewardMode.h"
#include "ModMenuBridge.cpp"
// Hooked actor updates. Begin runs before the game's own update and End after it. End places the
// flown actor on both sides of the game's code, so nothing in between can move it.
static void Begin(void* actor){actorBegin(actor);if(actor==Player()){CheatMenuBridge::Tick(actor);Tick(actor);}}
static void End(void* actor){
 if(enabled&&Same(actor)){Place(actor);if(flightPoseActive)FlightPose::Apply(actor,true);}
 actorEnd(actor);
 if(enabled&&Same(actor)){Place(actor);if(flightPoseActive)FlightPose::Apply(actor,true);}
 else FlightLanding::Frame(actor);
}
// The ground movement update is skipped for the flown actor, so walking and gravity don't fight flight.
static void GroundBegin(void* component){groundBegin(component);}
static void GroundUpdate(void* component){if(!Owner(component))groundUpdate(component);}
// Keep native input/camera updates alive; GroundUpdate still suppresses flight physics.
static void DriverUpdate(void* component){driverUpdate(component);}
// Replaces one game vtable entry, but only while it still points at the expected function.
// On another game version, or if another mod got there first, nothing is changed.
static bool Slot(ULONG_PTR table,int slot,ULONG_PTR expected,void* hook,UpdateFn* original){
 void** cell=(void**)(A(table)+slot*sizeof(void*));if(*cell!=(void*)A(expected))return false;
 DWORD old;if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&old))return false;
 *original=(UpdateFn)*cell;InterlockedExchangePointer(cell,hook);DWORD unused;VirtualProtect(cell,sizeof(void*),old,&unused);return true;
}
static DWORD WINAPI Init(void*){
 module=(BYTE*)GetModuleHandleW(0);
 // No graphics, profile, currency-HUD, audio or separate input-fix installation.
 // Check every function we hook before touching anything. On any other game version, install nothing.
 if(*(void**)(A(0x7ae830)+17*8)!=(void*)A(0x561870) || *(void**)(A(0x7ae830)+20*8)!=(void*)A(0x5567e0) ||
    *(void**)(A(0x7d7bd8)+16*8)!=(void*)A(0x439020) || *(void**)(A(0x7d7bd8)+17*8)!=(void*)A(0x439b00) ||
    *(void**)(A(0x7608c0)+16*8)!=(void*)A(0x50240)){
  Log(TS3_PRIVATE_TEXT("Unsupported build: no hooks installed"));return 0;
 }
 Slot(0x7ae830,20,0x5567e0,(void*)End,&actorEnd);
 Slot(0x7ae9e8,20,0x5567e0,(void*)End,&actorEnd);   // Bullseye's actor class: same end update
 Slot(0x7d7bd8,16,0x439020,(void*)GroundBegin,&groundBegin);
 Slot(0x7d7bd8,17,0x439b00,(void*)GroundUpdate,&groundUpdate);
 Slot(0x7608c0,16,0x50240,(void*)DriverUpdate,&driverUpdate);
 Slot(0x7ae830,17,0x561870,(void*)Begin,&actorBegin);
 Diagnostics::Write(Diagnostics::Success,"[Startup] Game hooks installed");
 return 0;
}
// The export the game calls. Load the real dinput8.dll from System32 and pass the call on, then
// attach the menu's input gate to the new DirectInput object.
extern "C" HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE h,DWORD v,REFIID id,LPVOID* out,LPVOID unknown){
 if(!realInput){WCHAR path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);lstrcatW(path,L"\\dinput8.dll");realInput=LoadLibraryW(path);}
 typedef HRESULT(WINAPI *Fn)(HINSTANCE,DWORD,REFIID,LPVOID*,LPVOID);
 Fn f=realInput?(Fn)GetProcAddress(realInput,"DirectInput8Create"):0;HRESULT result=f?f(h,v,id,out,unknown):E_FAIL;
 if(SUCCEEDED(result)&&out&&*out)MenuInputGate::AttachDirectInput(*out);
 return result;
}
#include "ModNativeLoader.h"
// Keep DllMain short: hook the game's DXGI import now, before it creates its graphics objects,
// and do everything else on a separate thread.
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);
 Diagnostics::Start();   // crash handler and session log first, before anything else can go wrong
 CheatNativeLoader::Install();
 HANDLE thread=CreateThread(0,0,Init,0,0,0);if(thread)CloseHandle(thread);}return TRUE;
}
