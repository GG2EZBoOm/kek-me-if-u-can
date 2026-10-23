#include "game/NetGame.h"
#include "game/Maps.h"
#include "game/Invite.h"

using namespace net;

static void Header(Writer& w, NetMsg m) {
  w.U16(NET_MAGIC);
  w.U8(m);
}

static u8 Q8(float v, float scale) { return (u8)Clamp(v * scale + 0.5f, 0.0f, 255.0f); }

static void WriteEvent(Writer& w, const GameEvent& e) {
  w.U32(e.id);
  w.U8(e.type);
  w.U8(e.a);
  w.U8(e.b);
  w.U16(e.c);
  w.Pos16(e.pos);
  w.Dir16(e.dir);
  w.F32(e.f);
  if (e.type == EV_CHAT) w.Str(e.text);
}

static GameEvent ReadEvent(Reader& r) {
  GameEvent e;
  e.id = r.U32();
  e.type = r.U8();
  e.a = r.U8();
  e.b = r.U8();
  e.c = r.U16();
  e.pos = r.Pos16();
  e.dir = r.Dir16();
  e.f = r.F32();
  if (e.type == EV_CHAT) e.text = r.Str();
  return e;
}

static void WritePlayer(Writer& w, const Player& p, int i, bool full) {
  w.U8((u8)i);
  u8 flags = (p.alive ? 1 : 0) | (p.bot ? 2 : 0) | (p.ms.onGround ? 4 : 0) | (p.ms.crouching ? 8 : 0) | (p.ms.rotLock ? 16 : 0);
  w.U8(flags);
  w.U8(p.team);
  w.I16((i16)p.propType);
  if (full) {
    w.V3(p.ms.pos);
    w.V3(p.ms.vel);
  } else {
    w.Pos16(p.ms.pos);
    w.Pos16(p.ms.vel);
  }
  w.Angle(p.ms.yaw);
  w.I16((i16)(p.ms.pitch * 10000));
  w.Angle(p.ms.colYaw);
  w.U16(p.ms.lastButtons);
  w.U8(Q8(p.ms.stamina, 255));
  w.U16((u16)Clamp(p.health * 10, 0.0f, 65535.0f));
  w.U16((u16)Clamp(p.maxHealth * 10, 0.0f, 65535.0f));
  w.U8(p.weapon);
  for (int k = 0; k < WPN_COUNT; k++) w.U8(p.ammo[k]);
  w.U8(Q8(p.reloadTimer, 20));
  w.U8(Q8(p.switchTimer, 20));
  w.U8(Q8(p.tauntTimer, 20));
  w.U8(Q8(p.tauntCooldown, 20));
  w.U8(Q8(p.autoTaunt, 1));
  w.U8(Q8(p.abilityTimer, 1));
  w.U8(Q8(p.disguiseTimer, 20));
  w.U8(Q8(p.hurtTimer, 50));
  w.U8(p.decoys);
  w.U8(Q8(p.modeMeter, 10));
  w.I16((i16)p.score);
  w.U8((u8)std::min(p.kills, 255));
  w.U8((u8)std::min(p.deaths, 255));
  w.U8((u8)std::min(p.ping / 4, 255));
  w.U32(p.lastCmdSeq);
}

