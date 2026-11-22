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
 nativePosition=(void*(*)(void*,void*))A(targets[0]);nativeRotation=(void*(*)(void*))A(targets[1]);nativeCamera=(void*(*)(void*))A(targets[2]);
 for(unsigned i=0;i<3;++i){*(LONG*)A(sites[i]+1)=(LONG)((ULONG_PTR)(relay+i*16)-(A(sites[i])+5));FlushInstructionCache(GetCurrentProcess(),(void*)A(sites[i]),5);}
 // Two position calls occupy the same protection page. Restore in reverse order.
 for(unsigned i=3;i;){--i;VirtualProtect((void*)A(sites[i]),5,protection[i],&unused);}
 return true;
}
}
