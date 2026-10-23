#include "game/Sim.h"
#include "game/Taunts.h"
#include <chrono>

int FindDisguiseTarget(const World& w, const MoveState& ms, int propType, Vec3* hitPos) {
  Vec3 pivot = PropPivot(ms, propType);
  Vec3 cam = ThirdPersonCamera(w, pivot, ms.yaw, ms.pitch, PropCamDistance(propType));
  Vec3 dir = DirFromYawPitch(ms.yaw, ms.pitch);
  RayHit h = w.Raycast(cam, dir, 14.0f, COL_SOLID, true);
  if (!h.hit || h.prop < 0) return -1;
  if (Distance(h.pos, pivot) > 4.5f) return -1;
  const WorldProp& wp = w.props[h.prop];
  if (!props::Get(wp.type).disguisable) return -1;
  if (hitPos) *hitPos = h.pos;
  return h.prop;
}

void Sim::Init(World* w, const MatchSettings& s) {
  world = w;
  settings = s;
  settings.mode = ClampI(settings.mode, 0, MODE_COUNT - 1);
  mode = MODE_CLASSIC;
  modeTimer = 0;
  for (auto& p : players) p = Player();
  for (auto& d : decoys) d = Decoy();
  for (auto& g : grenades) g = Grenade();
  phase = Phase::Waiting;
  phaseTimer = 2.0f;
  round = 0;
  tick = 0;
  events.clear();
  rng_.Seed((u64)std::chrono::steady_clock::now().time_since_epoch().count());
}

GameEvent& Sim::Emit(u8 type) {
  events.emplace_back();
  GameEvent& e = events.back();
  e.id = nextEventId_++;
  e.type = type;
  return e;
}

int Sim::AddPlayer(const std::string& name, bool bot) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    Player& p = players[i];
    if (p.active) continue;
    p = Player();
    p.active = true;
    p.bot = bot;
    strncpy(p.name, name.c_str(), NAME_LEN - 1);
    p.team = TEAM_SPECTATOR;
    p.alive = false;
    Emit(EV_JOIN).a = (u8)i;
    if (phase == Phase::Hiding) SpawnPlayer(i, TEAM_PROPS, rng_.Int(0, 15));
    else if (phase == Phase::Hunting) SpawnPlayer(i, TEAM_HUNTERS, rng_.Int(0, 15));
    return i;
  }
  return -1;
}

void Sim::RemovePlayer(int id) {
  if (id < 0 || id >= MAX_PLAYERS || !players[id].active) return;
  Emit(EV_LEAVE).a = (u8)id;
  players[id] = Player();
  for (auto& d : decoys)
    if (d.owner == id) d.active = false;
}

void Sim::QueueCmd(int id, const UserCmd& c) {
  if (id < 0 || id >= MAX_PLAYERS || !players[id].active) return;
  auto& q = players[id].pending;
  if (q.size() >= 10) q.erase(q.begin());
  q.push_back(c);
}

void Sim::Chat(int id, const std::string& text) {
  GameEvent& e = Emit(EV_CHAT);
  e.a = (u8)id;
  e.text = text.substr(0, 120);
}

int Sim::CountAlive(Team t) const {
  int n = 0;
  for (const Player& p : players)
    if (p.active && p.alive && p.team == t) n++;
  return n;
}

int Sim::CountActive() const {
  int n = 0;
  for (const Player& p : players)
    if (p.active) n++;
  return n;
}

MoveEnv Sim::MakeEnv(int ignore, std::vector<OBox>& storage) const {
  storage.clear();
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const Player& q = players[i];
    if (i == ignore || !q.active || !q.alive) continue;
    storage.push_back(PlayerBox(q.ms, q.team == TEAM_PROPS ? q.propType : -1));
  }
  for (const Decoy& d : decoys) {
    if (!d.active) continue;
    MoveState t;
    t.pos = d.pos;
    t.colYaw = d.yaw;
    storage.push_back(PlayerBox(t, d.propType));
  }
  MoveEnv env;
  env.mode = mode;
  env.world = world;
  env.others = &storage;
  return env;
}

void Sim::RestartMatch() {
  round = 0;
  hunterRotation_ = 0;
  for (Player& p : players) {
    if (!p.active) continue;
    p.score = p.kills = p.deaths = 0;
    p.team = TEAM_SPECTATOR;
    p.alive = false;
  }
  phase = Phase::Waiting;
  phaseTimer = 2.0f;
  Emit(EV_PHASE).b = (u8)phase;
}

