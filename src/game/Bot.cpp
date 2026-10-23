#include "game/Bot.h"
#include "game/Taunts.h"

void BotManager::Init(const World& w) {
  world_ = &w;
  nav_.Build(w);
  for (auto& b : brains_) b = Brain();
  sounds_.clear();
}

std::string BotManager::RandomName(Rng& rng, const Sim& sim) {
  static const char* names[] = {"Klaus",  "Heike",   "Jürgen", "Sabine",     "Dieter",   "Ute",   "Gisela",
                                "Kevin",  "Chantal", "Ronny",  "Mandy",      "Uwe",      "Petra", "Detlef",
                                "Birgit", "Torsten", "Jacqueline", "Waltraud", "Bernd",   "Elfriede"};
  for (int tries = 0; tries < 50; tries++) {
    std::string n = std::string("Bot ") + names[rng.Int(0, COUNTOF(names) - 1)];
    bool used = false;
    for (const Player& p : sim.players)
      if (p.active && n == p.name) used = true;
    if (!used) return n;
  }
  return StrFormat("Bot %d", rng.Int(10, 99));
}

int BotManager::SameTypeNearby(int type, Vec3 pos, float radius, int ignoreProp) const {
  int n = 0;
  float r2 = radius * radius;
  for (int i = 0; i < (int)world_->props.size(); i++) {
    if (i == ignoreProp) continue;
    const WorldProp& wp = world_->props[i];
    if (wp.type == type && LengthSq(wp.pos - pos) < r2) n++;
  }
  return n;
}

float BotManager::Skill(const Player&) const { return (float)skill_; }

bool BotManager::SetGoal(const Player& p, Brain& b, Vec3 goal) {
  b.goal = goal;
  b.hasGoal = nav_.FindPath(p.ms.pos, goal, b.path);
  b.pathIdx = b.path.size() > 1 ? 1 : 0;
  b.progressTimer = 0;
  b.lastProgressPos = p.ms.pos;
  b.stuck = 0;
  if (!b.hasGoal) b.path.clear();
  return b.hasGoal;
}

Vec3 BotManager::FollowPath(const Player& p, Brain& b) {
  Vec3 pos = p.ms.pos;
  while (b.pathIdx < b.path.size()) {
    Vec3 wp = b.path[b.pathIdx];
    float d = sqrtf(Sq(wp.x - pos.x) + Sq(wp.z - pos.z));
    bool last = b.pathIdx + 1 == b.path.size();
    if (d < (last ? 0.3f : 0.45f) && fabsf(wp.y - pos.y) < 1.2f) {
      b.pathIdx++;
      continue;
    }
    if (wp.y - pos.y > 0.7f && d < 1.2f) b.wantJump = true;
    return Normalize(Vec3(wp.x - pos.x, 0, wp.z - pos.z));
  }
  return Vec3();
}

void BotManager::Steer(const Player& p, Brain& b, Vec3 moveDir, bool faceMove, UserCmd& cmd, bool sprint) {
  if (Length(moveDir) < 0.01f) {
    b.progressTimer = 0;
    b.lastProgressPos = p.ms.pos;
    return;
  }
  if (faceMove) {
    float ty = atan2f(-moveDir.x, -moveDir.z);
    b.yaw = LerpAngle(b.yaw, ty, 0.22f);
  }
  Vec3 fwd(-sinf(b.yaw), 0, -cosf(b.yaw)), right(cosf(b.yaw), 0, -sinf(b.yaw));
  float f = Dot(moveDir, fwd), r = Dot(moveDir, right);
  if (f > 0.3f) cmd.buttons |= BTN_FORWARD;
  else if (f < -0.3f) cmd.buttons |= BTN_BACK;
  if (r > 0.3f) cmd.buttons |= BTN_RIGHT;
  else if (r < -0.3f) cmd.buttons |= BTN_LEFT;
  if (sprint && f > 0.5f) cmd.buttons |= BTN_SPRINT;
  b.progressTimer += TICK_DT;
  if (b.progressTimer >= 1.0f) {
    float moved = DistanceXZ(p.ms.pos, b.lastProgressPos);
    if (moved < 0.35f) {
      b.stuck++;
      b.wantJump = true;
      if (b.stuck >= 3) {
        b.path.clear();
        b.hasGoal = false;
        b.stuck = 0;
      }
    } else {
      b.stuck = 0;
    }
    b.progressTimer = 0;
    b.lastProgressPos = p.ms.pos;
  }
  if (b.wantJump) {
    if (!(b.lastButtons & BTN_JUMP)) cmd.buttons |= BTN_JUMP;
    b.wantJump = false;
  }
}