static void ReadPlayer(Reader& r, ClientPlayer& p, bool full) {
  u8 flags = r.U8();
  p.active = true;
  p.alive = flags & 1;
  p.bot = (flags & 2) != 0;
  p.onGround = (flags & 4) != 0;
  p.crouching = (flags & 8) != 0;
  p.rotLock = (flags & 16) != 0;
  p.team = (Team)r.U8();
  p.propType = r.I16();
  if (full) {
    p.pos = r.V3();
    p.vel = r.V3();
  } else {
    p.pos = r.Pos16();
    p.vel = r.Pos16();
  }
  p.yaw = r.Angle();
  p.pitch = r.I16() / 10000.0f;
  p.colYaw = r.Angle();
  p.lastButtons = r.U16();
  p.stamina = r.U8() / 255.0f;
  p.health = r.U16() / 10.0f;
  p.maxHealth = r.U16() / 10.0f;
  p.weapon = r.U8();
  for (int k = 0; k < WPN_COUNT; k++) p.ammo[k] = r.U8();
  p.reloadTimer = r.U8() / 20.0f;
  p.switchTimer = r.U8() / 20.0f;
  p.tauntTimer = r.U8() / 20.0f;
  p.tauntCooldown = r.U8() / 20.0f;
  p.autoTaunt = r.U8();
  p.abilityTimer = r.U8();
  p.disguiseTimer = r.U8() / 20.0f;
  p.hurtTimer = r.U8() / 50.0f;
  p.decoys = r.U8();
  p.modeMeter = r.U8() / 10.0f;
  p.score = r.I16();
  p.kills = r.U8();
  p.deaths = r.U8();
  p.ping = r.U8() * 4;
  p.lastCmdSeq = r.U32();
}

static MoveState MoveFromClient(const ClientPlayer& p) {
  MoveState m;
  m.pos = p.pos;
  m.vel = p.vel;
  m.yaw = p.yaw;
  m.pitch = p.pitch;
  m.colYaw = p.colYaw;
  m.stamina = p.stamina;
  m.onGround = p.onGround;
  m.crouching = p.crouching;
  m.rotLock = p.rotLock;
  m.lastButtons = p.lastButtons;
  return m;
}

// ---------------------------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------------------------

bool NetServer::Start(u16 port, const std::string& serverName) {
  name = serverName;
  if (!sock_.Open(port, true)) {
    LOG("Server: Port %d konnte nicht geöffnet werden", port);
    return false;
  }
  LOG("Server läuft auf UDP-Port %d", sock_.LocalPort());
  return true;
}

void NetServer::Stop(Sim& sim) {
  Writer w;
  Header(w, MSG_BYE);
  for (Client& c : clients_)
    if (c.active) {
      sock_.Send(c.addr, w.buf.data(), w.Size());
      sim.RemovePlayer(c.slot);
    }
  clients_.clear();
  sock_.Close();
}

int NetServer::ClientCount() const {
  int n = 0;
  for (const Client& c : clients_)
    if (c.active) n++;
  return n;
}

NetServer::Client* NetServer::Find(const Address& a) {
  for (Client& c : clients_)
    if (c.active && c.addr == a) return &c;
  return nullptr;
}

