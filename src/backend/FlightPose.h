// Native animation-channel API traced against the installed Complete Edition executable.
// Clip lookup is checked first: PlayClip clears a channel when a name is absent.
namespace FlightPose {
static const char* flyingNames[]={"HOVER_LOOP","Flight_HoverCycle","FALL","REACT_FALL","REACT_WINGHOVER"};
static void* Channel(void* actor){
 void* animation=At<void*>(actor,0x360);
 void* tree=animation?At<void*>(animation,0x18):0;
 return tree?((void*(*)(void*,int))A(0x340b40))(tree,0):0;
}
static int Find(void* channel,const char* name){
 return ((int(*)(void*,const char*))A(0x33f170))(channel,name);
}
static int Current(void* channel){return ((int(*)(void*))A(0x33fa40))(channel);}
// Plays name unless it is already playing, so its timeline and native events keep running.
static bool Play(void* actor,void* channel,const char* name){
 int clip=Find(channel,name);if(clip<0)return false;
 if(Current(channel)!=clip)((void(*)(void*,const char*,int,int,int))A(0x56bda0))(actor,name,0,0x11,0);
 return true;
}
// The flight pose: the first of flyingNames this character has.
static void Apply(void* actor,bool){
 void* channel=Channel(actor);if(!channel)return;
 for(const char* name:flyingNames)if(Play(actor,channel,name))return;
}
// True for the clips flight plays, including the falling clips used when it ends.
static bool FlightClip(void* channel,int clip){
 if(clip<0)return false;
 for(const char* name:flyingNames)if(Find(channel,name)==clip)return true;
 return false;
}
static void Stand(void* actor,void* channel){
 if(Play(actor,channel,"NORMAL_STANCE")||Play(actor,channel,"STANCE"))return;
 ((void(*)(void*,int))A(0x351270))(channel,0);   // no standing clip: just release the channel
}
static void* Component(void* actor,const char* name,ULONG_PTR vtable){
 void* c=((void*(*)(void*,const char*))A(0x558fb0))(actor,name);
 return c&&At<ULONG_PTR>(c,0)==A(vtable)&&At<void*>(c,8)==actor?c:0;
}
// Seen live (2026-11-25): something during flight can leave the game's VelocityController in charge
// of the character instead of TS3GroundController. Flight holds the speed at zero, so after landing the
// character hangs in mid-air and ignores all input, even after flying again. Hand control back to the
// walking controller with the game's own methods: Deactivate (slot 6) and Activate (slot 5), the same
// Activate call the game makes when Buzz's native flight ends (0x64e80).
static bool RestoreWalking(void* actor){
 void* walking=Component(actor,"TS3GroundController",0x76dda0);
 void* velocity=Component(actor,"VelocityController",0x7d7568);
 if(!walking||!velocity||At<BYTE>(walking,0x14)||!At<BYTE>(velocity,0x14))return false;
 void** vw=*(void***)walking;void** vv=*(void***)velocity;
 if(vv[6]!=(void*)A(0x65380)||vw[5]!=(void*)A(0x42e0e0))return false;   // not the methods traced above
 ((void(*)(void*))vv[6])(velocity);
 ((void(*)(void*))vw[5])(walking);
 return true;
}
}
// After flight ends, hand the character back to the game's own animation. The game's walking code is
// paused during flight, so it still thinks you are where you took off. Watch for a short while:
// falling plays the falling clip until the game lands you, and not moving down plays the standing clip.
// The moment the game picks an animation of its own, it is in charge again and this stops.
namespace FlightLanding {
static void* actor;static unsigned __int64 handle;
static float lastY;static unsigned frames,still;static bool falling;
static void Cancel(){actor=0;handle=0;}
static void Begin(void* a,bool animate){
 Cancel();if(!a)return;
 FlightPose::RestoreWalking(a);
 if(!animate)return;   // riding Bullseye: the horse has no flight pose to undo
 actor=a;handle=At<unsigned __int64>(a,0x38);lastY=At<float>(a,0x84);frames=still=0;falling=false;
}
static void Frame(void* a){
 if(!actor||a!=actor)return;
 if(!handle||Resolve(handle)!=actor){Cancel();return;}
 void* channel=FlightPose::Channel(a);if(!channel){Cancel();return;}
 int clip=FlightPose::Current(channel);
 if(clip>=0&&!FlightPose::FlightClip(channel,clip)){Cancel();return;}   // the game chose its own animation
 float y=At<float>(a,0x84);++frames;
 if(y<lastY-0.005f){falling=true;still=0;}else ++still;
 lastY=y;
 if(frames==3)FlightPose::RestoreWalking(a);   // in case the game switched controllers again on the way down
 if(still>=6||frames>=180){FlightPose::Stand(a,channel);Cancel();return;}   // standing on something, or ~3 s
 if(falling)FlightPose::Play(a,channel,"FALL")||FlightPose::Play(a,channel,"REACT_FALL");
}
}
