// Automatische Tests ohne Fenster: Bewegung, Navigation, komplettes Bot-Match, Netzwerk.
#include "engine/Upnp.h"
#include "game/Bot.h"
#include "game/Invite.h"
#include "game/Maps.h"
#include "game/NetGame.h"
#include "game/Taunts.h"
#include "game/Updater.h"
#include <chrono>
#include <thread>

static double Now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool Check(bool cond, const char* what, int& fails) {
  LOG("%s %s", cond ? "[OK]  " : "[FAIL]", what);
  if (!cond) fails++;
  return cond;
}

static void WalkTest(const World& w, Vec3 start, float yaw, float seconds, MoveState& s, int propType = -1, u16 extra = 0) {
  s = MoveState();
  s.pos = start;
  s.yaw = yaw;
  MoveEnv env;
  env.world = &w;
  UserCmd cmd;
  cmd.yaw = yaw;
  cmd.buttons = BTN_FORWARD | extra;
  int ticks = (int)(seconds * TICK_RATE);
  for (int i = 0; i < ticks; i++) ApplyMovement(env, s, cmd, propType, false);
}

static bool Check(bool cond, const std::string& what, int& fails) { return Check(cond, what.c_str(), fails); }

static float PathLength(const std::vector<Vec3>& p) {
  float len = 0;
  for (size_t i = 1; i < p.size(); i++) len += Distance(p[i - 1], p[i]);
  return len;
}

static std::string ZoneName(const World& w, Vec3 p) {
  const Zone* z = w.ZoneAt(p + Vec3(0, 0.5f, 0));
  return z ? z->name : "?";
}

// Jede Tür muss von beiden Seiten ohne Umweg passierbar sein
static void CheckDoors(const World& w, const NavGrid& nav, int& fails) {
  int bad = 0;
  for (const DoorInfo& d : w.doors) {
    Vec3 n = d.alongX ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
    float off = d.depth * 0.5f + 0.8f;
    Vec3 a = d.center - n * off + Vec3(0, 0.05f, 0), b = d.center + n * off + Vec3(0, 0.05f, 0);
    std::vector<Vec3> path;
    bool ok = nav.Nearest(a, 0.7f) >= 0 && nav.Nearest(b, 0.7f) >= 0 && nav.FindPath(a, b, path, 6000);
    if (ok && PathLength(path) > off * 2.0f + 3.0f) ok = false;
    if (!ok) {
      bad++;
      LOG("    Tür blockiert bei (%.1f %.1f %.1f), Breite %.1f: %s <-> %s", d.center.x, d.center.y, d.center.z, d.width,
          ZoneName(w, a).c_str(), ZoneName(w, b).c_str());
    }
  }
  Check(bad == 0, StrFormat("%d Türen passierbar (%d blockiert)", (int)w.doors.size() - bad, bad), fails);
}

// Jeder benannte Bereich muss erreichbar sein
static void CheckZones(const World& w, const NavGrid& nav, int& fails) {
  int bad = 0;
  for (const Zone& z : w.zones) {
    bool ok = nav.AnyReachableIn(z.mn, z.mx);
    if (!ok) {
      bad++;
      LOG("    Bereich nicht erreichbar: %s", z.name.c_str());
    }
  }
  Check(bad == 0, StrFormat("%d Bereiche erreichbar (%d nicht)", (int)w.zones.size() - bad, bad), fails);
}

static void CheckPaths(const NavGrid& nav, const std::vector<std::tuple<Vec3, Vec3, const char*>>& tests, int& fails) {
  for (const auto& t : tests) {
    std::vector<Vec3> path;
    double p0 = Now();
    bool ok = nav.FindPath(std::get<0>(t), std::get<1>(t), path);
    LOG("  Pfad %-42s %s  %.1f m, %.1f ms", std::get<2>(t), ok ? "ok" : "FEHLT", PathLength(path), (Now() - p0) * 1000);
    Check(ok, std::get<2>(t), fails);
  }
}

