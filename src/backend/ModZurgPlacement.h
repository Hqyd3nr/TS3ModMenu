// Narrow call-site adapters for a menu-initiated native Zurg switch.
// Native Switch otherwise reads actor+1d0 (stored placement), not live actor+50.
// CameraState::Attach explicitly resets the camera after assigning its new target.
namespace InPlace {
static void* source;
static DWORD thread;
static float pose[8];
static void* (*nativePosition)(void*,void*);
static void* (*nativeRotation)(void*);
static void* (*nativeCamera)(void*);
static bool OnThread(){return source&&thread==GetCurrentThreadId();}
static bool ForReference(void* reference){return OnThread()&&reference==(BYTE*)source+0x1d0;}
static void Capture(void* actor,float yaw){
 source=actor;thread=GetCurrentThreadId();
 for(unsigned i=0;i<8;++i)pose[i]=0;
 Vec p=At<Vec>(actor,0x80);pose[0]=p.x;pose[1]=p.y;pose[2]=p.z;pose[5]=yaw;
}
static void Clear(){source=0;thread=0;}
typedef void (*SwitchFn)(void*,void*,int,bool);
static void Invoke(void* shared,void* actor,int index,float yaw,SwitchFn call){
 Capture(actor,yaw);
 __try{call(shared,actor,index,false);}__finally{Clear();}
}
static void* Position(void* reference,void* output){
 if(!ForReference(reference))return nativePosition(reference,output);
 Copy(output,pose,16);return output;
}
static void* Rotation(void* reference){return ForReference(reference)?pose:nativeRotation(reference);}
static void NoReset(void*){}
static void* resetTable[2]={0,(void*)NoReset};
static void** resetProxy=resetTable;
static void* Camera(void* manager){
 // The caller only invokes Reset on the returned camera; attachment/retargeting
 // has already happened. Never suppress any other CameraState operations.
 void* camera=nativeCamera(manager);
 return OnThread()&&camera?&resetProxy:camera;
}
// Story levels (seen live 2026-11-25 in Andy's Room): the camera is reset one frame AFTER the switch,
// outside the Toy Box window above, and snaps to a fixed spot behind the character instead of staying
// where you turned it. The reset is the base camera Reset (0x402ef0, vtable slot 1 of DefaultCamera
// 0x7d3fb8 and RingCamera 0x7f85e0), which only flags the camera to snap on its next update. For a
// moment after a menu transformation in a story level, that one call is skipped.
static volatile LONG holdUntil;static bool holdActive;
static void (*nativeReset)(void*);
static void HoldReset(void* camera){
 if(holdActive&&LONG(holdUntil-(LONG)GetTickCount())>0)return;
 nativeReset(camera);
}
static void HoldCamera(){InterlockedExchange(&holdUntil,(LONG)GetTickCount()+750);holdActive=true;}
static void HoldCells(){
 const BYTE resetBytes[]={0xc6,0x81,0xa0,0x2,0x0,0x0,0x1,0xc7};   // mov byte [rcx+0x2a0],1; mov dword ...
 if(!RegionTeleport::Prefix(0x402ef0,resetBytes,sizeof(resetBytes)))return;
 const ULONG_PTR cells[]={0x7d3fb8+8,0x7f85e0+8};
 for(ULONG_PTR c:cells){
  void** p=(void**)A(c);if(*p!=(void*)A(0x402ef0))continue;DWORD old;if(!VirtualProtect(p,8,PAGE_READWRITE,&old))continue;
  InterlockedCompareExchangePointer(p,(void*)HoldReset,(void*)A(0x402ef0));DWORD unused;VirtualProtect(p,8,old,&unused);
 }
}
// Sunnyside Daycare (seen live 2026-11-25): its ring cameras snap anyway. Re-attaching the camera during
// the switch runs 0x3f8de0, which marks the active camera "reset pending" (+0x23d) when it stays on top;
// the camera's next update (RingCamera slot 2, 0x403310) then snaps it (+0x218). Remember both flags on
// every camera in player one's stack (players+0xd8 -> manager, stack at +0x78, count +0x80, camera at
// entry+0x50, as in 0x5bb040/0x3fb690) and put back any the switch set.
struct CameraFlags { void* camera; BYTE pending,snap; };
static unsigned SaveCameraFlags(CameraFlags* out,unsigned capacity){
 unsigned n=0;
 __try{
  void* players=*(void**)A(0xb2ef50);void* list=players?At<void*>(players,0xd8):0;
  void** cameras=list&&At<int>(list,0)>0?At<void**>(list,8):0;void* manager=cameras?cameras[0]:0;
  int count=manager?At<int>(manager,0x80):0;void** stack=manager?At<void**>(manager,0x78):0;
  for(int i=0;stack&&i<count&&n<capacity;++i){void* c=stack[i]?At<void*>(stack[i],0x50):0;if(c)out[n++]={c,At<BYTE>(c,0x23d),At<BYTE>(c,0x218)};}
 }__except(EXCEPTION_EXECUTE_HANDLER){n=0;}
 return n;
}
static void RestoreCameraFlags(const CameraFlags* saved,unsigned n){
 __try{for(unsigned i=0;i<n;++i){void* c=saved[i].camera;if(!saved[i].pending)At<BYTE>(c,0x23d)=0;if(!saved[i].snap)At<BYTE>(c,0x218)=0;}}
 __except(EXCEPTION_EXECUTE_HANDLER){}
}
static bool OriginalCall(ULONG_PTR site,ULONG_PTR target){
 return *(BYTE*)A(site)==0xe8&&A(site)+5+*(LONG*)A(site+1)==A(target);
}
static bool Install(){
 const ULONG_PTR sites[]={0x1871fd,0x187226,0x66d996};
 const ULONG_PTR targets[]={0x56ca30,0x56c9e0,0x3fb690};
 void* hooks[]={(void*)Position,(void*)Rotation,(void*)Camera};
 for(unsigned i=0;i<3;++i)if(!OriginalCall(sites[i],targets[i]))return false;
 // Only a relay page is allocated; no instruction relocation or global detour.
 BYTE* relay=0;ULONG_PTR origin=A(0)&~ULONG_PTR(0xffff);
 for(ULONG_PTR delta=0x10000;delta<0x70000000&&!relay;delta+=0x10000){
  relay=(BYTE*)VirtualAlloc((void*)(origin+delta),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(!relay&&origin>delta)relay=(BYTE*)VirtualAlloc((void*)(origin-delta),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 }
 if(!relay)return false;
 for(unsigned i=0;i<3;++i){BYTE* p=relay+i*16;p[0]=0xff;p[1]=0x25;*(DWORD*)(p+2)=0;*(void**)(p+6)=hooks[i];}
 DWORD unused;if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&unused)){VirtualFree(relay,0,MEM_RELEASE);return false;}
 FlushInstructionCache(GetCurrentProcess(),relay,48);
 DWORD protection[3]={};unsigned writable=0;
 for(;writable<3;++writable)if(!VirtualProtect((void*)A(sites[writable]),5,PAGE_EXECUTE_READWRITE,&protection[writable]))break;
 if(writable!=3){while(writable){--writable;VirtualProtect((void*)A(sites[writable]),5,protection[writable],&unused);}VirtualFree(relay,0,MEM_RELEASE);return false;}
 nativeReset=(void(*)(void*))A(0x402ef0);HoldCells();
 nativePosition=(void*(*)(void*,void*))A(targets[0]);nativeRotation=(void*(*)(void*))A(targets[1]);nativeCamera=(void*(*)(void*))A(targets[2]);
 for(unsigned i=0;i<3;++i){*(LONG*)A(sites[i]+1)=(LONG)((ULONG_PTR)(relay+i*16)-(A(sites[i])+5));FlushInstructionCache(GetCurrentProcess(),(void*)A(sites[i]),5);}
 // Two position calls occupy the same protection page. Restore in reverse order.
 for(unsigned i=3;i;){--i;VirtualProtect((void*)A(sites[i]),5,protection[i],&unused);}
 return true;
}
}