void Sim::SpawnPlayer(int id, Team team, int spawnIndex) {
  Player& p = players[id];
  p.team = team;
  p.alive = true;
  p.ms = MoveState();
  const std::vector<SpawnPoint>& sp = team == TEAM_HUNTERS ? world->hunterSpawns : world->propSpawns;
  if (!sp.empty()) {
    const SpawnPoint& s = sp[(size_t)spawnIndex % sp.size()];
    p.ms.pos = s.pos + Vec3(0, 0.02f, 0);
    p.ms.yaw = s.yaw;
  }
  p.ms.colYaw = WrapAngle(p.ms.yaw + PI);
  if (team == TEAM_PROPS) {
    p.propType = props::Find("karton");
    p.maxHealth = props::Get(p.propType).health;
  } else {
    p.propType = -1;
    p.maxHealth = 100;
    p.wasHunterThisRound = true;
  }
  p.health = p.maxHealth;
  p.weapon = WPN_SMG;
  for (int w = 0; w < 2; w++) p.builds[w] = ValidateWeaponBuild(w, p.nextBuilds[w]);
  for (int w = 0; w < WPN_COUNT; w++) p.ammo[w] = (u8)GetWeaponStats(w, w < 2 ? p.builds[w] : 0).magazine;
  p.fireTimer = p.reloadTimer = p.switchTimer = 0;
  p.aimProgress = p.recoilPitch = p.recoilYaw = p.sprintRecover = p.reloadDuration = p.reloadNextShell = 0;
  p.shotsFired = 0;
  p.ramDistance = 0;
  p.abilityTimer = 8;
  p.decoys = 2;
  if (mode == MODE_CLONES) p.decoys = 3;
  p.modeMeter = 0;
  p.modeAnchor = p.ms.pos;
  p.tauntTimer = p.tauntCooldown = 0;
  p.autoTaunt = AutoTauntInterval();
  p.disguiseTimer = 0;
  p.pingTimer = -1;
  p.respawnTimer = -1;
  p.aliveAcc = 0;
  p.pending.clear();
  p.prevButtons = 0xFFFF;
  p.ms.lastButtons = 0xFFFF;
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(id, others);
  FindFreeSpot(env, p.ms, team == TEAM_PROPS ? p.propType : -1);
  p.prevPos = p.ms.pos;
  p.modeAnchor = p.ms.pos;
  p.prevColYaw = p.ms.colYaw;
}

void Sim::StartRound() {
  std::vector<int> ids;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (players[i].active) ids.push_back(i);
  int n = (int)ids.size();
  if (n < 2) {
    phase = Phase::Waiting;
    phaseTimer = 2.0f;
    return;
  }
  round++;
  GameMode previousMode = mode;
  mode = (GameMode)ClampI(settings.mode, 0, MODE_COUNT - 1);
  if (mode == MODE_SHUFFLE) {
    do { mode = (GameMode)rng_.Int(MODE_ROULETTE, MODE_MOON); } while (mode == previousMode);
  }
  modeTimer = mode == MODE_ROULETTE ? ROULETTE_INTERVAL : 0;
  int hunters = std::max(1, (n * 2 + 2) / 5);
  std::vector<bool> isHunter(MAX_PLAYERS, false);
  if (settings.teamSwap) {
    for (int k = 0; k < hunters; k++) isHunter[ids[(hunterRotation_ + k) % n]] = true;
    hunterRotation_ = (hunterRotation_ + hunters) % n;
  } else {
    std::vector<int> sh = ids;
    for (int i = n - 1; i > 0; i--) std::swap(sh[i], sh[rng_.Int(0, i)]);
    for (int k = 0; k < hunters; k++) isHunter[sh[k]] = true;
  }
  int hs = rng_.Int(0, 15), ps = rng_.Int(0, 15);
  for (auto& d : decoys) d.active = false;
  for (auto& g : grenades) g.active = false;
  for (int id : ids) players[id].alive = false;
  for (int id : ids) {
    players[id].wasHunterThisRound = false;
    if (isHunter[id]) SpawnPlayer(id, TEAM_HUNTERS, hs++);
    else SpawnPlayer(id, TEAM_PROPS, ps++);
  }
  phase = Phase::Hiding;
  phaseTimer = settings.hideTime;
  lastCountdown_ = settings.hideTime;
  winner = TEAM_SPECTATOR;
  Emit(EV_PHASE).b = (u8)phase;
}

void Sim::EndRound(Team win) {
  phase = Phase::RoundEnd;
  phaseTimer = 8.0f;
  winner = win;
  for (Player& p : players)
    if (p.active && p.alive && p.team == win) p.score += 50;
  Emit(EV_ROUND_END).b = (u8)win;
}