void BotManager::AimAt(Brain& b, Vec3 from, Vec3 to, float rate, float dt) {
  Vec3 d = Normalize(to - from);
  float ty = YawFromDir(d), tp = asinf(Clamp(d.y, -1.0f, 1.0f));
  float dy = AngleDiff(b.yaw, ty);
  b.yaw = WrapAngle(b.yaw + Clamp(dy, -rate * dt, rate * dt));
  b.pitch += Clamp(tp - b.pitch, -rate * dt, rate * dt);
}

void BotManager::Think(Sim& sim) {
  time_ += TICK_DT;
  skill_ = ClampI(sim.settings.botSkill, 0, 2);
  for (int id = 0; id < MAX_PLAYERS; id++) {
    Player& p = sim.players[id];
    if (!p.active || !p.bot) continue;
    Brain& b = brains_[id];
    if (b.round != sim.round || b.team != p.team) {
      u32 seq = b.seq;
      b = Brain();
      b.seq = seq;
      b.round = sim.round;
      b.team = p.team;
      b.yaw = p.ms.yaw;
      b.lastHealth = p.health;
      b.sonarTimer = rng_.Range(5, 20);
    }
    UserCmd cmd;
    cmd.seq = ++b.seq;
    cmd.weapon = p.weapon;
    if (p.alive && (sim.phase == Phase::Hiding || sim.phase == Phase::Hunting)) {
      if (p.team == TEAM_PROPS) ThinkProp(sim, id, b, cmd);
      else if (p.team == TEAM_HUNTERS) ThinkHunter(sim, id, b, cmd);
    }
    cmd.yaw = WrapAngle(b.yaw + b.errYaw);
    cmd.pitch = Clamp(b.pitch + b.errPitch, -1.5f, 1.5f);
    b.lastButtons = cmd.buttons;
    sim.QueueCmd(id, cmd);
  }
  while (!sounds_.empty() && time_ - sounds_.front().time > 10.0f) sounds_.erase(sounds_.begin());
}