void NetServer::Receive(Sim& sim, double now) {
  u8 buf[2048];
  Address from;
  int n;
  bool changed = false;
  while ((n = sock_.Recv(from, buf, sizeof(buf))) > 0) {
    Reader r(buf, n);
    if (r.U16() != NET_MAGIC) continue;
    u8 type = r.U8();
    Client* c = Find(from);
    switch (type) {
      case MSG_DISCOVER: {
        Writer w;
        Header(w, MSG_INFO);
        w.U32(PROTOCOL_VERSION);
        w.Str(name);
        const auto& maps = AllMaps();
        w.Str(maps[ClampI(sim.settings.mapIndex, 0, (int)maps.size() - 1)].name);
        w.U8((u8)sim.CountActive());
        w.U8(MAX_PLAYERS);
        w.U16(sock_.LocalPort());
        sock_.Send(from, w.buf.data(), w.Size());
        break;
      }
      case MSG_HELLO: {
        u32 ver = r.U32();
        std::string pname = r.Str();
        auto reject = [&](const char* why) {
          Writer w;
          Header(w, MSG_REJECT);
          w.Str(why);
          sock_.Send(from, w.buf.data(), w.Size());
        };
        if (ver != PROTOCOL_VERSION) { reject("Andere Spielversion"); break; }
        if (!c) {
          if (sim.CountActive() >= MAX_PLAYERS) {
            for (int i = MAX_PLAYERS - 1; i >= 0; i--)
              if (sim.players[i].active && sim.players[i].bot) { sim.RemovePlayer(i); break; }
          }
          if (sim.CountActive() >= MAX_PLAYERS) { reject("Server ist voll"); break; }
          std::string clean;
          for (char ch : pname) if ((u8)ch >= 32) clean += ch;
          if (clean.empty()) clean = "Spieler";
          if (clean.size() > NAME_LEN - 4) clean.resize(NAME_LEN - 4);
          std::string unique = clean;
          for (int k = 2; k < 20; k++) {
            bool used = false;
            for (const Player& p : sim.players)
              if (p.active && unique == p.name) used = true;
            if (!used) break;
            unique = clean + StrFormat(" %d", k);
          }
          int slot = sim.AddPlayer(unique, false);
          if (slot < 0) { reject("Server ist voll"); break; }
          Client nc;
          nc.active = true;
          nc.addr = from;
          nc.slot = slot;
          nc.lastRecv = now;
          nc.ackEvent = eventLog_.empty() ? 0 : eventLog_.back().id;
          clients_.push_back(nc);
          c = &clients_.back();
          changed = true;
          LOG("Spieler '%s' verbunden von %s (Slot %d)", unique.c_str(), from.ToString().c_str(), slot);
        }
        Writer w;
        Header(w, MSG_WELCOME);
        w.U8((u8)c->slot);
        w.U8((u8)sim.settings.mapIndex);
        w.U8((u8)sim.settings.rounds);
        w.U8(sim.settings.infection ? 1 : 0);
        w.Str(name);
        sock_.Send(from, w.buf.data(), w.Size());
        c->lastRecv = now;
        break;
      }
      case MSG_CMDS: {
        if (!c) break;
        c->lastRecv = now;
        int count = r.U8();
        UserCmd cmds[8];
        count = std::min(count, 8);
        for (int k = 0; k < count; k++) {
          cmds[k].seq = r.U32();
          cmds[k].yaw = r.F32();
          cmds[k].pitch = r.F32();
          cmds[k].buttons = r.U16();
          cmds[k].weapon = r.U8();
          cmds[k].taunt = r.U8();
        }
        u32 ackEvent = r.U32();
        u32 ackTick = r.U32();
        if (!r.Ok()) break;
        for (int k = 0; k < count; k++) {
          if (cmds[k].seq > c->lastCmdSeq) {
            sim.QueueCmd(c->slot, cmds[k]);
            c->lastCmdSeq = cmds[k].seq;
          }
        }
        if (ackEvent > c->ackEvent) c->ackEvent = ackEvent;
        for (auto& st : sentTimes_)
          if (st.first == ackTick) {
            int ms = (int)((now - st.second) * 1000.0);
            Player& p = sim.players[c->slot];
            p.ping = p.ping == 0 ? ms : (p.ping * 7 + ms) / 8;
            break;
          }
        break;
      }
      case MSG_CHAT:
        if (c) {
          std::string t = r.Str();
          if (!t.empty()) sim.Chat(c->slot, t);
        }
        break;
      case MSG_BYE:
        if (c) {
          LOG("Spieler '%s' hat das Spiel verlassen", sim.players[c->slot].name);
          sim.RemovePlayer(c->slot);
          c->active = false;
          changed = true;
        }
        break;
    }
  }
  for (Client& c : clients_) {
    if (c.active && now - c.lastRecv > 10.0) {
      LOG("Zeitüberschreitung: '%s'", sim.players[c.slot].name);
      sim.RemovePlayer(c.slot);
      c.active = false;
      changed = true;
    }
  }
  clients_.erase(std::remove_if(clients_.begin(), clients_.end(), [](const Client& c) { return !c.active; }), clients_.end());
  if (changed && onPlayersChanged) onPlayersChanged();
}