void Sim::CheckRoundEnd() {
  if (phase != Phase::Hunting && phase != Phase::Hiding) return;
  int ah = CountAlive(TEAM_HUNTERS), ap = CountAlive(TEAM_PROPS);
  bool pendingHunters = false;
  for (const Player& p : players)
    if (p.active && p.respawnTimer > 0) pendingHunters = true;
  if (ap == 0) EndRound(TEAM_HUNTERS);
  else if (ah == 0 && !pendingHunters) EndRound(TEAM_PROPS);
}

void Sim::Tick() {
  tick++;
  for (Player& p : players) {
    p.prevPos = p.ms.pos;
    p.prevColYaw = p.ms.colYaw;
  }
  float before = phaseTimer;
  phaseTimer -= TICK_DT;
  switch (phase) {
    case Phase::Waiting:
      if (phaseTimer <= 0) {
        if (CountActive() >= 2) StartRound();
        else phaseTimer = 1.0f;
      }
      break;
    case Phase::Hiding: {
      int secBefore = (int)ceilf(before), secNow = (int)ceilf(phaseTimer);
      if (secNow != secBefore && secNow <= 5 && secNow > 0) Emit(EV_COUNTDOWN).b = (u8)secNow;
      if (phaseTimer <= 0) {
        phase = Phase::Hunting;
        phaseTimer = settings.roundTime;
        Emit(EV_PHASE).b = (u8)phase;
      }
      break;
    }
    case Phase::Hunting:
      if (before > 60.0f && phaseTimer <= 60.0f) Emit(EV_COUNTDOWN).b = 60;
      if (phaseTimer <= 0) EndRound(TEAM_PROPS);
      break;
    case Phase::RoundEnd:
      if (phaseTimer <= 0) {
        if (round >= settings.rounds) {
          phase = Phase::MatchEnd;
          phaseTimer = 14.0f;
          Emit(EV_PHASE).b = (u8)phase;
        } else {
          StartRound();
        }
      }
      break;
    case Phase::MatchEnd:
      if (phaseTimer <= 0) RestartMatch();
      break;
  }

  for (int id = 0; id < MAX_PLAYERS; id++) {
    Player& p = players[id];
    if (!p.active) continue;
    auto dec = [](float& t) { if (t > 0) t = std::max(0.0f, t - TICK_DT); };
    dec(p.fireTimer);
    dec(p.switchTimer);
    dec(p.abilityTimer);
    dec(p.tauntTimer);
    dec(p.tauntCooldown);
    dec(p.disguiseTimer);
    dec(p.hurtTimer);
    dec(p.sprintRecover);
    const WeaponStats wi = GetWeaponStats(p.weapon, p.weapon < 2 ? p.builds[p.weapon] : 0);
    bool aiming = p.alive && p.team == TEAM_HUNTERS && p.weapon < 2 && (p.prevButtons & BTN_AIM) &&
                  p.reloadTimer <= 0 && p.switchTimer <= 0 && p.sprintRecover <= 0;
    p.aimProgress = Clamp(p.aimProgress + (aiming ? 1 : -1) * TICK_DT / wi.adsTime, 0.0f, 1.0f);
    p.recoilPitch *= expf(-7.0f * TICK_DT);
    p.recoilYaw *= expf(-9.0f * TICK_DT);
    if (p.reloadTimer > 0) {
      p.reloadTimer -= TICK_DT;
      if (p.weapon == WPN_SHOTGUN) {
        p.reloadNextShell -= TICK_DT;
        if (p.reloadNextShell <= 0 && p.ammo[p.weapon] < wi.magazine) {
          p.ammo[p.weapon]++;
          p.reloadNextShell += wi.shellTime;
          GameEvent& e = Emit(EV_SHELL); e.a = (u8)id; e.pos = p.ms.pos;
        }
      }
      if (p.reloadTimer <= 0) {
        p.reloadTimer = 0;
        if (p.weapon == WPN_SMG) p.ammo[p.weapon] = (u8)wi.magazine;
      }
    }
    if (p.respawnTimer > 0) {
      p.respawnTimer -= TICK_DT;
      if (p.respawnTimer <= 0 && phase == Phase::Hunting) {
        SpawnPlayer(id, TEAM_HUNTERS, rng_.Int(0, 15));
        GameEvent& e = Emit(EV_TEAMSWITCH);
        e.a = (u8)id;
        e.b = TEAM_HUNTERS;
      }
    }
    if (p.alive && p.team == TEAM_PROPS && phase == Phase::Hunting) {
      p.aliveAcc += TICK_DT;
      while (p.aliveAcc >= 2.0f) { p.score += 1; p.aliveAcc -= 2.0f; }
      if (p.tauntTimer <= 0) {
        p.autoTaunt -= TICK_DT;
        if (p.autoTaunt <= 0) DoTaunt(id, -1);
      }
    }
    if (p.pingTimer > 0) {
      p.pingTimer -= TICK_DT;
      if (p.pingTimer <= 0) {
        p.pingTimer = -1;
        if (p.alive && p.team == TEAM_PROPS) {
          GameEvent& e = Emit(EV_PING);
          e.a = (u8)id;
          e.pos = p.ms.pos + Vec3(0, 0.4f, 0);
        }
      }
    }
    for (const UserCmd& c : p.pending) ProcessCmd(id, c);
    p.pending.clear();
    if (p.alive && p.ms.pos.y < world->killY) {
      if (p.team == TEAM_PROPS && !world->propSpawns.empty()) {
        p.ms.pos = world->propSpawns[rng_.Int(0, (int)world->propSpawns.size() - 1)].pos;
        p.ms.vel = Vec3();
      } else {
        Kill(id, -1, KILL_FALL);
      }
    }
  }
  UpdateGrenades();
  UpdateMode();
  CheckRoundEnd();
}

