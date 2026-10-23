#pragma once
// Discord Rich Presence über die lokale IPC-Schnittstelle (Named Pipe "discord-ipc-N") – ohne SDK.
// Zeigt Karte, Phase und Gruppengröße im Discord-Profil; Freunde können über "Beitreten" mitspielen.
#include "engine/Common.h"

struct DiscordActivity {
  std::string details, state;
  std::string largeImage = "logo", largeText = "Kek Me If You Can";
  long long startTime = 0;
  std::string partyId;
  int partySize = 0, partyMax = 0;
  std::string joinSecret;  // Einladungscode; leer = kein Beitreten
};

class DiscordRpc {
 public:
  ~DiscordRpc();
  // Registriert den Startbefehl für Discord und versucht die erste Verbindung
  void Init(const std::string& appId);
  // Ereignisse lesen, bei Bedarf neu verbinden (now = Sekunden)
  void Update(double now);
  // Wird nur gesendet, wenn sich etwas geändert hat
  void SetActivity(const DiscordActivity& a);
  void ClearActivity();
  void RespondJoinRequest(const std::string& userId, bool accept);
  bool Connected() const { return ready_; }
  const std::string& UserName() const { return userName_; }

  std::function<void(const std::string& secret)> onJoin;
  std::function<void(const std::string& userId, const std::string& name)> onJoinRequest;

 private:
  bool Open();
  void Close();
  bool Write(int op, const std::string& json);
  bool ReadFrame(int& op, std::string& json);
  void Handle(const std::string& json);
  void Send(const std::string& cmd, const std::string& argsJson, const char* evt = nullptr);

  void* pipe_ = nullptr;
  std::string appId_;
  bool ready_ = false;
  double nextTry_ = 0;
  int nonce_ = 0;
  std::string lastActivity_;
  std::string userName_;
};
