#include "game/Sim.h"

// Regeln laufen ausschließlich beim Host; Clients erhalten Modus und Timer im Snapshot.
void Sim::SetDisguise(int id, int type, const MoveState& state) {
  Player& p = players[id];
  float fraction = p.maxHealth > 0 ? p.health / p.maxHealth : 1;
  p.propType = type;
  p.maxHealth = props::Get(type).health;
  p.health = std::max(1.0f, fraction * p.maxHealth);
  p.ms = state;
  p.disguiseTimer = .8f;
  p.ramDistance = 0;
  p.switchTimer = std::max(p.switchTimer, 1.0f);
  GameEvent& e = Emit(EV_DISGUISE);
  e.a = (u8)id; e.c = (u16)type; e.pos = p.ms.pos;
}

void Sim::RandomDisguise(int id) {
  Player& p = players[id];
  std::vector<int> options;
  for (const WorldProp& wp : world->props) {
    const PropDef& def = props::Get(wp.type);
    if (!def.disguisable || wp.type == p.propType || def.volume > 2.5f || def.volume < .003f) continue;
    if (std::find(options.begin(), options.end(), wp.type) == options.end()) options.push_back(wp.type);
  }
  for (int i = (int)options.size() - 1; i > 0; i--) std::swap(options[i], options[rng_.Int(0, i)]);
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(id, others);
  for (int type : options) {
    // Kein Teleportieren durch Wände: die neue Form muss am jetzigen Ort passen.
    if (BoxBlocked(env, PlayerBox(p.ms, type), .02f)) continue;
    SetDisguise(id, type, p.ms);
    return;
  }
}

void Sim::SwapClone(int id) {
  Player& p = players[id];
  if (p.abilityTimer > 0 || phase != Phase::Hunting) return;
  Decoy* nearest = nullptr;
  float best = 20.0f;
  for (Decoy& d : decoys) {
    if (!d.active || d.owner != id) continue;
    float distance = Distance(d.pos, p.ms.pos);
    if (distance < best) { nearest = &d; best = distance; }
  }
  if (!nearest) { GameEvent& e = Emit(EV_NOFIT); e.a = (u8)id; e.b = 1; return; }
  Decoy& d = *nearest;
  MoveState destination = p.ms;
  destination.pos = d.pos;
  destination.colYaw = d.yaw;
  destination.vel = Vec3();
  destination.rotLock = true;
  destination.onGround = false;
  d.active = false;
  std::vector<OBox> others;
  MoveEnv env = MakeEnv(id, others);
  bool blocked = BoxBlocked(env, PlayerBox(destination, d.propType), .02f);
  d.active = true;
  if (blocked) { Emit(EV_NOFIT).a = (u8)id; return; }
  int type = d.propType;
  d.propType = p.propType; d.pos = p.ms.pos; d.yaw = p.ms.colYaw;
  d.life = 45; d.pingTimer = -1;
  SetDisguise(id, type, destination);
  p.prevPos = p.ms.pos; p.prevColYaw = p.ms.colYaw;
  p.abilityTimer = 10;
  GameEvent& e = Emit(EV_CLONE_SWAP); e.a = (u8)id; e.pos = d.pos;
}

void Sim::UpdateMode() {
  if (phase != Phase::Hunting) return;
  if (mode == MODE_ROULETTE) {
    float before = modeTimer;
    modeTimer -= TICK_DT;
    if (before > 5 && modeTimer <= 5) Emit(EV_MODE_WARNING).b = mode;
    if (modeTimer <= 0) {
      modeTimer += ROULETTE_INTERVAL;
      for (int id = 0; id < MAX_PLAYERS; id++)
        if (players[id].active && players[id].alive && players[id].team == TEAM_PROPS) RandomDisguise(id);
      Emit(EV_MODE_PULSE).b = mode;
    }
  }
  for (int id = 0; id < MAX_PLAYERS; id++) {
    Player& p = players[id];
    if (!p.active || !p.alive || p.team != TEAM_PROPS) continue;
    if (mode == MODE_HOT) {
      if (DistanceXZ(p.ms.pos, p.modeAnchor) >= 1.0f) { p.modeMeter = 0; p.modeAnchor = p.ms.pos; }
      else p.modeMeter = std::min(HOT_LIMIT, p.modeMeter + TICK_DT);
      if (p.modeMeter >= HOT_LIMIT && p.tauntCooldown <= 0) {
        p.tauntTimer = 1.1f; p.tauntCooldown = 3.5f;
        GameEvent& e = Emit(EV_TAUNT); e.a = (u8)id; e.b = 7; e.pos = p.ms.pos + Vec3(0, .3f, 0);
        GameEvent& ping = Emit(EV_PING); ping.a = (u8)id; ping.pos = p.ms.pos + Vec3(0, .4f, 0);
      }
    } else if (mode == MODE_CLONES) {
      if (p.decoys >= 3) p.modeMeter = 0;
      else {
        p.modeMeter += TICK_DT;
        if (p.modeMeter >= CLONE_RECHARGE) { p.modeMeter -= CLONE_RECHARGE; p.decoys++; }
      }
    }
  }
  if (mode == MODE_CLONES) {
    for (Decoy& d : decoys) {
      if (!d.active) continue;
      d.life -= TICK_DT;
      if (d.life <= 0) { d.active = false; continue; }
      if (d.pingTimer > 0) {
        d.pingTimer -= TICK_DT;
        if (d.pingTimer <= 0) {
          GameEvent& e = Emit(EV_PING); e.a = (u8)d.owner; e.pos = d.pos + Vec3(0, .4f, 0);
          d.pingTimer = -1;
        }
      }
    }
  }
}
