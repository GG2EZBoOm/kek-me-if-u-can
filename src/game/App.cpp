// Einstiegspunkt: Spiel, dedizierter Server, automatische Tests, Screenshot-Viewer.
#include "engine/Common.h"
#include "engine/GL.h"
#include "engine/Image.h"
#include "engine/Platform.h"
#include "engine/Renderer.h"
#include "engine/UI.h"
#include "engine/Upnp.h"
#include "game/GameView.h"
#include "game/Invite.h"
#include "game/Maps.h"
#include "game/Session.h"
#include "game/Settings.h"
#include "game/Updater.h"
#include "game/Version.h"
#include "game/WorldRender.h"
#include <chrono>
#include <thread>

int RunTests(int matchRounds);
int RunNetTest();
int RunGame(int argc, char** argv);

static void LookAtAngles(Vec3 from, Vec3 to, float& yaw, float& pitch) {
  Vec3 d = Normalize(to - from);
  yaw = YawFromDir(d);
  pitch = asinf(Clamp(d.y, -1.0f, 1.0f));
}

// Rendert Kamerafahrt-Ansichten der Karte als PNG (Entwicklungswerkzeug)
// hunters: am Blickpunkt (Brusthöhe) vier Jäger aufstellen (von vorn, gehend, von hinten, geduckt) – Vorschau des Modells
static int RunViewer(const std::string& shotDir, int width, int height, const std::vector<Vec3>& extraCams, int map, bool hunters) {
  if (!plat::Init("Kek Me If You Can – Kamera", width, height, true)) return 1;
  Renderer r;
  RenderSettings rs;
  if (!r.Init(rs)) return 2;
  UI ui;
  if (!ui.Init()) return 3;
  props::BuildCatalog();
  World w;
  BuildMap(map, w);
  WorldRender wr;
  wr.Init(r);
  wr.Load(r, w);
  GameView view;
  if (hunters) view.Init(r);
  std::vector<Vec3> path = extraCams.empty() ? w.cameraPath : extraCams;
  int shots = (int)path.size() / 2;
  for (int s = 0; s < shots; s++) {
    Camera cam;
    cam.pos = path[s * 2];
    LookAtAngles(cam.pos, path[s * 2 + 1], cam.yaw, cam.pitch);
    cam.fovDeg = 75;
    for (int pass = 0; pass < 3; pass++) {
      r.Begin(cam, w.env, width, height, 10.0f);
      Frustum fr;
      fr.FromMatrix(r.Cam().viewProj);
      r.SetLights(w.lights);
      wr.Submit(r, w, fr, cam.pos);
      if (hunters) {
        Vec3 target = path[s * 2 + 1];
        Vec3 fwd = Normalize(Vec3(target.x - cam.pos.x, 0, target.z - cam.pos.z));
        Vec3 right(-fwd.z, 0, fwd.x);
        float face = atan2f(fwd.x, fwd.z);  // Blick zur Kamera (Vorwärts = -Z)
        const float yaws[4] = {face, face + PI * 0.5f, face + PI, face + 0.4f};
        for (int i = 0; i < 4; i++) {
          Vec3 p = target + right * ((i - 1.5f) * 1.25f) - Vec3(0, 1.1f, 0);  // Blickpunkt = Brusthöhe
          float sky = w.SkyAt(p + Vec3(0, 1, 0));
          view.DrawHunterPreview(r, p, yaws[i], i == 3 ? 0.3f : -0.1f, i == 1 ? 1.2f : 0.0f, i == 1 ? 4.0f : 0.0f, i == 3, i == 2 ? 1 : 0, sky);
        }
      }
      ScreenFX fx;
      r.Render(fx);
      ui.Begin(width, height, false);
      const Zone* z = w.ZoneAt(cam.pos);
      ui.TextShadow(FONT_LARGE, 30, 24, z ? z->name : "?", Color(255, 255, 255));
      ui.End();
      glFinish();
    }
    std::vector<u8> px;
    int w2, h2;
    r.ReadBackbuffer(px, w2, h2);
    WritePNG(shotDir + StrFormat("/shot%d.png", s), w2, h2, px.data());
  }
  plat::Shutdown();
  return 0;
}