void Sim::ProcessCmd(int id, const UserCmd& cmd) {
  Player& p = players[id];
  p.lastCmdSeq = cmd.seq;
  for (int w = 0; w < 2; w++) {
    p.nextBuilds[w] = ValidateWeaponBuild(w, cmd.builds[w]);
    if ((phase == Phase::Waiting || phase == Phase::Hiding) && p.builds[w] != p.nextBuilds[w]) {
      p.builds[w] = p.nextBuilds[w];
      p.ammo[w] = (u8)GetWeaponStats(w, p.builds[w]).magazine;
      p.reloadTimer = p.reloadDuration = 0;
    }
  }
  if (!p.alive) {
    p.prevButtons = cmd.buttons;
    p.ms.yaw = cmd.yaw;
    p.ms.pitch = cmd.pitch;
    return;
  }
  u16 pressed = cmd.buttons & ~p.prevButtons;
  p.prevButtons = cmd.buttons;
  bool frozen = p.team == TEAM_HUNTERS && phase == Phase::Hiding;
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(id, others);
  int pt = p.team == TEAM_PROPS ? p.propType : -1;
  bool lockBefore = p.ms.rotLock;
  const MoveState before = p.ms;
  bool heavy = pt >= 0 && props::Get(pt).ramDamage > 0;
  std::vector<MoveContact> contacts;
  std::vector<int> playerIds;
  if (heavy) {
    // Gleiche Reihenfolge wie MakeEnv; danach folgen Klone ohne Spieler-ID.
    for (int i = 0; i < MAX_PLAYERS; i++)
      if (i != id && players[i].active && players[i].alive) playerIds.push_back(i);
  }
  if (ApplyMovement(env, p.ms, cmd, pt, frozen, heavy ? &contacts : nullptr)) {
    GameEvent& e = Emit(EV_JUMP);
    e.a = (u8)id;
    e.pos = p.ms.pos;
  }
  if (p.ms.rotLock != lockBefore) {
    GameEvent& e = Emit(EV_LOCK);
    e.a = (u8)id;
    e.b = p.ms.rotLock ? 1 : 0;
  }
  if (heavy) PropCollisions(id, cmd, before, contacts, playerIds);
  if (frozen) return;
  if (phase != Phase::Hiding && phase != Phase::Hunting) return;
  if (p.team == TEAM_HUNTERS) HunterActions(id, cmd, pressed);
  else if (p.team == TEAM_PROPS) PropActions(id, cmd, pressed);
}

