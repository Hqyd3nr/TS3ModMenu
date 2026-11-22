// Buzz's own laser, the way st_buzzgame (the game-within-a-game) runs it.
//
// Player2.sx starts BuzzStartLaser / LASER_TARGETING and fires buzz_laser only
// while currentTool==2 (gun mode) and currentGun==0 (his primary weapon), and in
// Toy Box its Story/imagination gate is already open (!storyMode || imagination).
// The native "ForceGun" script event selects tool 2 / gun 0 and sets forceTool,
// so Pixar Balls and the tool cycle cannot replace it; "UnForceTool" clears it
// (outside arcade mode it returns to no tool). Both are the script's own
// handlers; no PlayerDriver flags, weapon data, unlocks or save data are changed.
// While it is on, the menu refuses to equip other weapons (Weapons page).
namespace BuzzLaser {
static void* ActionVm(void* actor){void* scripts=actor?At<void*>(actor,0x3c0):0;return scripts?At<void*>(scripts,0x28):0;}
static BYTE* Entry(void* vm,const char* name){
 if(!vm)return 0;int index=((int(*)(void*,const char*))A(0x542310))(*(void**)vm,name);
 if(index<0||index>4096)return 0;void* vars=At<void*>(vm,8);BYTE* entries=vars?*(BYTE**)vars:0;
 return entries?entries+index*16:0;
}
static bool Int(void* vm,const char* name,__int64& out){BYTE* e=Entry(vm,name);if(!e||At<int>(e,0)!=0)return false;out=At<__int64>(e,8);return true;}
static bool Overlap(void* a,void* b){if(!a||!b)return false;for(int i=0;i<6;++i)if(At<DWORD>(a,0x18+i*4)&At<DWORD>(b,0x18+i*4))return true;return false;}
static bool IsBuzz(void* actor){
 __try{BYTE* mask=Entry(ActionVm(actor),"buzzActorType");return actor&&mask&&Overlap(At<void*>(actor,0x408),At<void*>(mask,8));}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// The forced laser selection, read from Buzz's own script variables.
static bool Active(const Context& c){
 if(!c.actor||!IsBuzz(c.actor))return false;
 __try{void* vm=ActionVm(c.actor);__int64 force=0,wantTool=0,wantGun=0;
  if(!Int(vm,"forceTool",force)||!Int(vm,"desiredTool",wantTool)||!Int(vm,"desiredGun",wantGun))return false;
  return force==1&&wantTool==2&&wantGun==0;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Available(const Context& c){
 if(!c.toyBox||!IsBuzz(c.actor))return false;
 __try{__int64 v;void* vm=ActionVm(c.actor);return Int(vm,"forceTool",v)&&Int(vm,"currentGun",v)&&Int(vm,"currentTool",v);}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void Event(const Context& c,const char* name,const char* value){
 ((void(*)(void*,const char*,const char*,void*,unsigned __int64))A(0x49e0b0))(*(void**)A(0xb29a70),name,value,0,c.handle);
}
static void Toggle(const Context& c){
 if(!c.toyBox){Message(TS3CheatNoGame,"Buzz's laser is Toy Box only.");return;}
 if(!IsBuzz(c.actor)){Message(TS3CheatInvalid,"Switch to Buzz to use his laser.");return;}
 if(!Available(c)){Message(TS3CheatFault,"Buzz's laser is unavailable right now.");return;}
 if(Active(c)){Event(c,"UnForceTool","0");Message(TS3CheatOk,"Laser off.");return;}
 Event(c,"ForceGun","0");
 Message(TS3CheatOk,"Laser on. Close the menu, aim and fire.");
}
}
