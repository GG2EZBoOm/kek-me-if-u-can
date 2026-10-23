#pragma once
// Autoritative Spielsimulation (läuft beim Host bzw. dedizierten Server).
#include "game/GameTypes.h"
#include "game/Movement.h"
#include "game/World.h"

struct Player {
  bool active = false;
  bool bot = false;
  char name[NAME_LEN] = {};
  Team team = TEAM_SPECTATOR;
  bool alive = false;
  MoveState ms;
  Vec3 prevPos;
  float prevColYaw = 0;
  int propType = -1;
  float health = 100, maxHealth = 100;
  u8 weapon = WPN_SMG;
  u8 ammo[WPN_COUNT] = {};
  float fireTimer = 0, reloadTimer = 0, switchTimer = 0;
  float abilityTimer = 0;
  u8 decoys = 0;
  float tauntTimer = 0, tauntCooldown = 0, autoTaunt = 45;
  float disguiseTimer = 0;
  float pingTimer = -1;
  float respawnTimer = -1;
  int score = 0, kills = 0, deaths = 0;
  float aliveAcc = 0;
  float modeMeter = 0;
  Vec3 modeAnchor;
  u32 lastCmdSeq = 0;
  u16 prevButtons = 0;
  float hurtTimer = 0;
  int ping = 0;
  bool wasHunterThisRound = false;
  std::vector<UserCmd> pending;
};

struct Decoy {
  bool active = false;
  int owner = -1;
  int propType = 0;
  Vec3 pos;
  float yaw = 0;
  float health = 40;
  float life = 0, pingTimer = -1;
  u32 bornTick = 0;
};

struct Grenade {
  bool active = false;
  int owner = -1;
  Vec3 pos, vel;
  float fuse = 0;
};

struct WeaponInfo {
  const char* name;
  int magazine;
  int pellets;
  float damage;
  float interval;
  float reload;
  float spreadHip, spreadAim;
  float penalty;  // Strafschaden pro Treffer auf ein falsches Objekt
  bool automatic;
};
const WeaponInfo& GetWeapon(int w);

// Wonach ein Prop-Spieler gerade zielt (für Verwandlung)
int FindDisguiseTarget(const World& w, const MoveState& ms, int propType, Vec3* hitPos = nullptr);

class Sim {
 public:
  MatchSettings settings;
  World* world = nullptr;
  Phase phase = Phase::Waiting;
  float phaseTimer = 0;
  int round = 0;
  Team winner = TEAM_SPECTATOR;
  u32 tick = 0;
  GameMode mode = MODE_CLASSIC;
  float modeTimer = 0;
  Player players[MAX_PLAYERS];
  Decoy decoys[MAX_DECOYS];
  Grenade grenades[MAX_GRENADES];
  std::vector<GameEvent> events;

  void Init(World* w, const MatchSettings& s);
  int AddPlayer(const std::string& name, bool bot);
  void RemovePlayer(int id);
  void QueueCmd(int id, const UserCmd& c);
  void Tick();
  void Chat(int id, const std::string& text);
  void RestartMatch();

  int CountAlive(Team t) const;
  int CountActive() const;
  bool Endspurt() const { return phase == Phase::Hunting && phaseTimer <= 60.0f; }
  MoveEnv MakeEnv(int ignorePlayer, std::vector<OBox>& storage) const;
  float AutoTauntInterval() const { return Endspurt() ? 20.0f : 45.0f; }

 private:
  GameEvent& Emit(u8 type);
  void StartRound();
  void EndRound(Team win);
  void SpawnPlayer(int id, Team team, int spawnIndex);
  void ProcessCmd(int id, const UserCmd& cmd);
  void HunterActions(int id, const UserCmd& cmd, u16 pressed);
  void PropActions(int id, const UserCmd& cmd, u16 pressed);
  void FireWeapon(int id);
  void ThrowGrenade(int id);
  void Damage(int victim, int attacker, float dmg, u8 cause, Vec3 pos);
  void Kill(int victim, int attacker, u8 cause);
  void TryDisguise(int id);
  void DoTaunt(int id, int which);
  void PlaceDecoy(int id);
  void UpdateMode();
  void RandomDisguise(int id);
  void SwapClone(int id);
  void SetDisguise(int id, int type, const MoveState& state);
  void UpdateGrenades();
  void Explode(Grenade& g);
  void CheckRoundEnd();

  Rng rng_{12345};
  u32 nextEventId_ = 1;
  int hunterRotation_ = 0;
  float lastCountdown_ = 0;
};