void Sim::PropCollisions(int id, const UserCmd& cmd, const MoveState& before,
                         const std::vector<MoveContact>& contacts, const std::vector<int>& playerIds) {
  Player& p = players[id];
  if (phase != Phase::Hunting || p.switchTimer > 0 || p.disguiseTimer > 0) { p.ramDistance = 0; return; }
  bool sprint = (cmd.buttons & BTN_SPRINT) && (cmd.buttons & BTN_FORWARD) && !(cmd.buttons & BTN_BACK) && before.stamina > .02f;
  if (!sprint || !before.onGround) p.ramDistance = 0;
  const float runup = p.ramDistance;
  for (const MoveContact& c : contacts) {
    if (c.other < 0 || c.other >= (int)playerIds.size()) continue;  // keine Weltobjekte/Klone
    int victim = playerIds[c.other];
    Player& q = players[victim];
    if (!q.alive || q.team != TEAM_HUNTERS) continue;
    bool crush = c.normal.y > .7f && !before.onGround && c.velocity.y <= -5.5f;
    bool ram = fabsf(c.normal.y) < .2f && sprint && before.onGround && runup >= PROP_RAM_RUNUP &&
               Length(Vec3(c.velocity.x, 0, c.velocity.z)) >= 5.0f && -Dot(c.velocity, c.normal) >= 4.0f;
    if ((!ram && !crush) || !world->LineOfSight(PlayerBox(before, p.propType).c, c.pos)) continue;
    p.switchTimer = PROP_IMPACT_COOLDOWN;
    p.ramDistance = 0;
    float damage = crush ? Clamp(100 + (-c.velocity.y - 5.5f) * 5, 100, 150) : props::Get(p.propType).ramDamage;
    if (ram) q.ms.vel += -c.normal * 6.0f + Vec3(0, 3.0f, 0);
    else { p.ms.vel.y = 3.5f; p.ms.onGround = false; }
    q.ms.onGround = false;
    Damage(victim, id, damage, crush ? KILL_CRUSH : KILL_RAM, c.pos);
    return;  // ein Opfer pro Aufprall, auch bei mehreren Kontakten/gebündelten Befehlen
  }
  float travel = DistanceXZ(before.pos, p.ms.pos);
  float speed = Length(Vec3(p.ms.vel.x, 0, p.ms.vel.z));
  if (sprint && before.onGround && p.ms.onGround && speed >= 4.5f && travel > .01f)
    p.ramDistance = std::min(PROP_RAM_RUNUP, p.ramDistance + std::min(travel, 7.0f * TICK_DT));
  else p.ramDistance = 0;  // an Wänden/Spielern drücken lädt keinen Angriff auf
}

void Sim::HunterActions(int id, const UserCmd& cmd, u16 pressed) {
  Player& p = players[id];
  if (cmd.weapon < WPN_COUNT && cmd.weapon != p.weapon) {
    if (!(cmd.weapon == WPN_GRENADE && p.ammo[WPN_GRENADE] == 0)) {
      p.weapon = cmd.weapon;
      p.switchTimer = 0.4f;
      p.reloadTimer = 0;
    }
  }
  if (phase != Phase::Hunting) return;
  const WeaponStats wi = GetWeaponStats(p.weapon, p.weapon < 2 ? p.builds[p.weapon] : 0);
  bool sprint = (cmd.buttons & BTN_SPRINT) && (cmd.buttons & BTN_FORWARD) && !(cmd.buttons & (BTN_BACK | BTN_AIM | BTN_FIRE)) &&
                p.ms.stamina > .02f && !p.ms.crouching;
  if (sprint) p.sprintRecover = .20f;
  if ((pressed & BTN_RELOAD) && p.weapon != WPN_GRENADE && p.ammo[p.weapon] < wi.magazine && p.reloadTimer <= 0) {
    BeginReload(id);
  }
  bool fire = p.weapon == WPN_GRENADE ? (pressed & BTN_FIRE) != 0 : (cmd.buttons & BTN_FIRE) != 0;
  if ((pressed & BTN_FIRE) && p.weapon == WPN_SHOTGUN && p.reloadTimer > 0 && p.ammo[p.weapon] > 0) {
    p.reloadTimer = 0; p.fireTimer = std::max(p.fireTimer, .2f);
  }
  if (fire && p.fireTimer <= 0 && p.reloadTimer <= 0 && p.switchTimer <= 0 && p.sprintRecover <= 0) {
    if (p.weapon == WPN_GRENADE) {
      if (p.ammo[WPN_GRENADE] > 0) ThrowGrenade(id);
    } else if (p.ammo[p.weapon] > 0) {
      FireWeapon(id);
    } else if (pressed & BTN_FIRE) {
      GameEvent& e = Emit(EV_EMPTY);
      e.a = (u8)id;
      e.pos = p.ms.pos;
      BeginReload(id);
    }
  }
  if ((pressed & BTN_ABILITY) && p.abilityTimer <= 0) {
    p.abilityTimer = 25.0f;
    GameEvent& e = Emit(EV_SONAR);
    e.a = (u8)id;
    e.pos = EyePos(p.ms, -1);
    for (Player& q : players) {
      if (!q.active || !q.alive || q.team != TEAM_PROPS) continue;
      float d = Distance(q.ms.pos, p.ms.pos);
      if (d < 16.0f) q.pingTimer = 0.5f + d * 0.04f;
    }
    if (mode == MODE_CLONES) {
      for (Decoy& d : decoys)
        if (d.active && Distance(d.pos, p.ms.pos) < 16.0f) d.pingTimer = 0.5f + Distance(d.pos, p.ms.pos) * 0.04f;
    }
  }
}

