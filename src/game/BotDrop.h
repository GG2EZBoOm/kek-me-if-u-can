#pragma once
#include "game/NavGrid.h"
#include "game/Movement.h"

// Directed traversal, deliberately separate from the bidirectional walking graph.
// A roof exit must never make the same roof reachable from below.
struct BotDropPlan {
  std::vector<Vec3> approach;
  Vec3 takeoff, landing;
  float yaw=0, seconds=0;
  bool jump=false;
};

bool SimulateBotDrop(const World& world,const NavGrid& nav,const MoveState& start,
                    GameMode mode,BotDropPlan& plan);
bool FindBotDrop(const World& world,const NavGrid& nav,Vec3 from,Vec3 target,
                 GameMode mode,BotDropPlan& plan);
