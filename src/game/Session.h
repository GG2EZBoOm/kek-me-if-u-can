#pragma once
// Spielsitzung: lokal/Host (eigene Simulation) oder entfernt (Netzwerk-Client).
#include "game/Bot.h"
#include "game/ClientState.h"
#include "game/Sim.h"

class NetServer;
class NetClient;

class Session {
 public:
  virtual ~Session() = default;
  // Liefert den Befehl des lokalen Spielers für den aktuellen Tick
  std::function<UserCmd()> makeCmd;
  virtual void Update(double frameDt) = 0;
  virtual const ClientState& State() const = 0;
  virtual int LocalId() const = 0;
  virtual const World& GetWorld() const = 0;
  virtual bool Ready() const = 0;
  virtual bool Failed() const { return false; }
  virtual std::string StatusText() const { return ""; }
  virtual bool IsHost() const = 0;
  virtual void SetNextMode(int) {}  // ausschließlich LocalSession/Host darf Rundenregeln ändern
  virtual void TakeEvents(std::vector<GameEvent>& out) = 0;
  virtual void SendChat(const std::string& text) = 0;
  // Bewegungszustand des lokalen Spielers (vorhergesagt bzw. autoritativ)
  virtual const MoveState* LocalMove() const = 0;
  virtual int LocalPropType() const = 0;
  virtual std::string ServerInfo() const { return ""; }
};

class LocalSession : public Session {
 public:
  LocalSession();
  ~LocalSession() override;
  bool Start(const MatchSettings& s, const std::string& playerName, bool network, const std::string& serverName);
  void Update(double frameDt) override;
  const ClientState& State() const override { return state_; }
  int LocalId() const override { return localId_; }
  const World& GetWorld() const override { return world_; }
  bool Ready() const override { return ready_; }
  bool IsHost() const override { return true; }
  void SetNextMode(int mode) override { sim_.settings.mode = ClampI(mode, 0, MODE_COUNT - 1); }
  void TakeEvents(std::vector<GameEvent>& out) override;
  void SendChat(const std::string& text) override;
  const MoveState* LocalMove() const override;
  int LocalPropType() const override;
  std::string ServerInfo() const override;
  Sim& GetSim() { return sim_; }
  // Für dedizierte Server ohne lokalen Spieler
  void SetDedicated(bool d) { dedicated_ = d; }

 private:
  void BuildState(float alpha);
  void BalanceBots();

  World world_;
  Sim sim_;
  BotManager bots_;
  std::unique_ptr<NetServer> net_;
  ClientState state_;
  std::vector<GameEvent> events_;
  double acc_ = 0, clock_ = 0;
  int localId_ = -1;
  bool ready_ = false;
  bool dedicated_ = false;
  Rng rng_{99};
  int desiredBots_ = 0;
};

// Füllt den Client-Zustand aus der Simulation (mit Interpolation alpha zwischen letztem und aktuellem Tick)
void FillClientState(const Sim& sim, ClientState& out, float alpha);