void Sim::PropActions(int id, const UserCmd& cmd, u16 pressed) {
  if (pressed & BTN_USE) TryDisguise(id);
  if (pressed & BTN_TAUNT) {
    Player& p = players[id];
    if (p.tauntTimer <= 0 && p.tauntCooldown <= 0) DoTaunt(id, cmd.taunt > 0 ? cmd.taunt - 1 : -1);
  }
  if (pressed & BTN_DECOY) PlaceDecoy(id);
  if ((pressed & BTN_ABILITY) && mode == MODE_CLONES) SwapClone(id);
}

void Sim::BeginReload(int id) {
  Player& p = players[id];
  WeaponStats wi = GetWeaponStats(p.weapon, p.builds[p.weapon]);
  p.reloadDuration = p.weapon == WPN_SHOTGUN ? .55f + wi.shellTime * (wi.magazine - p.ammo[p.weapon]) : wi.reload;
  p.reloadTimer = p.reloadDuration;
  p.reloadNextShell = .35f + wi.shellTime;
  GameEvent& e = Emit(EV_RELOAD); e.a = (u8)id; e.b = p.weapon; e.f = p.reloadDuration; e.pos = p.ms.pos;
}

void Sim::FireWeapon(int id) {
  Player& p = players[id];
  const WeaponStats wi = GetWeaponStats(p.weapon, p.builds[p.weapon]);
  p.ammo[p.weapon]--;
  p.fireTimer = wi.interval;
  Vec3 eye = EyePos(p.ms, -1);
  Vec3 fwd = DirFromYawPitch(p.ms.yaw + p.recoilYaw, p.ms.pitch + p.recoilPitch);
  Vec3 right = RightFromYaw(p.ms.yaw);
  Vec3 up = Cross(right, fwd);
  float spread = Lerp(wi.spreadHip, wi.spreadAim, p.aimProgress);
  float speed = sqrtf(p.ms.vel.x * p.ms.vel.x + p.ms.vel.z * p.ms.vel.z);
  spread *= 1.0f + speed * 0.12f;
  if (!p.ms.onGround) spread *= 2.0f;
  GameEvent& se = Emit(EV_SHOT);
  se.a = (u8)id;
  se.b = p.weapon;
  se.c = p.builds[p.weapon];
  se.pos = eye;
  se.dir = fwd;
  float kick = (p.weapon == WPN_SMG ? .013f : .048f) * wi.recoil * (p.ms.crouching ? .85f : 1.0f);
  p.recoilPitch = std::min(.13f, p.recoilPitch + kick);
  p.recoilYaw += sinf(++p.shotsFired * 2.4f) * kick * .35f;
  float penalty = 0;
  Vec3 penPos;
  u8 cause = p.weapon == WPN_SHOTGUN ? KILL_SHOTGUN : KILL_SMG;
  for (int k = 0; k < wi.pellets; k++) {
    float ang = rng_.Float() * TAU, rad = sqrtf(rng_.Float()) * spread;
    Vec3 d = Normalize(fwd + right * (cosf(ang) * rad) + up * (sinf(ang) * rad));
    RayHit wh = world->Raycast(eye, d, 150.0f, COL_BULLET, true);
    float best = wh.hit ? wh.t : 150.0f;
    int hitPlayer = -1, hitDecoy = -1;
    for (int j = 0; j < MAX_PLAYERS; j++) {
      const Player& q = players[j];
      if (j == id || !q.active || !q.alive || q.team != TEAM_PROPS) continue;
      float t;
      Vec3 n;
      if (World::RayProp(eye, d, q.propType, q.ms.pos, q.ms.colYaw, best, t, n) && t < best) {
        best = t;
        hitPlayer = j;
        hitDecoy = -1;
      }
    }
    for (int j = 0; j < MAX_DECOYS; j++) {
      const Decoy& dc = decoys[j];
      if (!dc.active) continue;
      float t;
      Vec3 n;
      if (World::RayProp(eye, d, dc.propType, dc.pos, dc.yaw, best, t, n) && t < best) {
        best = t;
        hitDecoy = j;
        hitPlayer = -1;
      }
    }
    Vec3 hp = eye + d * best;
    float dmg = wi.damage * (best > 25.0f ? 0.7f : 1.0f);
    if (hitPlayer >= 0) {
      Damage(hitPlayer, id, dmg, cause, hp);
    } else if (hitDecoy >= 0) {
      Decoy& dc = decoys[hitDecoy];
      dc.health -= dmg;
      penalty += wi.penalty;
      penPos = hp;
      GameEvent& ie = Emit(EV_IMPACT);
      ie.pos = hp;
      ie.dir = -d;
      ie.b = MAT_WOOD;
      ie.c = 1;
      if (dc.health <= 0) {
        dc.active = false;
        GameEvent& be = Emit(EV_DECOY_BREAK);
        be.pos = dc.pos;
        be.c = (u16)dc.propType;
      }
    } else if (wh.hit) {
      GameEvent& ie = Emit(EV_IMPACT);
      ie.pos = wh.pos;
      ie.dir = wh.normal;
      ie.b = wh.surface;
      ie.c = wh.prop >= 0 ? 1 : 0;
      if (wh.prop >= 0) {
        penalty += wi.penalty;
        penPos = wh.pos;
      }
    }
  }
  if (penalty > 0) {
    GameEvent& pe = Emit(EV_PENALTY);
    pe.a = (u8)id;
    pe.f = penalty;
    pe.pos = penPos;
    Damage(id, id, penalty, KILL_PENALTY, penPos);
  }
}

