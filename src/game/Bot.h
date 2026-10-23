#pragma once
// KI-Mitspieler für beide Teams.
#include "game/NavGrid.h"
#include "game/Sim.h"

class BotManager {
 public:
  void Init(const World& w);
  // Erzeugt pro Tick die Befehle der Bots (vor Sim::Tick aufrufen)
  void Think(Sim& sim);
  // Ereignisse nach Sim::Tick weiterreichen (Geräusche, Treffer)
  void OnEvents(const Sim& sim, const std::vector<GameEvent>& evs);
  const NavGrid& Nav() const { return nav_; }
  static std::string RandomName(Rng& rng, const Sim& sim);
  int CountHiddenProps(const Sim& sim) const;

 private:
  struct Sound {
    Vec3 pos;
    float time;
  };
  struct Brain {
    int round = -1;
    Team team = TEAM_SPECTATOR;
    u32 seq = 0;
    float yaw = 0, pitch = 0;
    float errYaw = 0, errPitch = 0;
    std::vector<Vec3> path;
    size_t pathIdx = 0;
    Vec3 goal;
    bool hasGoal = false;
    float repath = 0;
    Vec3 lastProgressPos;
    float progressTimer = 0;
    int stuck = 0;
    bool wantJump = false;
    u16 lastButtons = 0;
    // Prop
    int state = 0;
    int disguiseTarget = -1;
    Vec3 hideSpot;
    float hideYaw = 0;
    float stateTimer = 0;
    float lastHealth = 0;
    bool decoyPlaced = false;
    float lookTimer = 0;
    // Jäger
    int target = -1;
    float targetSeen = 0;
    float reaction = 0;
    float suspicion[MAX_PLAYERS] = {};
    Vec3 investigate;
    bool hasInvestigate = false;
    float investigateTimer = 0;
    float perceptTimer = 0;
    float sonarTimer = 10;
    int testProp = -1;
    float testTimer = 0;
    int testShots = 0;
    std::vector<int> cleared;
    u8 weaponWant = WPN_SMG;
  };

  void ThinkProp(Sim& sim, int id, Brain& b, UserCmd& cmd);
  void ThinkHunter(Sim& sim, int id, Brain& b, UserCmd& cmd);
  // Folgt dem Pfad; gibt gewünschte Laufrichtung (XZ) zurück, Länge 0 = angekommen
  Vec3 FollowPath(const Player& p, Brain& b);
  bool SetGoal(const Player& p, Brain& b, Vec3 goal);
  void Steer(const Player& p, Brain& b, Vec3 moveDir, bool faceMove, UserCmd& cmd, bool sprint);
  void AimAt(Brain& b, Vec3 from, Vec3 to, float rate, float dt);
  int SameTypeNearby(int type, Vec3 pos, float radius, int ignoreProp = -1) const;
  float Skill(const Player& p) const;

  const World* world_ = nullptr;
  NavGrid nav_;
  Rng rng_{777};
  Brain brains_[MAX_PLAYERS];
  std::vector<Sound> sounds_;
  float time_ = 0;
  int skill_ = 1;
};
