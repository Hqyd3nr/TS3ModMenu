#pragma once
#include <stdint.h>
inline constexpr uint32_t TS3MenuApiVersion=9;
// Objects: every carriable model the game ships (assets/characters/carriable),
// minus five without carry motions or with no real model. Order is shared
// with the menu's names/categories.
inline constexpr unsigned TS3ObjectCount=27;
// Toy Box places: native avatar start/drop-in points in the WesternTown hub scene.
inline constexpr unsigned TS3PlaceCount=11;
inline constexpr unsigned TS3VehicleCount=12;
inline bool TS3VehicleIsCar(unsigned i){return i<4||(i>=6&&i<=9)||i==11;}

// Fixed-width POD ABI between dinput8 (game thread) and the in-game renderer.
// Callbacks never expose game pointers or accept memory addresses.
enum TS3CheatAction : uint32_t {
 TS3CheatFlightToggle=1, TS3CheatFlightSpeed=2,
 TS3CheatRememberPosition=3, TS3CheatReturnPosition=4,
 TS3CheatAddCoins=5, TS3CheatAddPixar=6, TS3CheatGodToggle=7, TS3CheatSpawnObject=8, TS3CheatClearObjects=9, TS3CheatSuperSpeed=10, TS3CheatSuperJump=11, TS3CheatEquipWeapon=12,
 TS3CheatSpawnNpc=13, TS3CheatClearNpcs=14, TS3CheatZurgTransform=15, TS3CheatFreezeTimers=16,
 TS3CheatSpawnVehicle=17, TS3CheatWorldSky=18, TS3CheatRunMultiplier=19,
 TS3CheatJumpMultiplier=20, TS3CheatClearPosition=21, TS3CheatClearVehicles=22,
 TS3CheatTeleportPlace=23, TS3CheatBuzzLaser=24, TS3CheatSwitchCharacter=25
};
enum TS3CheatResult : uint32_t {
 TS3CheatNone=0, TS3CheatOk=1, TS3CheatNoGame=2, TS3CheatStale=3,
 TS3CheatBusy=4, TS3CheatInvalid=5, TS3CheatFault=6,
 TS3CheatMaxBalance=7, TS3CheatNoPosition=8, TS3CheatPartial=9
};
struct TS3CheatCommand {
 uint32_t id, contextId, action, amount;
 float value;
};
struct TS3CheatSnapshot {
 uint32_t version, tick, contextId, gameReady, toyBox, currencyReady;
 // currencyPending: 0 = idle, 1 = coins, 2 = Pixar Balls.
 uint32_t flightEnabled, savedPosition, currencyPending, lastCommandId, result;
 int32_t slot, coins, pixar;
 float flightSpeed;
 char status[128];
 uint32_t teleportPending,returnAvailable;
 uint32_t godEnabled,objectsReady,spawnPending;
 uint32_t superSpeed,superJump,movementReady,weaponMask;
 uint32_t npcsReady,npcPending,spawnedNpcs;
 uint32_t spawnedObjects;
 uint32_t zurgReady,zurgActive;
 uint32_t timersReady,timersFrozen;
 uint32_t vehiclesReady,vehiclePending,skyReady,skyPending;
 uint32_t vehicleSummonedMask,vehicleAvailableMask,vehicleSpawnedMask;
 uint32_t spawnedVehicles;
 uint32_t positionMask,returnMask;
 float runMultiplier,jumpMultiplier;
 int32_t activeSky; // -1 unknown; menu sky index from native current-world selection.
 uint32_t placesReady; // main Toy Box hub world, travel idle.
 uint32_t buzzLaserReady,buzzLaserEnabled; // Toy Box Buzz; laser forced as in st_buzzgame.
 uint32_t characterReady; // Toy Box player; Player > Character switch possible.
 int32_t character; // 0 Woody, 1 Buzz, 2 Jessie, -1 other (Zurg) or unknown. Also the command amount.
};
struct TS3CheatMenuApi {
 uint32_t version, bytes;
 void (*SetOpen)(uint32_t open);
 bool (*ReadSnapshot)(TS3CheatSnapshot* out,uint32_t bytes);
 bool (*Submit)(const TS3CheatCommand* command,uint32_t bytes);
};
// SetOpen(1) once per visible frame; expires after 750 ms without refresh.
// SetOpen(2) requests one gameplay snapshot without opening or capturing input.
// SetOpen(0) on close/device loss. ReadSnapshot is a cheap copy, not a scan.
// Submit accepts one outstanding command; observe lastCommandId before another.
// Commands are accepted on the game's actor update thread and are context-bound.
// Region travel is deferred to the native main-loop callback for safe streaming.
typedef const TS3CheatMenuApi* (*TS3GetCheatMenuApiFn)(uint32_t version);


// NPC selection is category in the low byte and a validated variant in the next byte.
// Townsperson variant zero randomises a verified native outfit; 1..8 select one.
inline bool TS3ValidNpcSelection(uint32_t selection){
 const uint32_t counts[]={9,17,1,5,1,3,2,18};
 uint32_t group=selection&255,variant=selection>>8;
 return group<8&&variant<counts[group];
}