void Sim::ThrowGrenade(int id) {
  Player& p = players[id];
  for (Grenade& g : grenades) {
    if (g.active) continue;
    Vec3 fwd = DirFromYawPitch(p.ms.yaw, p.ms.pitch);
    g.active = true;
    g.owner = id;
    g.pos = EyePos(p.ms, -1) + fwd * 0.5f;
    g.vel = fwd * 14.0f + Vec3(0, 3.0f, 0) + p.ms.vel * 0.5f;
    g.fuse = 2.2f;
    p.ammo[WPN_GRENADE]--;
    p.fireTimer = 0.8f;
    p.weapon = WPN_SMG;
    p.switchTimer = 0.5f;
    GameEvent& e = Emit(EV_GRENADE);
    e.a = (u8)id;
    e.pos = g.pos;
    return;
  }
}

void Sim::UpdateGrenades() {
  for (Grenade& g : grenades) {
    if (!g.active) continue;
    g.vel.y -= (mode == MODE_MOON ? 8.0f : 20.0f) * TICK_DT;
    Vec3 move = g.vel * TICK_DT;
    float len = Length(move);
    if (len > 1e-5f) {
      RayHit h = world->Raycast(g.pos, move / len, len + 0.08f, COL_SOLID, false);
      if (h.hit) {
        g.pos = h.pos + h.normal * 0.08f;
        g.vel = (g.vel - h.normal * (2.0f * Dot(g.vel, h.normal))) * 0.42f;
      } else {
        g.pos += move;
      }
    }
    g.fuse -= TICK_DT;
    if (g.fuse <= 0) Explode(g);
  }
}

void Sim::Explode(Grenade& g) {
  g.active = false;
  GameEvent& e = Emit(EV_EXPLOSION);
  e.a = (u8)std::max(0, g.owner);
  e.pos = g.pos;
  const float radius = 6.0f;
  for (int j = 0; j < MAX_PLAYERS; j++) {
    Player& q = players[j];
    if (!q.active || !q.alive || q.team != TEAM_PROPS) continue;
    Vec3 c = PlayerBox(q.ms, q.propType).c;
    float d = Distance(c, g.pos);
    if (d > radius || !world->LineOfSight(g.pos, c)) continue;
    Damage(j, g.owner, 120.0f * (1.0f - d / radius) + 10.0f, KILL_GRENADE, c);
  }
  for (Decoy& dc : decoys) {
    if (!dc.active || Distance(dc.pos, g.pos) > radius) continue;
    dc.active = false;
    GameEvent& be = Emit(EV_DECOY_BREAK);
    be.pos = dc.pos;
    be.c = (u16)dc.propType;
  }
}

void Sim::Damage(int victim, int attacker, float dmg, u8 cause, Vec3 pos) {
  Player& v = players[victim];
  if (!v.alive) return;
  v.health -= dmg;
  v.hurtTimer = 0.4f;
  if (attacker != victim && attacker >= 0) {
    GameEvent& e = Emit(EV_HIT);
    e.a = (u8)attacker;
    e.b = (u8)victim;
    e.c = cause;
    e.f = dmg;
    e.pos = pos;
  }
  if (v.health <= 0) {
    v.health = 0;
    Kill(victim, attacker, cause);
  }
}

void Sim::Kill(int victim, int attacker, u8 cause) {
  Player& v = players[victim];
  if (!v.alive) return;
  v.alive = false;
  v.deaths++;
  if (attacker >= 0 && attacker < MAX_PLAYERS && attacker != victim) {
    Player& k = players[attacker];
    k.kills++;
    k.score += 100;
    if (k.alive) k.health = std::min(k.maxHealth, k.health + 25.0f);
  }
  GameEvent& e = Emit(EV_KILL);
  e.a = attacker < 0 ? 255 : (u8)attacker;
  e.b = (u8)victim;
  e.c = cause;
  e.pos = v.ms.pos;
  e.f = (float)v.propType;
  for (auto& d : decoys)
    if (d.owner == victim) d.active = false;
  if (settings.infection && v.team == TEAM_PROPS && phase == Phase::Hunting) v.respawnTimer = 2.5f;
}

