// Included by PublicModMenu.cpp.
// The renderer submits POD commands only. Native routines run in Tick(actor).
#include "../menu/ModMenuApi.h"
namespace CheatMenuBridge {
static volatile LONG openFlag,openTick,queueState,snapshotBusy,probeRequested,probeTick;
static TS3CheatCommand queued;
static TS3CheatSnapshot published;
static DWORD lastPublish,lastCommandId,contextId;
static bool codeChecked,codeReady,inputInitialized;
static volatile LONG travelInput;
struct Context {void* actor;void* worldManager;void* saveData;unsigned __int64 handle;int world,slot,coins,pixar;bool toyBox,currency;};
static Context previous;
static Vec remembered;
static bool hasRemembered;
static int pendingKind=-1,pendingTarget,pendingSlot=-1;
static int pendingLastBalance;
static DWORD pendingLastTick,pendingQuietMs;
static TS3CheatResult result;
static char status[128];
static DWORD statusTick;
static bool StatusVisible(DWORD now){return DWORD(now-statusTick)<4000;}
static void Copy(void* to,const void* from,SIZE_T bytes){
 volatile BYTE* d=(volatile BYTE*)to;const volatile BYTE* s=(const volatile BYTE*)from;
 for(SIZE_T i=0;i<bytes;++i)d[i]=s[i];
}
static bool Open(){return openFlag&&(DWORD)(GetTickCount()-(DWORD)openTick)<750;}
static void SetOpen(uint32_t open){
 if(open==2){InterlockedExchange(&probeTick,(LONG)GetTickCount());InterlockedExchange(&probeRequested,1);return;}
 if(open){InterlockedExchange(&openTick,(LONG)GetTickCount());InterlockedExchange(&openFlag,1);}
 else InterlockedExchange(&openFlag,0);
 MenuInputGate::SetExternalMenu(open!=0||travelInput);
}
static void SetTravelInput(bool active){InterlockedExchange(&travelInput,active?1:0);MenuInputGate::SetExternalMenu(active||Open());}
static bool ReadSnapshot(TS3CheatSnapshot* out,uint32_t bytes){
 if(!out||bytes!=sizeof(TS3CheatSnapshot)||InterlockedCompareExchange(&snapshotBusy,1,0))return false;
 Copy(out,&published,sizeof(published));InterlockedExchange(&snapshotBusy,0);
 if((DWORD)(GetTickCount()-out->tick)>1500){out->gameReady=out->currencyReady=out->savedPosition=out->teleportPending=out->returnAvailable=0;}
 return out->version==TS3MenuApiVersion;
}
static bool Submit(const TS3CheatCommand* command,uint32_t bytes){
 if(!Open()||!command||bytes!=sizeof(TS3CheatCommand)||!command->id||!command->contextId||
    command->action<TS3CheatFlightToggle||command->action>TS3CheatSwitchCharacter)return false;
 if(command->action==TS3CheatFlightSpeed&&!(command->value>=1&&command->value<=200))return false;
 if((command->action==TS3CheatAddCoins||command->action==TS3CheatAddPixar)&&
    (!command->amount||command->amount>0x7fffffffU))return false;
 if(command->action==TS3CheatSpawnObject&&command->amount>=TS3ObjectCount)return false;
 if(command->action==TS3CheatEquipWeapon&&command->amount>=9)return false;
 if(command->action==TS3CheatSpawnNpc&&!TS3ValidNpcSelection(command->amount))return false;
 if(command->action==TS3CheatSpawnVehicle&&command->amount>=TS3VehicleCount)return false;
 if(command->action==TS3CheatWorldSky&&command->amount>=4)return false;
 if(command->action==TS3CheatTeleportPlace&&command->amount>=TS3PlaceCount)return false;
 if((command->action==TS3CheatRememberPosition||command->action==TS3CheatReturnPosition||command->action==TS3CheatClearPosition)&&command->amount>=6)return false;
 if(command->action==TS3CheatRunMultiplier&&!(command->value>=0.5f&&command->value<=5.f))return false;
 if(command->action==TS3CheatJumpMultiplier&&!(command->value>=0.5f&&command->value<=3.f))return false;
 if(InterlockedCompareExchange(&queueState,1,0))return false;
 Copy(&queued,command,sizeof(queued));MemoryBarrier();InterlockedExchange(&queueState,2);return true;
}
static bool Foreground(){DWORD id=0;GetWindowThreadProcessId(GetForegroundWindow(),&id);return id==GetCurrentProcessId();}
static bool Pose(const Vec& v){return v.x>-100000&&v.x<100000&&v.y>-100000&&v.y<100000&&v.z>-100000&&v.z<100000;}
static bool NativeMenuOpen(){
 // The same five native screen stacks used by the existing shop bridge.
 // HUD objects are separate; +38 is visible and +39 enables GUI input.
 BYTE* manager=*(BYTE**)A(0xb2ef10);if(!manager)return true;
 for(int i=0;i<5;++i){BYTE* entry=*(BYTE**)(manager+0x1c8+i*8);if(!entry)continue;
  BYTE* screen=*(BYTE**)(entry+0x70);if(screen&&screen[0x38]&&screen[0x39])return true;
 }
 return false;
}
static bool BackupCurrency(const Context& c,DWORD id,int kind,unsigned int amount,const char* weapon=0){
 // Only on an explicit currency or weapon command, never while hidden or on a timer.
 WCHAR source[MAX_PATH],folder[MAX_PATH],saved[MAX_PATH],readme[MAX_PATH];
 DWORD n=GetEnvironmentVariableW(L"APPDATA",source,MAX_PATH);
 if(!n||n>MAX_PATH-80)return false;
 lstrcatW(source,L"\\Disney\\Toy Story 3 Complete Edition\\savedata\\Saved Game");
 n=GetModuleFileNameW(0,folder,MAX_PATH);if(!n||n>=MAX_PATH)return false;
 int slash=-1;for(unsigned i=0;i<n;++i)if(folder[i]=='\\')slash=(int)i;
 if(slash<0||slash>MAX_PATH-125)return false;folder[slash+1]=0;
 lstrcatW(folder,L"Mod Menu\\Backups");
 if(!CreateDirectoryW(folder,0)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
 SYSTEMTIME time;GetSystemTime(&time);WCHAR suffix[96];
 wsprintfW(suffix,L"\\%04u%02u%02uT%02u%02u%02uZ-slot%u-cheat-change-%lu-%lu",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,c.slot+1,GetTickCount(),id);
 lstrcatW(folder,suffix);if(!CreateDirectoryW(folder,0))return false;
 lstrcpyW(saved,folder);lstrcatW(saved,L"\\Saved Game");lstrcpyW(readme,folder);lstrcatW(readme,L"\\README.txt");
 HANDLE input=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0);
 if(input==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size={};bool ok=GetFileSizeEx(input,&size)&&size.QuadPart>0&&size.QuadPart<64*1024*1024;
 if(ok)ok=CopyFileW(source,saved,TRUE)!=0;
 HANDLE verify=ok?CreateFileW(saved,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0):INVALID_HANDLE_VALUE;
 if(verify==INVALID_HANDLE_VALUE)ok=false;
 static BYTE left[4096],right[4096];LONGLONG remaining=size.QuadPart;
 while(ok&&remaining>0){DWORD ask=remaining>4096?4096:(DWORD)remaining,a=0,b=0;
  ok=ReadFile(input,left,ask,&a,0)&&a==ask&&ReadFile(verify,right,ask,&b,0)&&b==ask;
  if(ok)for(DWORD i=0;i<ask;++i)if(left[i]!=right[i]){ok=false;break;}
  remaining-=ask;
 }
 if(verify!=INVALID_HANDLE_VALUE)CloseHandle(verify);CloseHandle(input);
 if(!ok)return false;
 char text[1024];int length=wsprintfA(text,"Mod Menu save backup, made before adding currency.\r\nSlot: %d\r\nRequested change: add %u %s.\r\nThis is a full copy of your last saved game, including all slots and the shared profile. Anything not yet saved in the game is not included. The game was not forced to save.\r\nTo restore it, close the game, then copy this Saved Game file over the one in %%APPDATA%%\\Disney\\Toy Story 3 Complete Edition\\savedata.\r\nThis backup was checked byte for byte before the currency was added.\r\n",c.slot+1,amount,kind?"Pixar Balls":"coins");
 if(weapon)length=wsprintfA(text,"Mod Menu save backup, made before adding a weapon.\r\nSlot: %d\r\nWeapon: %s\r\nThe weapon is unlocked and equipped without spending currency. Save normally to keep it. This is a full copy of your last saved game, including all slots and the shared profile. Anything not yet saved in the game is not included.\r\nTo restore it, close the game, then copy this Saved Game file over the one in %%APPDATA%%\\Disney\\Toy Story 3 Complete Edition\\savedata.\r\nThis backup was checked byte for byte before the weapon was unlocked.\r\n",c.slot+1,weapon);
 HANDLE note=CreateFileW(readme,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);if(note==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;ok=WriteFile(note,text,length,&written,0)&&written==(DWORD)length;CloseHandle(note);return ok;
}
static bool CheckCode(){
 // Native pickup spawn has a bool fourth argument (r9b), saved at [rsp+20].
 // Match the installed executable and the existing validated viewer award path.
 const BYTE prefix[]={0x44,0x88,0x4c,0x24,0x20,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08};
 __try{for(unsigned i=0;i<sizeof(prefix);++i)if(*(BYTE*)(A(0x1b7af0)+i)!=prefix[i])return false;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return true;
}
static bool ReadContext(void* actor,Context& c){
 c.actor=0;c.worldManager=0;c.saveData=0;c.handle=0;c.world=-1;c.slot=-1;c.coins=c.pixar=-1;c.toyBox=c.currency=false;
 __try{
  if(!actor||actor!=Player())return false;
  unsigned __int64 handle=At<unsigned __int64>(actor,0x38);
  if(!handle||Resolve(handle)!=actor||!At<void*>(actor,0x28)||!Pose(At<Vec>(actor,0x80)))return false;
  BYTE* save=*(BYTE**)A(0xb29a60);void* wm=*(void**)A(0xb2c760);
  if(!save||!*(void**)(save+0x1a0)||!wm)return false;
  int slot=*(int*)(save+0x64),world=At<int>(wm,0x3080);
  if(slot<0||slot>=16||world<0||!At<void*>(wm,0x3088)||NativeMenuOpen())return false;
  c.actor=actor;c.handle=handle;c.slot=slot;c.world=world;c.worldManager=wm;c.saveData=*(void**)(save+0x1a0);
  c.toyBox=RewardWorldIsToyBox(wm,(RewardPropertyFn)A(0x476ef0),(RewardValueFn)A(0x477200));
  BYTE* stats=*(BYTE**)A(0xb2fdb8);
  if(stats){BYTE* records=*(BYTE**)(stats+8);int count=*(int*)(stats+0x10);
   if(records&&count>=247&&count<=512){c.coins=*(int*)(records+47*96+0x4c);c.pixar=*(int*)(records+102*96+0x4c);}}
  BYTE* pickups=*(BYTE**)A(0xb2fda0);
  c.currency=codeReady&&c.toyBox&&c.coins>=0&&c.pixar>=0&&pickups&&*(void**)pickups==(void*)A(0x7a4b70)&&*(void**)(pickups+0x158)&&*(void**)(pickups+0x170);
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void Message(TS3CheatResult r,const char* text){result=r;statusTick=GetTickCount();lstrcpynA(status,text,128);}
static void ExtrasFrame();
#include "ModRegionTeleport.h"
#include "ModPlayerObjects.h"
#include "ModNpcs.h"
#include "ModNpcMovement.h"
#include "ModBuzzLaser.h"
#include "ModMovementWeapons.h"
#include "ModZurg.h"
#include "ModCharacter.h"
#include "ModTimers.h"
#include "ModZurgHud.h"
#include "ModVehiclesWorld.h"
static void ExtrasFrame(){PlayerObjects::Frame();WorldNpcs::Frame();ZurgTransform::Frame();CharacterSwitch::Frame();ZurgTransform::Retired::Frame();VehiclesWorld::Frame();}
static bool UpdatePendingCurrency(bool valid,const Context& c,DWORD now=GetTickCount()){
 DWORD elapsed=now-pendingLastTick;pendingLastTick=now;
 // Pauses, loading and suspended frames do not consume the recovery interval.
 if(!valid||pendingKind<0)return false;
 if(pendingSlot>=0&&c.slot!=pendingSlot){pendingKind=-1;pendingSlot=-1;return false;}
 int balance=pendingKind?c.pixar:c.coins;if(balance<0)return false;
 // Spending while pickups are arriving must not leave an unreachable target.
 if(balance<pendingLastBalance)pendingTarget-=pendingLastBalance-balance;
 if(balance!=pendingLastBalance)pendingQuietMs=0;
 else pendingQuietMs+=elapsed>250?250:elapsed;
 pendingLastBalance=balance;
 bool complete=balance>=pendingTarget;
 bool stalled=!complete&&pendingQuietMs>=15000;
 if(complete||stalled){pendingKind=-1;pendingSlot=-1;pendingQuietMs=0;}
 // A stalled/partial native award is never retried or filled automatically.
 return stalled;
}
static void Execute(const TS3CheatCommand& command,const Context& c){
 if(RegionTeleport::Active()||PlayerObjects::spawnPending||WorldNpcs::pending||ZurgTransform::pending||CharacterSwitch::pending||VehiclesWorld::pending||VehiclesWorld::skyPending){Message(TS3CheatBusy,"Wait for the current action to finish.");return;}
 __try{
  switch(command.action){
  case TS3CheatClearVehicles:VehiclesWorld::Request(c,0xffffffffU,false);break;
  case TS3CheatSpawnVehicle:VehiclesWorld::Request(c,command.amount,false);break;
  case TS3CheatWorldSky:VehiclesWorld::Request(c,command.amount,true);break;
  case TS3CheatRunMultiplier:MovementWeapons::SetMultiplier(c,true,command.value);break;
  case TS3CheatJumpMultiplier:MovementWeapons::SetMultiplier(c,false,command.value);break;
  case TS3CheatClearPosition:RegionTeleport::ClearSlot(command.amount);break;
  case TS3CheatFreezeTimers:ChallengeTimers::Toggle();break;
  case TS3CheatZurgTransform:ZurgTransform::Request(c);break;
  case TS3CheatSpawnNpc:WorldNpcs::Request(c,command.amount);break;
  case TS3CheatClearNpcs:WorldNpcs::Request(c,0xffffffffU);break;
  case TS3CheatSuperSpeed:MovementWeapons::Toggle(c,true);break;
  case TS3CheatSuperJump:MovementWeapons::Toggle(c,false);break;
  case TS3CheatEquipWeapon:
   // Buzz's forced laser and the Zurg transformation each own the weapon slot.
   {const bool owned=(MovementWeapons::OwnedMask(c)&(1u<<command.amount))!=0;
    if(BuzzLaser::Active(c)){Message(TS3CheatInvalid,owned?"Disable Buzz's laser to equip first.":"Disable Buzz's laser to add and equip first.");break;}
    if(ZurgTransform::Active(c)){Message(TS3CheatInvalid,owned?"Transform back to equip first.":"Transform back to add and equip first.");break;}}
   MovementWeapons::Equip(c,command.amount,command.id);break;
  case TS3CheatBuzzLaser:BuzzLaser::Toggle(c);break;
  case TS3CheatSwitchCharacter:CharacterSwitch::Request(c,command.amount);break;
  case TS3CheatClearObjects:PlayerObjects::Request(c,TS3ObjectCount);break;
  case TS3CheatGodToggle:PlayerObjects::Toggle(c);break;
  case TS3CheatSpawnObject:PlayerObjects::Request(c,command.amount);break;
  case TS3CheatFlightToggle:
   if(enabled)Stop("Flight OFF: cheat menu");else Start(c.actor);
   Message(TS3CheatOk,enabled?"Flight / noclip enabled.":"Flight / noclip disabled.");break;
  case TS3CheatFlightSpeed:
   if(!enabled){Message(TS3CheatInvalid,"Enable flight to adjust its speed.");break;}
   if(!(command.value>=1&&command.value<=200)){Message(TS3CheatInvalid,"Choose a flight speed from 1 to 200.");break;}
   speed=command.value;Message(TS3CheatOk,"Flight speed updated for this session.");break;
  case TS3CheatRememberPosition:
   RegionTeleport::Remember(c,command.amount);break;
  case TS3CheatReturnPosition:
   RegionTeleport::ReturnSlot(c,command.amount);break;
  case TS3CheatTeleportPlace:
   RegionTeleport::GoPlace(c,command.amount);break;
  case TS3CheatAddCoins:case TS3CheatAddPixar:{
   if(!c.currency){Message(TS3CheatNoGame,"Currency cheats are available during Toy Box gameplay.");break;}
   if(pendingKind>=0){Message(TS3CheatBusy,"Collect the previous currency pickup first.");break;}
   int kind=command.action==TS3CheatAddCoins?0:1,balance=kind?c.pixar:c.coins;
   unsigned int room=0x7fffffffU-(unsigned int)balance,amount=command.amount<room?command.amount:room;
   if(!amount){Message(TS3CheatMaxBalance,"This currency is already at the game limit.");break;}
   if(!command.amount||command.amount>0x7fffffffU){Message(TS3CheatInvalid,"Invalid amount.");break;}
   if(!BackupCurrency(c,command.id,kind,amount)){Message(TS3CheatFault,"Save backup could not be verified. Currency was not changed.");break;}
   Vec at=At<Vec>(c.actor,0x80);at.y+=0.25f;Vec second=at;
   typedef bool(*Spawn)(void*,int,int,bool,const Vec*,const Vec*,bool,bool,const char*,bool);
   pendingKind=kind;pendingTarget=balance+(int)amount;pendingSlot=c.slot;
   pendingLastBalance=balance;pendingLastTick=GetTickCount();pendingQuietMs=0;
   bool ok=((Spawn)A(0x1b7af0))(*(void**)A(0xb2fda0),kind,(int)amount,false,&at,&second,false,false,0,false);
   Message(ok?TS3CheatOk:TS3CheatPartial,ok?"Currency added. Collect the pickup, then save normally.":"Currency may be partly added. Collect it before trying again.");break;
  }
  default:Message(TS3CheatInvalid,"Unsupported command.");break;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){Message(TS3CheatFault,"Game state changed; command stopped. Do not repeat a partial currency award.");}
}
static void Tick(void* actor){
 // Tab requests a single game-thread validation without opening or blocking input.
 if(InterlockedExchange(&probeRequested,0)&&DWORD(GetTickCount()-(DWORD)probeTick)<750){
  Context probeContext;bool valid=ReadContext(actor,probeContext);
  TS3CheatSnapshot probe;for(unsigned i=0;i<sizeof(probe);++i)((volatile BYTE*)&probe)[i]=0;ReadSnapshot(&probe,sizeof(probe));probe.version=TS3MenuApiVersion;probe.tick=GetTickCount();probe.gameReady=valid;
  probe.toyBox=valid&&probeContext.toyBox;probe.currencyReady=valid&&probeContext.currency;probe.coins=probeContext.coins;probe.pixar=probeContext.pixar;probe.movementReady=MovementWeapons::ready;
  probe.slot=valid?probeContext.slot:-1;
  probe.runMultiplier=MovementWeapons::fast?MovementWeapons::runMultiplier:1.f;probe.jumpMultiplier=MovementWeapons::jump?MovementWeapons::jumpMultiplier:1.f;
  probe.vehiclePending=VehiclesWorld::pending;probe.skyPending=VehiclesWorld::skyPending;
  probe.vehicleSummonedMask=probe.vehicleAvailableMask=probe.vehicleSpawnedMask=0;
  if(valid)VehiclesWorld::VehicleState(probeContext,probe.vehicleSummonedMask,probe.vehicleAvailableMask,probe.vehicleSpawnedMask);
  probe.spawnedVehicles=valid?VehiclesWorld::CountOwned():0;
  probe.timersReady=ChallengeTimers::ready;probe.timersFrozen=ChallengeTimers::frozen;
  probe.buzzLaserReady=valid&&BuzzLaser::Available(probeContext);probe.buzzLaserEnabled=valid&&BuzzLaser::Active(probeContext);
  probe.zurgReady=valid&&ZurgTransform::Available(probeContext);probe.zurgActive=valid&&ZurgTransform::Active(probeContext);
  probe.characterReady=valid&&CharacterSwitch::Available(probeContext);probe.character=valid?CharacterSwitch::Current(probeContext):-1;
  probe.flightEnabled=enabled;probe.flightSpeed=speed;probe.superSpeed=MovementWeapons::fast;probe.superJump=MovementWeapons::jump;probe.godEnabled=PlayerObjects::god;
  if(!InterlockedCompareExchange(&snapshotBusy,1,0)){Copy(&published,&probe,sizeof(probe));InterlockedExchange(&snapshotBusy,0);}
  else InterlockedExchange(&probeRequested,1);
 }

 // One cheap lease/queue check while closed; no files, threads or world scans.
 if(!Open()&&queueState!=2&&pendingKind<0&&!PlayerObjects::god&&!PlayerObjects::spawnPending&&!WorldNpcs::pending&&!ChallengeTimers::frozen)return;
 DWORD now=GetTickCount();if(queueState!=2&&(DWORD)(now-lastPublish)<100)return;
 if(!module)return;
 if(Open()&&!inputInitialized){MenuInputGate::InitializeExternal();inputInitialized=true;}
 if(!codeChecked){codeReady=CheckCode();RegionTeleport::Install();PlayerObjects::Install();MovementWeapons::Install();ZurgTransform::Install();ChallengeTimers::Install();ZurgHud::Install();VehiclesWorld::Install();codeChecked=true;}
 Context c;bool valid=ReadContext(actor,c);RegionTeleport::Observe(c,valid);PlayerObjects::Protect(c,valid);PlayerObjects::Observe(c,valid);WorldNpcs::Observe(c,valid);if(valid){ZurgTransform::Observe(c);CharacterSwitch::Observe(c);}
 ChallengeTimers::Observe(c,valid);
 bool same=valid&&c.actor==previous.actor&&c.handle==previous.handle&&c.world==previous.world&&c.slot==previous.slot&&c.worldManager==previous.worldManager&&c.saveData==previous.saveData;
 if(!same){++contextId;if(!contextId)++contextId;Copy(&previous,&c,sizeof(c));}
 if(UpdatePendingCurrency(valid,c))Message(TS3CheatPartial,"Award not fully confirmed. Adding is available again; no extra currency was granted.");
 if(queueState==2){
  TS3CheatCommand command;Copy(&command,&queued,sizeof(command));
  if(command.id<=lastCommandId)Message(TS3CheatStale,"This command has already been handled.");
  else{
   lastCommandId=command.id;
   if(!valid||!Foreground())Message(TS3CheatNoGame,"Return to gameplay before using this cheat.");
   else if(!Open()||command.contextId!=contextId)Message(TS3CheatStale,"The active player or area changed. Choose the command again.");
   else Execute(command,c);
  }
  InterlockedExchange(&queueState,0);
 }
 TS3CheatSnapshot snap;for(unsigned i=0;i<sizeof(snap);++i)((volatile BYTE*)&snap)[i]=0;
 snap.timersReady=ChallengeTimers::ready;snap.timersFrozen=ChallengeTimers::frozen;
 snap.zurgReady=valid&&ZurgTransform::Available(c);snap.zurgActive=valid&&ZurgTransform::Active(c);
 snap.characterReady=valid&&CharacterSwitch::Available(c);snap.character=valid?CharacterSwitch::Current(c):-1;
 snap.version=TS3MenuApiVersion;snap.tick=now;snap.contextId=contextId;
 // Gameplay presence and action availability are separate: spawning does not
 // unload the level. The UI gates actions using the individual pending flags.
 snap.gameReady=valid;
 snap.npcsReady=PlayerObjects::ready&&RegionTeleport::hookReady;snap.npcPending=WorldNpcs::pending;snap.spawnedNpcs=valid?WorldNpcs::Count():0;
 snap.superSpeed=MovementWeapons::fast;snap.superJump=MovementWeapons::jump;snap.movementReady=MovementWeapons::ready;snap.weaponMask=valid?MovementWeapons::OwnedMask(c):0;
 snap.spawnedObjects=valid?PlayerObjects::Count():0;
 snap.godEnabled=PlayerObjects::god;snap.objectsReady=PlayerObjects::ready&&RegionTeleport::hookReady;snap.spawnPending=PlayerObjects::spawnPending;snap.toyBox=valid&&c.toyBox;snap.currencyReady=valid&&c.currency;
 snap.flightEnabled=enabled;snap.flightSpeed=speed;snap.savedPosition=valid&&hasRemembered;snap.currencyPending=pendingKind>=0?pendingKind+1:0;
 snap.slot=valid?c.slot:-1;snap.coins=valid?c.coins:-1;snap.pixar=valid?c.pixar:-1;
 // Character switch and Zurg transform complete on the next world frame; Execute already refuses
 // overlapping commands, so they are not shown as pending (that disabled every control for a frame).
 snap.teleportPending=RegionTeleport::Active();snap.returnAvailable=valid&&RegionTeleport::ReturnAvailable(c);
 snap.vehiclesReady=valid&&VehiclesWorld::ready&&RegionTeleport::hookReady;snap.vehiclePending=VehiclesWorld::pending;
 if(valid)VehiclesWorld::VehicleState(c,snap.vehicleSummonedMask,snap.vehicleAvailableMask,snap.vehicleSpawnedMask);
 snap.spawnedVehicles=valid?VehiclesWorld::CountOwned():0;
 snap.skyReady=valid&&VehiclesWorld::SkyAvailable(c);snap.skyPending=VehiclesWorld::skyPending;snap.activeSky=valid?VehiclesWorld::ActiveSky(c):-1;
 snap.placesReady=valid&&RegionTeleport::PlacesAvailable(c);snap.buzzLaserReady=valid&&BuzzLaser::Available(c);snap.buzzLaserEnabled=valid&&BuzzLaser::Active(c);snap.positionMask=valid?RegionTeleport::SlotMask(c,false):0;snap.returnMask=valid?RegionTeleport::SlotMask(c,true):0;
 snap.runMultiplier=MovementWeapons::fast?MovementWeapons::runMultiplier:1.f;snap.jumpMultiplier=MovementWeapons::jump?MovementWeapons::jumpMultiplier:1.f;
 snap.lastCommandId=lastCommandId;snap.result=result;if(StatusVisible(GetTickCount()))lstrcpynA(snap.status,status,128);
 if(!InterlockedCompareExchange(&snapshotBusy,1,0)){Copy(&published,&snap,sizeof(snap));InterlockedExchange(&snapshotBusy,0);lastPublish=now;}
}
static const TS3CheatMenuApi api={TS3MenuApiVersion,sizeof(TS3CheatMenuApi),SetOpen,ReadSnapshot,Submit};
}
extern "C" __declspec(dllexport) const TS3CheatMenuApi* GetCheatMenuApi(uint32_t version){return version==TS3MenuApiVersion?&CheatMenuBridge::api:0;}