void BotManager::ThinkProp(Sim& sim, int id, Brain& b, UserCmd& cmd) {
  Player& p = sim.players[id];
  const World& w = *world_;
  auto press = [&](u16 btn) { if (!(b.lastButtons & btn)) cmd.buttons |= btn; };
  b.stateTimer += TICK_DT;
  float nearH = 1e9f;
  int nearHid = -1;
  for (int j = 0; j < MAX_PLAYERS; j++) {
    const Player& q = sim.players[j];
    if (!q.active || !q.alive || q.team != TEAM_HUNTERS) continue;
    float d = Distance(q.ms.pos, p.ms.pos);
    if (d < nearH) { nearH = d; nearHid = j; }
  }
  bool damaged = p.health < b.lastHealth - 0.5f;
  b.lastHealth = p.health;
  auto startFlee = [&]() {
    Vec3 hunterPos = nearHid >= 0 ? sim.players[nearHid].ms.pos : p.ms.pos;
    for (int t = 0; t < 12; t++) {
      int n = nav_.RandomNodeNear(rng_, p.ms.pos, 30.0f);
      if (n < 0) continue;
      Vec3 np = nav_.NodePos(n);
      if (Distance(np, hunterPos) < 12.0f) continue;
      if (SetGoal(p, b, np)) {
        b.state = 6;
        b.stateTimer = 0;
        return;
      }
    }
  };
  if (damaged && b.state != 6 && sim.phase == Phase::Hunting) startFlee();
  if (sim.phase == Phase::Hunting && sim.mode == MODE_HOT && b.state != 6) startFlee();
  if (sim.phase == Phase::Hunting && sim.mode == MODE_ROULETTE && b.state < 3) { b.state = 3; b.stateTimer = 0; }
  if (sim.phase == Phase::Hunting && sim.mode == MODE_CLONES) {
    int clones = 0;
    for (const Decoy& d : sim.decoys) if (d.active && d.owner == id) clones++;
    if (clones < 3 && p.decoys && rng_.Chance(.3f * TICK_DT)) press(BTN_DECOY);
    if (clones && nearH < 5 && p.abilityTimer <= 0) press(BTN_ABILITY);
  }

  switch (b.state) {
    case 0: {  // Tarnung auswählen
      int best = -1;
      float bestScore = -1e9f;
      float maxDist = sim.phase == Phase::Hiding ? std::max(8.0f, sim.phaseTimer * 3.2f) : 22.0f;
      for (int k = 0; k < 70; k++) {
        int i = rng_.Int(0, (int)w.props.size() - 1);
        const WorldProp& wp = w.props[i];
        const PropDef& d = props::Get(wp.type);
        if (!d.disguisable || d.volume > 1.6f || d.volume < 0.004f) continue;
        if (std::max(d.colHalf.x, d.colHalf.z) > 0.75f) continue;  // zu sperrig für enge Gänge
        float dist = Distance(wp.pos, p.ms.pos);
        if (dist > maxDist) continue;
        int node = nav_.Nearest(wp.pos, 1.6f);
        if (node < 0 || fabsf(nav_.NodePos(node).y - wp.pos.y) > 1.0f) continue;  // auch Dinge auf Tischen
        int same = SameTypeNearby(wp.type, wp.pos, 7.0f, i);
        if (same == 0 && skill_ > 0) continue;  // braucht mindestens ein Gegenstück zum Untertauchen
        float score = std::min(same, 3) * 1.2f + rng_.Range(0.0f, 4.0f) - fabsf(dist - 18.0f) / 12.0f;
        if (skill_ == 0) score = rng_.Range(0.0f, 5.0f);
        // Verteilen: nicht alle Props im selben Raum oder als dasselbe Objekt
        for (int j = 0; j < MAX_PLAYERS; j++) {
          const Player& q = sim.players[j];
          if (j == id || !q.active || !q.alive || q.team != TEAM_PROPS) continue;
          const Brain& ob = brains_[j];
          Vec3 other = (ob.state >= 1 && ob.disguiseTarget >= 0) ? w.props[ob.disguiseTarget].pos : q.ms.pos;
          if (Distance(other, wp.pos) < 10.0f) score -= 3.5f;
          int otherType = ob.disguiseTarget >= 0 ? w.props[ob.disguiseTarget].type : q.propType;
          if (otherType == wp.type) score -= 1.2f;
        }
        if (score > bestScore) { bestScore = score; best = i; }
      }
      if (best >= 0 && SetGoal(p, b, w.props[best].pos)) {
        b.disguiseTarget = best;
        b.state = 1;
        b.stateTimer = 0;
      }
      break;
    }
    case 1: {  // zur Tarnung laufen
      const WorldProp& wp = w.props[b.disguiseTarget];
      if (DistanceXZ(p.ms.pos, wp.pos) < 2.0f && fabsf(wp.pos.y - p.ms.pos.y) < 1.5f) {
        b.state = 2;
        b.stateTimer = 0;
        break;
      }
      if (p.ms.rotLock) press(BTN_LOCK);
      Vec3 dir = FollowPath(p, b);
      if (Length(dir) < 0.01f && !SetGoal(p, b, wp.pos)) b.state = 0;
      Steer(p, b, dir, true, cmd, sim.phase == Phase::Hunting || sim.phaseTimer < 15.0f);
      b.pitch = Lerp(b.pitch, -0.2f, 0.1f);
      if (b.stateTimer > 30.0f) b.state = 0;
      break;
    }
    case 2: {  // verwandeln
      const WorldProp& wp = w.props[b.disguiseTarget];
      const PropDef& d = props::Get(wp.type);
      Vec3 c = wp.pos + RotateY((d.bmin + d.bmax) * 0.5f, wp.yaw);
      AimAt(b, PropPivot(p.ms, p.propType), c, 9.0f, TICK_DT);
      if (b.stateTimer > 0.3f) press(BTN_USE);
      if (p.propType == wp.type) {
        b.state = 3;
        b.stateTimer = 0;
      } else if (b.stateTimer > 2.5f) {
        b.state = 0;
      }
      break;
    }
    case 3: {  // Versteck neben gleichartigem Objekt wählen
      std::vector<int> same;
      for (int i = 0; i < (int)w.props.size(); i++)
        if (w.props[i].type == p.propType && Distance(w.props[i].pos, p.ms.pos) < 14.0f) same.push_back(i);
      // Kandidaten neben gleichartigen Objekten; bevorzugt an Wänden/Möbeln statt mitten im Gang
      bool ok = false;
      float bestScore = -1e9f;
      Vec3 bestSpot;
      float bestYaw = p.ms.colYaw;
      std::vector<int> qidx;
      for (int t = 0; t < 24 && !same.empty(); t++) {
        const WorldProp& ref = w.props[same[rng_.Int(0, (int)same.size() - 1)]];
        float a = rng_.Float() * TAU, r = rng_.Range(0.8f, 2.2f);
        Vec3 spot = ref.pos + Vec3(cosf(a) * r, 0, sinf(a) * r);
        int n = nav_.Nearest(spot, 0.7f);
        if (n < 0) continue;
        Vec3 np = nav_.NodePos(n);
        if (fabsf(np.y - ref.pos.y) > 1.0f) continue;
        bool crowded = false;
        for (const Player& q : sim.players)
          if (q.active && q.alive && &q != &p && Distance(q.ms.pos, np) < 1.5f) crowded = true;
        if (crowded) continue;
        w.Query(np + Vec3(-0.9f, 0.2f, -0.9f), np + Vec3(0.9f, 1.5f, 0.9f), qidx);
        int cover = 0;
        for (int ci : qidx)
          if (w.cols[ci].flags & COL_PLAYER) cover++;
        float score = std::min(cover, 6) * 0.8f - r * 0.3f + rng_.Range(0.0f, 1.5f);
        if (score > bestScore) {
          bestScore = score;
          bestSpot = np;
          bestYaw = ref.yaw;
          ok = true;
        }
      }
      if (ok && SetGoal(p, b, bestSpot)) {
        b.hideSpot = bestSpot;
        b.hideYaw = bestYaw;
      } else {
        ok = false;
      }
      if (!ok) {
        b.hideSpot = p.ms.pos;
        b.hideYaw = p.ms.colYaw;
        b.path.clear();
      }
      b.state = 4;
      b.stateTimer = 0;
      break;
    }
    case 4: {  // zum Versteck
      Vec3 dir = FollowPath(p, b);
      if (Length(dir) < 0.01f || DistanceXZ(p.ms.pos, b.hideSpot) < 0.35f || b.stateTimer > 25.0f) {
        b.state = 5;
        b.stateTimer = 0;
        break;
      }
      if (p.ms.rotLock) press(BTN_LOCK);
      Steer(p, b, dir, true, cmd, sim.phase == Phase::Hunting || sim.phaseTimer < 10.0f);
      break;
    }
    case 5: {  // versteckt
      float diff = AngleDiff(p.ms.colYaw, b.hideYaw);
      if (fabsf(diff) > 0.04f && b.stateTimer < 4.0f) {
        if (p.ms.rotLock) press(BTN_LOCK);
        else b.yaw = WrapAngle(b.hideYaw - PI);
      } else if (!p.ms.rotLock) {
        press(BTN_LOCK);
      } else {
        b.lookTimer -= TICK_DT;
        if (b.lookTimer <= 0) {
          b.lookTimer = rng_.Range(2.0f, 6.0f);
          b.goal = Vec3(rng_.Range(-PI, PI), rng_.Range(-0.4f, 0.1f), 0);
        }
        b.yaw = LerpAngle(b.yaw, b.goal.x, 0.02f);
        b.pitch = Lerp(b.pitch, b.goal.y, 0.02f);
      }
      if (sim.phase == Phase::Hunting && p.tauntCooldown <= 0 && p.tauntTimer <= 0 && p.ms.rotLock) {
        float chance = nearH > 18.0f ? 0.05f : (nearH > 10.0f ? 0.012f : 0.002f);
        if (skill_ == 0) chance *= 2.5f;
        if (rng_.Chance(chance * TICK_DT * 2.0f)) {
          press(BTN_TAUNT);
          cmd.taunt = 0;
        }
      }
      if (!b.decoyPlaced && skill_ >= 1 && sim.phase == Phase::Hunting && b.stateTimer > 3.0f && p.ms.rotLock) {
        b.pitch = -0.45f;
        press(BTN_DECOY);
        b.decoyPlaced = true;
      }
      if (nearHid >= 0 && nearH < 2.2f && skill_ >= 1 && rng_.Chance(0.004f)) startFlee();
      break;
    }
    case 6: {  // Flucht
      Vec3 dir = FollowPath(p, b);
      if (p.ms.rotLock) press(BTN_LOCK);
      if (Length(dir) < 0.01f || b.stateTimer > 9.0f) {
        b.state = 0;
        b.stateTimer = 0;
        break;
      }
      Steer(p, b, dir, true, cmd, true);
      break;
    }
  }
}

