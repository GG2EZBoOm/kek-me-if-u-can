#pragma once
// Automatische Updates über GitHub-Releases: HTTPS über WinHTTP, SHA-256-Prüfung über BCrypt (beides Teil von Windows).
// Ablauf im Hintergrund: neueste Version abfragen, EXEs als *.new laden und prüfen, dann austauschen
// (die laufende EXE wird zu *.old umbenannt – das erlaubt Windows auch während sie läuft).
#include "engine/Common.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace update {

constexpr const char* kRepo = "GG2EZBoOm/kek-me-if-u-can";

struct Status {
  enum State { Idle, Checking, UpToDate, Downloading, Installed, Failed } state = Idle;
  std::string latest;   // neueste Version laut GitHub, z. B. "1.0.1"
  std::string message;  // Fehlerursache bzw. Hinweis
  float progress = 0;   // 0..1 beim Herunterladen
};

struct Release {
  struct Asset {
    std::string name, url, sha256;  // sha256 leer, wenn GitHub keinen Digest liefert
    u64 size = 0;
  };
  std::string tag, version;
  std::vector<Asset> assets;
};

class Updater {
 public:
  ~Updater();
  // Im Hintergrund prüfen (install = false) bzw. prüfen und installieren
  void Start(const std::string& currentVersion, bool install);
  // Dasselbe blockierend (Konsole)
  Status Run(const std::string& currentVersion, bool install);
  Status GetStatus() const;
  bool Busy() const;
  // Nur für Vorschau-Screenshots
  void SetPreview(const Status& s);

 private:
  void Work(std::string current, bool install);
  void Set(Status::State st, const std::string& msg = "", float progress = -1);
  mutable std::mutex mu_;
  Status status_;
  std::thread thread_;
  bool busy_ = false;
  std::atomic<bool> cancel_{false};  // beim Beenden laufenden Download abbrechen
};

// Startet die (ausgetauschte) EXE mit denselben Argumenten neu; danach sollte sich das Programm beenden
bool RestartSelf();
// Entfernt Reste früherer Updates (*.old, *.new neben der EXE); versucht es kurz erneut, falls die alte EXE noch läuft
void CleanupOldFiles(int retries = 0);

// Hilfen (auch für die Tests)
int CompareVersions(const std::string& a, const std::string& b);  // -1 / 0 / 1, "v1.2.3" und "1.2" erlaubt
bool ParseRelease(const std::string& json, Release& out, std::string& err);
std::string Sha256Hex(const void* data, size_t size);

}  // namespace update