void NetServer::PostTick(Sim& sim, const std::vector<GameEvent>& evs, double now) {
  for (const GameEvent& e : evs) eventLog_.push_back(e);
  while (eventLog_.size() > 512) eventLog_.pop_front();
  if ((++sendCounter_ & 1) != 0) return;
  for (Client& c : clients_)
    if (c.active) SendSnapshot(sim, c, now);
  sentTimes_.push_back({sim.tick, now});
  while (sentTimes_.size() > 64) sentTimes_.pop_front();
}

void NetServer::SendSnapshot(Sim& sim, Client& c, double now) {
  (void)now;
  Writer w;
  Header(w, MSG_SNAPSHOT);
  w.U32(sim.tick);
  w.U8((u8)sim.phase);
  w.F32(sim.phaseTimer);
  w.U8((u8)sim.round);
  w.U8((u8)sim.settings.rounds);
  w.U8((u8)sim.winner);
  w.U8((u8)sim.settings.mapIndex);
  bool names = (c.snapshots % 15) == 0;
  w.U8((sim.settings.infection ? 1 : 0) | (names ? 2 : 0));
  w.U8(sim.mode);
  w.U8((u8)sim.settings.mode);
  w.F32(sim.modeTimer);
  u16 mask = 0;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (sim.players[i].active) mask |= (u16)(1 << i);
  w.U16(mask);
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (sim.players[i].active) WritePlayer(w, sim.players[i], i, i == c.slot);
  int nd = 0;
  for (const Decoy& d : sim.decoys) if (d.active) nd++;
  w.U8((u8)nd);
  for (int i = 0; i < MAX_DECOYS; i++) {
    const Decoy& d = sim.decoys[i];
    if (!d.active) continue;
    w.U8((u8)i);
    w.I16((i16)d.propType);
    w.Pos16(d.pos);
    w.Angle(d.yaw);
  }
  int ng = 0;
  for (const Grenade& g : sim.grenades) if (g.active) ng++;
  w.U8((u8)ng);
  for (int i = 0; i < MAX_GRENADES; i++) {
    if (!sim.grenades[i].active) continue;
    w.U8((u8)i);
    w.Pos16(sim.grenades[i].pos);
  }
  if (names) {
    for (int i = 0; i < MAX_PLAYERS; i++)
      if (sim.players[i].active) {
        w.U8((u8)i);
        w.Str(sim.players[i].name);
      }
  }
  sock_.Send(c.addr, w.buf.data(), w.Size());
  // Ereignisse dürfen bei voller Lobby mit vielen Klonen nicht vom Snapshot verdrängt werden.
  // Ein eigenes kleines Paket wird bis zur Bestätigung erneut gesendet.
  Writer ew;
  Header(ew, MSG_EVENTS);
  ew.U8(0);
  int ne = 0;
  for (const GameEvent& e : eventLog_) {
    if (e.id <= c.ackEvent) continue;
    Writer entry; WriteEvent(entry, e);
    if (ne >= 24 || ew.Size() + entry.Size() > 1200) break;
    ew.buf.insert(ew.buf.end(), entry.buf.begin(), entry.buf.end());
    ne++;
  }
  ew.buf[3] = (u8)ne;
  if (ne) sock_.Send(c.addr, ew.buf.data(), ew.Size());
  if (c.snapshots % 15 == 0) {
    Writer address;
    Header(address, MSG_INTERNET_ADDRESS);
    address.U32(sim.tick);
    address.Str(InternetInviteAddress(internetAddress));
    sock_.Send(c.addr, address.buf.data(), address.Size());
  }
  c.snapshots++;
}

// ---------------------------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------------------------

std::string NetClient::InternetAddress() const {
  if (!internetAddress_.empty()) return internetAddress_;
  return InternetInviteAddress(server_.ToString());
}

NetClient::~NetClient() {
  if (state_ != FailedState && sock_.IsOpen()) {
    Writer w;
    Header(w, MSG_BYE);
    for (int i = 0; i < 3; i++) sock_.Send(server_, w.buf.data(), w.Size());
  }
}

