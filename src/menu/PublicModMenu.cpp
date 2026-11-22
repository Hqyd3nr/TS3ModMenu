#include "ReleaseStrings.h"
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
static float flightYaw;
static Vec position;
static float speed=15.0f,fastMultiplier=3.0f;
static UpdateFn actorBegin,actorEnd,groundBegin,groundUpdate,driverUpdate;
static DWORD lastLog;
static ULONG_PTR A(ULONG_PTR rva){return (ULONG_PTR)module+rva;}
template<class T> static T& At(void* p,SIZE_T offset){return *(T*)((BYTE*)p+offset);}
static void Log(const char* message,void* actor=0){
 if(!logPath[0])return;
 char line[400];int n=wsprintfA(line,"[%lu] %s actor=%p\r\n",GetTickCount(),message,actor);
 HANDLE f=CreateFileW(logPath,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(f!=INVALID_HANDLE_VALUE){DWORD done;WriteFile(f,line,n,&done,0);CloseHandle(f);}
}
static void* Players(){return *(void**)A(0xb2ef50);}
static void* Player(){void* p=Players();return p?At<void*>(p,0x1a0):0;}
static void* Resolve(unsigned __int64 handle){return ((void*(*)(unsigned __int64))A(0x558400))(handle);}
static bool Same(void* p){return p && p==flightActor && p==Player() && At<unsigned __int64>(p,0x38)==flightHandle;}
static bool Owner(void* component){return enabled && Same(At<void*>(component,8));}
static void Collision(void* actor,bool on){
 void* instance=At<void*>(actor,0x28);
 if(instance)((void(*)(void*,int))A(0x4170a0))(instance,on?1:0);
}
static void Commit(void* actor){((void(*)(void*))A(0x56dab0))((BYTE*)actor+0x50);}
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
static void Stop(const char* reason){
 void* previous=flightHandle?Resolve(flightHandle):0;
 if(enabled && previous==flightActor && previous){
  Vec zero={0,0,0};At<Vec>(previous,0xc0)=zero;Commit(previous);Collision(previous,oldCollision);if(flightPoseActive)FlightPose::Apply(previous,false);
 }
 enabled=false;flightPoseActive=false;flightFacing=false;flightActor=0;flightHandle=0;Log(reason,previous);
}
static void Start(void* actor,bool animate=true){
 void* instance=At<void*>(actor,0x28);if(!instance){Log(TS3_PRIVATE_TEXT("Toggle ignored: no collision instance"),actor);return;}
 flightActor=actor;flightHandle=At<unsigned __int64>(actor,0x38);
 if(!flightHandle || Resolve(flightHandle)!=actor){flightActor=0;flightHandle=0;Log(TS3_PRIVATE_TEXT("Toggle ignored: invalid actor handle"),actor);return;}
 oldCollision=(At<DWORD>(instance,0x48)&1)!=0;
 position=At<Vec>(actor,0x80);flightFacing=false;enabled=true;flightPoseActive=animate;Collision(actor,false);Place(actor);if(flightPoseActive)FlightPose::Apply(actor,true);Log(TS3_PRIVATE_TEXT("Flight ON"),actor);
}
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
static bool ReadFlightPad(FlightPad& pad){
 typedef DWORD(WINAPI* ReadFn)(DWORD,FlightPad*);
 static ReadFn read;static bool attempted;
 if(!attempted){attempted=true;HMODULE x=GetModuleHandleW(L"XINPUT1_4.dll");if(x)read=(ReadFn)GetProcAddress(x,"XInputGetState");}
 if(!read)return false;
 for(DWORD i=0;i<4;++i)if(read(i,&pad)==0)return true;
 return false;
}
#include "FlightInput.h"
static void Tick(void* actor){
 if(enabled && !Same(actor))Stop("Flight OFF: player changed");
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
 bool blocked=foreground!=GetCurrentProcessId()
    ||ModMenuInputBlocked()
 ;
 FlightPad pad={};bool hasPad=enabled&&ReadFlightPad(pad);
 pad.buttons=FlightAvailableButtons(pad.buttons,blocked);
 if(blocked)return;
 if(!enabled)return;
 float dt=*(float*)A(0x95483c);if(!(dt>0 && dt<0.25f))return;
 Vec forward=Direction(actor);
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
static void Begin(void* actor){actorBegin(actor);if(actor==Player()){CheatMenuBridge::Tick(actor);Tick(actor);}}
static void End(void* actor){if(enabled&&Same(actor)){Place(actor);if(flightPoseActive)FlightPose::Apply(actor,true);}actorEnd(actor);if(enabled&&Same(actor))Place(actor);}
static void GroundBegin(void* component){groundBegin(component);}
static void GroundUpdate(void* component){if(!Owner(component))groundUpdate(component);}
// Keep native input/camera updates alive; GroundUpdate still suppresses flight physics.
static void DriverUpdate(void* component){driverUpdate(component);}
static bool Slot(ULONG_PTR table,int slot,ULONG_PTR expected,void* hook,UpdateFn* original){
 void** cell=(void**)(A(table)+slot*sizeof(void*));if(*cell!=(void*)A(expected))return false;
 DWORD old;if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&old))return false;
 *original=(UpdateFn)*cell;InterlockedExchangePointer(cell,hook);DWORD unused;VirtualProtect(cell,sizeof(void*),old,&unused);return true;
}
static DWORD WINAPI Init(void*){
 module=(BYTE*)GetModuleHandleW(0);
 // No graphics, profile, currency-HUD, audio or separate input-fix installation.
 DWORD n=GetModuleFileNameW(0,logPath,MAX_PATH);
 if(n&&n<MAX_PATH-40){while(n&&logPath[n-1]!=L'\\')--n;logPath[n]=0;lstrcatW(logPath,L"Mod Menu\\backend.log");}else logPath[0]=0;
 if(*(void**)(A(0x7ae830)+17*8)!=(void*)A(0x561870) || *(void**)(A(0x7ae830)+20*8)!=(void*)A(0x5567e0) ||
    *(void**)(A(0x7d7bd8)+16*8)!=(void*)A(0x439020) || *(void**)(A(0x7d7bd8)+17*8)!=(void*)A(0x439b00) ||
    *(void**)(A(0x7608c0)+16*8)!=(void*)A(0x50240)){
  Log(TS3_PRIVATE_TEXT("Unsupported build: no hooks installed"));return 0;
 }
 Slot(0x7ae830,20,0x5567e0,(void*)End,&actorEnd);
 Slot(0x7d7bd8,16,0x439020,(void*)GroundBegin,&groundBegin);
 Slot(0x7d7bd8,17,0x439b00,(void*)GroundUpdate,&groundUpdate);
 Slot(0x7608c0,16,0x50240,(void*)DriverUpdate,&driverUpdate);
 Slot(0x7ae830,17,0x561870,(void*)Begin,&actorBegin);
 return 0;
}
extern "C" HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE h,DWORD v,REFIID id,LPVOID* out,LPVOID unknown){
 if(!realInput){WCHAR path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);lstrcatW(path,L"\\dinput8.dll");realInput=LoadLibraryW(path);}
 typedef HRESULT(WINAPI *Fn)(HINSTANCE,DWORD,REFIID,LPVOID*,LPVOID);
 Fn f=realInput?(Fn)GetProcAddress(realInput,"DirectInput8Create"):0;HRESULT result=f?f(h,v,id,out,unknown):E_FAIL;
 if(SUCCEEDED(result)&&out&&*out)MenuInputGate::AttachDirectInput(*out);
 return result;
}
#include "ModNativeLoader.h"
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);
 CheatNativeLoader::Install();
 HANDLE thread=CreateThread(0,0,Init,0,0,0);if(thread)CloseHandle(thread);}return TRUE;
}
