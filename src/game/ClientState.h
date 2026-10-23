#pragma once
// Was ein Client vom Spiel sieht (aus der Simulation oder aus Netzwerk-Snapshots).
#include "game/GameTypes.h"

struct ClientPlayer {
  bool active = false;
  bool bot = false;
  char name[NAME_LEN] = {};
  Team team = TEAM_SPECTATOR;
  bool alive = false;
  Vec3 pos, vel;
  float yaw = 0, pitch = 0, colYaw = 0;
  bool onGround = true, crouching = false, rotLock = false;
  int propType = -1;
  float health = 0, maxHealth = 100;
  u8 weapon = 0;
  u8 ammo[WPN_COUNT] = {};
  float reloadTimer = 0, switchTimer = 0, fireTimer = 0;
  float tauntTimer = 0, tauntCooldown = 0, autoTaunt = 0;
  float abilityTimer = 0, disguiseTimer = 0, hurtTimer = 0;
  float stamina = 1;
  u16 lastButtons = 0;
  u8 decoys = 0;
  int score = 0, kills = 0, deaths = 0;
  int ping = 0;
  u32 lastCmdSeq = 0;
};

struct ClientDecoy {
  bool active = false;
  int propType = 0;
  Vec3 pos;
  float yaw = 0;
};

struct ClientGrenade {
  bool active = false;
  Vec3 pos;
};

struct ClientState {
  bool valid = false;
  u32 tick = 0;
  Phase phase = Phase::Waiting;
  float phaseTimer = 0;
  int round = 0;
  int maxRounds = 6;
  Team winner = TEAM_SPECTATOR;
  int mapIndex = 0;
  bool infection = false;
  ClientPlayer players[MAX_PLAYERS];
  ClientDecoy decoys[MAX_DECOYS];
  ClientGrenade grenades[MAX_GRENADES];

  int CountAlive(Team t) const {
    int n = 0;
    for (const ClientPlayer& p : players)
      if (p.active && p.alive && p.team == t) n++;
    return n;
  }
  int CountTeam(Team t) const {
    int n = 0;
    for (const ClientPlayer& p : players)
      if (p.active && p.team == t) n++;
    return n;
  }
};
