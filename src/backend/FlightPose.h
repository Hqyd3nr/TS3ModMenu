// Native animation-channel API traced against the installed Complete Edition executable.
// Clip lookup is checked first: PlayClip clears a channel when a name is absent.
namespace FlightPose {
static void* Channel(void* actor){
 void* animation=At<void*>(actor,0x360);
 void* tree=animation?At<void*>(animation,0x18):0;
 return tree?((void*(*)(void*,int))A(0x340b40))(tree,0):0;
}
static int Find(void* channel,const char* name){
 return ((int(*)(void*,const char*))A(0x33f170))(channel,name);
}
static void Apply(void* actor,bool flying){
 void* channel=Channel(actor);if(!channel)return;
 const char* flyingNames[]={"HOVER_LOOP","Flight_HoverCycle","FALL","REACT_FALL","REACT_WINGHOVER"};
 const char* normalNames[]={"FALL","REACT_FALL","NORMAL_STANCE","STANCE"};
 const char** names=flying?flyingNames:normalNames;int count=flying?5:4;
 for(int i=0;i<count;++i){
  int clip=Find(channel,names[i]);if(clip<0)continue;
  // Do not restart the animation every frame; let its timeline and native events run.
  if(((int(*)(void*))A(0x33fa40))(channel)!=clip)
   ((void(*)(void*,const char*,int,int,int))A(0x56bda0))(actor,names[i],0,0x11,0);
  return;
 }
 // Characters without a normal/fall clip still release the flight animation.
 if(!flying)((void(*)(void*,int))A(0x351270))(channel,0);
}
}
