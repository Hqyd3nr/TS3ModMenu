#pragma once
// English names for the session log, so a log says "Spawn object (Breakable crate)" instead of an index.
// The lists follow the menu's own lists in ModMenuUI.cpp and must stay in the same order. Logs are always
// in English, whatever language the menu uses, so anyone helping with a bug report can read them.
namespace LogNames {
static const char* const places[TS3PlaceCount]={"Town","Farm","Suburbs","Bandit hideout","Quarry","Stagecoach","Sid's Haunted House",
 "Enchanted Glen","Zurg's Spaceport","Stunt Park","Skate Park"};
static const char* const objects[TS3ObjectCount]={
 "Medium barrel","Large barrel","Breakable barrel","Large breakable barrel","Wooden crate","Breakable crate","Huge crate","Parcel","TNT barrel",
 "Ball","AA battery","D battery","9-volt battery","Andy's phone","Crushed cube","Huge crushed cube",
 "Small money bag","Money bag","Large money bag","Red stone",
 "Space battery","Space parcel A","Space parcel B",
 "Carrot","Corn","Pumpkin","Tomato"};
// Native vehicle index -> name and variant (the menu groups them into five cards).
static const char* const vehicles[TS3VehicleCount]={"Sports car","Teeny Racer, blue & white","Sports car, monster","Zurg's car","Bullseye","Dragon",
 "Teeny Racer, black","Teeny Racer, blue flames","Teeny Racer, flowers","Teeny Racer, red","Bullseye, Bizarro","Zurg's car, monster"};
static const char* const skies[4]={"Western","Sid's Haunted House","Enchanted Glen","Zurg's Spaceport"};
// Weapon 0 is the character's own default weapon; the menu offers 1..8.
static const char* const weapons[9]={"Default weapon","Lightning Rod","UFO Gun","Fairy Wand","Blaster","Pickaxe","Toolbox","Camera","Pict-O-Matic"};

static const char* const npcGroups[8]={"Townsperson","Alien","Fairy","Bandit","Gargoyle","Zurg robot","Muffin","Character"};
static const char* const town[9]={"random outfit","Pioneer","Miner","City slicker","Farmer","Western man","Western woman","Dance man","Dance woman"};
static const char* const alien[17]={"random outfit","Default","Commando","Ogre","Star Command 1","Star Command 2","Zombie","Mummy","Renegade","Deputy",
 "Cowboy","Blacksmith","Mechanic","Tourist","Zurg fan","Disco","Strawberry"};
static const char* const bandit[5]={"random","Standard","Red outlaw","Green outlaw","Orange outlaw"};
static const char* const robot[3]={"Toy Box robot","Story robot","Captain robot"};
static const char* const muffin[2]={"Normal","Giant"};
static const char* const characters[18]={"Stinky Pete","Wheezy","Bo Peep","Mr. Pricklepants","Zurg","Hamm","Slinky","Rex","Lotso","Big Baby",
 "Buttercup","Chunk","Sarge","Sparks","Stretch","Trixie","Twitch","Dolly Witch"};
// "Alien, Zombie" / "Hamm" / "Fairy". The selection was validated before it gets here.
static void Npc(uint32_t selection,char* out){
 unsigned group=selection&255,variant=selection>>8;
 static const char* const* const lists[8]={town,alien,0,bandit,0,robot,muffin,characters};
 if(group>=8){lstrcpyA(out,"?");return;}
 if(group==7){lstrcpyA(out,characters[variant]);return;}
 if(!lists[group]){lstrcpyA(out,npcGroups[group]);return;}
 wsprintfA(out,"%s, %s",npcGroups[group],lists[group][variant]);
}

// Level names for the world file the game loaded. The keys are the game's own world files; the names come
// from the game's own level list and text. Anything not listed is logged by its file name only.
static void World(const char* file,bool toyBox,char* out){
 static const struct {const char* file;const char* name;} known[]={
  {"st_train","Train Rescue"},{"st_andysroom","Andy's Room"},{"st_andysbasement","Andy's Basement"},{"st_buzzflight","Buzz Flight"},
  {"st_buzzgame","Buzz Video Game"},{"st_daycare","Sunnyside Daycare"},{"st_bonnie_woody","Bonnie's House"},{"st_prisonbreak","Prison Break"},
  {"st_junkyard","Junkyard"},{"st_hauntedbakery","Haunted Bakery"}};
 // The stored value may carry a folder or an extension: keep just the base name, in lower case.
 char base[48];int n=0;
 if(file){const char* p=file;for(const char* q=file;*q;++q)if(*q=='/'||*q=='\\')p=q+1;
  for(;*p&&*p!='.'&&n<47;++p)base[n++]=(*p>='A'&&*p<='Z')?char(*p+32):*p;}
 base[n]=0;
 const char* name=toyBox?"Toy Box":0;
 for(auto& k:known)if(!name&&lstrcmpA(base,k.file)==0)name=k.name;
 if(name&&n)wsprintfA(out,"%s (%s)",name,base);else if(name)lstrcpyA(out,name);else lstrcpyA(out,n?base:"unknown world");
}
}
