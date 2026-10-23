#include "game/Session.h"
#include "game/Maps.h"
#include "game/NetGame.h"

void FillClientState(const Sim& sim, ClientState& out, float alpha) {
  out.valid = true;
  out.tick = sim.tick;
  out.phase = sim.phase;
  out.phaseTimer = sim.phaseTimer;
  out.round = sim.round;
  out.maxRounds = sim.settings.rounds;
  out.winner = sim.winner;
  out.mapIndex = sim.settings.mapIndex;
  out.infection = sim.settings.infection;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const Player& p = sim.players[i];
    ClientPlayer& c = out.players[i];
    c = ClientPlayer();
    if (!p.active) continue;
    c.active = true;
    c.bot = p.bot;
    memcpy(c.name, p.name, NAME_LEN);
    c.team = p.team;
    c.alive = p.alive;
    bool smooth = Distance(p.prevPos, p.ms.pos) < 2.0f;
    c.pos = smooth ? Lerp(p.prevPos, p.ms.pos, alpha) : p.ms.pos;
    c.vel = p.ms.vel;
    c.yaw = p.ms.yaw;
    c.pitch = p.ms.pitch;
    c.colYaw = smooth ? LerpAngle(p.prevColYaw, p.ms.colYaw, alpha) : p.ms.colYaw;
    c.onGround = p.ms.onGround;
    c.crouching = p.ms.crouching;
    c.rotLock = p.ms.rotLock;
    c.lastButtons = p.ms.lastButtons;
    c.propType = p.propType;
    c.health = p.health;
    c.maxHealth = p.maxHealth;
    c.weapon = p.weapon;
    memcpy(c.ammo, p.ammo, sizeof(c.ammo));
    c.reloadTimer = p.reloadTimer;
    c.switchTimer = p.switchTimer;
    c.fireTimer = p.fireTimer;
    c.tauntTimer = p.tauntTimer;
    c.tauntCooldown = p.tauntCooldown;
    c.autoTaunt = p.autoTaunt;
    c.abilityTimer = p.abilityTimer;
    c.disguiseTimer = p.disguiseTimer;
    c.hurtTimer = p.hurtTimer;
    c.stamina = p.ms.stamina;
    c.decoys = p.decoys;
    c.score = p.score;
    c.kills = p.kills;
    c.deaths = p.deaths;
    c.ping = p.ping;
    c.lastCmdSeq = p.lastCmdSeq;
  }
  for (int i = 0; i < MAX_DECOYS; i++) {
    const Decoy& d = sim.decoys[i];
    out.decoys[i].active = d.active;
    out.decoys[i].propType = d.propType;
    out.decoys[i].pos = d.pos;
    out.decoys[i].yaw = d.yaw;
  }
  for (int i = 0; i < MAX_GRENADES; i++) {
    out.grenades[i].active = sim.grenades[i].active;
    out.grenades[i].pos = sim.grenades[i].pos;
  }
}

LocalSession::LocalSession() = default;

LocalSession::~LocalSession() {
  if (net_) net_->Stop(sim_);
}

bool LocalSession::Start(const MatchSettings& s, const std::string& playerName, bool network, const std::string& serverName) {
  props::BuildCatalog();
  if (!BuildMap(s.mapIndex, world_)) return false;
  bots_.Init(world_);
  sim_.Init(&world_, s);
  desiredBots_ = ClampI(s.bots, 0, MAX_PLAYERS - 1);
  if (!dedicated_) localId_ = sim_.AddPlayer(playerName.empty() ? "Spieler" : playerName, false);
  for (int i = 0; i < desiredBots_; i++) sim_.AddPlayer(BotManager::RandomName(rng_, sim_), true);
  if (network) {
    net_ = std::make_unique<NetServer>();
    if (!net_->Start(GAME_PORT, serverName)) {
      net_.reset();
      return false;
    }
    net_->onPlayersChanged = [this]() { BalanceBots(); };
  }
  BuildState(1.0f);
  ready_ = true;
  return true;
}

void LocalSession::BalanceBots() {
  int humans = 0, bots = 0;
  for (const Player& p : sim_.players)
    if (p.active) (p.bot ? bots : humans)++;
  int target = std::min(desiredBots_, MAX_PLAYERS - humans);
  while (bots > target) {
    // zuerst tote/zuschauende Bots entfernen
    int victim = -1;
    for (int i = MAX_PLAYERS - 1; i >= 0; i--) {
      const Player& p = sim_.players[i];
      if (p.active && p.bot && (!p.alive || victim < 0)) {
        victim = i;
        if (!p.alive) break;
      }
    }
    if (victim < 0) break;
    sim_.RemovePlayer(victim);
    bots--;
  }
  while (bots < target) {
    if (sim_.AddPlayer(BotManager::RandomName(rng_, sim_), true) < 0) break;
    bots++;
  }
}

void LocalSession::Update(double frameDt) {
  acc_ += frameDt;
  if (acc_ > 0.25) acc_ = 0.25;
  while (acc_ >= TICK_DT) {
    acc_ -= TICK_DT;
    clock_ += TICK_DT;
    if (net_) net_->Receive(sim_, clock_);
    if (localId_ >= 0 && makeCmd) sim_.QueueCmd(localId_, makeCmd());
    bots_.Think(sim_);
    sim_.Tick();
    bots_.OnEvents(sim_, sim_.events);
    events_.insert(events_.end(), sim_.events.begin(), sim_.events.end());
    if (net_) net_->PostTick(sim_, sim_.events, clock_);
    sim_.events.clear();
  }
  if (events_.size() > 2000) events_.erase(events_.begin(), events_.begin() + 1000);
  BuildState((float)(acc_ / TICK_DT));
}

void LocalSession::BuildState(float alpha) { FillClientState(sim_, state_, alpha); }

void LocalSession::TakeEvents(std::vector<GameEvent>& out) {
  out.insert(out.end(), events_.begin(), events_.end());
  events_.clear();
}

void LocalSession::SendChat(const std::string& text) {
  if (localId_ >= 0) sim_.Chat(localId_, text);
}

const MoveState* LocalSession::LocalMove() const { return localId_ >= 0 ? &sim_.players[localId_].ms : nullptr; }

int LocalSession::LocalPropType() const {
  if (localId_ < 0) return -1;
  const Player& p = sim_.players[localId_];
  return p.team == TEAM_PROPS ? p.propType : -1;
}

std::string LocalSession::ServerInfo() const {
  if (!net_) return "Offline-Spiel";
  return StrFormat("Server \"%s\" – UDP-Port %d – %d Spieler verbunden", net_->name.c_str(), net_->Port(), net_->ClientCount());
}