void BotManager::ThinkHunter(Sim& sim, int id, Brain& b, UserCmd& cmd) {
  Player& p = sim.players[id];
  const World& w = *world_;
  auto press = [&](u16 btn) { if (!(b.lastButtons & btn)) cmd.buttons |= btn; };
  float sk = (float)skill_;
  cmd.weapon = b.weaponWant;
  if (sim.phase == Phase::Hiding) {
    b.yaw = WrapAngle(b.yaw + 0.25f * TICK_DT);
    return;
  }
  Vec3 eye = EyePos(p.ms, -1);
  b.perceptTimer -= TICK_DT;
  if (b.perceptTimer <= 0) {
    b.perceptTimer = 0.2f;
    Vec3 fwd = DirFromYawPitch(b.yaw, 0);
    int best = -1;
    float bestS = 1.0f;
    for (int j = 0; j < MAX_PLAYERS; j++) {
      const Player& q = sim.players[j];
      if (!q.active || !q.alive || q.team != TEAM_PROPS) { b.suspicion[j] = 0; continue; }
      Vec3 c = PlayerBox(q.ms, q.propType).c;
      Vec3 to = c - eye;
      float d = Length(to);
      bool vis = d < 32.0f && d > 0.1f && Dot(to / d, fwd) > 0.45f && w.LineOfSight(eye, c);
      if (vis) {
        // Gut getarnte Props fallen durch bloßes Hinsehen kaum auf – verräterisch sind Bewegung,
        // Geräusche, Einzelstücke ohne Gegenstück in der Nähe und schiefe Ausrichtung.
        float rate = 0.015f + 0.015f * sk;
        float speed = sqrtf(q.ms.vel.x * q.ms.vel.x + q.ms.vel.z * q.ms.vel.z);
        if (speed > 0.6f) rate += 2.5f;
        if (q.tauntTimer > 0) rate += 3.0f;
        for (const Sound& s : sounds_)
          if (time_ - s.time < 8.0f && Distance(s.pos, c) < 4.0f) rate += 1.0f;
        int same = SameTypeNearby(q.propType, q.ms.pos, 6.0f);
        if (same == 0) rate += 0.12f + 0.1f * sk;
        else {
          // schief zu den Nachbarn ausgerichtet?
          float bestD = 1e9f, nYaw = 0;
          for (const WorldProp& wp : w.props)
            if (wp.type == q.propType) {
              float dd = LengthSq(wp.pos - q.ms.pos);
              if (dd < bestD) { bestD = dd; nYaw = wp.yaw; }
            }
          if (fabsf(AngleDiff(q.ms.colYaw, nYaw)) > 0.35f) rate += 0.05f + 0.04f * sk;
        }
        rate *= Clamp(1.6f - d / 20.0f, 0.2f, 1.6f);
        b.suspicion[j] += rate * 0.2f;
        if (rate < 0.1f) b.suspicion[j] = std::max(0.0f, b.suspicion[j] - 0.004f);
      } else {
        b.suspicion[j] = std::max(0.0f, b.suspicion[j] - 0.02f);
      }
      if (b.suspicion[j] > bestS) { bestS = b.suspicion[j]; best = j; }
    }
    if (b.target < 0 && best >= 0) {
      b.target = best;
      b.reaction = 0.75f - 0.22f * sk;
      b.targetSeen = 0;
    }
  }

  if (b.target >= 0) {
    const Player& q = sim.players[b.target];
    if (!q.active || !q.alive || q.team != TEAM_PROPS) {
      b.target = -1;
    } else {
      Vec3 c = PlayerBox(q.ms, q.propType).c;
      bool vis = w.LineOfSight(eye, c);
      if (vis) b.targetSeen = 0;
      else b.targetSeen += TICK_DT;
      if (b.targetSeen > 3.0f) {
        b.suspicion[b.target] *= 0.4f;
        b.target = -1;
      } else {
        AimAt(b, eye, c, 3.0f + 2.5f * sk, TICK_DT);
        float amp = 0.05f - 0.015f * sk;
        b.errYaw = Clamp(b.errYaw + rng_.Range(-1.0f, 1.0f) * amp * 0.3f, -amp, amp);
        b.errPitch = Clamp(b.errPitch + rng_.Range(-1.0f, 1.0f) * amp * 0.2f, -amp * 0.6f, amp * 0.6f);
        b.reaction -= TICK_DT;
        Vec3 toT = c - eye;
        float d = Length(toT);
        Vec3 look = DirFromYawPitch(b.yaw, b.pitch);
        float ang = acosf(Clamp(Dot(look, toT / std::max(d, 0.01f)), -1.0f, 1.0f));
        if (b.reaction <= 0 && vis && ang < 0.06f + 0.4f / std::max(d, 1.0f)) cmd.buttons |= BTN_FIRE;
        b.weaponWant = (sk >= 2 && d < 5.0f) ? WPN_SHOTGUN : WPN_SMG;
        cmd.weapon = b.weaponWant;
        if (d > 6.0f || !vis) {
          b.repath -= TICK_DT;
          if (b.repath <= 0) {
            SetGoal(p, b, q.ms.pos);
            b.repath = 1.0f;
          }
          Steer(p, b, FollowPath(p, b), false, cmd, false);
        } else {
          cmd.buttons |= ((int)(time_ * 0.7f + id) % 2) ? BTN_LEFT : BTN_RIGHT;
        }
        if (p.ammo[p.weapon] == 0) press(BTN_RELOAD);
        return;
      }
    }
  }
  b.errYaw *= 0.9f;
  b.errPitch *= 0.9f;

  // Testschüsse auf verdächtige Gegenstände
  if (b.testProp >= 0) {
    const WorldProp& wp = w.props[b.testProp];
    const PropDef& d = props::Get(wp.type);
    Vec3 c = wp.pos + RotateY((d.bmin + d.bmax) * 0.5f, wp.yaw);
    AimAt(b, eye, c, 5.0f, TICK_DT);
    b.testTimer += TICK_DT;
    Vec3 look = DirFromYawPitch(b.yaw, b.pitch);
    if (b.testTimer > 0.4f && Dot(look, Normalize(c - eye)) > 0.995f && p.fireTimer <= 0 && b.testShots > 0) {
      cmd.buttons |= BTN_FIRE;
      b.testShots--;
    }
    if (b.testShots <= 0 || b.testTimer > 3.0f || p.health < 40.0f) {
      b.cleared.push_back(b.testProp);
      b.testProp = -1;
    }
    return;
  }

  if (b.hasInvestigate) {
    if (!b.hasGoal) {
      if (!SetGoal(p, b, b.investigate)) b.hasInvestigate = false;
    }
    Vec3 dir = FollowPath(p, b);
    b.investigateTimer += TICK_DT;
    if (Length(dir) < 0.01f || DistanceXZ(p.ms.pos, b.investigate) < 2.5f || b.investigateTimer > 15.0f) {
      if (p.health > 45.0f && rng_.Chance(0.6f + 0.15f * sk)) {
        // Wie ein Mensch: irgendein Objekt in der Nähe des Geräuschs prüfen (echte und falsche gleich behandeln)
        struct Cand { int prop, player; float w; };
        std::vector<Cand> cands;
        for (int i = 0; i < (int)w.props.size(); i++) {
          const WorldProp& wp = w.props[i];
          if (!props::Get(wp.type).disguisable) continue;
          if (std::find(b.cleared.begin(), b.cleared.end(), i) != b.cleared.end()) continue;
          float d = Distance(wp.pos, b.investigate);
          if (d < 4.0f && w.LineOfSight(eye, wp.pos + Vec3(0, 0.3f, 0))) cands.push_back({i, -1, 1.0f / (0.5f + d)});
        }
        for (int j = 0; j < MAX_PLAYERS; j++) {
          const Player& q = sim.players[j];
          if (!q.active || !q.alive || q.team != TEAM_PROPS) continue;
          float d = Distance(q.ms.pos, b.investigate);
          if (d < 4.0f) cands.push_back({-1, j, 1.0f / (0.5f + d)});
        }
        int bestProp = -1, bestPlayer = -1;
        float total = 0;
        for (const Cand& c : cands) total += c.w;
        float pick = rng_.Float() * total;
        for (const Cand& c : cands) {
          pick -= c.w;
          if (pick <= 0) { bestProp = c.prop; bestPlayer = c.player; break; }
        }
        if (bestPlayer >= 0) {
          b.suspicion[bestPlayer] = 2.0f;
          b.target = bestPlayer;
          b.reaction = 0.4f;
          b.targetSeen = 0;
        } else if (bestProp >= 0) {
          b.testProp = bestProp;
          b.testTimer = 0;
          b.testShots = 3;
        }
      }
      b.hasInvestigate = false;
      b.hasGoal = false;
      b.path.clear();
    } else {
      Steer(p, b, dir, true, cmd, true);
      b.pitch = Lerp(b.pitch, -0.1f, 0.05f);
    }
    return;
  }

  // Patrouille
  if (!b.hasGoal || b.pathIdx >= b.path.size()) {
    for (int t = 0; t < 6; t++) {
      int n = nav_.RandomNode(rng_);
      if (n < 0) break;
      Vec3 np = nav_.NodePos(n);
      if (w.SkyAt(np + Vec3(0, 0.5f, 0)) > 0.9f && rng_.Chance(0.75f)) continue;
      if (SetGoal(p, b, np)) break;
    }
  }
  Vec3 dir = FollowPath(p, b);
  Steer(p, b, dir, true, cmd, false);
  b.pitch = Lerp(b.pitch, -0.08f, 0.05f);
  b.sonarTimer -= TICK_DT;
  if (b.sonarTimer <= 0 && p.abilityTimer <= 0) {
    press(BTN_ABILITY);
    b.sonarTimer = 26.0f + rng_.Range(0.0f, 10.0f) - sk * 5.0f;
  }
  if (p.weapon != WPN_GRENADE && p.ammo[p.weapon] < GetWeapon(p.weapon).magazine / 2 && p.reloadTimer <= 0)
    press(BTN_RELOAD);
}

