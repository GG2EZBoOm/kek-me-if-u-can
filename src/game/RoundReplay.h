#pragma once
#include "game/GameTypes.h"
#include <deque>

class Sim;
struct ReplayActor {
  Team team = TEAM_SPECTATOR; // Only living actors are recorded.
  Vec3 pos;
  float yaw=0, pitch=0, colYaw=0;
  int type=-1;
  u8 weapon=0;
  bool crouch=false;
  u32 shots=0;
};
struct ReplayProp { Vec3 pos; float yaw; int type; };
struct ReplayScene {
  ReplayActor actors[MAX_PLAYERS];
  std::vector<ReplayProp> decoys;
};
struct ReplayView {
  ReplayScene scene;
  int hunter=-1, prop=-1;
  float distance=0, elapsed=0;
};
constexpr float REPLAY_SECONDS=6.f;
ReplayActor BlendReplayActor(const ReplayActor& a,const ReplayActor& b,float t);
class RoundReplay {
 public:
  void Reset();
  void ForgetRecent() { recent_.clear(); }
  void Record(const Sim& sim);
  ReplayView At(float elapsed) const;
 private:
  std::deque<ReplayScene> recent_;
  std::vector<ReplayScene> best_;
  float distance_=4.f;
  int hunter_=-1,prop_=-1;
};