bool NetClient::Connect(const std::string& address, const std::string& playerName) {
  name_ = playerName;
  if (!Address::Parse(address, GAME_PORT, server_)) {
    state_ = FailedState;
    status_ = "Adresse ungültig: " + address;
    return false;
  }
  if (!sock_.Open(0, false)) {
    state_ = FailedState;
    status_ = "Netzwerk konnte nicht geöffnet werden";
    return false;
  }
  state_ = Connecting;
  status_ = "Verbinde mit " + server_.ToString() + " ...";
  helloTimer_ = 0;
  props::BuildCatalog();
  return true;
}

int NetClient::LocalPropType() const {
  if (!haveSnap_ || slot_ < 0) return -1;
  const ClientPlayer& me = snaps_.back().st.players[slot_];
  return me.team == TEAM_PROPS ? me.propType : -1;
}

void NetClient::TakeEvents(std::vector<GameEvent>& out) {
  out.insert(out.end(), events_.begin(), events_.end());
  events_.clear();
}

void NetClient::SendChat(const std::string& text) {
  Writer w;
  Header(w, MSG_CHAT);
  w.Str(text);
  sock_.Send(server_, w.buf.data(), w.Size());
}

MoveEnv NetClient::PredictEnv(std::vector<OBox>& storage) const {
  storage.clear();
  const ClientState& L = snaps_.back().st;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const ClientPlayer& q = L.players[i];
    if (i == slot_ || !q.active || !q.alive) continue;
    storage.push_back(PlayerBox(MoveFromClient(q), q.team == TEAM_PROPS ? q.propType : -1));
  }
  for (const ClientDecoy& d : L.decoys) {
    if (!d.active) continue;
    MoveState t;
    t.pos = d.pos;
    t.colYaw = d.yaw;
    storage.push_back(PlayerBox(t, d.propType));
  }
  MoveEnv env;
  env.world = &world_;
  env.mode = L.mode;
  env.others = &storage;
  return env;
}