void BotManager::OnEvents(const Sim& sim, const std::vector<GameEvent>& evs) {
  for (const GameEvent& e : evs) {
    if (e.a < MAX_PLAYERS && sim.players[e.a].bot &&
        ((e.type == EV_DISGUISE && sim.mode == MODE_ROULETTE) || e.type == EV_CLONE_SWAP)) {
      brains_[e.a].state = 3; brains_[e.a].stateTimer = 0; brains_[e.a].path.clear();
    }
    if (e.type == EV_TAUNT || e.type == EV_PING) {
      sounds_.push_back({e.pos, time_});
      for (int j = 0; j < MAX_PLAYERS; j++) {
        const Player& q = sim.players[j];
        if (!q.active || !q.bot || !q.alive || q.team != TEAM_HUNTERS) continue;
        Brain& b = brains_[j];
        float d = Distance(q.ms.pos, e.pos);
        if (d > 40.0f || b.target >= 0) continue;
        float err = d * (0.18f - 0.05f * skill_);
        b.investigate = e.pos + Vec3(rng_.Range(-err, err), 0, rng_.Range(-err, err));
        b.hasInvestigate = true;
        b.investigateTimer = 0;
        b.hasGoal = false;
      }
    } else if (e.type == EV_PENALTY) {
      if (e.a < MAX_PLAYERS) brains_[e.a].testProp = -1;
    } else if (e.type == EV_HIT) {
      if (e.a < MAX_PLAYERS && e.b < MAX_PLAYERS && sim.players[e.a].bot && sim.players[e.b].team == TEAM_PROPS) {
        Brain& b = brains_[e.a];
        b.suspicion[e.b] = 5.0f;
        b.target = e.b;
        b.targetSeen = 0;
        b.testProp = -1;
      }
    }
  }
}

int BotManager::CountHiddenProps(const Sim& sim) const {
  int n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (sim.players[i].active && sim.players[i].bot && sim.players[i].alive && sim.players[i].team == TEAM_PROPS && brains_[i].state == 5) n++;
  return n;
}
