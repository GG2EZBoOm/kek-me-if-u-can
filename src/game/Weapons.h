#pragma once
#include "game/GameTypes.h"

// Six two-bit sockets. A loadout is data, never a client-supplied stat multiplier.
enum WeaponSlot { WS_OPTIC, WS_BARREL, WS_MUZZLE, WS_STOCK, WS_MAGAZINE, WS_GRIP, WS_COUNT };
struct WeaponInfo {
  const char* name;
  int magazine, pellets;
  float damage, interval, reload, spreadHip, spreadAim, penalty;
  bool automatic;
};
struct WeaponStats : WeaponInfo {
  float weight = 0, recoil = 1, adsTime = .24f, zoom = 1.25f;
  float shellTime = .55f, soundRadius = 60;
  bool suppressed = false;
};
const WeaponInfo& GetWeapon(int weapon);
int WeaponPart(u16 build, int slot);
int WeaponPartCount(int slot);
const char* WeaponSlotName(int slot);
const char* WeaponPartName(int weapon, int slot, int option);
const char* WeaponPartDescription(int weapon, int slot, int option);
const char* WeaponPartBlock(int weapon, u16 build, int slot, int option);
u16 ValidateWeaponBuild(int weapon, u16 build);
u16 WithWeaponPart(int weapon, u16 build, int slot, int option);
u16 WeaponPreset(int weapon, int preset);
WeaponStats GetWeaponStats(int weapon, u16 build);
float WeaponMuzzleZ(int weapon, u16 build);
float WeaponSightHeight(int weapon, u16 build);