void Sim::TryDisguise(int id) {
  Player& p = players[id];
  if (mode == MODE_ROULETTE && phase == Phase::Hunting) {
    GameEvent& e = Emit(EV_NOFIT); e.a = (u8)id; e.b = 2;
    return;
  }
  if (p.disguiseTimer > 0) return;
  int target = FindDisguiseTarget(*world, p.ms, p.propType);
  if (target < 0) return;
  const WorldProp& wp = world->props[target];
  if (wp.type == p.propType) return;
  MoveState t = p.ms;
  t.colYaw = wp.yaw;
  t.rotLock = true;
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(id, others);
  if (!FindFreeSpot(env, t, wp.type)) {
    Emit(EV_NOFIT).a = (u8)id;
    return;
  }
  SetDisguise(id, wp.type, t);
}

void Sim::DoTaunt(int id, int which) {
  Player& p = players[id];
  int t = (which >= 0 && which < NUM_TAUNTS) ? which : rng_.Int(0, NUM_TAUNTS - 1);
  float dur = kTaunts[t].duration;
  p.tauntVariant = p.tauntVariant < 0 ? rng_.Int(0, TAUNT_VARIANTS - 1)
                                    : (p.tauntVariant + 1 + rng_.Int(0, TAUNT_VARIANTS - 2)) % TAUNT_VARIANTS;
  p.tauntTimer = dur;
  p.tauntCooldown = dur + 1.2f;
  p.autoTaunt = AutoTauntInterval();
  if (phase == Phase::Hunting) p.score += (int)(dur * 6 + 0.5f);
  GameEvent& e = Emit(EV_TAUNT);
  e.a = (u8)id;
  e.b = (u8)t;
  e.c = (u16)p.tauntVariant;
  e.pos = p.ms.pos + Vec3(0, 0.3f, 0);
  if (mode == MODE_CLONES) {
    for (const Decoy& d : decoys) {
      if (!d.active || d.owner != id) continue;
      GameEvent& echo = Emit(EV_TAUNT);
      echo.a = (u8)id; echo.b = (u8)t; echo.c = (u16)p.tauntVariant; echo.pos = d.pos + Vec3(0, .3f, 0);
    }
  }
}

void Sim::PlaceDecoy(int id) {
  Player& p = players[id];
  if (p.decoys == 0) return;
  if (mode == MODE_CLONES) {
    int owned = 0;
    for (const Decoy& d : decoys) if (d.active && d.owner == id) owned++;
    if (owned >= 3) { GameEvent& e = Emit(EV_NOFIT); e.a = (u8)id; e.b = 3; return; }
  }
  Vec3 pivot = PropPivot(p.ms, p.propType);
  Vec3 cam = ThirdPersonCamera(*world, pivot, p.ms.yaw, p.ms.pitch, PropCamDistance(p.propType));
  Vec3 dir = DirFromYawPitch(p.ms.yaw, p.ms.pitch);
  RayHit h = world->Raycast(cam, dir, 12.0f, COL_SOLID, true);
  Vec3 target = h.hit ? h.pos + h.normal * 0.1f : cam + dir * 8.0f;
  Vec3 off = target - pivot;
  float dist = Length(off);
  if (dist > 4.0f) target = pivot + off * (4.0f / dist);
  RayHit g = world->Raycast(target + Vec3(0, 0.3f, 0), Vec3(0, -1, 0), 8.0f, COL_PLAYER, false);
  if (!g.hit) return;
  MoveState t;
  t.pos = g.pos + Vec3(0, 0.01f, 0);
  t.colYaw = p.ms.colYaw;
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(-1, others);
  if (!FindFreeSpot(env, t, p.propType)) {
    Emit(EV_NOFIT).a = (u8)id;
    return;
  }
  for (Decoy& d : decoys) {
    if (d.active) continue;
    d.active = true;
    d.owner = id;
    d.propType = p.propType;
    d.pos = t.pos;
    d.yaw = t.colYaw;
    d.health = 40;
    d.life = mode == MODE_CLONES ? 45.0f : 0;
    d.pingTimer = -1;
    d.bornTick = tick;
    p.decoys--;
    GameEvent& e = Emit(EV_DECOY);
    e.a = (u8)id;
    e.pos = d.pos;
    e.c = (u16)d.propType;
    return;
  }
}
