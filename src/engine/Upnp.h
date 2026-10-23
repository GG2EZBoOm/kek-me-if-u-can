#pragma once
// Internetspiel über IPv4: automatische Portfreigabe am Router (UPnP-IGD), lokale und öffentliche Adresse.
// Alles läuft in Hintergrund-Threads, das Spiel fragt nur den Zustand ab.
#include "engine/Common.h"
#include <mutex>
#include <thread>

namespace upnp {

struct Status {
  enum State { Idle, Working, Mapped, Failed } state = Idle;
  std::string externalIp;  // vom Router gemeldete öffentliche Adresse
  std::string localIp;     // eigene Adresse im Heimnetz
  std::string message;     // Hinweis bzw. Fehlerursache
};

class PortMapper {
 public:
  ~PortMapper();
  // Startet Router-Suche und Portfreigabe (UDP) im Hintergrund
  void Start(u16 port, const std::string& description);
  // Entfernt die Freigabe wieder (kurz blockierend)
  void Remove();
  Status GetStatus() const;

 private:
  void Run(u16 port, std::string desc);
  void Join();
  mutable std::mutex mu_;
  Status status_;
  std::thread thread_;
  std::string controlHost_, controlPath_, serviceType_;
  u16 controlPort_ = 0, port_ = 0;
  bool mapped_ = false;
};

// Fragt beim Hosten die öffentliche IPv4 per HTTPS bei api.ipify.org ab.
class PublicIpLookup {
 public:
  ~PublicIpLookup();
  void Start();
  bool Busy() const;
  std::string Result() const;  // leer, solange unbekannt
  std::string Error() const;

 private:
  mutable std::mutex mu_;
  std::thread thread_;
  bool busy_ = false;
  std::string result_, error_;
};

// Primäre lokale IPv4-Adresse (Schnittstelle mit Standardroute)
std::string LocalIPv4();
// 10/8, 172.16/12, 192.168/16, 100.64/10 (Carrier-NAT), 127/8, 169.254/16
bool IsPrivateIPv4(const std::string& ip);
// Für Internet-Einladungen: filtert private, lokale, Multicast- und bekannte Sondernetze.
bool IsPublicIPv4(const std::string& ip);
bool ParseIPv4(const std::string& text, u32& ip);

}  // namespace upnp