static void BundestagTests(const World& w, const NavGrid& nav, int& fails) {
  MoveState s;
  WalkTest(w, Vec3(-27.5f, 0.05f, 0), -PI * 0.5f, 6.0f, s);
  Check(fabsf(s.pos.y - 5.0f) < 0.1f, "Große Treppe hinauf bis auf den Balkon (y=5)", fails);
  Check(s.pos.x > -16.3f && s.pos.x < -15.4f, "Stopp an der Ringwand (keine Durchdringung)", fails);
  WalkTest(w, Vec3(-20, 0.05f, -12.5f), PI * 0.5f, 4.0f, s);
  Check(s.pos.x > -29.0f && s.pos.x < -28.4f, "Außenwand blockiert", fails);
  WalkTest(w, Vec3(-27.5f, 0.05f, 0.6f), -PI * 0.5f, 7.0f, s, props::Find("kaffeebecher"));
  Check(s.pos.y > 4.5f, "Kaffeebecher kommt die Treppe hoch", fails);
  WalkTest(w, Vec3(0, 12.0f, -16.0f), 0, 1.5f, s);
  Check(fabsf(s.pos.y - 10.0f) < 0.1f, "Landung auf der Dachterrasse", fails);
  WalkTest(w, Vec3(-10.5f, 0.05f, 0.0f), -PI * 0.5f, 6.0f, s);
  Check(s.pos.y < -0.5f, "Stufen im Plenarsaal hinab", fails);
  // Seiteneingang durch den Metalldetektor (Torbogen)
  WalkTest(w, Vec3(-28.6f, 0.05f, 5.0f), -PI * 0.5f, 3.0f, s);
  Check(s.pos.x > -24.5f, "Durch den Metalldetektor am Seiteneingang", fails);
  CheckPaths(nav,
             {{Vec3(-70, -1.2f, 0), Vec3(-22, 0, 8), "Platz -> Eingangshalle"},
              {Vec3(-22, 0, 8), Vec3(0, 10, 16), "Eingangshalle -> Dachterrasse (Turm)"},
              {Vec3(-70, -1.2f, 0), Vec3(0, 16, 4.5f), "Platz -> Kuppel-Plattform"},
              {Vec3(-22, 0, 8), Vec3(6.4f, -1.62f, 2.0f), "Eingangshalle -> Rednerpult"},
              {Vec3(-22, 0, -8), Vec3(14.5f, 5, -13.5f), "Eingangshalle -> Sitzungssaal (1. OG)"},
              {Vec3(20, 0, -8), Vec3(22, 10, 0), "Kantine -> Dachrestaurant"}},
             fails);
}

static void OktoberfestTests(const World& w, const NavGrid& nav, int& fails) {
  MoveState s;
  WalkTest(w, Vec3(26.9f, 0.05f, 5.6f), PI, 4.0f, s);
  Check(fabsf(s.pos.y - 3.0f) < 0.1f, "Treppe hinauf auf die Empore (y=3)", fails);
  WalkTest(w, Vec3(6.2f, 0.05f, 0), PI * 0.5f, 2.5f, s);
  Check(fabsf(s.pos.y - 1.0f) < 0.1f, "Stufen auf das Musikpodium", fails);
  WalkTest(w, Vec3(0, 0.05f, 24.0f), 0, 4.0f, s);
  Check(s.pos.z < 15.0f, "Durch den Haupteingang ins Zelt", fails);
  WalkTest(w, Vec3(-7.2f, 0.05f, 50.0f), PI, 3.0f, s);
  Check(fabsf(s.pos.y - 0.25f) < 0.05f, "Auf die Autoscooter-Fläche", fails);
  CheckPaths(nav,
             {{Vec3(55, 0, 34), Vec3(0, 0, 5.2f), "Jäger-Start -> Festzelt"},
              {Vec3(0, 0, 5.2f), Vec3(-15, 3.0f, -14), "Festzelt -> Empore"},
              {Vec3(0, 0, 5.2f), Vec3(26.2f, 0, 0), "Festzelt -> hinter die Schänke"},
              {Vec3(0, 0, 5.2f), Vec3(-22, 0, 14.5f + 1.0f), "Festzelt -> Boxen"},
              {Vec3(55, 0, 34), Vec3(-50, 0, 42.5f), "Straße -> in den Lebkuchenstand"},
              {Vec3(55, 0, 34), Vec3(-2, 0.25f, 64), "Straße -> Autoscooter"},
              {Vec3(55, 0, 34), Vec3(-36, 0.2f, 68), "Straße -> Riesenrad-Plattform"},
              {Vec3(0, 0, 5.2f), Vec3(10, 0, -25), "Festzelt -> Hinterhof"}},
             fails);
}