void NetClient::HandlePacket(const u8* data, int size) {
  Reader r(data, size);
  if (r.U16() != NET_MAGIC) return;
  u8 type = r.U8();
  switch (type) {
    case MSG_WELCOME: {
      int slot = r.U8();
      int map = r.U8();
      r.U8();
      r.U8();
      serverName_ = r.Str();
      if (!r.Ok()) return;
      slot_ = slot;
      if (!worldReady_) {
        if (!BuildMap(map, world_)) {
          state_ = FailedState;
          status_ = "Unbekannte Karte";
          return;
        }
        worldReady_ = true;
      }
      if (state_ == Connecting) LOG("Verbunden mit '%s' als Slot %d", serverName_.c_str(), slot_);
      state_ = Connected;
      status_.clear();
      lastRecv_ = time_;
      break;
    }
    case MSG_REJECT:
      state_ = FailedState;
      status_ = "Abgelehnt: " + r.Str();
      break;
    case MSG_BYE:
      state_ = FailedState;
      status_ = "Der Server wurde beendet";
      break;
    case MSG_INTERNET_ADDRESS: {
      if (state_ != Connected) return;
      u32 tick = r.U32();
      std::string address = r.Str();
      if (r.Ok() && tick >= internetAddressTick_) {
        internetAddress_ = InternetInviteAddress(address);
        internetAddressTick_ = tick;
      }
      break;
    }
    case MSG_EVENTS: {
      if (state_ != Connected) return;
      int ne = r.U8();
      u32 maxId = ackEvent_;
      std::vector<GameEvent> fresh;
      for (int k = 0; k < ne; k++) {
        GameEvent e = ReadEvent(r);
        if (!r.Ok()) return;
        if (e.id > ackEvent_) { fresh.push_back(e); maxId = std::max(maxId, e.id); }
      }
      if (!r.Ok()) return;
      events_.insert(events_.end(), fresh.begin(), fresh.end());
      ackEvent_ = maxId;
      break;
    }
    case MSG_SNAPSHOT: {
      if (state_ != Connected || slot_ < 0) return;
      Snap s;
      s.tick = r.U32();
      ClientState& st = s.st;
      st.valid = true;
      st.tick = s.tick;
      st.phase = (Phase)r.U8();
      st.phaseTimer = r.F32();
      st.round = r.U8();
      st.maxRounds = r.U8();
      st.winner = (Team)r.U8();
      st.mapIndex = r.U8();
      u8 flags = r.U8();
      st.infection = flags & 1;
      st.mode = (GameMode)ClampI(r.U8(), 0, MODE_MOON);
      st.nextMode = ClampI(r.U8(), 0, MODE_COUNT - 1);
      st.modeTimer = r.F32();
      u16 mask = r.U16();
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!(mask & (1 << i))) continue;
        int idx = r.U8();
        if (idx < 0 || idx >= MAX_PLAYERS) return;
        ReadPlayer(r, st.players[idx], idx == slot_);
      }
      int nd = r.U8();
      for (int k = 0; k < nd; k++) {
        int idx = r.U8();
        ClientDecoy d;
        d.active = true;
        d.propType = r.I16();
        d.pos = r.Pos16();
        d.yaw = r.Angle();
        if (idx < MAX_DECOYS) st.decoys[idx] = d;
      }
      int ng = r.U8();
      for (int k = 0; k < ng; k++) {
        int idx = r.U8();
        Vec3 p = r.Pos16();
        if (idx < MAX_GRENADES) { st.grenades[idx].active = true; st.grenades[idx].pos = p; }
      }
      if (flags & 2) {
        while (!r.AtEnd()) {
          int idx = r.U8();
          std::string n = r.Str();
          if (!r.Ok()) break;
          if (idx < MAX_PLAYERS) names_[idx] = n;
        }
      }
      if (!r.Ok()) return;
      lastRecv_ = time_;
      if (s.tick <= lastSnapTick_ && haveSnap_) return;  // veraltet
      lastSnapTick_ = s.tick;
      for (int i = 0; i < MAX_PLAYERS; i++)
        if (st.players[i].active) strncpy(st.players[i].name, names_[i].c_str(), NAME_LEN - 1);
      // Abgleich der eigenen Vorhersage
      const ClientPlayer& me = st.players[slot_];
      MoveState server = MoveFromClient(me);
      while (!pending_.empty() && pending_.front().seq <= me.lastCmdSeq) pending_.pop_front();
      snaps_.push_back(s);
      while (snaps_.size() > 40) snaps_.pop_front();
      if (!haveSnap_ || !me.alive || Distance(server.pos, pred_.pos) > 4.0f) {
        pred_ = server;
        predPrev_ = server;
        smoothErr_ = Vec3();
        for (const UserCmd& c : pending_) {
          std::vector<OBox> stg;
          MoveEnv env = PredictEnv(stg);
          ApplyMovement(env, pred_, c, me.team == TEAM_PROPS ? me.propType : -1, me.team == TEAM_HUNTERS && st.phase == Phase::Hiding);
        }
        predPrev_ = pred_;
      } else {
        Vec3 old = pred_.pos;
        pred_ = server;
        std::vector<OBox> stg;
        MoveEnv env = PredictEnv(stg);
        for (const UserCmd& c : pending_)
          ApplyMovement(env, pred_, c, me.team == TEAM_PROPS ? me.propType : -1, me.team == TEAM_HUNTERS && st.phase == Phase::Hiding);
        Vec3 d = pred_.pos - old;
        predPrev_.pos += d;
        smoothErr_ -= d;
        if (Length(smoothErr_) > 1.0f) smoothErr_ = Vec3();
      }
      if (!haveSnap_) renderTick_ = s.tick - 6.0;
      haveSnap_ = true;
      break;
    }
  }
}