// Dedizierter Server ohne Fenster
static int RunServer(int argc, char** argv) {
  Settings cfg;
  cfg.Load(ExeDir() + "prophunt.ini");
  MatchSettings ms = cfg.match;
  std::string name = cfg.serverName;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--bots" && i + 1 < argc) ms.bots = atoi(argv[++i]);
    else if (a == "--rounds" && i + 1 < argc) ms.rounds = atoi(argv[++i]);
    else if (a == "--name" && i + 1 < argc) name = argv[++i];
    else if (a == "--skill" && i + 1 < argc) ms.botSkill = atoi(argv[++i]);
    else if (a == "--infection") ms.infection = true;
    else if (a == "--map" && i + 1 < argc) {
      int m = FindMap(argv[++i]);
      if (m < 0) LOG("Unbekannte Karte \"%s\" – Standardkarte wird genommen", argv[i]);
      ms.mapIndex = std::max(0, m);
    }
  }
  LocalSession s;
  s.SetDedicated(true);
  if (!s.Start(ms, "", true, name)) {
    LOG("Server konnte nicht gestartet werden.");
    return 1;
  }
  LOG("Dedizierter Server \"%s\" läuft (Karte %s). Beenden mit Strg+C.", name.c_str(), s.GetWorld().name.c_str());
  u32 lanIp = 0;
  std::string lan = upnp::LocalIPv4();
  if (upnp::ParseIPv4(lan, lanIp))
    LOG("Einladung im Heimnetz: %s (%s:%d) – fürs Internet UDP-Port %d im Router freigeben", EncodeInvite(lanIp, GAME_PORT).c_str(),
        lan.c_str(), GAME_PORT, GAME_PORT);
  auto last = std::chrono::steady_clock::now();
  while (true) {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last).count();
    last = now;
    s.Update(std::min(dt, 0.25));
    std::vector<GameEvent> evs;
    s.TakeEvents(evs);
    const Sim& sim = s.GetSim();
    for (const GameEvent& e : evs) {
      if (e.type == EV_ROUND_END) LOG("Runde %d: %s gewinnen", sim.round, e.b == TEAM_PROPS ? "Props" : "Jäger");
      if (e.type == EV_CHAT && e.a < MAX_PLAYERS) LOG("[Chat] %s: %s", sim.players[e.a].name, e.text.c_str());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return 0;
}

// Konsole: "--update" sucht auf GitHub nach einer neuen Version und tauscht die EXEs aus.
// "--pretend-version x.y.z" gibt eine ältere eigene Version vor (zum Testen des Updates).
static int RunUpdate(int argc, char** argv) {
  std::string current = KEK_VERSION_STR;
  for (int i = 1; i + 1 < argc; i++)
    if (std::string(argv[i]) == "--pretend-version") current = argv[i + 1];
  LOG("Suche Update für Version %s (%s) …", current.c_str(), update::kRepo);
  update::Updater u;
  update::Status s = u.Run(current, true);
  switch (s.state) {
    case update::Status::Installed: LOG("Update auf Version %s installiert.", s.latest.c_str()); return 0;
    case update::Status::UpToDate:
      LOG("%s", s.message.empty() ? StrFormat("Version %s ist aktuell.", current.c_str()).c_str() : s.message.c_str());
      return 0;
    default: LOG("Update fehlgeschlagen: %s", s.message.c_str()); return 1;
  }
}

int AppMain(int argc, char** argv, bool console) {
  LogInit((ExeDir() + "prophunt.log").c_str(), console);
  LOG("Kek Me If You Can %s", KEK_VERSION_STR);
  std::string shotDir;
  int width = 1600, height = 900;
  std::vector<Vec3> extraCams;
  int map = 0;
  bool hunters = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--nettest") return RunNetTest();
    if (a == "--test") return RunTests(i + 1 < argc ? atoi(argv[i + 1]) : 2);
    if (a == "--server") return RunServer(argc, argv);
    if (a == "--update") return RunUpdate(argc, argv);
    if (a == "--shots" && i + 1 < argc) shotDir = argv[++i];
    else if (a == "--map" && i + 1 < argc) map = std::max(0, FindMap(argv[++i]));
    else if (a == "--hunters") hunters = true;
    else if (a == "--size" && i + 2 < argc) { width = atoi(argv[++i]); height = atoi(argv[++i]); }
    else if (a == "--cam" && i + 6 < argc) {
      extraCams.push_back(Vec3((float)atof(argv[i + 1]), (float)atof(argv[i + 2]), (float)atof(argv[i + 3])));
      extraCams.push_back(Vec3((float)atof(argv[i + 4]), (float)atof(argv[i + 5]), (float)atof(argv[i + 6])));
      i += 6;
    }
  }
  if (!shotDir.empty()) return RunViewer(shotDir, width, height, extraCams, map, hunters);
  return RunGame(argc, argv);
}
