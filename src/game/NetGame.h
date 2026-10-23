#pragma once
// Netzwerkspiel: autoritativer Server, Client mit Vorhersage/Interpolation, LAN-Suche.
#include "engine/Net.h"
#include "game/Session.h"
#include <deque>

enum NetMsg : u8 {
  MSG_HELLO = 1,
  MSG_WELCOME,
  MSG_REJECT,
  MSG_CMDS,
  MSG_SNAPSHOT,
  MSG_CHAT,
  MSG_BYE,
  MSG_DISCOVER,
  MSG_INFO,
  MSG_EVENTS,
};
constexpr u16 NET_MAGIC = 0x4850;

class NetServer {
 public:
  bool Start(u16 port, const std::string& name);
  void Stop(Sim& sim);
  // Pakete verarbeiten (vor Sim::Tick)
  void Receive(Sim& sim, double now);
  // Ereignisse sammeln und Snapshots senden (nach Sim::Tick)
  void PostTick(Sim& sim, const std::vector<GameEvent>& evs, double now);
  int ClientCount() const;
  u16 Port() const { return sock_.LocalPort(); }
  std::function<void()> onPlayersChanged;
  std::string name;

 private:
  struct Client {
    bool active = false;
    net::Address addr;
    int slot = -1;
    double lastRecv = 0;
    u32 ackEvent = 0;
    u32 lastCmdSeq = 0;
    u32 snapshots = 0;
  };
  void SendSnapshot(Sim& sim, Client& c, double now);
  Client* Find(const net::Address& a);
  net::Socket sock_;
  std::vector<Client> clients_;
  std::deque<GameEvent> eventLog_;
  u32 sendCounter_ = 0;
  std::deque<std::pair<u32, double>> sentTimes_;
};

class NetClient : public Session {
 public:
  ~NetClient() override;
  bool Connect(const std::string& address, const std::string& playerName);
  void Update(double frameDt) override;
  const ClientState& State() const override { return view_; }
  int LocalId() const override { return slot_; }
  const World& GetWorld() const override { return world_; }
  bool Ready() const override { return state_ == Connected && worldReady_ && haveSnap_; }
  bool Failed() const override { return state_ == FailedState; }
  std::string StatusText() const override { return status_; }
  bool IsHost() const override { return false; }
  void TakeEvents(std::vector<GameEvent>& out) override;
  void SendChat(const std::string& text) override;
  const MoveState* LocalMove() const override { return &pred_; }
  int LocalPropType() const override;
  std::string ServerInfo() const override { return serverName_ + " (" + server_.ToString() + ")"; }

 private:
  enum ConnState { Connecting, Connected, FailedState };
  struct Snap {
    u32 tick = 0;
    ClientState st;
  };
  void HandlePacket(const u8* data, int size);
  void BuildView(float alpha);
  MoveEnv PredictEnv(std::vector<OBox>& storage) const;

  net::Socket sock_;
  net::Address server_;
  ConnState state_ = Connecting;
  std::string status_ = "Verbinde ...";
  std::string name_;
  std::string serverName_;
  double time_ = 0, helloTimer_ = 0, lastRecv_ = 0;
  int slot_ = -1;
  World world_;
  bool worldReady_ = false;
  bool haveSnap_ = false;
  std::deque<Snap> snaps_;
  double renderTick_ = 0;
  MoveState pred_, predPrev_;
  Vec3 smoothErr_;
  u32 cmdSeq_ = 0;
  std::deque<UserCmd> pending_;
  double acc_ = 0;
  ClientState view_;
  u32 ackEvent_ = 0;
  u32 lastSnapTick_ = 0;
  std::vector<GameEvent> events_;
  std::string names_[MAX_PLAYERS];
};

struct LanServer {
  net::Address addr;
  std::string name, map;
  int players = 0, maxPlayers = MAX_PLAYERS;
  double lastSeen = 0;
};

class LanBrowser {
 public:
  void Start();
  void Stop();
  void Update(double now);
  void Refresh() { nextQuery_ = 0; }
  const std::vector<LanServer>& Servers() const { return servers_; }

 private:
  net::Socket sock_;
  std::vector<LanServer> servers_;
  double nextQuery_ = 0;
};