void NetClient::Update(double frameDt) {
  time_ += frameDt;
  u8 buf[4096];  // volle Lobby, 48 Klone und alle Spielernamen
  Address from;
  int n;
  while ((n = sock_.Recv(from, buf, sizeof(buf))) > 0)
    if (from == server_) HandlePacket(buf, n);
  if (state_ == FailedState) return;
  if (state_ == Connecting) {
    helloTimer_ -= frameDt;
    if (helloTimer_ <= 0) {
      Writer w;
      Header(w, MSG_HELLO);
      w.U32(PROTOCOL_VERSION);
      w.Str(name_);
      sock_.Send(server_, w.buf.data(), w.Size());
      helloTimer_ = 0.5;
    }
    if (time_ > 8.0) {
      state_ = FailedState;
      status_ = "Der Server antwortet nicht (" + server_.ToString() + ")";
    }
    return;
  }
  if (time_ - lastRecv_ > 10.0) {
    state_ = FailedState;
    status_ = "Verbindung zum Server verloren";
    return;
  }
  if (!haveSnap_) return;
  const ClientState& L = snaps_.back().st;
  acc_ += frameDt;
  if (acc_ > 0.25) acc_ = 0.25;
  while (acc_ >= TICK_DT) {
    acc_ -= TICK_DT;
    UserCmd cmd = makeCmd ? makeCmd() : UserCmd();
    cmd.seq = ++cmdSeq_;
    pending_.push_back(cmd);
    while (pending_.size() > 120) pending_.pop_front();
    predPrev_ = pred_;
    const ClientPlayer& me = L.players[slot_];
    if (me.alive) {
      std::vector<OBox> stg;
      MoveEnv env = PredictEnv(stg);
      ApplyMovement(env, pred_, cmd, me.team == TEAM_PROPS ? me.propType : -1, me.team == TEAM_HUNTERS && L.phase == Phase::Hiding);
    } else {
      pred_.yaw = cmd.yaw;
      pred_.pitch = cmd.pitch;
    }
    Writer w;
    Header(w, MSG_CMDS);
    int count = std::min<int>(3, (int)pending_.size());
    w.U8((u8)count);
    for (int k = (int)pending_.size() - count; k < (int)pending_.size(); k++) {
      const UserCmd& c = pending_[k];
      w.U32(c.seq);
      w.F32(c.yaw);
      w.F32(c.pitch);
      w.U16(c.buttons);
      w.U8(c.weapon);
      w.U8(c.taunt);
    }
    w.U32(ackEvent_);
    w.U32(lastSnapTick_);
    sock_.Send(server_, w.buf.data(), w.Size());
  }
  renderTick_ += frameDt * TICK_RATE;
  double target = (double)snaps_.back().tick - 6.0;
  if (fabs(renderTick_ - target) > 30.0) renderTick_ = target;
  else renderTick_ += (target - renderTick_) * 0.03;
  smoothErr_ *= expf(-(float)frameDt * 10.0f);
  BuildView((float)(acc_ / TICK_DT));
}

