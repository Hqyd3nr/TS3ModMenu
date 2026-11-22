// Positive identification from the same current-world metadata used by the game.
// A false Story flag alone is insufficient: frontends and unknown worlds must deny.
typedef void* (*RewardPropertyFn)(void*,int,const char*);
typedef const char* (*RewardValueFn)(void*,int);
static bool RewardTextEqual(const char* a,const char* b){
 if(!a||!b)return false;
 while(*a&&*a==*b){++a;++b;}
 return *a==*b;
}
static const char* RewardWorldProperty(void* database,int world,const char* name,RewardPropertyFn property,RewardValueFn value){
 void* record=property(database,world,name);
 return record?value(record,0):0;
}
static bool RewardWorldIsToyBox(void* manager,RewardPropertyFn property,RewardValueFn value){
 if(!manager||!property||!value)return false;
 int world=*(int*)((unsigned char*)manager+0x3080);
 void* database=*(void**)((unsigned char*)manager+0x3088);
 if(world<0||!database)return false;
 const char* hud=RewardWorldProperty(database,world,"hud",property,value);
 if(!RewardTextEqual(hud,"ToyBoxHud"))return false;
 const char* story=RewardWorldProperty(database,world,"storyMode",property,value);
 // Some debug worlds use ToyBoxHud with storyMode=true. Deny them too.
 return !story||RewardTextEqual(story,"false");
}
