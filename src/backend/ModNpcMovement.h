// Adjust only a menu-owned Slinky instance after its native root AI initializes.
// The root initializes before SpawnComplete records ownership, so an ownership-
// guarded initialization hook cannot reliably intercept the stationary override.
namespace NpcMovement {
static unsigned Hash(const BYTE* bytes,unsigned length){unsigned h=2166136261U;for(unsigned i=0;i<length;++i)h=(h^bytes[i])*16777619U;return h;}
static bool Override(bool owned,unsigned index,unsigned words,unsigned hash){return owned&&index==243&&words==2434&&hash==0xda95246fU;}
static bool ApplyState(BYTE* values){
 // firstPass must have completed; never alter another variable or value type.
 if(!values||At<int>(values,2*16)!=0||At<unsigned __int64>(values,2*16+8)!=0||At<int>(values,4*16)!=0)return false;
 if(At<unsigned __int64>(values,4*16+8)>1)return false;
 At<unsigned __int64>(values,4*16+8)=0;return true;
}
static void* Control(void* actor){
 void* owner=At<void*>(actor,0x370);if(!owner)return 0;
 BYTE* bag=(BYTE*)owner+0x18;unsigned count=At<unsigned>(bag,0x58);void** table=At<void**>(bag,0x48);
 if(count>1024||(!table&&count))return 0;
 for(unsigned i=0;i<count;++i){void* node=table[i];unsigned hops=0;
  while(node&&++hops<4096){const char* name=At<const char*>(node,0x10);
   if(name&&lstrcmpA(name,"AiBehaviorControl")==0)return At<void*>(node,0x18);node=At<void*>(node,0);
  }
 }return 0;
}
static BYTE* RootVm(void* control,void* actor){
 if(!control||*(void**)control!=(void*)A(0x804818)||At<void*>(control,8)!=actor)return 0;
 void* root=At<void*>(control,0x38);
 if(!root||*(void**)root!=(void*)A(0x800fc8)||At<void*>(root,0x20)!=control)return 0;
 void* evaluator=At<void*>(root,0x68);
 if(!evaluator||*(void**)evaluator!=(void*)A(0x8010e0)||At<void*>(evaluator,8)!=control||At<void*>(evaluator,0x10)!=root)return 0;
 return At<BYTE*>(evaluator,0x20);
}
static void Frame(){
 using namespace WorldNpcs;
 static DWORD last;DWORD now=GetTickCount();if((DWORD)(now-last)<100)return;last=now;
 __try{
  Context c;if(!tracked||!ReadContext(Player(),c)||!SameWorld(c,ownerContext))return;
  for(Record* r=records;r;r=r->next){
   void* actor=Resolve(r->handle);if(!Owned(r,actor,c.actor))continue;
   const char* name=At<const char*>(actor,0x148);if(!name||lstrcmpA(name,"Slinky"))continue;
   BYTE* vm=RootVm(Control(actor),actor);if(!vm)continue;
   if(r->movementScript==vm)continue;
   BYTE* header=*(BYTE**)vm;
   if(!header||At<unsigned>(header,0)!=0x474e4c53||At<unsigned>(header,0x18)!=2434||At<unsigned>(header,0x38)!=63||At<void*>(vm,0x50)!=actor)continue;
   BYTE* code=header+0x40+At<unsigned>(header,0x40)-0x4000000;
   if(!Override(true,243,2434,Hash(code,2434*2)))continue;
   BYTE** values=At<BYTE**>(vm,8);
   if(values&&ApplyState(*values))r->movementScript=vm;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}