void NetClient::BuildView(float alpha) {
  const Snap& latest = snaps_.back();
  const Snap* a = &latest;
  const Snap* b = &latest;
  for (size_t i = 0; i + 1 < snaps_.size(); i++) {
    if (snaps_[i].tick <= renderTick_ && snaps_[i + 1].tick > renderTick_) {
      a = &snaps_[i];
      b = &snaps_[i + 1];
      break;
    }
  }
  if (renderTick_ < snaps_.front().tick) a = b = &snaps_.front();
  float t = b->tick > a->tick ? (float)((renderTick_ - a->tick) / (double)(b->tick - a->tick)) : 1.0f;
  t = Saturate(t);
  view_ = latest.st;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (i == slot_) continue;
    const ClientPlayer& pa = a->st.players[i];
    const ClientPlayer& pb = b->st.players[i];
    if (!pb.active) {
      view_.players[i] = pb;
      continue;
    }
    ClientPlayer p = pb;
    strncpy(p.name, latest.st.players[i].name, NAME_LEN - 1);
    p.score = latest.st.players[i].score;
    p.kills = latest.st.players[i].kills;
    p.deaths = latest.st.players[i].deaths;
    p.ping = latest.st.players[i].ping;
    if (pa.active && pa.alive == pb.alive && Distance(pa.pos, pb.pos) < 5.0f) {
      p.pos = Lerp(pa.pos, pb.pos, t);
      p.yaw = LerpAngle(pa.yaw, pb.yaw, t);
      p.pitch = Lerp(pa.pitch, pb.pitch, t);
      p.colYaw = LerpAngle(pa.colYaw, pb.colYaw, t);
    }
    view_.players[i] = p;
  }
  for (int i = 0; i < MAX_DECOYS; i++) view_.decoys[i] = b->st.decoys[i];
  for (int i = 0; i < MAX_GRENADES; i++) {
    view_.grenades[i] = b->st.grenades[i];
    if (a->st.grenades[i].active && b->st.grenades[i].active)
      view_.grenades[i].pos = Lerp(a->st.grenades[i].pos, b->st.grenades[i].pos, t);
  }
  if (slot_ >= 0) {
    ClientPlayer& me = view_.players[slot_];
    me.pos = Lerp(predPrev_.pos, pred_.pos, alpha) + smoothErr_;
    me.vel = pred_.vel;
    me.yaw = pred_.yaw;
    me.pitch = pred_.pitch;
    me.colYaw = LerpAngle(predPrev_.colYaw, pred_.colYaw, alpha);
    me.onGround = pred_.onGround;
    me.crouching = pred_.crouching;
    me.rotLock = pred_.rotLock;
    me.stamina = pred_.stamina;
  }
}

// ---------------------------------------------------------------------------------------------
// LAN-Suche
// ---------------------------------------------------------------------------------------------

void LanBrowser::Start() {
  sock_.Open(0, true);
  servers_.clear();
  nextQuery_ = 0;
}

void LanBrowser::Stop() { sock_.Close(); }

void LanBrowser::Update(double now) {
  if (!sock_.IsOpen()) return;
  if (now >= nextQuery_) {
    Writer w;
    Header(w, MSG_DISCOVER);
    sock_.Send(Address::Broadcast(GAME_PORT), w.buf.data(), w.Size());
    sock_.Send(Address::Loopback(GAME_PORT), w.buf.data(), w.Size());
    nextQuery_ = now + 2.0;
  }
  u8 buf[1024];
  Address from;
  int n;
  while ((n = sock_.Recv(from, buf, sizeof(buf))) > 0) {
    Reader r(buf, n);
    if (r.U16() != NET_MAGIC || r.U8() != MSG_INFO) continue;
    u32 ver = r.U32();
    LanServer s;
    s.name = r.Str();
    s.map = r.Str();
    s.players = r.U8();
    s.maxPlayers = r.U8();
    u16 port = r.U16();
    if (!r.Ok() || ver != PROTOCOL_VERSION) continue;
    s.addr = from;
    s.addr.port = port;
    s.lastSeen = now;
    bool found = false;
    for (LanServer& o : servers_) {
      // Loopback und echte Adresse desselben Rechners nicht doppelt zeigen
      if (o.addr == s.addr || (o.name == s.name && o.addr.port == s.addr.port)) {
        if (o.addr.ip == 0x7F000001u) o.addr = s.addr;
        o.players = s.players;
        o.lastSeen = now;
        found = true;
      }
    }
    if (!found) servers_.push_back(s);
  }
  servers_.erase(std::remove_if(servers_.begin(), servers_.end(), [&](const LanServer& s) { return now - s.lastSeen > 6.0; }),
                 servers_.end());
}