static void BotMatch(World& w, BotManager& bots, int rounds, int& fails) {
  Sim sim;
  MatchSettings ms;
  ms.rounds = rounds;
  ms.hideTime = 25;
  ms.roundTime = 150;
  ms.botSkill = 1;
  sim.Init(&w, ms);
  Rng nr(3);
  for (int i = 0; i < 10; i++) sim.AddPlayer(BotManager::RandomName(nr, sim), true);
  int kills = 0, penalties = 0, taunts = 0, disguises = 0, shots = 0, decoys = 0, hits = 0;
  int propWins = 0, hunterWins = 0;
  double m0 = Now();
  int ticks = 0;
  int maxTicks = (int)((ms.hideTime + ms.roundTime + 10) * rounds * TICK_RATE) + 600;
  bool matchEnded = false;
  while (ticks < maxTicks && !matchEnded) {
    bots.Think(sim);
    sim.Tick();
    bots.OnEvents(sim, sim.events);
    for (const GameEvent& e : sim.events) {
      switch (e.type) {
        case EV_KILL: kills++; break;
        case EV_PENALTY: penalties++; break;
        case EV_TAUNT: taunts++; break;
        case EV_DISGUISE: disguises++; break;
        case EV_SHOT: shots++; break;
        case EV_DECOY: decoys++; break;
        case EV_HIT: hits++; break;
        case EV_ROUND_END:
          if (e.b == TEAM_PROPS) propWins++;
          else hunterWins++;
          LOG("  Runde %d: %s gewinnen (%.0f s, Props übrig: %d)", sim.round, e.b == TEAM_PROPS ? "Props" : "Jäger",
              ms.roundTime - std::max(0.0f, sim.phaseTimer), sim.CountAlive(TEAM_PROPS));
          break;
        case EV_PHASE:
          if (e.b == (u8)Phase::MatchEnd) matchEnded = true;
          if (e.b == (u8)Phase::Hunting) {
            std::string where;
            for (const Player& p : sim.players)
              if (p.active && p.alive && p.team == TEAM_PROPS)
                where += " " + props::Get(p.propType).name + "@" + ZoneName(w, p.ms.pos) + ";";
            LOG("  Verstecke (%d/%d versteckt):%s", bots.CountHiddenProps(sim), sim.CountAlive(TEAM_PROPS), where.c_str());
          }
          break;
      }
    }
    sim.events.clear();
    ticks++;
  }
  double dt = Now() - m0;
  LOG("  Match: %.0f s Spielzeit in %.2f s (%.0fx Echtzeit) | Verwandlungen %d, Taunts %d, Köder %d, Schüsse %d, Treffer %d, Strafen %d, Kills %d",
      ticks / 60.0, dt, ticks / 60.0 / dt, disguises, taunts, decoys, shots, hits, penalties, kills);
  LOG("  Siege: Props %d, Jäger %d", propWins, hunterWins);
  Check(disguises >= rounds * 3, "Bots verwandeln sich", fails);
  Check(shots > 0, "Jäger-Bots schießen", fails);
  Check(propWins + hunterWins == rounds, "Alle Runden beendet", fails);
}

