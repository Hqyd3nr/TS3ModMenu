// Called only for newly created menu-owned actors. Named characters with an
// empty low model are admitted to a native streaming budget, even nearby.
// A live capture had 42 registered Hamms: 28 unloaded and 14 loaded. Giving
// low/high the same model name did not prevent EntityUnloading notifications.
// Remove only these copies from that budget using native teardown, preserving
// the global budget, original mission actors, scripts and authored audio names.
namespace NpcAppearance {
static bool NeedsFallback(const char* low,const char* high){return low&&high&&!low[0]&&high[0];}
typedef void(*SwapFn)(void*,void*);
typedef void(*UnlinkFn)(void*,void*);
typedef void(*DestroyFn)(void*,unsigned);
static bool Detach(void* lod,void* manager,UnlinkFn unlink,DestroyFn destroy){
 if(!lod)return false;
 void* node=At<void*>(lod,0x30);if(!node)return true;
 if(!manager||!unlink||!destroy)return false;
 // Match LODEntitySwapper's destructor: unlink before destroying the node.
 // Native destruction cancels pending work and releases its callback.
 unlink(manager,node);destroy(node,1);At<void*>(lod,0x30)=0;return true;
}
static void* Lod(void* actor){
 void* owner=At<void*>(actor,0x370);if(!owner)return 0;
 BYTE* bag=(BYTE*)owner+0x18;unsigned count=At<unsigned>(bag,0x58);void** table=At<void**>(bag,0x48);
 if(count>1024||(!table&&count))return 0;
 for(unsigned i=0;i<count;++i){void* node=table[i];unsigned hops=0;
  while(node&&++hops<4096){const char* name=At<const char*>(node,0x10);
   if(name&&lstrcmpA(name,"LODEntitySwapper")==0)return At<void*>(node,0x18);
   node=At<void*>(node,0);
  }
 }
 return 0;
}
static bool Prepare(void* actor){
 void* lod=Lod(actor);if(!lod)return true;
 if(*(void**)lod!=(void*)A(0x806060)||At<void*>(lod,8)!=actor)return false;
 const char* low=At<const char*>(lod,0x18);const char* high=At<const char*>(lod,0x20);
 if(!NeedsFallback(low,high))return true;
 // Native node teardown and model switching. Do not merely rename low LOD.
#include "ModNpcAppearanceGuards.h"
 void* node=At<void*>(lod,0x30);
 if(node){
  if(*(void**)node!=(void*)A(0x7a9620))return false;
  void* callback=At<void*>(node,0x40);
  if(!callback||*(void**)callback!=(void*)A(0x7fd3f0))return false;
  // Restore activation saved by the native streaming callback before release.
  ((void(*)(void*,bool))A(0x5f5670))(callback,false);
  if(!Detach(lod,*(void**)A(0xb2ef20),(UnlinkFn)A(0x5da450),(DestroyFn)A(0x1c7670)))return false;
 }
 ((SwapFn)A(0x66f5f0))(lod,(BYTE*)lod+0x20);
 // Initialize hides Toy Box actors until the budget admits them. Our copies
 // are resident now, so use the native visibility setter to reveal them.
 ((void(*)(void*,bool))A(0x55b850))(actor,false);
 return true;
}
}