// Einladungscodes: Hin- und Rückweg, Tippvarianten, Fehlererkennung, Adressen in eingefügtem Text
static void InviteTests(int& fails) {
  Rng rng(4711);
  int bad = 0, withL = 0;
  for (int i = 0; i < 20000; i++) {
    u32 ip = rng.Next() | 1;
    u16 port = (u16)(1 + rng.Next() % 65535);
    std::string code = EncodeInvite(ip, port);
    withL += code.find('L') != std::string::npos;
    u32 ip2 = 0;
    u16 port2 = 0;
    if (!DecodeInvite(code, ip2, port2) || ip2 != ip || port2 != port) bad++;
  }
  Check(bad == 0, StrFormat("Einladungscodes hin und zurück (%d Fehler, %d Codes mit L)", bad, withL), fails);
  std::string code = EncodeInvite((203u << 24) | (0u << 16) | (113u << 8) | 7u, 27015);
  std::string lower = code;
  for (char& c : lower) c = (char)tolower((unsigned char)c);
  std::string spaced = code;
  for (char& c : spaced)
    if (c == '-') c = ' ';
  Check(ResolveJoinInput(code) == "203.0.113.7:27015", "Code wird zur Adresse aufgelöst", fails);
  Check(ResolveJoinInput("  " + lower + " ") == "203.0.113.7:27015", "Code in Kleinbuchstaben", fails);
  Check(ResolveJoinInput(spaced) == "203.0.113.7:27015", "Code mit Leerzeichen statt Strichen", fails);
  Check(ResolveJoinInput(InviteMessage("203.0.113.7:27015", code, "Oktoberfest")) == "203.0.113.7:27015",
        "Komplette Discord-Nachricht eingefügt", fails);
  Check(ResolveJoinInput("Komm rein: 192.168.0.42:27016 !") == "192.168.0.42:27016", "IPv4 mit Port aus Text", fails);
  Check(ResolveJoinInput(" 10.0.0.5 ") == "10.0.0.5", "Reine IPv4 bleibt", fails);
  std::string broken = code;
  broken[5] = broken[5] == '2' ? '3' : '2';
  u32 bip;
  u16 bport;
  int caught = 0;
  for (char c : std::string("23456789ABCDEFGHJKLMNPQRSTUVWXYZ")) {
    broken[5] = c;
    if (broken != code && (!DecodeInvite(broken, bip, bport) || bip != ((203u << 24) | (0u << 16) | (113u << 8) | 7u))) caught++;
  }
  Check(caught >= 31, StrFormat("Tippfehler werden erkannt oder ändern die Adresse (%d/31)", caught), fails);
  Check(upnp::IsPrivateIPv4("192.168.1.2") && upnp::IsPrivateIPv4("100.70.1.1") && !upnp::IsPrivateIPv4("203.0.113.7"),
        "Private und öffentliche IPv4 unterschieden", fails);
}

// Updater: Versionsvergleich, Release-Antwort von GitHub, SHA-256
static void UpdaterTests(int& fails) {
  using update::CompareVersions;
  Check(CompareVersions("1.0.1", "1.0.0") == 1 && CompareVersions("v1.0.0", "1.0") == 0 && CompareVersions("1.9.0", "1.10.0") == -1 &&
            CompareVersions("2", "1.99.99") == 1,
        "Versionsvergleich (v-Präfix, fehlende Stellen, zweistellige Nummern)", fails);
  const char* json = R"({"tag_name":"v1.2.3","name":"Kek \u00e4 \ud83c\udf7a","draft":false,"body":"Neu:\n- Dosen","assets":[
    {"name":"KekMeIfYouCan.exe","size":1435136,"digest":"sha256:ABCDEF01","browser_download_url":"https://github.com/GG2EZBoOm/kek-me-if-u-can/releases/download/v1.2.3/KekMeIfYouCan.exe","uploader":{"login":"x","id":1}},
    {"name":"KekMeIfYouCanCLI.exe","size":12,"digest":null,"browser_download_url":"https://github.com/GG2EZBoOm/kek-me-if-u-can/releases/download/v1.2.3/KekMeIfYouCanCLI.exe"},
    {"name":"Fremd.exe","size":5,"browser_download_url":"https://evil.example/KekMeIfYouCan.exe"}]})";
  update::Release r;
  std::string err;
  bool ok = update::ParseRelease(json, r, err);
  Check(ok && r.version == "1.2.3" && r.assets.size() == 2, "Release-Antwort gelesen, fremde Download-Adresse verworfen", fails);
  Check(ok && r.assets.size() == 2 && r.assets[0].sha256 == "ABCDEF01" && r.assets[0].size == 1435136 && r.assets[1].sha256.empty(),
        "Prüfsumme und Größe der Dateien übernommen", fails);
  Check(!update::ParseRelease("{\"tag_name\":\"v1\",", r, err) && !update::ParseRelease("{\"tag_name\":\"nightly\"}", r, err),
        "Kaputte Antwort und Tag ohne Version abgelehnt", fails);
  Check(update::Sha256Hex("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 (Testvektor \"abc\")",
        fails);
}

int RunTests(int matchRounds) {
  int fails = 0;
  InviteTests(fails);
  UpdaterTests(fails);
  props::BuildCatalog();
  for (int m = 0; m < (int)AllMaps().size(); m++) {
    World w;
    double t0 = Now();
    BuildMap(m, w);
    LOG("=== Karte %s: gebaut in %.0f ms, %d Türen, %d Bereiche ===", w.name.c_str(), (Now() - t0) * 1000, (int)w.doors.size(),
        (int)w.zones.size());
    BotManager bots;
    bots.Init(w);
    CheckDoors(w, bots.Nav(), fails);
    CheckZones(w, bots.Nav(), fails);
    if (w.id == "bundestag") BundestagTests(w, bots.Nav(), fails);
    if (w.id == "oktoberfest") OktoberfestTests(w, bots.Nav(), fails);
    if (matchRounds > 0) BotMatch(w, bots, matchRounds, fails);
  }
  LOG("Tests beendet: %d Fehler", fails);
  return fails;
}
// Netzwerk über Loopback: Server mit Bots + ein Client, der läuft und schießt
int RunNetTest() {
  int fails = 0;
  props::BuildCatalog();
  MatchSettings ms;
  ms.bots = 5;
  ms.hideTime = 3;
  ms.roundTime = 60;
  LocalSession host;
  host.SetDedicated(true);
  if (!Check(host.Start(ms, "", true, "Testserver"), "Server startet auf Port 27015", fails)) return fails;
  NetClient client;
  Check(client.Connect("127.0.0.1", "Tester"), "Client öffnet Verbindung", fails);
  int frame = 0;
  client.makeCmd = [&]() {
    UserCmd c;
    c.yaw = frame * 0.01f;
    c.buttons = BTN_FORWARD | ((frame / 30) % 2 ? BTN_FIRE : 0);
    return c;
  };
  double t0 = Now();
  int events = 0;
  float maxErr = 0;
  for (frame = 0; frame < 60 * 12; frame++) {
    host.Update(TICK_DT);
    client.Update(TICK_DT);
    std::vector<GameEvent> evs;
    client.TakeEvents(evs);
    events += (int)evs.size();
    if (client.Ready() && frame > 240) {
      int id = client.LocalId();
      const Player& sp = host.GetSim().players[id];
      // Vorhersage gegenüber Server (Server liegt ~1 Tick + Latenz zurück, daher grobe Schranke)
      float err = DistanceXZ(sp.ms.pos, client.LocalMove()->pos);
      maxErr = std::max(maxErr, err);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
  LOG("Netztest: %.2f s, Client bereit=%d, Slot=%d, Ereignisse=%d, max. Abweichung Vorhersage=%.2f m", Now() - t0,
      client.Ready(), client.LocalId(), events, maxErr);
  LOG("  %s | %s", host.ServerInfo().c_str(), client.ServerInfo().c_str());
  Check(client.Ready(), "Client verbunden und Snapshots empfangen", fails);
  int seen = 0;
  for (const ClientPlayer& p : client.State().players)
    if (p.active) seen++;
  LOG("  Client sieht %d Spieler (Server: %d)", seen, host.GetSim().CountActive());
  Check(seen == host.GetSim().CountActive(), "Alle Spieler im Snapshot", fails);
  Check(events > 0, "Ereignisse kommen an", fails);
  Check(maxErr < 1.5f, "Vorhersage bleibt nah am Server", fails);
  bool named = client.State().players[client.LocalId()].name == std::string("Tester");
  Check(named, "Spielername übertragen", fails);
  LOG("Netztest beendet: %d Fehler", fails);
  return fails;
}