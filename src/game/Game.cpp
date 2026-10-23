// Das Spiel: Menüs, Eingabe, HUD, Sound-Anbindung und Hauptschleife.
#include "engine/Audio.h"
#include "engine/Upnp.h"
#include "engine/Video.h"
#include "engine/Image.h"
#include "engine/Platform.h"
#include "engine/UI.h"
#include "game/Discord.h"
#include "game/GameView.h"
#include "game/Invite.h"
#include "game/Maps.h"
#include "game/NetGame.h"
#include "game/Sim.h"
#include "game/Settings.h"
#include "game/Sounds.h"
#include "game/Taunts.h"
#include "game/Updater.h"
#include "game/Version.h"
#include <windows.h>
#include <ctime>

namespace {

const Color kGold = Color::Hex(0xF2B600);
const Color kHunter = Color::Hex(0x5aa9ff);
const Color kProp = Color::Hex(0xffb84d);
const Color kRed = Color::Hex(0xff5a4e);
const Color kWhite = Color(245, 246, 250);
const Color kDim = Color(180, 186, 198);
const Color kPanel = Color::Hex(0x0f141e, 215);

Color TeamColor(Team t) { return t == TEAM_HUNTERS ? kHunter : (t == TEAM_PROPS ? kProp : kDim); }

std::string FmtTime(float s) {
  int t = std::max(0, (int)ceilf(s));
  return StrFormat("%d:%02d", t / 60, t % 60);
}

struct FeedEntry {
  std::string killer, victim, extra;
  Team kt = TEAM_SPECTATOR, vt = TEAM_SPECTATOR;
  float time = 0;
};
struct ChatLine {
  std::string who, text;
  Team team = TEAM_SPECTATOR;
  bool system = false;
  float time = 0;
};
struct Popup {
  std::string text;
  Color color;
  float time = 0, life = 1.5f;
};

enum class Screen { Main, Play, Join, Settings, Connecting, InGame };

class Game {
 public:
  int Run(int argc, char** argv);

 private:
  // Ablauf
  void Frame(float dt);
  void StartLocal(bool host);
  void StartJoin(const std::string& addr);
  void LeaveGame(const std::string& reason = "");
  void ApplySettings();
  bool InGameInput() const;
  UserCmd MakeCmd();
  void UpdateView(float dt);
  void HandleEvent(const GameEvent& e);
  void UpdateAudio(float dt);
  void Center(const std::string& text, Color c, float dur = 3.0f);
  void AddChat(const std::string& who, const std::string& text, Team team, bool system);
  // Oberfläche
  void DrawMenuBackground();
  float Appear(float delay) const;
  void DrawLogo(float x, float y, bool big);
  void SelectMenuMap(int index);
  void DrawMapCard(const Rect& card, bool arrows);
  void DrawProfileChip();
  bool PageHeader(float x, float y, float w, const std::string& title, const std::string& sub);
  void DrawMain();
  void DrawPlaySetup();
  void DrawJoin();
  void DrawSettings(bool overlay);
  void DrawConnecting();
  void DrawHUD();
  void DrawScoreboard(float alpha);
  void DrawPause();
  void DrawHelp();
  void DrawTauntMenu();
  void DrawCrosshair(const ClientPlayer& me);
  void DrawError();
  // Einladungen, Internetspiel und Discord
  void UpdatePresence();
  std::string InviteAddress(std::string* note, Color* noteColor) const;
  void DrawInvitePanel(const Rect& r);
  void DrawJoinRequests();
  void CopyInvite(const std::string& text, const std::string& what);
  // Automatische Updates
  void UpdateUpdater();
  void DrawUpdateCard(const Rect& r);
  std::string UpdateStatusText() const;
  void StartUpdateCheck();
  // Intro-Videos vor dem Hauptmenü
  void PlayIntros();
  bool IntroShot(int index, double t, const char* name);
  void SaveCfg() { if (!autopilot_) cfg_.Save(cfgPath_); }
  Rect R(float x, float y, float w, float h) const { return {ui_.S(x), ui_.S(y), ui_.S(w), ui_.S(h)}; }
  // Demo-/Screenshot-Modus
  void Autopilot();
  void Shot(const std::string& name);

  Settings cfg_;
  std::string cfgPath_;
  Renderer r_;
  UI ui_;
  GameView view_;
  World menuWorld_;
  const World* loadedWorld_ = nullptr;
  std::unique_ptr<Session> session_;
  Screen screen_ = Screen::Main;
  Screen lastScreen_ = Screen::InGame;
  float screenT_ = 0;      // Zeit seit dem letzten Bildschirmwechsel (Einblendungen)
  float hideTotal_ = 0;    // Länge der aktuellen Versteckphase (für den Ring)
  double flybyStart_ = 0;
  bool settingsOverlay_ = false;
  bool paused_ = false, chatOpen_ = false, tauntMenu_ = false, help_ = false;
  std::string chatInput_;
  std::string error_;
  float errorTime_ = 0;
  bool hostSetup_ = false;
  std::string joinAddr_;
  LanBrowser lan_;
  float yaw_ = 0, pitch_ = 0;
  int spectate_ = -1;
  bool freeCam_ = false;
  Vec3 freeCamPos_;
  u8 weapon_ = WPN_SMG;
  int tauntChoice_ = 0;
  bool tauntTap_ = false;
  std::vector<FeedEntry> feed_;
  std::vector<ChatLine> chat_;
  std::string center_, centerSub_;
  Color centerCol_ = kWhite;
  float centerTime_ = 0, centerDur_ = 3;
  std::vector<Popup> popups_;
  float hitMarker_ = 0, killMarker_ = 0;
  Team lastTeam_ = TEAM_SPECTATOR;
  bool lastAlive_ = false;
  int lastRound_ = -1;
  int ambOut_ = 0, ambIn_ = 0, music_ = 0;
  float ambOutVol_ = 0, ambInVol_ = 0, musicVol_ = 0;
  float stepAcc_[MAX_PLAYERS] = {};
  double time_ = 0;
  float fps_ = 60, frameMs_ = 16;
  Rng rng_{31337};
  upnp::PortMapper portMapper_;
  upnp::PublicIpLookup ipLookup_;
  DiscordRpc discord_;
  bool hosting_ = false;      // eigener Server mit Netzwerk
  std::string localIp_;
  std::string partyId_;
  std::string serverAddr_;    // Adresse des beigetretenen Servers (für Einladungen als Gast)
  std::string pendingJoin_;   // Einladung, die über Discord angenommen wurde
  long long sessionStart_ = 0;
  float presenceTimer_ = 0;
  struct JoinRequest {
    std::string userId, name;
    double time;
  };
  std::vector<JoinRequest> joinRequests_;
  update::Updater updater_;
  bool noUpdate_ = false;
  bool noIntro_ = false;
  bool updateHandled_ = false;    // "installiert" wurde schon ausgewertet
  double updateCheckStart_ = -1;  // Zeitpunkt der letzten Prüfung (für die Anzeige)
  double updateFailAt_ = -1;      // Fehlermeldung nur kurz zeigen
  double restartAt_ = 0;          // > 0: geplanter Neustart in die neue Version
  int lastUpdateState_ = 0;
  // Demo-Modus
  std::string shotDir_;
  bool autopilot_ = false;
  bool forceScores_ = false;
  bool hidden_ = false;
};

// ---------------------------------------------------------------------------------------------

int Game::Run(int argc, char** argv) {
  cfgPath_ = ExeDir() + "prophunt.ini";
  cfg_.Load(cfgPath_);
  int w = 1600, h = 900;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--gameshot" && i + 1 < argc) { shotDir_ = argv[++i]; autopilot_ = true; hidden_ = true; }
    if ((a == "--connect" || a == "--join") && i + 1 < argc) joinAddr_ = argv[++i];
    if (a == "--no-update") noUpdate_ = true;
    if (a == "--no-intro") noIntro_ = true;
  }
  if (!plat::Init("Kek Me If You Can", w, h, hidden_)) {
    MessageBoxW(nullptr, L"OpenGL 3.3 konnte nicht initialisiert werden.\nBitte Grafiktreiber aktualisieren.", L"Kek Me If You Can", MB_ICONERROR);
    return 1;
  }
  RenderSettings rs;
  rs.shadowSize = cfg_.shadowQuality == 0 ? 2048 : 4096;
  if (!r_.Init(rs) || !ui_.Init()) return 2;
  ui_.onClick = []() { audio::Play(Snd(SND_CLICK), audio::PlayParams{false, {}, 0.6f, 1.0f, false, 2, 40, 2}); };
  ui_.onHover = []() { audio::Play(Snd(SND_HOVER), audio::PlayParams{false, {}, 0.4f, 1.0f, false, 2, 40, 2}); };
  if (!hidden_ || autopilot_) audio::Init();
  if (autopilot_) cfg_.masterVolume = 0.0f;  // Testlauf stumm
  double s0 = plat::Time();
  BuildSounds();
  LOG("Klänge synthetisiert in %.0f ms", (plat::Time() - s0) * 1000);
  props::BuildCatalog();
  view_.Init(r_);
  cfg_.match.mapIndex = ClampI(cfg_.match.mapIndex, 0, (int)AllMaps().size() - 1);
  BuildMap(cfg_.match.mapIndex, menuWorld_);
  view_.LoadWorld(r_, menuWorld_);
  loadedWorld_ = &menuWorld_;
  ApplySettings();
  if (cfg_.fullscreen && !hidden_) plat::SetFullscreen(true);
  if (!autopilot_) {
    localIp_ = upnp::LocalIPv4();
    discord_.Init("1563117727370055710");
    discord_.onJoin = [this](const std::string& secret) { pendingJoin_ = secret; };
    discord_.onJoinRequest = [this](const std::string& id, const std::string& name) {
      for (const JoinRequest& j : joinRequests_)
        if (j.userId == id) return;
      joinRequests_.push_back({id, name.empty() ? "Ein Discord-Freund" : name, time_});
      audio::Play(Snd(SND_CHAT), audio::PlayParams{false, {}, 0.6f, 1.0f, false, 2, 40, 2});
    };
    update::CleanupOldFiles();
    if (cfg_.autoUpdate && !noUpdate_) StartUpdateCheck();  // läuft im Hintergrund, das Menü bleibt bedienbar
  }
  bool connectArg = !joinAddr_.empty();
  if (joinAddr_.empty()) joinAddr_ = cfg_.lastServer;
  if (connectArg) StartJoin(joinAddr_);  // Kommandozeile: --connect <adresse>
  music_ = audio::Play(Snd(SND_MUSIC_MENU), audio::PlayParams{false, {}, 0.0f, 1.0f, true, 2, 40, 1});
  ambOut_ = audio::Play(Snd(SND_AMB_OUTDOOR), audio::PlayParams{false, {}, 0.0f, 1.0f, true, 2, 40, 1});
  ambIn_ = audio::Play(Snd(SND_AMB_INDOOR), audio::PlayParams{false, {}, 0.0f, 1.0f, true, 2, 40, 1});
  audio::SetOcclusionTest([this](Vec3 a, Vec3 b) {
    const World* w2 = loadedWorld_;
    if (!w2) return false;
    RayHit hh = w2->Raycast(a, b - a, Length(b - a), COL_BULLET, false);
    return hh.hit && hh.t < Length(b - a) - 0.4f;
  });

  // Intros: nicht bei Testläufen, nicht beim direkten Beitreten (Discord/Kommandozeile)
  if (!autopilot_ && !hidden_ && cfg_.intros && !noIntro_ && !connectArg) PlayIntros();

  if (autopilot_) {
    Autopilot();
  } else {
    double last = plat::Time();
    while (plat::PumpEvents()) {
      double now = plat::Time();
      float dt = (float)std::min(0.1, now - last);
      last = now;
      Frame(dt);
      plat::Present();
    }
  }
  if (session_) LeaveGame();
  cfg_.fullscreen = plat::IsFullscreen();
  SaveCfg();
  audio::Shutdown();
  plat::Shutdown();
  return 0;
}

void Game::ApplySettings() {
  RenderSettings rs = r_.Settings();
  rs.shadowSize = cfg_.shadowQuality == 0 ? 2048 : 4096;
  rs.ssao = cfg_.ssao;
  rs.bloom = cfg_.bloom;
  rs.fxaa = cfg_.fxaa;
  r_.ApplySettings(rs);
  plat::SetVSync(cfg_.vsync);
  audio::SetMasterVolume(cfg_.masterVolume);
  audio::SetGroupVolume(1, cfg_.musicVolume * 2.0f);
}

bool Game::InGameInput() const {
  return screen_ == Screen::InGame && session_ && session_->Ready() && !paused_ && !chatOpen_ && plat::IsFocused() && !settingsOverlay_;
}

void Game::StartLocal(bool host) {
  SaveCfg();
  auto s = std::make_unique<LocalSession>();
  if (!s->Start(cfg_.match, cfg_.name, host, cfg_.serverName)) {
    error_ = host ? "Server konnte nicht gestartet werden (UDP-Port 27015 belegt?)" : "Spiel konnte nicht gestartet werden";
    errorTime_ = 6;
    return;
  }
  session_ = std::move(s);
  session_->makeCmd = [this]() { return MakeCmd(); };
  hosting_ = host;
  if (host) portMapper_.Start(GAME_PORT, "Kek Me If You Can");
  partyId_ = StrFormat("kek-%08x%08x", rng_.Next(), (u32)time(nullptr));
  sessionStart_ = (long long)time(nullptr);
  serverAddr_.clear();
  presenceTimer_ = 0;
  view_.LoadWorld(r_, session_->GetWorld());
  loadedWorld_ = &session_->GetWorld();
  screen_ = Screen::InGame;
  paused_ = chatOpen_ = tauntMenu_ = false;
  feed_.clear();
  chat_.clear();
  popups_.clear();
  lastRound_ = -1;
  AddChat("", host ? "Server läuft. Esc → Freunde einladen (Code, Discord oder IP-Adresse)."
                   : "Offline-Spiel gegen Bots. F1 zeigt die Steuerung.",
          TEAM_SPECTATOR, true);
}

void Game::StartJoin(const std::string& addr) {
  std::string resolved = ResolveJoinInput(addr);
  cfg_.lastServer = addr;
  SaveCfg();
  auto c = std::make_unique<NetClient>();
  c->Connect(resolved, cfg_.name);
  hosting_ = false;
  // Einmal numerisch auflösen, damit Pausenmenü und Discord nicht ständig DNS fragen
  net::Address na;
  serverAddr_ = net::Address::Parse(resolved, GAME_PORT, na) && na.ip != 0 ? na.ToString() : "";
  partyId_ = "kek-" + serverAddr_;
  sessionStart_ = (long long)time(nullptr);
  presenceTimer_ = 0;
  session_ = std::move(c);
  session_->makeCmd = [this]() { return MakeCmd(); };
  screen_ = Screen::Connecting;
  feed_.clear();
  chat_.clear();
  lastRound_ = -1;
}

void Game::LeaveGame(const std::string& reason) {
  session_.reset();
  if (hosting_) portMapper_.Remove();
  hosting_ = false;
  serverAddr_.clear();
  presenceTimer_ = 0;
  plat::SetMouseCaptured(false);
  view_.LoadWorld(r_, menuWorld_);
  loadedWorld_ = &menuWorld_;
  screen_ = Screen::Main;
  paused_ = chatOpen_ = tauntMenu_ = settingsOverlay_ = false;
  if (!reason.empty()) {
    error_ = reason;
    errorTime_ = 7;
  }
}

void Game::Center(const std::string& text, Color c, float dur) {
  center_ = text;
  centerSub_.clear();
  centerCol_ = c;
  centerTime_ = dur;
  centerDur_ = dur;
}

void Game::AddChat(const std::string& who, const std::string& text, Team team, bool system) {
  ChatLine l;
  l.who = who;
  l.text = text;
  l.team = team;
  l.system = system;
  l.time = (float)time_;
  chat_.push_back(l);
  if (chat_.size() > 40) chat_.erase(chat_.begin());
}

// ---------------------------------------------------------------------------------------------
// Eingabe
// ---------------------------------------------------------------------------------------------

UserCmd Game::MakeCmd() {
  UserCmd c;
  c.yaw = yaw_;
  c.pitch = pitch_;
  c.weapon = weapon_;
  if (!InGameInput()) return c;
  const ClientState& st = session_->State();
  int me = session_->LocalId();
  Team team = me >= 0 ? st.players[me].team : TEAM_SPECTATOR;
  auto tap = [](int vk) { bool t = plat::ConsumeKeyTap(vk); return t || plat::KeyDown(vk); };
  if (plat::KeyDown('W')) c.buttons |= BTN_FORWARD;
  if (plat::KeyDown('S')) c.buttons |= BTN_BACK;
  if (plat::KeyDown('A')) c.buttons |= BTN_LEFT;
  if (plat::KeyDown('D')) c.buttons |= BTN_RIGHT;
  if (tap(VK_SPACE)) c.buttons |= BTN_JUMP;
  if (plat::KeyDown(VK_SHIFT)) c.buttons |= BTN_SPRINT;
  if (plat::KeyDown(VK_CONTROL) || plat::KeyDown('C')) c.buttons |= BTN_CROUCH;
  if (!tauntMenu_) {
    bool fire = plat::ConsumeMouseTap(plat::MOUSE_LEFT) || plat::MouseDown(plat::MOUSE_LEFT);
    if (fire) c.buttons |= BTN_FIRE;
    if (plat::MouseDown(plat::MOUSE_RIGHT)) c.buttons |= BTN_AIM;
  }
  if (tap('E')) c.buttons |= BTN_USE;
  if (tap('R')) c.buttons |= team == TEAM_PROPS ? BTN_LOCK : BTN_RELOAD;
  if (tap('Q')) c.buttons |= team == TEAM_PROPS ? BTN_DECOY : BTN_ABILITY;
  if (tap('G')) c.buttons |= team == TEAM_PROPS ? BTN_DECOY : BTN_ABILITY;
  if (tauntTap_) {
    c.buttons |= BTN_TAUNT;
    c.taunt = (u8)(tauntChoice_ + 1);
    tauntTap_ = false;
  } else if (tap('F')) {
    c.buttons |= BTN_TAUNT;
    c.taunt = 0;
  }
  return c;
}

void Game::UpdateView(float dt) {
  if (!session_ || !session_->Ready()) return;
  const ClientState& st = session_->State();
  int me = session_->LocalId();
  const ClientPlayer* lp = me >= 0 ? &st.players[me] : nullptr;
  bool look = InGameInput() && !tauntMenu_;
  plat::SetMouseCaptured(look);
  if (look) {
    Vec2 md = plat::MouseDelta();
    float sens = 0.0022f * cfg_.sensitivity * (plat::MouseDown(plat::MOUSE_RIGHT) && lp && lp->team == TEAM_HUNTERS ? 0.62f : 1.0f);
    yaw_ = WrapAngle(yaw_ - md.x * sens);
    pitch_ = Clamp(pitch_ - md.y * sens * (cfg_.invertY ? -1.0f : 1.0f), -1.52f, 1.52f);
  }
  // Teamwechsel / Tod / Rundenstart erkennen
  if (lp) {
    if (lp->team != lastTeam_ || (lp->alive && !lastAlive_)) {
      if (lp->alive) {
        // Blickrichtung des nächsten Spawnpunkts übernehmen (der Server übernimmt danach unsere Richtung)
        const World& w = session_->GetWorld();
        const auto& sp = lp->team == TEAM_HUNTERS ? w.hunterSpawns : w.propSpawns;
        float best = 1e9f;
        yaw_ = lp->yaw;
        for (const SpawnPoint& s : sp) {
          float d = DistanceXZ(s.pos, lp->pos);
          if (d < best) { best = d; yaw_ = s.yaw; }
        }
        pitch_ = 0;
        weapon_ = WPN_SMG;
      }
      spectate_ = -1;
      freeCam_ = false;
      tauntMenu_ = false;
    }
    if (!lp->alive && lastAlive_) {
      freeCamPos_ = view_.Cam().pos;
      spectate_ = -1;
    }
    lastTeam_ = lp->team;
    lastAlive_ = lp->alive;
  }
  if (!InGameInput()) return;
  // Waffenwahl
  if (lp && lp->alive && lp->team == TEAM_HUNTERS) {
    if (plat::KeyPressed('1')) weapon_ = WPN_SMG;
    if (plat::KeyPressed('2')) weapon_ = WPN_SHOTGUN;
    if (plat::KeyPressed('3') && lp->ammo[WPN_GRENADE] > 0) weapon_ = WPN_GRENADE;
    int wh = plat::WheelDelta();
    if (wh != 0) {
      int maxW = lp->ammo[WPN_GRENADE] > 0 ? 3 : 2;
      weapon_ = (u8)(((int)weapon_ - wh % maxW + maxW * 4) % maxW);
    }
    if (lp->weapon == WPN_SMG && weapon_ == WPN_GRENADE && lp->ammo[WPN_GRENADE] == 0) weapon_ = WPN_SMG;
  }
  // Taunt-Menü
  if (lp && lp->alive && lp->team == TEAM_PROPS) {
    if (plat::KeyPressed('T')) tauntMenu_ = !tauntMenu_;
    if (tauntMenu_) {
      for (int k = 0; k < 9; k++)
        if (plat::KeyPressed('1' + k)) {
          tauntChoice_ = k;
          tauntTap_ = true;
          tauntMenu_ = false;
        }
    }
  } else {
    tauntMenu_ = false;
  }
  // Zuschauer
  if (lp && !lp->alive) {
    auto cycle = [&](int dir) {
      for (int k = 1; k <= MAX_PLAYERS; k++) {
        int i = ((spectate_ < 0 ? 0 : spectate_) + dir * k + MAX_PLAYERS * 4) % MAX_PLAYERS;
        if (st.players[i].active && st.players[i].alive) { spectate_ = i; return; }
      }
      spectate_ = -1;
    };
    if (plat::MousePressed(plat::MOUSE_LEFT)) { cycle(1); freeCam_ = false; }
    if (plat::MousePressed(plat::MOUSE_RIGHT)) { cycle(-1); freeCam_ = false; }
    if (plat::KeyPressed(VK_SPACE)) {
      freeCam_ = !freeCam_;
      freeCamPos_ = view_.Cam().pos;
    }
    if (spectate_ >= 0 && !st.players[spectate_].alive) cycle(1);
    if (spectate_ < 0 && !freeCam_) cycle(1);
    if (freeCam_) {
      Vec3 f = DirFromYawPitch(yaw_, pitch_), rgt = RightFromYaw(yaw_);
      float sp = plat::KeyDown(VK_SHIFT) ? 22.0f : 8.0f;
      if (plat::KeyDown('W')) freeCamPos_ += f * sp * dt;
      if (plat::KeyDown('S')) freeCamPos_ -= f * sp * dt;
      if (plat::KeyDown('D')) freeCamPos_ += rgt * sp * dt;
      if (plat::KeyDown('A')) freeCamPos_ -= rgt * sp * dt;
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Ereignisse
// ---------------------------------------------------------------------------------------------

void Game::HandleEvent(const GameEvent& e) {
  const ClientState& st = session_->State();
  const World& w = session_->GetWorld();
  int me = session_->LocalId();
  const ClientPlayer* lp = me >= 0 ? &st.players[me] : nullptr;
  view_.OnEvent(e, st, me, w);
  auto P3 = [](Vec3 pos, float vol, float minD, float maxD, float pitch = 1.0f) {
    audio::PlayParams p;
    p.positional = true;
    p.pos = pos;
    p.volume = vol;
    p.minDist = minD;
    p.maxDist = maxD;
    p.pitch = pitch;
    return p;
  };
  auto P2 = [](float vol, float pitch = 1.0f) {
    audio::PlayParams p;
    p.volume = vol;
    p.pitch = pitch;
    return p;
  };
  auto name = [&](int id) -> std::string { return id >= 0 && id < MAX_PLAYERS ? st.players[id].name : "?"; };
  float rp = rng_.Range(0.94f, 1.06f);
  switch (e.type) {
    case EV_SHOT: {
      int snd = e.b == WPN_SHOTGUN ? SND_SHOTGUN : SND_SMG;
      if (e.a == me) audio::Play(Snd(snd), P2(0.55f, rp));
      else audio::Play(Snd(snd), P3(e.pos, 1.0f, 4.0f, 110.0f, rp));
      break;
    }
    case EV_IMPACT: {
      int snd = SND_IMPACT_WALL;
      if (e.b == MAT_METAL || e.b == MAT_MIRROR || e.b == MAT_GOLD) snd = SND_IMPACT_METAL;
      else if (e.b == MAT_GLASS) snd = SND_IMPACT_GLASS;
      else if (e.c || e.b == MAT_WOOD || e.b == MAT_PARQUET) snd = SND_IMPACT_WOOD;
      audio::Play(Snd(snd), P3(e.pos, 0.5f, 1.5f, 30.0f, rp));
      break;
    }
    case EV_HIT:
      audio::Play(Snd(SND_HIT_PROP), P3(e.pos, 0.8f, 2.0f, 35.0f, rp));
      if (e.a == me) {
        hitMarker_ = 0.25f;
        audio::Play(Snd(SND_HITMARKER), P2(0.5f));
      }
      if (e.b == me) view_.Shake(0.25f);
      break;
    case EV_PENALTY:
      if (e.a == me) {
        audio::Play(Snd(SND_PENALTY), P2(0.5f));
        Popup p;
        p.text = StrFormat("−%.0f HP · falsches Objekt", e.f);
        p.color = kRed;
        p.life = p.time = 1.6f;
        popups_.push_back(p);
      }
      break;
    case EV_KILL: {
      bool propVictim = e.f >= 0;
      audio::Play(Snd(propVictim ? SND_KILL_PROP : SND_KILL_HUNTER), P3(e.pos, 1.0f, 3.0f, 50.0f));
      FeedEntry fe;
      fe.victim = name(e.b);
      fe.vt = propVictim ? TEAM_PROPS : TEAM_HUNTERS;
      fe.time = (float)time_;
      if (e.a < MAX_PLAYERS && e.a != e.b) {
        fe.killer = name(e.a);
        fe.kt = TEAM_HUNTERS;
      }
      if (propVictim) fe.extra = "als " + props::Get((int)e.f).name;
      else if (e.c == KILL_PENALTY) fe.extra = "zu viele Fehlschüsse";
      else if (e.c == KILL_FALL) fe.extra = "abgestürzt";
      feed_.push_back(fe);
      if (feed_.size() > 6) feed_.erase(feed_.begin());
      if (e.a == me && e.b != me) {
        killMarker_ = 0.5f;
        Center(StrFormat("Gefunden: %s", name(e.b).c_str()), kHunter, 2.0f);
        centerSub_ = "+100 Punkte, +25 HP";
      }
      if (e.b == me) {
        if (e.c == KILL_PENALTY) Center("Zu viele Fehlschüsse!", kRed, 4.0f);
        else Center(e.a < MAX_PLAYERS && e.a != me ? "Gefunden von " + name(e.a) : "Ausgeschieden", kRed, 4.0f);
        centerSub_ = st.infection && propVictim ? "Du jagst gleich mit!" : "Linke/rechte Maustaste: Spieler wechseln · Leertaste: freie Kamera";
      }
      break;
    }
    case EV_TAUNT:
      audio::Play(Snd(SND_TAUNT_FIRST + ClampI(e.b, 0, NUM_TAUNTS - 1)), P3(e.pos, 1.0f, 4.0f, 75.0f));
      break;
    case EV_DISGUISE:
      audio::Play(Snd(SND_DISGUISE), P3(e.pos, 0.6f, 2.0f, 22.0f));
      if (e.a == me) {
        Popup p;
        p.text = "Verwandelt: " + props::Get(e.c).name + " (Drehung gesperrt – R zum Lösen)";
        p.color = kProp;
        p.life = p.time = 2.5f;
        popups_.push_back(p);
      }
      break;
    case EV_DECOY:
      audio::Play(Snd(SND_DECOY), P3(e.pos, 0.6f, 2.0f, 20.0f));
      break;
    case EV_DECOY_BREAK:
      audio::Play(Snd(SND_DECOY_BREAK), P3(e.pos, 0.9f, 3.0f, 40.0f));
      break;
    case EV_SONAR:
      audio::Play(Snd(SND_SONAR), P3(e.pos, 0.9f, 4.0f, 55.0f));
      break;
    case EV_PING:
      audio::Play(Snd(SND_PING), P3(e.pos, 1.0f, 3.0f, 45.0f));
      break;
    case EV_PHASE:
      if (e.b == (u8)Phase::Hiding) {
        audio::Play(Snd(SND_ROUND_START), P2(0.6f));
        if (lp && lp->team == TEAM_HUNTERS) Center("Augen zu! Die Props verstecken sich", kHunter, 4.0f);
        else Center(StrFormat("Runde %d – versteck dich!", st.round), kProp, 4.0f);
        centerSub_ = lp && lp->team == TEAM_PROPS ? "E: in ein Objekt verwandeln · R: Drehung sperren · F/T: Taunt · Q: Köder"
                                                  : "Gleich darfst du suchen. Falsche Treffer kosten Lebenspunkte!";
      } else if (e.b == (u8)Phase::Hunting) {
        audio::Play(Snd(SND_GO), P2(0.5f));
        audio::Play(Snd(SND_HUNT_START), P2(0.45f));
        Center("Die Jagd beginnt!", kGold, 2.5f);
      } else if (e.b == (u8)Phase::MatchEnd) {
        int best = -1;
        for (int i = 0; i < MAX_PLAYERS; i++)
          if (st.players[i].active && (best < 0 || st.players[i].score > st.players[best].score)) best = i;
        Center("Spielende!", kGold, 8.0f);
        if (best >= 0) centerSub_ = StrFormat("Sieger: %s mit %d Punkten", st.players[best].name, st.players[best].score);
      }
      break;
    case EV_ROUND_END: {
      bool win = !lp || lp->team == TEAM_SPECTATOR || lp->team == (Team)e.b;
      audio::Play(Snd(win ? SND_WIN : SND_LOSE), P2(0.6f));
      Center(e.b == TEAM_PROPS ? "Die Props gewinnen!" : "Die Jäger gewinnen!", TeamColor((Team)e.b), 6.0f);
      centerSub_ = e.b == TEAM_PROPS ? "Überlebende Props erhalten +50 Punkte" : "Alle Props wurden gefunden";
      break;
    }
    case EV_COUNTDOWN:
      if (e.b == 60) {
        audio::Play(Snd(SND_ENDSPURT), P2(0.6f));
        Center("Endspurt!", kRed, 3.0f);
        centerSub_ = "Noch 60 Sekunden – Props tauntern jetzt alle 20 Sekunden";
      } else {
        audio::Play(Snd(SND_COUNTDOWN), P2(0.45f));
      }
      break;
    case EV_JUMP:
      if (e.a != me) audio::Play(Snd(SND_JUMP), P3(e.pos, 0.4f, 1.5f, 15.0f, rp));
      break;
    case EV_RELOAD:
      if (e.a == me) audio::Play(Snd(e.b == WPN_SHOTGUN ? SND_RELOAD_SHOTGUN : SND_RELOAD_SMG), P2(0.5f));
      else audio::Play(Snd(e.b == WPN_SHOTGUN ? SND_RELOAD_SHOTGUN : SND_RELOAD_SMG), P3(e.pos + Vec3(0, 1, 0), 0.5f, 1.5f, 18.0f));
      break;
    case EV_EMPTY:
      if (e.a == me) audio::Play(Snd(SND_EMPTY), P2(0.5f));
      break;
    case EV_GRENADE:
      audio::Play(Snd(SND_GRENADE_THROW), P3(e.pos, 0.7f, 2.0f, 25.0f));
      break;
    case EV_EXPLOSION:
      audio::Play(Snd(SND_EXPLOSION), P3(e.pos, 1.0f, 8.0f, 140.0f));
      break;
    case EV_CHAT:
      audio::Play(Snd(SND_CHAT), P2(0.4f));
      AddChat(name(e.a), e.text, e.a < MAX_PLAYERS ? st.players[e.a].team : TEAM_SPECTATOR, false);
      break;
    case EV_JOIN:
      if (e.a < MAX_PLAYERS && !st.players[e.a].bot) AddChat("", name(e.a) + " ist dem Spiel beigetreten", TEAM_SPECTATOR, true);
      break;
    case EV_LEAVE:
      break;
    case EV_LOCK:
      if (e.a == me) {
        audio::Play(Snd(SND_LOCK), P2(0.5f));
        Popup p;
        p.text = e.b ? "Drehung gesperrt" : "Drehung folgt der Kamera";
        p.color = kWhite;
        p.life = p.time = 1.2f;
        popups_.push_back(p);
      }
      break;
    case EV_NOFIT:
      if (e.a == me) {
        audio::Play(Snd(SND_NOFIT), P2(0.5f));
        Popup p;
        p.text = "Hier ist kein Platz dafür";
        p.color = kRed;
        p.life = p.time = 1.5f;
        popups_.push_back(p);
      }
      break;
    case EV_TEAMSWITCH:
      if (e.a == me) Center("Du jagst jetzt mit!", kHunter, 3.0f);
      break;
  }
}

void Game::UpdateAudio(float dt) {
  bool inGame = screen_ == Screen::InGame && session_ && session_->Ready();
  const Camera& cam = view_.Cam();
  audio::SetListener(cam.pos, cam.Forward(), cam.Right());
  float sky = 0.5f;
  if (inGame) sky = session_->GetWorld().SkyAt(cam.pos);
  float targetOut = inGame ? 0.35f * sky + 0.05f : 0.08f;
  float targetIn = inGame ? 0.3f * (1.0f - sky) : 0.0f;
  float targetMusic = inGame ? 0.0f : 0.5f;
  ambOutVol_ = Damp(ambOutVol_, targetOut, 2.0f, dt);
  ambInVol_ = Damp(ambInVol_, targetIn, 2.0f, dt);
  musicVol_ = Damp(musicVol_, targetMusic, 1.0f, dt);
  audio::SetVoiceVolume(ambOut_, ambOutVol_);
  audio::SetVoiceVolume(ambIn_, ambInVol_);
  audio::SetVoiceVolume(music_, musicVol_);
  if (inGame) {
    const ClientState& st = session_->State();
    int me = session_->LocalId();
    for (int i = 0; i < MAX_PLAYERS; i++) {
      const ClientPlayer& p = st.players[i];
      if (!p.active || !p.alive || !p.onGround) { stepAcc_[i] = 0; continue; }
      float sp = sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z);
      if (sp < 1.0f) continue;
      stepAcc_[i] += sp * dt;
      float stride = sp > 5.5f ? 2.1f : 1.7f;
      if (stepAcc_[i] < stride) continue;
      stepAcc_[i] = 0;
      bool prop = p.team == TEAM_PROPS;
      audio::PlayParams pp;
      pp.pitch = rng_.Range(0.9f, 1.1f) * (prop ? 1.25f : 1.0f);
      if (i == me) {
        pp.volume = prop ? 0.15f : 0.28f;
      } else {
        pp.positional = true;
        pp.pos = p.pos;
        pp.volume = prop ? 0.3f : 0.7f;
        pp.minDist = 1.5f;
        pp.maxDist = prop ? 12.0f : 24.0f;
      }
      audio::Play(Snd(SND_STEP0 + rng_.Int(0, 3)), pp);
    }
  }
  audio::Update(dt);
}

// ---------------------------------------------------------------------------------------------
// Hauptschleife
// ---------------------------------------------------------------------------------------------

void Game::Frame(float dt) {
  time_ += dt;
  fps_ = Lerp(fps_, 1.0f / std::max(dt, 0.0001f), 0.05f);
  int W = plat::Width(), H = plat::Height();
  if (plat::KeyPressed(VK_F11)) plat::SetFullscreen(!plat::IsFullscreen());

  // Tasten mit Vorrang
  if (screen_ == Screen::InGame && session_) {
    if (plat::KeyPressed(VK_ESCAPE)) {
      if (settingsOverlay_) settingsOverlay_ = false, SaveCfg();
      else if (chatOpen_) chatOpen_ = false;
      else if (tauntMenu_) tauntMenu_ = false;
      else if (help_) help_ = false;
      else paused_ = !paused_;
    }
    if (!paused_ && plat::KeyPressed(VK_F1)) help_ = !help_;
    if (!paused_ && plat::KeyPressed(VK_RETURN)) {
      if (chatOpen_) {
        if (!chatInput_.empty()) session_->SendChat(chatInput_);
        chatInput_.clear();
        chatOpen_ = false;
        ui_.ClearFocus();
      } else if (!settingsOverlay_) {
        chatOpen_ = true;
        chatInput_.clear();
        ui_.Focus("chat");
      }
    }
  }

  if (session_) {
    session_->Update(dt);
    if (session_->Failed()) {
      LeaveGame(session_ ? session_->StatusText() : "Verbindung beendet");
    } else if (screen_ == Screen::Connecting && session_->Ready()) {
      screen_ = Screen::InGame;
      view_.LoadWorld(r_, session_->GetWorld());
      loadedWorld_ = &session_->GetWorld();
      AddChat("", "Verbunden mit " + session_->ServerInfo(), TEAM_SPECTATOR, true);
    }
  }
  if (session_ && screen_ == Screen::InGame && session_->Ready()) {
    UpdateView(dt);
    std::vector<GameEvent> evs;
    session_->TakeEvents(evs);
    for (const GameEvent& e : evs) HandleEvent(e);
    view_.Update(dt);
    const ClientState& st = session_->State();
    if (st.round != lastRound_) lastRound_ = st.round;
    ViewInput vi;
    vi.yaw = yaw_;
    vi.pitch = pitch_;
    int me = session_->LocalId();
    vi.aiming = InGameInput() && plat::MouseDown(plat::MOUSE_RIGHT) && me >= 0 && st.players[me].team == TEAM_HUNTERS;
    vi.fov = cfg_.fov;
    vi.spectate = spectate_;
    vi.freeCam = freeCam_;
    vi.freeCamPos = freeCamPos_;
    ScreenFX fx;
    double r0 = plat::Time();
    view_.Render(r_, st, me, session_->GetWorld(), vi, (float)time_, W, H, fx);
    frameMs_ = Lerp(frameMs_, (float)(plat::Time() - r0) * 1000.0f, 0.1f);
  } else {
    plat::SetMouseCaptured(false);
    view_.RenderFlyby(r_, menuWorld_, (float)(time_ - flybyStart_), W, H);
  }
  UpdateAudio(dt);
  // Discord: Ereignisse, Beitritt über Einladung, Anzeige im Profil
  discord_.Update(time_);
  if (!pendingJoin_.empty() && screen_ != Screen::Connecting) {
    std::string j = pendingJoin_;
    pendingJoin_.clear();
    if (session_) LeaveGame();
    joinAddr_ = j;
    StartJoin(j);
  }
  UpdateUpdater();
  presenceTimer_ -= dt;
  if (presenceTimer_ <= 0) {
    presenceTimer_ = 2.0f;
    UpdatePresence();
  }
  joinRequests_.erase(std::remove_if(joinRequests_.begin(), joinRequests_.end(), [&](const JoinRequest& j) { return time_ - j.time > 30; }),
                      joinRequests_.end());
  if (!joinRequests_.empty() && (plat::KeyPressed(VK_F8) || plat::KeyPressed(VK_F9))) {
    bool accept = plat::KeyPressed(VK_F8);
    discord_.RespondJoinRequest(joinRequests_.front().userId, accept);
    if (accept) AddChat("", joinRequests_.front().name + " wurde über Discord eingeladen", TEAM_SPECTATOR, true);
    joinRequests_.erase(joinRequests_.begin());
  }

  ui_.Begin(W, H, !plat::IsMouseCaptured(), dt);
  ui_.SetBackdrop(r_.BackdropTexture());
  if (screen_ != lastScreen_) {
    lastScreen_ = screen_;
    screenT_ = 0;
  }
  screenT_ = std::min(3.0f, screenT_ + dt);
  if (!paused_) ui_.Anim("pause_in", 0.0f, 60.0f);
  switch (screen_) {
    case Screen::Main: DrawMenuBackground(); DrawMain(); break;
    case Screen::Play: DrawMenuBackground(); DrawPlaySetup(); break;
    case Screen::Join: DrawMenuBackground(); DrawJoin(); break;
    case Screen::Settings: DrawMenuBackground(); DrawSettings(false); break;
    case Screen::Connecting: DrawMenuBackground(); DrawConnecting(); break;
    case Screen::InGame:
      DrawHUD();
      if (paused_ && !settingsOverlay_) DrawPause();
      if (settingsOverlay_) DrawSettings(true);
      DrawJoinRequests();
      break;
  }
  DrawError();
  if (cfg_.showFps)
    ui_.TextShadow(FONT_SMALL, W - ui_.S(10), ui_.S(6), StrFormat("%.0f FPS · %.1f ms", fps_, frameMs_), kDim, ALIGN_RIGHT);
  ui_.End();
  errorTime_ = std::max(0.0f, errorTime_ - dt);
  centerTime_ = std::max(0.0f, centerTime_ - dt);
  hitMarker_ = std::max(0.0f, hitMarker_ - dt);
  killMarker_ = std::max(0.0f, killMarker_ - dt);
  for (Popup& p : popups_) p.time -= dt;
  popups_.erase(std::remove_if(popups_.begin(), popups_.end(), [](const Popup& p) { return p.time <= 0; }), popups_.end());
}

// ---------------------------------------------------------------------------------------------
// Menüs
// ---------------------------------------------------------------------------------------------

void Game::DrawMenuBackground() {
  float W = (float)ui_.W(), H = (float)ui_.H();
  // Abdunklung links (Lesbarkeit), oben und unten – die Kamerafahrt bleibt sichtbar
  ui_.GradientH({0, 0, W * 0.6f, H}, Color(4, 6, 11, 225), Color(4, 6, 11, 0));
  ui_.GradientV({0, H * 0.6f, W, H * 0.4f}, Color(4, 6, 11, 0), Color(4, 6, 11, 175));
  ui_.GradientV({0, 0, W, ui_.S(150)}, Color(4, 6, 11, 130), Color(4, 6, 11, 0));
}

float Game::Appear(float delay) const {
  float t = Saturate((screenT_ - delay) * 2.2f);
  return t * t * (3 - 2 * t);
}

void Game::DrawLogo(float x, float y, bool big) {
  if (big) {
    ui_.TextShadow(FONT_TITLE, x - ui_.S(4), y, "KEK ME", kWhite);
    float ly = y + ui_.LineHeight(FONT_TITLE) * 0.78f;
    ui_.TextShadow(FONT_HUGE, x, ly, "IF YOU CAN", theme::Accent, ALIGN_LEFT, ui_.S(2));
    float by = ly + ui_.LineHeight(FONT_HUGE) * 0.95f;
    ui_.RectGradH({x, by + ui_.S(6), ui_.S(46), ui_.S(4)}, theme::Accent2, theme::Accent, ui_.S(2));
    ui_.Label(x + ui_.S(58), by, "Prop Hunt · Verstecken · Suchen · Taunten", theme::Dim);
  } else {
    float w = ui_.TextShadow(FONT_LARGE, x, y, "KEK ME ", kWhite);
    ui_.TextShadow(FONT_LARGE, x + w, y, "IF YOU CAN", theme::Accent);
  }
}

void Game::SelectMenuMap(int index) {
  int n = (int)AllMaps().size();
  index = ((index % n) + n) % n;
  if (index == cfg_.match.mapIndex && menuWorld_.id == AllMaps()[index].id) return;
  cfg_.match.mapIndex = index;
  BuildMap(index, menuWorld_);
  if (!session_) {
    view_.LoadWorld(r_, menuWorld_);
    loadedWorld_ = &menuWorld_;
  }
  flybyStart_ = time_;
}

void Game::DrawMapCard(const Rect& card, bool arrows) {
  const auto& maps = AllMaps();
  int idx = ClampI(cfg_.match.mapIndex, 0, (int)maps.size() - 1);
  ui_.Panel(card, ui_.S(18));
  float px = card.x + ui_.S(24), py = card.y + ui_.S(20);
  ui_.Label(px, py, StrFormat("Karte %d / %d", idx + 1, (int)maps.size()), theme::Accent);
  ui_.Text(FONT_LARGE, px, py + ui_.S(18), maps[idx].name, kWhite);
  auto lines = ui_.Wrap(FONT_SMALL, maps[idx].description, card.w - ui_.S(48));
  float ly = py + ui_.S(18) + ui_.LineHeight(FONT_LARGE) + ui_.S(2);
  for (size_t i = 0; i < lines.size() && i < 4; i++) {
    ui_.Text(FONT_SMALL, px, ly, lines[i], theme::Dim);
    ly += ui_.LineHeight(FONT_SMALL);
  }
  // Kartenpunkte
  float dy = card.y + card.h - ui_.S(26);
  for (int i = 0; i < (int)maps.size(); i++) {
    bool on = i == idx;
    float dw = on ? ui_.S(22) : ui_.S(8);
    float dx = px + i * ui_.S(14) + (i > idx ? ui_.S(14) : 0);
    ui_.Rect_({dx, dy, dw, ui_.S(5)}, on ? theme::Accent : Color(255, 255, 255, 60), ui_.S(2.5f));
  }
  if (arrows && maps.size() > 1) {
    float bs = ui_.S(38);
    Rect l = {card.x + card.w - ui_.S(24) - bs * 2 - ui_.S(8), card.y + ui_.S(18), bs, bs};
    Rect r = {card.x + card.w - ui_.S(24) - bs, card.y + ui_.S(18), bs, bs};
    if (ui_.IconButton("mc_prev", l, icon::Left)) SelectMenuMap(idx - 1);
    if (ui_.IconButton("mc_next", r, icon::Right)) SelectMenuMap(idx + 1);
  }
}

void Game::DrawProfileChip() {
  float W = (float)ui_.W();
  std::string name = cfg_.name.empty() ? "Spieler" : cfg_.name;
  float nw = std::max(ui_.Measure(FONT_MEDIUM, name).x, ui_.Measure(FONT_SMALL, "Profil bearbeiten").x);
  Rect r = {W - ui_.S(40) - nw - ui_.S(84), ui_.S(30), nw + ui_.S(84), ui_.S(58)};
  bool hover = ui_.Hovered(r);
  float t = ui_.Anim("profile_chip", hover ? 1.0f : 0.0f, 14.0f);
  ui_.Panel(r, r.h * 0.5f);
  if (t > 0.01f) ui_.Border(r, theme::Accent.Alpha(0.8f * t), r.h * 0.5f, ui_.S(1.2f));
  Vec2 c = {r.x + ui_.S(29), r.y + r.h * 0.5f};
  ui_.Shadow({c.x - ui_.S(20), c.y - ui_.S(18), ui_.S(40), ui_.S(40)}, ui_.S(20), ui_.S(6), Color::Hex(0xFF8A1A, 80));
  ui_.RectGrad({c.x - ui_.S(20), c.y - ui_.S(20), ui_.S(40), ui_.S(40)}, Color::Hex(0xFFC444), Color::Hex(0xFF8F1F), ui_.S(20));
  size_t i0 = 0;
  Utf8Next(name.data(), name.size(), i0);
  std::string initial = UI::Upper(name.substr(0, i0));
  ui_.TextIn(FONT_MEDIUM, {c.x - ui_.S(20), c.y - ui_.S(20), ui_.S(40), ui_.S(40)}, initial, Color::Hex(0x1a1206));
  ui_.Text(FONT_MEDIUM, r.x + ui_.S(60), r.y + ui_.S(6), name, kWhite);
  ui_.Text(FONT_SMALL, r.x + ui_.S(60), r.y + ui_.S(31), "Profil bearbeiten", theme::Dim);
  if (hover && ui_.Clicked()) screen_ = Screen::Settings;
}

void Game::DrawMain() {
  float H = (float)ui_.H(), W = (float)ui_.W();
  float a0 = Appear(0);
  ui_.SetAlpha(a0);
  DrawLogo(ui_.S(80) - (1 - a0) * ui_.S(30), ui_.S(56), true);
  struct Item { const char* id; const char* label; const char* sub; const char* ic; };
  const Item items[] = {{"m_play", "Spielen", "Gegen Bots auf der gewählten Karte", icon::Play},
                        {"m_host", "Server erstellen", "Freunde im LAN, per IP oder Discord einladen", icon::Globe},
                        {"m_join", "Server beitreten", "Netzwerksuche, IP-Adresse oder Einladung", icon::People},
                        {"m_set", "Einstellungen", "Grafik, Steuerung und Audio", icon::Settings},
                        {"m_quit", "Beenden", "Zurück zum Desktop", icon::Power}};
  float y = 336;
  for (int i = 0; i < 5; i++) {
    float ai = Appear(0.08f + i * 0.05f);
    ui_.SetAlpha(ai);
    Rect r = R(70 - (1 - ai) * 40, y, 450, 68);
    if (ui_.MenuItem(items[i].id, r, items[i].label, items[i].sub, items[i].ic, i == 0)) {
      if (i == 0) { hostSetup_ = false; screen_ = Screen::Play; }
      if (i == 1) { hostSetup_ = true; screen_ = Screen::Play; }
      if (i == 2) { screen_ = Screen::Join; lan_.Start(); }
      if (i == 3) screen_ = Screen::Settings;
      if (i == 4) plat::RequestQuit();
    }
    y += 76;
  }
  ui_.SetAlpha(Appear(0.35f));
  DrawUpdateCard(R(70, y + 12, 450, 64));
  float ac = Appear(0.3f);
  ui_.SetAlpha(ac);
  DrawMapCard({W - ui_.S(510) + (1 - ac) * ui_.S(40), H - ui_.S(268), ui_.S(460), ui_.S(218)}, true);
  ui_.SetAlpha(Appear(0.2f));
  DrawProfileChip();
  if (discord_.Connected()) {
    std::string t = "Discord verbunden" + (discord_.UserName().empty() ? std::string() : " · " + discord_.UserName());
    float tw = ui_.Measure(FONT_SMALL, t).x;
    Rect dr = {W - ui_.S(40) - tw - ui_.S(46), ui_.S(98), tw + ui_.S(46), ui_.S(30)};
    ui_.Glass(dr, dr.h * 0.5f, 1.0f, Color::Hex(0x0b0f17, 170));
    ui_.Circle({dr.x + ui_.S(16), dr.y + dr.h * 0.5f}, ui_.S(4), Color::Hex(0x5865F2));
    ui_.Text(FONT_SMALL, dr.x + ui_.S(30), dr.y + (dr.h - ui_.LineHeight(FONT_SMALL)) * 0.5f, t, theme::Dim);
  }
  ui_.SetAlpha(Appear(0.4f));
  ui_.Label(ui_.S(80), H - ui_.S(42), "Eigene C++-Engine · OpenGL 3.3 · prozeduraler Klang · v" KEK_VERSION_STR, theme::Faint);
  float kx = W - ui_.S(510);
  kx += ui_.KeyCap(kx, H - ui_.S(46), "F11") + ui_.S(8);
  ui_.Text(FONT_SMALL, kx, H - ui_.S(45), "Vollbild", theme::Dim);
  ui_.SetAlpha(1);
}

// Kopfzeile der Unterseiten: Zurück-Knopf, Titel, Untertitel
bool Game::PageHeader(float x, float y, float w, const std::string& title, const std::string& sub) {
  bool back = ui_.IconButton("pg_back", {x, y + ui_.S(4), ui_.S(42), ui_.S(42)}, icon::Back);
  ui_.Text(FONT_LARGE, x + ui_.S(58), y - ui_.S(2), title, kWhite);
  ui_.Label(x + ui_.S(60), y + ui_.S(40), sub, theme::Dim);
  ui_.Rect_({x, y + ui_.S(68), w, std::max(1.0f, ui_.S(1))}, theme::Line);
  return back;
}

void Game::DrawPlaySetup() {
  float H = (float)ui_.H(), W = (float)ui_.W();
  float a = Appear(0);
  ui_.SetAlpha(a);
  DrawLogo(ui_.S(80), ui_.S(40), false);
  MatchSettings& m = cfg_.match;
  int rows = hostSetup_ ? 8 : 7;
  float rh = 50, gap = 8;
  float pw = 580, ph = 100 + rows * (rh + gap) + (hostSetup_ ? 30 : 0) + 90;
  float px = 70 - (1 - a) * 30, py = std::max(110.0f, (H / ui_.Scale() - ph) * 0.5f + 20);
  ui_.Panel(R(px, py, pw, ph), ui_.S(20));
  float x = px + 24, w = pw - 48, y = py + 22;
  bool back = PageHeader(ui_.S(x), ui_.S(y), ui_.S(w), hostSetup_ ? "Server erstellen" : "Spiel gegen Bots",
                         hostSetup_ ? "Mehrspieler · Freunde laden sich selbst ein" : "Offline · lokale Bots");
  y += 86;
  std::vector<std::string> maps;
  for (auto& mi : AllMaps()) maps.push_back(mi.name);
  int mapIdx = m.mapIndex;
  if (ui_.Selector("p_map", R(x, y, w, rh), "Karte", &mapIdx, maps)) SelectMenuMap(mapIdx);
  y += rh + gap;
  float bots = (float)m.bots;
  if (ui_.Slider("p_bots", R(x, y, w, rh), "Bots", &bots, 0, 15, "%.0f")) m.bots = (int)roundf(bots);
  y += rh + gap;
  ui_.Selector("p_skill", R(x, y, w, rh), "Bot-Stärke", &m.botSkill, {"Leicht", "Normal", "Schwer"});
  y += rh + gap;
  std::vector<int> roundsOpt = {2, 4, 6, 8, 10, 12};
  std::vector<std::string> roundsLbl;
  int ri = 2;
  for (int k = 0; k < (int)roundsOpt.size(); k++) {
    roundsLbl.push_back(StrFormat("%d Runden", roundsOpt[k]));
    if (roundsOpt[k] == m.rounds) ri = k;
  }
  if (ui_.Selector("p_rounds", R(x, y, w, rh), "Runden", &ri, roundsLbl)) m.rounds = roundsOpt[ri];
  y += rh + gap;
  std::vector<float> hideOpt = {20, 30, 45, 60};
  int hi = 1;
  for (int k = 0; k < 4; k++) if (fabsf(hideOpt[k] - m.hideTime) < 0.5f) hi = k;
  if (ui_.Selector("p_hide", R(x, y, w, rh), "Versteckzeit", &hi, {"20 Sekunden", "30 Sekunden", "45 Sekunden", "60 Sekunden"})) m.hideTime = hideOpt[hi];
  y += rh + gap;
  std::vector<float> roundOpt = {120, 180, 240, 300, 360};
  int rti = 2;
  for (int k = 0; k < 5; k++) if (fabsf(roundOpt[k] - m.roundTime) < 0.5f) rti = k;
  if (ui_.Selector("p_rtime", R(x, y, w, rh), "Rundenzeit", &rti, {"2 Minuten", "3 Minuten", "4 Minuten", "5 Minuten", "6 Minuten"})) m.roundTime = roundOpt[rti];
  y += rh + gap;
  ui_.Checkbox("p_inf", R(x, y, w, rh), "Infektion – Gefundene jagen mit", &m.infection);
  y += rh + gap;
  if (hostSetup_) {
    ui_.TextField("p_sname", R(x, y, w, rh), &cfg_.serverName, 30, "Servername", icon::Edit);
    y += rh + 6;
    ui_.Icon(ui_.S(x + 4), ui_.S(y + 3), icon::Info, theme::Faint);
    ui_.Text(FONT_SMALL, ui_.S(x + 30), ui_.S(y + 2), "LAN automatisch · Internet: Port 27015 wird per UPnP freigegeben · Einladen im Esc-Menü", theme::Faint);
    y += 30;
  }
  y += 12;
  if (ui_.ButtonEx("p_start", R(x, y, w * 0.62f, 58), hostSetup_ ? "Server starten" : "Spiel starten", icon::Play, UI::BTN_PRIMARY)) StartLocal(hostSetup_);
  if (ui_.ButtonEx("p_back", R(x + w * 0.65f, y, w * 0.35f, 58), "Zurück", nullptr, UI::BTN_SECONDARY) || back ||
      (plat::KeyPressed(VK_ESCAPE) && !ui_.AnyActive())) {
    SaveCfg();
    screen_ = Screen::Main;
  }
  float ac = Appear(0.2f);
  ui_.SetAlpha(ac);
  DrawMapCard({W - ui_.S(510) + (1 - ac) * ui_.S(40), H - ui_.S(268), ui_.S(460), ui_.S(218)}, true);
  ui_.SetAlpha(1);
}

void Game::DrawJoin() {
  float H = (float)ui_.H();
  float a = Appear(0);
  ui_.SetAlpha(a);
  DrawLogo(ui_.S(80), ui_.S(40), false);
  float pw = 600, ph = 640;
  float px = 70 - (1 - a) * 30, py = std::max(110.0f, (H / ui_.Scale() - ph) * 0.5f + 20);
  ui_.Panel(R(px, py, pw, ph), ui_.S(20));
  float x = px + 24, w = pw - 48, y = py + 22, h = 52;
  bool back = PageHeader(ui_.S(x), ui_.S(y), ui_.S(w), "Server beitreten", "LAN-Suche oder direkte Verbindung");
  y += 88;
  ui_.Label(ui_.S(x + 2), ui_.S(y), "Dein Name", theme::Dim);
  y += 22;
  ui_.TextField("j_name", R(x, y, w, h), &cfg_.name, 20, "Spielername", icon::Contact);
  y += h + 18;
  ui_.Label(ui_.S(x + 2), ui_.S(y), "IP-Adresse oder Einladung", theme::Dim);
  y += 22;
  ui_.TextField("j_addr", R(x, y, w - 222, h), &joinAddr_, 120, "Code KEK-… oder IP wie 203.0.113.7", icon::Globe);
  if (ui_.IconButton("j_paste", R(x + w - 212, y + 6, 40, 40), icon::Paste)) {
    std::string clip = plat::GetClipboardText();
    if (!clip.empty()) joinAddr_ = clip.substr(0, 200);
  }
  bool go = ui_.ButtonEx("j_go", R(x + w - 160, y, 160, h), "Verbinden", icon::Send, UI::BTN_PRIMARY, !joinAddr_.empty());
  if (ui_.Focused("j_addr") && plat::KeyPressed(VK_RETURN) && !joinAddr_.empty()) go = true;
  if (go) {
    lan_.Stop();
    StartJoin(joinAddr_);
    ui_.SetAlpha(1);
    return;
  }
  {
    std::string res = ResolveJoinInput(joinAddr_);
    std::string trimmed = joinAddr_;
    while (!trimmed.empty() && isspace((unsigned char)trimmed.back())) trimmed.pop_back();
    if (!joinAddr_.empty() && res != trimmed) {
      ui_.Icon(ui_.S(x + 4), ui_.S(y + h + 5), icon::Check, theme::Ok);
      ui_.Text(FONT_SMALL, ui_.S(x + 28), ui_.S(y + h + 4), "Einladung erkannt: " + res, theme::Dim);
    } else {
      ui_.Text(FONT_SMALL, ui_.S(x + 4), ui_.S(y + h + 4), "Einladungscodes aus Discord können komplett eingefügt werden.", theme::Faint);
    }
  }
  y += h + 34;
  lan_.Update(plat::Time());
  ui_.Text(FONT_MEDIUM, ui_.S(x), ui_.S(y), "Im Netzwerk", kWhite);
  if (ui_.IconButton("j_ref", R(x + w - 38, y - 2, 38, 38), icon::Refresh)) lan_.Refresh();
  y += 46;
  const auto& list = lan_.Servers();
  if (list.empty()) {
    Rect er = R(x, y, w, 64);
    ui_.Rect_(er, Color(255, 255, 255, 8), ui_.S(12));
    ui_.Border(er, theme::Line, ui_.S(12), ui_.S(1));
    ui_.Arc({er.x + ui_.S(34), er.y + er.h * 0.5f}, ui_.S(11), ui_.S(3), 0.3f, theme::Accent, (float)time_ * 1.3f);
    ui_.Text(FONT_NORMAL, er.x + ui_.S(60), er.y + ui_.S(10), "Suche nach Servern …", kWhite);
    ui_.Text(FONT_SMALL, er.x + ui_.S(60), er.y + ui_.S(35), "Server im selben Netzwerk erscheinen hier automatisch.", theme::Faint);
  }
  for (size_t i = 0; i < list.size() && i < 4; i++) {
    const LanServer& s = list[i];
    Rect row = R(x, y, w, 64);
    if (ui_.ButtonEx(StrFormat("j_srv%d", (int)i).c_str(), row, "", nullptr, UI::BTN_SECONDARY)) {
      lan_.Stop();
      joinAddr_ = s.addr.ToString();
      StartJoin(joinAddr_);
      ui_.SetAlpha(1);
      return;
    }
    Rect ib = {row.x + ui_.S(12), row.y + ui_.S(12), ui_.S(40), ui_.S(40)};
    ui_.Rect_(ib, theme::Accent.Alpha(0.16f), ui_.S(10));
    ui_.IconIn(FONT_ICON, ib, icon::Game, theme::Accent);
    ui_.Text(FONT_MEDIUM, row.x + ui_.S(66), row.y + ui_.S(6), s.name, kWhite);
    ui_.Text(FONT_SMALL, row.x + ui_.S(66), row.y + ui_.S(36), s.map + " · " + s.addr.ToString(), theme::Dim);
    Rect pill = {row.x + row.w - ui_.S(86), row.y + ui_.S(18), ui_.S(70), ui_.S(28)};
    ui_.Rect_(pill, Color(255, 255, 255, 20), ui_.S(14));
    ui_.TextIn(FONT_LABEL, pill, StrFormat("%d / %d", s.players, s.maxPlayers), kWhite);
    y += 72;
  }
  if (back || (plat::KeyPressed(VK_ESCAPE) && !ui_.AnyActive())) {
    lan_.Stop();
    SaveCfg();
    screen_ = Screen::Main;
  }
  ui_.SetAlpha(1);
}

void Game::DrawSettings(bool overlay) {
  float W = (float)ui_.W(), H = (float)ui_.H();
  float a = overlay ? 1.0f : Appear(0);
  if (overlay) {
    ui_.Glass({0, 0, W, H}, 0, 1.0f, Color::Hex(0x05070c, 150));
  } else {
    ui_.SetAlpha(a);
    DrawLogo(ui_.S(80), ui_.S(40), false);
  }
  float pw = 1040, ph = 744;
  float px = overlay ? (W / ui_.Scale() - pw) * 0.5f : 70 - (1 - a) * 30;
  float py = std::max(overlay ? 40.0f : 110.0f, (H / ui_.Scale() - ph) * 0.5f + (overlay ? 0 : 20));
  ui_.Panel(R(px, py, pw, ph), ui_.S(20));
  float x = px + 24, w = pw - 48, y = py + 22, h = 48, gap = 8;
  bool back = PageHeader(ui_.S(x), ui_.S(y), ui_.S(w), "Einstellungen", "Werden automatisch gespeichert");
  y += 88;
  float cw = (w - 28) * 0.5f;
  float x2 = x + cw + 28;
  // linke Spalte: Spiel + Audio
  float yl = y;
  ui_.Label(ui_.S(x + 2), ui_.S(yl), "Spiel", theme::Accent);
  yl += 24;
  ui_.TextField("s_name", R(x, yl, cw, h), &cfg_.name, 20, "Spielername", icon::Contact);
  yl += h + gap;
  ui_.Slider("s_sens", R(x, yl, cw, h), "Mausempfindlichkeit", &cfg_.sensitivity, 0.2f, 3.0f, "%.2f");
  yl += h + gap;
  ui_.Checkbox("s_inv", R(x, yl, cw, h), "Maus invertieren", &cfg_.invertY);
  yl += h + gap;
  ui_.Slider("s_fov", R(x, yl, cw, h), "Sichtfeld", &cfg_.fov, 70, 110, "%.0f°");
  yl += h + gap + 14;
  ui_.Label(ui_.S(x + 2), ui_.S(yl), "Audio", theme::Accent);
  yl += 24;
  if (ui_.Slider("s_vol", R(x, yl, cw, h), "Lautstärke", &cfg_.masterVolume, 0, 1, "%.2f")) ApplySettings();
  yl += h + gap;
  if (ui_.Slider("s_mus", R(x, yl, cw, h), "Musik & Atmosphäre", &cfg_.musicVolume, 0, 1, "%.2f")) ApplySettings();
  yl += h + gap + 14;
  ui_.Label(ui_.S(x + 2), ui_.S(yl), "Updates", theme::Accent);
  yl += 24;
  ui_.Checkbox("s_upd", R(x, yl, cw, h), "Beim Start automatisch aktualisieren", &cfg_.autoUpdate);
  yl += h + gap;
  {
    update::Status us = updater_.GetStatus();
    bool busy = updater_.Busy();
    bool ready = us.state == update::Status::Installed;
    std::string label = ready ? "Neu starten" : busy ? "Prüfe …" : "Jetzt prüfen";
    float bw = 190;
    if (ui_.ButtonEx("s_updnow", R(x + cw - bw, yl, bw, h), label, ready ? icon::Refresh : icon::Download,
                     ready ? UI::BTN_PRIMARY : UI::BTN_SECONDARY, !busy || ready)) {
      if (ready) restartAt_ = time_;
      else StartUpdateCheck();
    }
    ui_.Text(FONT_NORMAL, ui_.S(x + 4), ui_.S(yl + 1), "Version " KEK_VERSION_STR, kWhite);
    auto lines = ui_.Wrap(FONT_SMALL, UpdateStatusText(), ui_.S(cw - bw - 16));
    Color sc = us.state == update::Status::Failed ? theme::Danger : ready ? theme::Ok : theme::Dim;
    if (!lines.empty()) ui_.Text(FONT_SMALL, ui_.S(x + 4), ui_.S(yl + 26), lines[0], sc);
  }
  // rechte Spalte: Grafik
  float yr = y;
  ui_.Label(ui_.S(x2 + 2), ui_.S(yr), "Grafik", theme::Accent);
  yr += 24;
  if (ui_.Selector("s_shadow", R(x2, yr, cw, h), "Schatten", &cfg_.shadowQuality, {"Mittel", "Hoch"})) ApplySettings();
  yr += h + gap;
  if (ui_.Checkbox("s_ssao", R(x2, yr, cw, h), "Umgebungsverdeckung (SSAO)", &cfg_.ssao)) ApplySettings();
  yr += h + gap;
  if (ui_.Checkbox("s_bloom", R(x2, yr, cw, h), "Bloom", &cfg_.bloom)) ApplySettings();
  yr += h + gap;
  if (ui_.Checkbox("s_fxaa", R(x2, yr, cw, h), "Kantenglättung (FXAA)", &cfg_.fxaa)) ApplySettings();
  yr += h + gap;
  if (ui_.Checkbox("s_vsync", R(x2, yr, cw, h), "VSync", &cfg_.vsync)) ApplySettings();
  yr += h + gap;
  bool fs = plat::IsFullscreen();
  if (ui_.Checkbox("s_fs", R(x2, yr, cw, h), "Vollbild (F11)", &fs)) plat::SetFullscreen(fs);
  yr += h + gap;
  ui_.Checkbox("s_fps", R(x2, yr, cw, h), "FPS anzeigen", &cfg_.showFps);
  yr += h + gap;
  ui_.Checkbox("s_intro", R(x2, yr, cw, h), "Intros beim Start zeigen", &cfg_.intros);
  float by = py + ph - 24 - 56;
  if (ui_.ButtonEx("s_back", R(x + w - 220, by, 220, 56), "Fertig", icon::Check, UI::BTN_PRIMARY) || back ||
      (plat::KeyPressed(VK_ESCAPE) && !ui_.AnyActive() && !overlay)) {
    cfg_.fullscreen = plat::IsFullscreen();
    SaveCfg();
    if (overlay) settingsOverlay_ = false;
    else screen_ = Screen::Main;
  }
  ui_.SetAlpha(1);
}

void Game::DrawConnecting() {
  float W = (float)ui_.W(), H = (float)ui_.H();
  float a = Appear(0);
  ui_.SetAlpha(a);
  Rect p = {W * 0.5f - ui_.S(250), H * 0.5f - ui_.S(120) + (1 - a) * ui_.S(20), ui_.S(500), ui_.S(240)};
  ui_.Panel(p, ui_.S(20));
  Vec2 c = {p.x + p.w * 0.5f, p.y + ui_.S(62)};
  ui_.Ring(c, ui_.S(24), ui_.S(4), Color(255, 255, 255, 30));
  ui_.Arc(c, ui_.S(24), ui_.S(4), 0.28f, theme::Accent, (float)time_ * 1.2f);
  ui_.TextIn(FONT_LARGE, {p.x, p.y + ui_.S(96), p.w, ui_.S(44)}, "Verbinde …", kWhite);
  ui_.TextIn(FONT_SMALL, {p.x, p.y + ui_.S(138), p.w, ui_.S(24)}, session_ ? session_->StatusText() : "", theme::Dim);
  if (ui_.ButtonEx("c_cancel", {p.x + p.w * 0.5f - ui_.S(90), p.y + p.h - ui_.S(64), ui_.S(180), ui_.S(46)}, "Abbrechen", icon::Close,
                   UI::BTN_SECONDARY) ||
      plat::KeyPressed(VK_ESCAPE))
    LeaveGame();
  ui_.SetAlpha(1);
}

void Game::DrawError() {
  if (errorTime_ <= 0 || error_.empty()) return;
  float a = Saturate(errorTime_ * 2.0f);
  float W = (float)ui_.W();
  Vec2 sz = ui_.Measure(FONT_NORMAL, error_);
  Rect r = {W * 0.5f - sz.x * 0.5f - ui_.S(40), ui_.S(26) - (1 - a) * ui_.S(16), sz.x + ui_.S(70), ui_.S(50)};
  ui_.SetAlpha(a);
  ui_.Shadow({r.x, r.y + ui_.S(4), r.w, r.h}, r.h * 0.5f, ui_.S(14), Color(0, 0, 0, 120));
  ui_.Glass(r, r.h * 0.5f, 1.0f, Color::Hex(0x3a0d0d, 200));
  ui_.Border(r, theme::Danger.Alpha(0.7f), r.h * 0.5f, ui_.S(1.2f));
  ui_.IconIn(FONT_ICON, {r.x + ui_.S(12), r.y, ui_.S(26), r.h}, icon::Warning, theme::Danger);
  ui_.Text(FONT_NORMAL, r.x + ui_.S(46), r.y + (r.h - ui_.LineHeight(FONT_NORMAL)) * 0.5f, error_, kWhite);
  ui_.SetAlpha(1);
}

// ---------------------------------------------------------------------------------------------
// Intro-Videos (in der EXE eingebettet, res/intro.rc)
// ---------------------------------------------------------------------------------------------

// Reihenfolge: Studio (Kekish Games), Engine, Spiel
static const int kIntroIds[] = {100, 101, 102};

static bool IntroResource(int id, const void*& data, size_t& size) {
  HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
  HGLOBAL g = res ? LoadResource(nullptr, res) : nullptr;
  data = g ? LockResource(g) : nullptr;
  size = res ? SizeofResource(nullptr, res) : 0;
  return data && size > 0;
}

// Leertaste/Enter/Klick: nächstes Video, Esc: alle überspringen. Läuft vor der Hauptschleife.
void Game::PlayIntros() {
  bool skipAll = false;
  for (int id : kIntroIds) {
    if (skipAll || !plat::PumpEvents()) break;
    const void* data = nullptr;
    size_t size = 0;
    if (!IntroResource(id, data, size)) continue;
    VideoPlayer vp;
    if (!vp.Open(data, size)) continue;
    double t0 = plat::Time(), last = t0, fadeOut = -1;
    vp.Play(t0, 1.0f);
    LOG("Intro %d gestartet (%.1f s)", id, vp.Duration());
    while (plat::PumpEvents()) {
      double now = plat::Time();
      float dt = (float)std::min(0.1, now - last);
      last = now;
      if (fadeOut < 0) {
        if (plat::KeyPressed(VK_ESCAPE)) {
          skipAll = true;
          fadeOut = now;
          LOG("Intros mit Esc übersprungen (nach %.1f s)", now - t0);
        } else if (plat::KeyPressed(VK_SPACE) || plat::KeyPressed(VK_RETURN) || plat::MousePressed(plat::MOUSE_LEFT)) {
          fadeOut = now;
          LOG("Intro %d übersprungen (nach %.1f s)", id, now - t0);
        }
      }
      bool running = vp.Update(now);
      float alpha = Saturate((float)(now - t0) / 0.2f);
      if (fadeOut >= 0) {
        float f = 1.0f - Saturate((float)(now - fadeOut) / 0.25f);
        alpha *= f;
        vp.SetVolume(f);
      }
      if (!running || (fadeOut >= 0 && now - fadeOut > 0.25)) break;
      int W = plat::Width(), H = plat::Height();
      vp.Draw(W, H, alpha);
      // Hinweis zum Überspringen unten rechts, erscheint nach einer Sekunde
      ui_.Begin(W, H, false, dt);
      float ha = Saturate((float)(now - t0 - 1.0) / 0.6f) * alpha * 0.8f;
      if (ha > 0.01f) {
        ui_.SetAlpha(ha);
        float kh = ui_.S(26), gap = ui_.S(8), space = ui_.S(26);
        const char* t1 = "Weiter";
        const char* t2 = "Alle überspringen";
        float total = ui_.KeyCapWidth("Leertaste", kh) + gap + ui_.Measure(FONT_SMALL, t1).x + space + ui_.KeyCapWidth("Esc", kh) + gap +
                      ui_.Measure(FONT_SMALL, t2).x;
        float x = W - ui_.S(40) - total, y = H - ui_.S(52);
        float ty = y + (kh - ui_.LineHeight(FONT_SMALL)) * 0.5f;
        x += ui_.KeyCap(x, y, "Leertaste", kh) + gap;
        x += ui_.Text(FONT_SMALL, x, ty, t1, kWhite) + space;
        x += ui_.KeyCap(x, y, "Esc", kh) + gap;
        ui_.Text(FONT_SMALL, x, ty, t2, theme::Dim);
        ui_.SetAlpha(1);
      }
      ui_.End();
      audio::Update(dt);
      plat::Present();
    }
  }
  flybyStart_ = time_;
  LOG("Intros beendet");
}

// Testlauf: zeigt das Bild bei t Sekunden aus Intro index und speichert es
bool Game::IntroShot(int index, double t, const char* name) {
  const void* data = nullptr;
  size_t size = 0;
  if (index < 0 || index >= COUNTOF(kIntroIds) || !IntroResource(kIntroIds[index], data, size)) return false;
  VideoPlayer vp;
  if (!vp.Open(data, size) || !vp.ShowFrameAt(t)) return false;
  vp.Draw(plat::Width(), plat::Height(), 1.0f);
  Shot(name);
  return true;
}

// ---------------------------------------------------------------------------------------------
// Automatische Updates (GitHub-Releases)
// ---------------------------------------------------------------------------------------------

void Game::StartUpdateCheck() {
  updater_.Start(KEK_VERSION_STR, true);
  updateCheckStart_ = time_;
  updateFailAt_ = -1;
}

void Game::UpdateUpdater() {
  if (autopilot_) return;
  update::Status s = updater_.GetStatus();
  if ((int)s.state != lastUpdateState_) {
    lastUpdateState_ = (int)s.state;
    if (s.state == update::Status::Failed) updateFailAt_ = time_;
  }
  if (s.state == update::Status::Installed && !updateHandled_) {
    updateHandled_ = true;
    bool inMenu = !session_ && screen_ != Screen::Connecting;
    if (inMenu && time_ < 45) {
      restartAt_ = time_ + 2.5;  // gerade erst gestartet: gleich in die neue Version wechseln
    } else if (session_) {
      AddChat("", "Update auf Version " + s.latest + " installiert – aktiv beim nächsten Start.", TEAM_SPECTATOR, true);
    }
  }
  if (restartAt_ > 0 && time_ >= restartAt_) {
    restartAt_ = 0;
    if (session_) LeaveGame();
    cfg_.fullscreen = plat::IsFullscreen();
    SaveCfg();
    if (update::RestartSelf()) {
      plat::RequestQuit();
    } else {
      error_ = "Neustart fehlgeschlagen – bitte das Spiel neu öffnen";
      errorTime_ = 6;
    }
  }
}

std::string Game::UpdateStatusText() const {
  update::Status s = updater_.GetStatus();
  switch (s.state) {
    case update::Status::Idle: return cfg_.autoUpdate ? "Noch nicht geprüft" : "Automatische Prüfung ist aus";
    case update::Status::Checking: return "Suche nach Updates …";
    case update::Status::UpToDate: return s.message.empty() ? "Aktuell" : s.message;
    case update::Status::Downloading: return StrFormat("Lade Version %s … %d %%", s.latest.c_str(), (int)(s.progress * 100));
    case update::Status::Installed: return "Version " + s.latest + " installiert – Neustart nötig";
    case update::Status::Failed: return "Prüfung fehlgeschlagen: " + s.message;
  }
  return "";
}

void Game::DrawUpdateCard(const Rect& r) {
  update::Status s = updater_.GetStatus();
  bool checking = s.state == update::Status::Checking && time_ - updateCheckStart_ > 0.6;
  bool failed = s.state == update::Status::Failed && updateFailAt_ >= 0 && time_ - updateFailAt_ < 8;
  bool loading = s.state == update::Status::Downloading;
  bool ready = s.state == update::Status::Installed;
  if (!checking && !failed && !loading && !ready) return;
  if (checking || failed) {
    // schmale Zeile statt Karte
    std::string t = checking ? "Suche nach Updates …" : "Update-Prüfung fehlgeschlagen: " + s.message;
    float tw = ui_.Measure(FONT_SMALL, t).x;
    Rect p = {r.x, r.y, tw + ui_.S(50), ui_.S(32)};
    ui_.Glass(p, p.h * 0.5f, 1.0f, Color::Hex(0x0b0f17, 170));
    Vec2 c = {p.x + ui_.S(17), p.y + p.h * 0.5f};
    if (checking) ui_.Arc(c, ui_.S(6), ui_.S(2), 0.3f, theme::Accent, (float)time_ * 1.4f);
    else ui_.Circle(c, ui_.S(4), theme::Danger);
    ui_.Text(FONT_SMALL, p.x + ui_.S(32), p.y + (p.h - ui_.LineHeight(FONT_SMALL)) * 0.5f, t, failed ? theme::Dim : theme::Dim);
    return;
  }
  ui_.Panel(r, ui_.S(16));
  float pad = ui_.S(14);
  Vec2 c = {r.x + pad + ui_.S(18), r.y + r.h * 0.5f};
  ui_.Circle(c, ui_.S(18), Color::Hex(0xFFB627, ready ? 255 : 40));
  if (ready && restartAt_ > 0) ui_.Arc(c, ui_.S(18), ui_.S(3), 0.3f, kWhite, (float)time_ * 1.6f);
  ui_.Icon(c.x - ui_.S(9), c.y - ui_.S(10), ready ? icon::Check : icon::Download, ready ? Color::Hex(0x141821) : theme::Accent);
  float tx = r.x + pad + ui_.S(48);
  std::string title = ready ? (restartAt_ > 0 ? "Update installiert – Neustart …" : "Update " + s.latest + " ist bereit")
                            : "Update " + s.latest + " wird geladen";
  ui_.Text(FONT_NORMAL, tx, r.y + ui_.S(10), title, kWhite);
  if (loading) {
    float bw = r.x + r.w - pad - tx;
    Rect bar = {tx, r.y + r.h - ui_.S(20), bw, ui_.S(6)};
    ui_.Rect_(bar, Color(255, 255, 255, 30), bar.h * 0.5f);
    ui_.RectGradH({bar.x, bar.y, std::max(bar.h, bar.w * Saturate(s.progress)), bar.h}, theme::Accent, theme::Accent2, bar.h * 0.5f);
    std::string pct = StrFormat("%d %%", (int)(s.progress * 100));
    ui_.Text(FONT_SMALL, r.x + r.w - pad - ui_.Measure(FONT_SMALL, pct).x, r.y + ui_.S(12), pct, theme::Dim);
  } else if (restartAt_ > 0) {
    ui_.Text(FONT_SMALL, tx, r.y + ui_.S(34), "Version " + s.latest + " startet gleich", theme::Dim);
  } else {
    ui_.Text(FONT_SMALL, tx, r.y + ui_.S(34), "Aktiv ab dem nächsten Start", theme::Dim);
    float bw = ui_.S(158);
    if (ui_.ButtonEx("upd_restart", {r.x + r.w - pad - bw, r.y + (r.h - ui_.S(40)) * 0.5f, bw, ui_.S(40)}, "Neu starten", icon::Refresh,
                     UI::BTN_PRIMARY))
      restartAt_ = time_;
  }
}

// ---------------------------------------------------------------------------------------------
// Einladungen, Internetspiel und Discord
// ---------------------------------------------------------------------------------------------

std::string Game::InviteAddress(std::string* note, Color* noteColor) const {
  upnp::Status s = portMapper_.GetStatus();
  std::string routerIp = !s.externalIp.empty() && !upnp::IsPrivateIPv4(s.externalIp) ? s.externalIp : "";
  std::string pub = !routerIp.empty() ? routerIp : ipLookup_.Result();
  std::string lan = !s.localIp.empty() ? s.localIp : localIp_;
  std::string ip = !pub.empty() ? pub : lan;
  if (note && noteColor) {
    if (s.state == upnp::Status::Working) {
      *note = "Router wird eingerichtet …";
      *noteColor = theme::Dim;
    } else if (s.state == upnp::Status::Mapped && !pub.empty()) {
      *note = "Port 27015 ist offen – Freunde können über das Internet beitreten.";
      *noteColor = theme::Ok;
    } else if (!pub.empty()) {
      *note = (s.message.empty() ? std::string("Keine automatische Freigabe") : s.message) +
              ". Gib UDP-Port 27015 im Router für " + lan + " frei.";
      *noteColor = theme::Accent;
    } else if (!s.externalIp.empty() && upnp::IsPrivateIPv4(s.externalIp)) {
      *note = "Dein Anschluss hat keine öffentliche IPv4 (Carrier-NAT/DS-Lite) – Internetspiel nur per VPN, im LAN klappt es.";
      *noteColor = theme::Danger;
    } else {
      *note = "Nur im Heimnetz erreichbar, solange die öffentliche IP unbekannt ist.";
      *noteColor = theme::Accent;
    }
  }
  return ip.empty() ? "" : StrFormat("%s:%d", ip.c_str(), GAME_PORT);
}

void Game::CopyInvite(const std::string& text, const std::string& what) {
  plat::SetClipboardText(text);
  Popup p;
  p.text = what + " kopiert – in Discord mit Strg+V einfügen";
  p.color = theme::Ok;
  p.life = p.time = 2.5f;
  popups_.push_back(p);
  if (ui_.onClick) ui_.onClick();
}

void Game::DrawInvitePanel(const Rect& p) {
  ui_.Panel(p, ui_.S(20));
  float x = p.x + ui_.S(24), y = p.y + ui_.S(20), w = p.w - ui_.S(48);
  ui_.Label(x, y, "Freunde einladen", theme::Accent);
  std::string note;
  Color nc = theme::Dim;
  std::string addr;
  if (hosting_) {
    addr = InviteAddress(&note, &nc);
  } else {
    addr = serverAddr_;
    note = "Du bist Gast – eingeladene Freunde kommen auf denselben Server.";
  }
  net::Address a;
  bool ok = !addr.empty() && net::Address::Parse(addr, GAME_PORT, a) && a.ip != 0;
  std::string code = ok ? EncodeInvite(a.ip, a.port) : "–";
  ui_.Text(FONT_SMALL, x, y + ui_.S(26), "Einladungscode", theme::Dim);
  ui_.Text(FONT_LARGE, x, y + ui_.S(44), code, kWhite, ALIGN_LEFT, ui_.S(1));
  ui_.Icon(x, y + ui_.S(96), icon::Globe, theme::Dim);
  ui_.Text(FONT_NORMAL, x + ui_.S(28), y + ui_.S(94), addr.empty() ? "Adresse wird ermittelt …" : addr, kWhite);
  auto lines = ui_.Wrap(FONT_SMALL, note, w - ui_.S(28));
  float ly = y + ui_.S(126);
  ui_.Icon(x, ly + ui_.S(1), nc.r == theme::Ok.r && nc.g == theme::Ok.g ? icon::Check : icon::Info, nc);
  for (size_t i = 0; i < lines.size() && i < 3; i++) {
    ui_.Text(FONT_SMALL, x + ui_.S(28), ly, lines[i], nc);
    ly += ui_.LineHeight(FONT_SMALL);
  }
  float by = p.y + p.h - ui_.S(158);
  World const* wld = session_ ? &session_->GetWorld() : nullptr;
  std::string mapName = wld ? wld->name : "";
  if (ui_.ButtonEx("inv_copy", {x, by, w, ui_.S(50)}, "Einladung kopieren", icon::Copy, UI::BTN_PRIMARY, ok))
    CopyInvite(InviteMessage(addr, code, mapName), "Einladung");
  by += ui_.S(58);
  float half = (w - ui_.S(10)) * 0.5f;
  if (ui_.ButtonEx("inv_code", {x, by, half, ui_.S(44)}, "Nur Code", icon::Copy, UI::BTN_SECONDARY, ok)) CopyInvite(code, "Code");
  upnp::Status st = portMapper_.GetStatus();
  bool needIp = hosting_ && (st.externalIp.empty() || upnp::IsPrivateIPv4(st.externalIp)) && ipLookup_.Result().empty() &&
                st.state != upnp::Status::Working;
  if (needIp) {
    bool busy = ipLookup_.Busy();
    if (ui_.ButtonEx("inv_ip", {x + half + ui_.S(10), by, half, ui_.S(44)}, busy ? "Frage ab …" : "Öffentliche IP", icon::World,
                     UI::BTN_SECONDARY, !busy))
      ipLookup_.Start();
  } else {
    if (ui_.ButtonEx("inv_addr", {x + half + ui_.S(10), by, half, ui_.S(44)}, "Nur Adresse", icon::Copy, UI::BTN_SECONDARY, ok))
      CopyInvite(addr, "Adresse");
  }
  by += ui_.S(56);
  bool dc = discord_.Connected();
  ui_.Circle({x + ui_.S(6), by + ui_.S(10)}, ui_.S(4), dc ? Color::Hex(0x5865F2) : theme::Faint);
  ui_.Text(FONT_SMALL, x + ui_.S(20), by + ui_.S(1),
           dc ? "Discord: Freunde sehen „Beitreten“ in deinem Profil" : "Discord nicht gestartet – Einladung kopieren und teilen", theme::Dim);
  if (!ipLookup_.Error().empty() && needIp) ui_.Text(FONT_SMALL, x, p.y + p.h + ui_.S(8), ipLookup_.Error(), theme::Danger);
}

void Game::DrawJoinRequests() {
  if (joinRequests_.empty()) return;
  const JoinRequest& j = joinRequests_.front();
  float W = (float)ui_.W(), H = (float)ui_.H();
  float pw = ui_.S(380), ph = ui_.S(112);
  Rect p = {W - pw - ui_.S(24), H * 0.32f, pw, ph};
  ui_.Panel(p, ui_.S(18));
  ui_.Rect_({p.x, p.y + ui_.S(16), ui_.S(3), p.h - ui_.S(32)}, Color::Hex(0x5865F2), ui_.S(2));
  ui_.Label(p.x + ui_.S(20), p.y + ui_.S(16), "Discord", Color::Hex(0x8b95ff));
  ui_.Text(FONT_MEDIUM, p.x + ui_.S(20), p.y + ui_.S(34), j.name + " möchte mitspielen", kWhite);
  float kx = p.x + ui_.S(20), ky = p.y + ph - ui_.S(38);
  kx += ui_.KeyCap(kx, ky, "F8", ui_.S(24), theme::Ok) + ui_.S(8);
  kx += ui_.Text(FONT_SMALL, kx, ky + ui_.S(3), "Annehmen", kWhite) + ui_.S(20);
  kx += ui_.KeyCap(kx, ky, "F9", ui_.S(24)) + ui_.S(8);
  ui_.Text(FONT_SMALL, kx, ky + ui_.S(3), "Ablehnen", theme::Dim);
}

void Game::UpdatePresence() {
  if (!discord_.Connected()) return;
  DiscordActivity a;
  a.startTime = sessionStart_;
  if (!session_ || !session_->Ready()) {
    a.details = screen_ == Screen::Connecting ? "Verbindet mit einem Server …" : "Im Hauptmenü";
    a.state = "Karte: " + std::string(AllMaps()[ClampI(cfg_.match.mapIndex, 0, (int)AllMaps().size() - 1)].name);
    a.startTime = 0;
    discord_.SetActivity(a);
    return;
  }
  const ClientState& st = session_->State();
  const World& w = session_->GetWorld();
  bool online = hosting_ || !serverAddr_.empty();
  a.details = std::string(online ? (hosting_ ? "Eigener Server · " : "Online · ") : "Gegen Bots · ") + w.name;
  const char* phase = "Wartet";
  switch (st.phase) {
    case Phase::Waiting: phase = "Wartet auf Spieler"; break;
    case Phase::Hiding: phase = "Versteckphase"; break;
    case Phase::Hunting: phase = "Jagd"; break;
    case Phase::RoundEnd: phase = "Rundenende"; break;
    case Phase::MatchEnd: phase = "Spielende"; break;
  }
  int me = session_->LocalId();
  std::string role = me >= 0 ? (st.players[me].team == TEAM_HUNTERS ? " · Jäger" : (st.players[me].team == TEAM_PROPS ? " · Prop" : "")) : "";
  a.state = StrFormat("%s · Runde %d/%d", phase, std::max(1, st.round), st.maxRounds) + role;
  if (online) {
    int humans = 0;
    for (const ClientPlayer& p : st.players)
      if (p.active && !p.bot) humans++;
    a.partyId = partyId_;
    a.partySize = humans;
    a.partyMax = MAX_PLAYERS;
    std::string addr = hosting_ ? InviteAddress(nullptr, nullptr) : serverAddr_;
    net::Address na;
    if (!addr.empty() && net::Address::Parse(addr, GAME_PORT, na) && na.ip != 0) {
      a.joinSecret = EncodeInvite(na.ip, na.port);
      a.partyId = "party-" + a.joinSecret;  // Gastgeber und Gäste landen in derselben Discord-Gruppe
    }
  }
  discord_.SetActivity(a);
}

// ---------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------

void Game::DrawCrosshair(const ClientPlayer& me) {
  float cx = ui_.W() * 0.5f, cy = ui_.H() * 0.5f;
  if (me.team == TEAM_HUNTERS) {
    const WeaponInfo& wi = GetWeapon(me.weapon);
    float speed = sqrtf(me.vel.x * me.vel.x + me.vel.z * me.vel.z);
    bool aim = plat::MouseDown(plat::MOUSE_RIGHT);
    float spread = (aim ? wi.spreadAim : wi.spreadHip) * (1.0f + speed * 0.12f) * (me.onGround ? 1.0f : 2.0f);
    float gap = ui_.S(6) + spread * ui_.H() * 1.1f;
    float len = ui_.S(9), th = std::max(1.5f, ui_.S(2));
    Color c = Color(255, 255, 255, 235);
    if (me.weapon == WPN_GRENADE) {
      ui_.Ring({cx, cy}, ui_.S(11), th + 2, Color(0, 0, 0, 120));
      ui_.Ring({cx, cy}, ui_.S(11), th, c);
    } else {
      Color o(0, 0, 0, 140);
      float e = std::max(1.0f, ui_.S(1));
      ui_.Rect_({cx - th * 0.5f - e, cy - gap - len - e, th + 2 * e, len + 2 * e}, o, e);
      ui_.Rect_({cx - th * 0.5f - e, cy + gap - e, th + 2 * e, len + 2 * e}, o, e);
      ui_.Rect_({cx - gap - len - e, cy - th * 0.5f - e, len + 2 * e, th + 2 * e}, o, e);
      ui_.Rect_({cx + gap - e, cy - th * 0.5f - e, len + 2 * e, th + 2 * e}, o, e);
      ui_.Rect_({cx - th * 0.5f, cy - gap - len, th, len}, c, th * 0.5f);
      ui_.Rect_({cx - th * 0.5f, cy + gap, th, len}, c, th * 0.5f);
      ui_.Rect_({cx - gap - len, cy - th * 0.5f, len, th}, c, th * 0.5f);
      ui_.Rect_({cx + gap, cy - th * 0.5f, len, th}, c, th * 0.5f);
      ui_.Circle({cx, cy}, std::max(1.4f, ui_.S(1.6f)), c);
    }
    if (hitMarker_ > 0 || killMarker_ > 0) {
      bool kill = killMarker_ > 0;
      float a = kill ? killMarker_ * 2 : hitMarker_ * 4;
      Color hc = kill ? kRed.Alpha(Saturate(a)) : Color(255, 255, 255, (u8)(255 * Saturate(a)));
      float o = ui_.S(kill ? 9 : 7), l = ui_.S(kill ? 12 : 9);
      for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) ui_.Line({cx + sx * o, cy + sy * o}, {cx + sx * (o + l), cy + sy * (o + l)}, th * 1.3f, hc);
    }
  } else {
    bool target = view_.HighlightProp() >= 0;
    float t = ui_.Anim("xhair_target", target ? 1.0f : 0.0f, 18.0f);
    ui_.Circle({cx, cy}, ui_.S(3.5f), Color(0, 0, 0, 110));
    ui_.Circle({cx, cy}, ui_.S(2.5f), Color::Lerp(Color(255, 255, 255, 220), theme::Accent, t));
    if (t > 0.01f) {
      ui_.Ring({cx, cy}, ui_.S(10 + 4 * (1 - t)), ui_.S(2), theme::Accent.Alpha(0.9f * t));
      ui_.Arc({cx, cy}, ui_.S(16), ui_.S(2), 0.12f, theme::Accent.Alpha(t), (float)time_ * 0.5f);
      ui_.Arc({cx, cy}, ui_.S(16), ui_.S(2), 0.12f, theme::Accent.Alpha(t), (float)time_ * 0.5f + 0.5f);
    }
  }
}

void Game::DrawTauntMenu() {
  float W = (float)ui_.W();
  float rowH = 34;
  float pw = ui_.S(360), ph = ui_.S(64 + NUM_TAUNTS * rowH + 10);
  Rect p = {W - pw - ui_.S(28), ui_.H() * 0.5f - ph * 0.5f, pw, ph};
  ui_.Panel(p, ui_.S(18));
  ui_.Label(p.x + ui_.S(20), p.y + ui_.S(18), "Taunts", theme::Accent);
  float kw = ui_.KeyCapWidth("T");
  ui_.Text(FONT_SMALL, p.x + p.w - ui_.S(20) - kw - ui_.S(8), p.y + ui_.S(16), "schließen", theme::Dim, ALIGN_RIGHT);
  ui_.KeyCap(p.x + p.w - ui_.S(20) - kw, p.y + ui_.S(13), "T");
  float maxDur = 0;
  for (int k = 0; k < NUM_TAUNTS; k++) maxDur = std::max(maxDur, kTaunts[k].duration);
  for (int k = 0; k < NUM_TAUNTS; k++) {
    Rect row = {p.x + ui_.S(10), p.y + ui_.S(52) + k * ui_.S(rowH), p.w - ui_.S(20), ui_.S(rowH - 4)};
    bool hover = ui_.Hovered(row);
    float t = ui_.Anim(StrFormat("taunt%d", k).c_str(), hover ? 1.0f : 0.0f, 18.0f);
    if (t > 0.01f) ui_.Rect_(row, theme::Accent.Alpha(0.18f * t), ui_.S(8));
    if (hover && ui_.Clicked()) {
      tauntChoice_ = k;
      tauntTap_ = true;
      tauntMenu_ = false;
    }
    if (k < 9) ui_.KeyCap(row.x + ui_.S(6), row.y + ui_.S(3), StrFormat("%d", k + 1), ui_.S(24));
    else ui_.Circle({row.x + ui_.S(18), row.y + row.h * 0.5f}, ui_.S(2.5f), theme::Faint);
    ui_.Text(FONT_NORMAL, row.x + ui_.S(42), row.y + (row.h - ui_.LineHeight(FONT_NORMAL)) * 0.5f, kTaunts[k].name, kWhite);
    float bw = ui_.S(46);
    Rect bar = {row.x + row.w - ui_.S(64) - bw, row.y + row.h * 0.5f - ui_.S(2), bw, ui_.S(4)};
    ui_.Rect_(bar, Color(255, 255, 255, 30), ui_.S(2));
    ui_.Rect_({bar.x, bar.y, bar.w * kTaunts[k].duration / maxDur, bar.h}, kProp.Alpha(0.85f), ui_.S(2));
    ui_.Text(FONT_LABEL, row.x + row.w - ui_.S(8), row.y + (row.h - ui_.LineHeight(FONT_LABEL)) * 0.5f,
             StrFormat("+%d", (int)(kTaunts[k].duration * 6 + 0.5f)), kProp, ALIGN_RIGHT);
  }
}

void Game::DrawScoreboard(float alpha) {
  const ClientState& st = session_->State();
  int me = session_->LocalId();
  float W = (float)ui_.W(), H = (float)ui_.H();
  float pw = std::min(W - ui_.S(40), ui_.S(1080)), ph = ui_.S(580);
  Rect p = {W * 0.5f - pw * 0.5f, H * 0.5f - ph * 0.5f + ui_.S(20), pw, ph};
  ui_.SetAlpha(alpha);
  ui_.Panel(p, ui_.S(20));
  const World& w = session_->GetWorld();
  ui_.Label(p.x + ui_.S(28), p.y + ui_.S(22), StrFormat("Runde %d von %d", std::max(1, st.round), st.maxRounds), theme::Accent);
  ui_.Text(FONT_LARGE, p.x + ui_.S(28), p.y + ui_.S(40), w.name, kWhite);
  ui_.Text(FONT_SMALL, p.x + p.w - ui_.S(28), p.y + ui_.S(24), session_->ServerInfo(), theme::Dim, ALIGN_RIGHT);
  for (int col = 0; col < 2; col++) {
    Team t = col == 0 ? TEAM_HUNTERS : TEAM_PROPS;
    float x0 = p.x + ui_.S(24) + col * (p.w * 0.5f - ui_.S(12)), cw = p.w * 0.5f - ui_.S(36);
    float y = p.y + ui_.S(100);
    Color tc = TeamColor(t);
    Rect hb = {x0, y, cw, ui_.S(46)};
    ui_.RectGradH(hb, tc.Alpha(0.32f), tc.Alpha(0.04f), ui_.S(10));
    ui_.Rect_({hb.x, hb.y + ui_.S(8), ui_.S(3), hb.h - ui_.S(16)}, tc, ui_.S(2));
    ui_.IconIn(FONT_ICON, {hb.x + ui_.S(10), hb.y, ui_.S(30), hb.h}, t == TEAM_HUNTERS ? icon::Search : icon::Package, tc);
    ui_.Text(FONT_MEDIUM, hb.x + ui_.S(46), hb.y + (hb.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, TeamName(t), kWhite);
    ui_.Text(FONT_LABEL, hb.x + hb.w - ui_.S(14), hb.y + (hb.h - ui_.LineHeight(FONT_LABEL)) * 0.5f,
             StrFormat("%d / %d AM LEBEN", st.CountAlive(t), st.CountTeam(t)), tc, ALIGN_RIGHT);
    y += ui_.S(56);
    float cScore = x0 + cw - ui_.S(170), cK = x0 + cw - ui_.S(116), cT = x0 + cw - ui_.S(70), cP = x0 + cw - ui_.S(12);
    ui_.Label(cScore, y, "Punkte", theme::Faint, ALIGN_RIGHT);
    ui_.Label(cK, y, "K", theme::Faint, ALIGN_RIGHT);
    ui_.Label(cT, y, "T", theme::Faint, ALIGN_RIGHT);
    ui_.Label(cP, y, "Ping", theme::Faint, ALIGN_RIGHT);
    y += ui_.S(24);
    std::vector<int> ids;
    for (int i = 0; i < MAX_PLAYERS; i++)
      if (st.players[i].active && st.players[i].team == t) ids.push_back(i);
    std::sort(ids.begin(), ids.end(), [&](int a, int b) { return st.players[a].score > st.players[b].score; });
    for (int i : ids) {
      const ClientPlayer& pl = st.players[i];
      Rect row = {x0, y, cw, ui_.S(38)};
      if (i == me) {
        ui_.Rect_(row, Color(255, 255, 255, 22), ui_.S(8));
        ui_.Rect_({row.x, row.y + ui_.S(8), ui_.S(3), row.h - ui_.S(16)}, theme::Accent, ui_.S(2));
      }
      bool dead = !pl.alive && st.phase != Phase::Waiting;
      Color nc = dead ? theme::Faint : kWhite;
      Vec2 av = {row.x + ui_.S(26), row.y + row.h * 0.5f};
      ui_.Circle(av, ui_.S(13), (dead ? theme::Faint : tc).Alpha(0.28f));
      ui_.Ring(av, ui_.S(13), ui_.S(1.5f), (dead ? theme::Faint : tc).Alpha(0.8f));
      std::string nm = pl.name;
      size_t i0 = 0;
      Utf8Next(nm.data(), nm.size(), i0);
      ui_.TextIn(FONT_LABEL, {av.x - ui_.S(13), av.y - ui_.S(13), ui_.S(26), ui_.S(26)}, UI::Upper(nm.substr(0, i0)), nc);
      float nx = row.x + ui_.S(50);
      float tw = ui_.Text(FONT_NORMAL, nx, row.y + (row.h - ui_.LineHeight(FONT_NORMAL)) * 0.5f, pl.name, nc);
      if (pl.bot) {
        Rect bp = {nx + tw + ui_.S(8), row.y + ui_.S(10), ui_.S(36), ui_.S(18)};
        ui_.Rect_(bp, Color(255, 255, 255, 22), ui_.S(9));
        ui_.TextIn(FONT_LABEL, bp, "BOT", theme::Dim);
      }
      if (dead) ui_.IconIn(FONT_ICON, {cScore - ui_.S(118), row.y, ui_.S(24), row.h}, icon::Close, kRed.Alpha(0.8f));
      float ty = row.y + (row.h - ui_.LineHeight(FONT_NORMAL)) * 0.5f;
      ui_.Text(FONT_NORMAL, cScore, ty, StrFormat("%d", pl.score), nc, ALIGN_RIGHT);
      ui_.Text(FONT_NORMAL, cK, ty, StrFormat("%d", pl.kills), nc, ALIGN_RIGHT);
      ui_.Text(FONT_NORMAL, cT, ty, StrFormat("%d", pl.deaths), nc, ALIGN_RIGHT);
      ui_.Text(FONT_NORMAL, cP, ty, pl.bot ? "–" : StrFormat("%d", pl.ping), pl.bot ? theme::Faint : nc, ALIGN_RIGHT);
      y += ui_.S(40);
    }
  }
  std::vector<int> spec;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (st.players[i].active && st.players[i].team == TEAM_SPECTATOR) spec.push_back(i);
  if (!spec.empty()) {
    std::string s;
    for (size_t k = 0; k < spec.size(); k++) s += (k ? ", " : "") + std::string(st.players[spec[k]].name);
    ui_.Label(p.x + ui_.S(28), p.y + p.h - ui_.S(36), "Zuschauer", theme::Faint);
    ui_.Text(FONT_SMALL, p.x + ui_.S(120), p.y + p.h - ui_.S(38), s, theme::Dim);
  }
  ui_.SetAlpha(1);
}

void Game::DrawPause() {
  float W = (float)ui_.W(), H = (float)ui_.H();
  float a = ui_.Anim("pause_in", 1.0f, 10.0f);
  ui_.Glass({0, 0, W, H}, 0, a, Color::Hex(0x05070c, 140));
  ui_.SetAlpha(a);
  float w = 400, h = 58, gap = 12;
  bool online = hosting_ || !serverAddr_.empty();
  float cx = W / ui_.Scale() * 0.5f - (online ? 240 : 0);
  float x = cx - w * 0.5f, y = H / ui_.Scale() * 0.5f - 160;
  ui_.Label(ui_.S(cx), ui_.S(y - 92), "Pausiert", theme::Accent, ALIGN_CENTER);
  float tw = ui_.Measure(FONT_LARGE, "KEK ME IF YOU CAN").x;
  DrawLogo(ui_.S(cx) - tw * 0.5f, ui_.S(y - 70), false);
  if (online) DrawInvitePanel(R(cx + w * 0.5f + 40, y - 6, 460, 388));
  if (ui_.ButtonEx("pa_resume", R(x, y, w, h), "Weiter", icon::Play, UI::BTN_PRIMARY)) paused_ = false;
  y += h + gap;
  if (ui_.ButtonEx("pa_help", R(x, y, w, h), "Steuerung", icon::Keyboard, UI::BTN_SECONDARY)) { help_ = true; paused_ = false; }
  y += h + gap;
  if (ui_.ButtonEx("pa_set", R(x, y, w, h), "Einstellungen", icon::Settings, UI::BTN_SECONDARY)) settingsOverlay_ = true;
  y += h + gap;
  if (ui_.ButtonEx("pa_leave", R(x, y, w, h), session_ && session_->IsHost() ? "Spiel beenden" : "Server verlassen", icon::Back,
                   UI::BTN_DANGER)) {
    ui_.SetAlpha(1);
    LeaveGame();
    return;
  }
  y += h + gap;
  if (ui_.ButtonEx("pa_quit", R(x, y, w, h), "Zum Desktop", icon::Power, UI::BTN_GHOST)) plat::RequestQuit();
  if (session_) ui_.Text(FONT_SMALL, ui_.S(cx), ui_.S(y + h + 26), session_->ServerInfo(), theme::Faint, ALIGN_CENTER);
  ui_.SetAlpha(1);
}

void Game::DrawHelp() {
  float W = (float)ui_.W(), H = (float)ui_.H();
  float pw = ui_.S(1000), ph = ui_.S(610);
  Rect p = {W * 0.5f - pw * 0.5f, H * 0.5f - ph * 0.5f, pw, ph};
  ui_.Panel(p, ui_.S(20));
  ui_.Label(p.x + ui_.S(30), p.y + ui_.S(24), "Hilfe", theme::Accent);
  ui_.Text(FONT_LARGE, p.x + ui_.S(30), p.y + ui_.S(42), "Steuerung", kWhite);
  float kw = ui_.KeyCapWidth("F1");
  ui_.KeyCap(p.x + p.w - ui_.S(30) - kw, p.y + ui_.S(30), "F1");
  ui_.Text(FONT_SMALL, p.x + p.w - ui_.S(40) - kw, p.y + ui_.S(33), "schließen", theme::Dim, ALIGN_RIGHT);
  struct K { std::vector<const char*> keys; const char* what; };
  std::vector<K> common = {{{"W", "A", "S", "D"}, "Laufen"}, {{"Umschalt"}, "Sprinten (Ausdauer)"}, {{"Leertaste"}, "Springen"},
                           {{"Tab"}, "Punktetafel"}, {{"Enter"}, "Chat"}, {{"Esc"}, "Pause"}, {{"F11"}, "Vollbild"}};
  std::vector<K> hunter = {{{"Maus L"}, "Schießen"}, {{"Maus R"}, "Zielen"}, {{"R"}, "Nachladen"}, {{"1", "2", "3"}, "MP · Schrot · Granate"},
                           {{"Q"}, "Sonar – verrät nahe Props"}, {{"Strg"}, "Ducken"}};
  std::vector<K> prop = {{{"E"}, "In anvisiertes Objekt verwandeln"}, {{"R"}, "Drehung sperren / lösen"}, {{"F"}, "Zufälliger Taunt (+Punkte)"},
                         {{"T"}, "Taunt-Auswahl"}, {{"Q"}, "Köder platzieren (2×)"}, {{"Strg"}, "Ducken"}};
  const std::vector<K>* lists[3] = {&common, &hunter, &prop};
  const char* heads[3] = {"Allgemein", "Jäger", "Props"};
  const char* icons[3] = {icon::Keyboard, icon::Search, icon::Package};
  Color hc[3] = {theme::Accent, kHunter, kProp};
  float colW = (pw - ui_.S(60)) / 3.0f;
  for (int c = 0; c < 3; c++) {
    float x = p.x + ui_.S(30) + c * colW;
    float y = p.y + ui_.S(108);
    Rect hb = {x, y, colW - ui_.S(20), ui_.S(40)};
    ui_.RectGradH(hb, hc[c].Alpha(0.25f), hc[c].Alpha(0.0f), ui_.S(10));
    ui_.IconIn(FONT_ICON, {hb.x + ui_.S(8), hb.y, ui_.S(28), hb.h}, icons[c], hc[c]);
    ui_.Text(FONT_MEDIUM, hb.x + ui_.S(42), hb.y + (hb.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, heads[c], kWhite);
    y += ui_.S(56);
    for (const K& k : *lists[c]) {
      float kx = x;
      for (const char* key : k.keys) kx += ui_.KeyCap(kx, y, key, ui_.S(26)) + ui_.S(4);
      ui_.Text(FONT_SMALL, x, y + ui_.S(29), k.what, theme::Dim);
      y += ui_.S(52);
    }
  }
  ui_.Icon(p.x + ui_.S(30), p.y + p.h - ui_.S(40), icon::Info, theme::Accent);
  ui_.Text(FONT_SMALL, p.x + ui_.S(56), p.y + p.h - ui_.S(41),
           "Tipp für Props: Stell dich neben Gleichartiges und richte dich aus. Jäger: Falsche Treffer kosten Lebenspunkte!", theme::Dim);
}

void Game::DrawHUD() {
  if (!session_ || !session_->Ready()) return;
  const ClientState& st = session_->State();
  const World& w = session_->GetWorld();
  int me = session_->LocalId();
  float W = (float)ui_.W(), H = (float)ui_.H();
  const ClientPlayer* lp = me >= 0 ? &st.players[me] : nullptr;
  bool hiddenHunter = lp && lp->alive && lp->team == TEAM_HUNTERS && st.phase == Phase::Hiding;
  if (st.phase == Phase::Hiding) hideTotal_ = std::max(hideTotal_, st.phaseTimer);
  else hideTotal_ = 0;

  // Namensschilder der Teamkameraden
  if (lp && lp->team != TEAM_SPECTATOR && !hiddenHunter) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
      const ClientPlayer& p = st.players[i];
      if (i == me || !p.active || !p.alive || p.team != lp->team) continue;
      float height = p.team == TEAM_HUNTERS ? 2.1f : props::Get(std::max(0, p.propType)).bmax.y + 0.4f;
      Vec3 head = p.pos + Vec3(0, height, 0);
      float d = Distance(head, view_.Cam().pos);
      if (d > 40.0f || !w.LineOfSight(view_.Cam().pos, head)) continue;
      Vec2 s;
      if (!view_.Project(head, (int)W, (int)H, s)) continue;
      float a = Saturate(1.4f - d / 40.0f);
      Vec2 sz = ui_.Measure(FONT_SMALL, p.name);
      Rect tag = {s.x - sz.x * 0.5f - ui_.S(10), s.y - ui_.S(26), sz.x + ui_.S(20), ui_.S(22)};
      ui_.Rect_(tag, Color(8, 10, 16, (u8)(150 * a)), ui_.S(11));
      ui_.Rect_({tag.x + ui_.S(6), tag.y + tag.h * 0.5f - ui_.S(3), ui_.S(6), ui_.S(6)}, TeamColor(p.team).Alpha(a), ui_.S(3));
      ui_.TextIn(FONT_SMALL, {tag.x + ui_.S(6), tag.y, tag.w, tag.h}, p.name, kWhite.Alpha(a));
    }
  }

  if (hiddenHunter) {
    Vec2 c = {W * 0.5f, H * 0.36f};
    float frac = hideTotal_ > 0 ? Saturate(st.phaseTimer / hideTotal_) : 0;
    ui_.Circle(c, ui_.S(96), Color(6, 8, 14, 150));
    ui_.Ring(c, ui_.S(88), ui_.S(6), Color(255, 255, 255, 26));
    ui_.Arc(c, ui_.S(88), ui_.S(6), frac, kHunter);
    ui_.TextIn(FONT_NUM, {c.x - ui_.S(90), c.y - ui_.S(40), ui_.S(180), ui_.S(64)}, FmtTime(st.phaseTimer), kWhite);
    ui_.Label(c.x, c.y + ui_.S(26), "Augen zu", kHunter, ALIGN_CENTER);
    ui_.TextShadow(FONT_LARGE, W * 0.5f, c.y + ui_.S(120), "Die Props verstecken sich …", kWhite, ALIGN_CENTER);
    ui_.TextShadow(FONT_NORMAL, W * 0.5f, c.y + ui_.S(170),
                   "Achte auf Dinge, die nicht hierher passen, schief stehen oder doppelt sind. Taunts verraten Props!", theme::Dim, ALIGN_CENTER);
  }

  // Phase, Zeit und Teams oben
  {
    std::string label;
    Color lc = kWhite;
    bool endspurt = false;
    switch (st.phase) {
      case Phase::Waiting: label = "Warte auf Spieler"; lc = theme::Dim; break;
      case Phase::Hiding: label = lp && lp->team == TEAM_PROPS ? "Verstecken" : "Warten"; lc = kProp; break;
      case Phase::Hunting:
        endspurt = st.phaseTimer <= 60;
        label = endspurt ? "Endspurt" : "Jagd";
        lc = endspurt ? kRed : kHunter;
        break;
      case Phase::RoundEnd: label = "Rundenende"; lc = theme::Accent; break;
      case Phase::MatchEnd: label = "Spielende"; lc = theme::Accent; break;
    }
    float pw = ui_.S(190), ph = ui_.S(66);
    Rect p = {W * 0.5f - pw * 0.5f, ui_.S(16), pw, ph};
    if (endspurt) {
      float pulse = 0.5f + 0.5f * sinf((float)time_ * 6.0f);
      ui_.Shadow(p, ui_.S(18), ui_.S(14), kRed.Alpha(0.25f + 0.35f * pulse));
    }
    ui_.Panel(p, ui_.S(18));
    ui_.Label(W * 0.5f, p.y + ui_.S(9), label, lc, ALIGN_CENTER);
    ui_.TextIn(FONT_NUM, {p.x, p.y + ui_.S(20), p.w, ui_.S(44)}, FmtTime(st.phaseTimer), endspurt ? Color::Lerp(kWhite, kRed, 0.4f) : kWhite);
    // Teams als Punktreihen
    auto roster = [&](Team t, bool leftSide) {
      std::vector<bool> alive;
      for (int i = 0; i < MAX_PLAYERS; i++)
        if (st.players[i].active && st.players[i].team == t) alive.push_back(st.players[i].alive);
      std::sort(alive.begin(), alive.end(), std::greater<bool>());
      Color tc = TeamColor(t);
      float dot = ui_.S(10), dg = ui_.S(5);
      std::string cnt = StrFormat("%d", st.CountAlive(t));
      float cw = ui_.Measure(FONT_MEDIUM, cnt).x;
      float iw = ui_.S(28);
      float rw = ui_.S(18) + iw + cw + ui_.S(12) + alive.size() * (dot + dg) + ui_.S(8);
      Rect r = {leftSide ? p.x - ui_.S(10) - rw : p.x + p.w + ui_.S(10), p.y + ui_.S(12), rw, ui_.S(42)};
      ui_.Panel(r, ui_.S(14));
      float x = r.x + ui_.S(12);
      if (!leftSide) {
        ui_.IconIn(FONT_ICON, {x, r.y, iw, r.h}, icon::Package, tc);
        x += iw;
        ui_.Text(FONT_MEDIUM, x, r.y + (r.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, cnt, kWhite);
        x += cw + ui_.S(12);
      }
      for (size_t i = 0; i < alive.size(); i++) {
        Rect d = {x, r.y + r.h * 0.5f - dot * 0.5f, dot, dot};
        if (alive[i]) ui_.Rect_(d, tc, dot * 0.5f);
        else ui_.Border(d, tc.Alpha(0.45f), dot * 0.5f, ui_.S(1.5f));
        x += dot + dg;
      }
      if (leftSide) {
        x += ui_.S(6);
        ui_.Text(FONT_MEDIUM, x, r.y + (r.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, cnt, kWhite);
        x += cw;
        ui_.IconIn(FONT_ICON, {x, r.y, iw, r.h}, icon::Search, tc);
      }
    };
    roster(TEAM_HUNTERS, true);
    roster(TEAM_PROPS, false);
    std::string sub = StrFormat("Runde %d / %d", std::max(1, st.round), st.maxRounds);
    const Zone* z = w.ZoneAt(view_.Cam().pos);
    float sy = p.y + p.h + ui_.S(8);
    if (z) {
      std::string zn = UI::Upper(z->name);
      float zw = ui_.Measure(FONT_LABEL, zn, ui_.S(1.4f)).x + ui_.S(22);
      ui_.TextShadow(FONT_ICON, W * 0.5f - zw * 0.5f, sy - ui_.S(3), icon::Pin, kWhite);
      ui_.TextShadow(FONT_LABEL, W * 0.5f - zw * 0.5f + ui_.S(22), sy, zn, kWhite, ALIGN_LEFT, ui_.S(1.4f));
      sy += ui_.S(20);
    }
    ui_.TextShadow(FONT_LABEL, W * 0.5f, sy, UI::Upper(sub), theme::Dim, ALIGN_CENTER, ui_.S(1.4f));
  }

  // Killfeed
  {
    float y = ui_.S(18);
    for (auto it = feed_.rbegin(); it != feed_.rend(); ++it) {
      float age = (float)time_ - it->time;
      if (age > 7.0f) continue;
      float a = Saturate(7.0f - age) * Saturate(age * 6.0f);
      ui_.SetAlpha(a);
      float kwid = it->killer.empty() ? 0 : ui_.Measure(FONT_SMALL, it->killer).x + ui_.S(32);
      float vw = ui_.Measure(FONT_SMALL, it->victim).x;
      std::string extra = it->extra.empty() ? "" : "  " + it->extra;
      float ew = ui_.Measure(FONT_SMALL, extra).x;
      float rw = kwid + vw + ew + ui_.S(28);
      Rect r = {W - rw - ui_.S(24) + (1 - Saturate(age * 6.0f)) * ui_.S(30), y, rw, ui_.S(32)};
      ui_.Glass(r, ui_.S(10), 1.0f, Color::Hex(0x0b0f17, 170));
      ui_.Rect_({r.x, r.y + ui_.S(7), ui_.S(3), r.h - ui_.S(14)}, TeamColor(it->vt), ui_.S(2));
      float x = r.x + ui_.S(14), ty = r.y + (r.h - ui_.LineHeight(FONT_SMALL)) * 0.5f;
      if (!it->killer.empty()) {
        x += ui_.Text(FONT_SMALL, x, ty, it->killer, kHunter);
        ui_.IconIn(FONT_ICON, {x + ui_.S(4), r.y, ui_.S(24), r.h}, icon::Target, theme::Dim);
        x += ui_.S(32);
      }
      x += ui_.Text(FONT_SMALL, x, ty, it->victim, TeamColor(it->vt));
      if (!extra.empty()) ui_.Text(FONT_SMALL, x, ty, extra, theme::Dim);
      y += ui_.S(38);
    }
    ui_.SetAlpha(1);
  }

  // Spieler-Status unten
  if (lp && lp->alive && !hiddenHunter) {
    Rect p = {ui_.S(24), H - ui_.S(112), ui_.S(350), ui_.S(88)};
    ui_.Panel(p, ui_.S(18));
    Color tc = TeamColor(lp->team);
    float frac = lp->maxHealth > 0 ? Saturate(lp->health / lp->maxHealth) : 0;
    std::string who = lp->team == TEAM_PROPS ? "Du bist · " + props::Get(std::max(0, lp->propType)).name : "Jäger";
    ui_.Rect_({p.x + ui_.S(18), p.y + ui_.S(17), ui_.S(8), ui_.S(8)}, tc, ui_.S(4));
    ui_.Label(p.x + ui_.S(32), p.y + ui_.S(13), who, tc);
    Color hc = frac > 0.35f ? Color::Lerp(Color::Hex(0xf2c230), theme::Ok, Saturate((frac - 0.35f) * 2)) : kRed;
    float nw = ui_.Text(FONT_NUM, p.x + ui_.S(16), p.y + ui_.S(30), StrFormat("%.0f", ceilf(lp->health)), kWhite);
    ui_.Label(p.x + ui_.S(20) + nw, p.y + ui_.S(56), "HP", theme::Dim);
    float bx = p.x + ui_.S(130), bw = p.w - ui_.S(130) - ui_.S(20);
    const int segs = 10;
    float sg = ui_.S(3), sw = (bw - sg * (segs - 1)) / segs;
    for (int i = 0; i < segs; i++) {
      float f0 = (float)i / segs, fill = Saturate((frac - f0) * segs);
      Rect sr = {bx + i * (sw + sg), p.y + ui_.S(44), sw, ui_.S(12)};
      ui_.Rect_(sr, Color(255, 255, 255, 26), ui_.S(3));
      if (fill > 0) ui_.Rect_({sr.x, sr.y, sr.w * fill, sr.h}, hc, ui_.S(3));
    }
    Rect stb = {bx, p.y + ui_.S(66), bw, ui_.S(4)};
    ui_.Rect_(stb, Color(255, 255, 255, 22), ui_.S(2));
    ui_.Rect_({stb.x, stb.y, stb.w * lp->stamina, stb.h}, Color(170, 205, 255, 200), ui_.S(2));

    if (lp->team == TEAM_PROPS) {
      float ix = p.x + p.w + ui_.S(12);
      auto chip = [&](const char* key, const std::string& t, Color c, bool active) {
        float kw = ui_.KeyCapWidth(key, ui_.S(22));
        Vec2 sz = ui_.Measure(FONT_SMALL, t);
        Rect r = {ix, p.y + ui_.S(4), kw + sz.x + ui_.S(26), ui_.S(36)};
        ui_.Panel(r, ui_.S(12));
        if (active) ui_.Border(r, theme::Accent.Alpha(0.7f), ui_.S(12), ui_.S(1.2f));
        ui_.KeyCap(r.x + ui_.S(8), r.y + ui_.S(7), key, ui_.S(22));
        ui_.Text(FONT_SMALL, r.x + ui_.S(16) + kw, r.y + (r.h - ui_.LineHeight(FONT_SMALL)) * 0.5f, t, c);
        ix += r.w + ui_.S(8);
      };
      chip("R", lp->rotLock ? "Drehung gesperrt" : "Drehung frei", lp->rotLock ? theme::Accent : kWhite, lp->rotLock);
      chip("Q", StrFormat("Köder × %d", lp->decoys), lp->decoys ? kWhite : theme::Faint, false);
      chip("F", lp->tauntTimer > 0 ? "Taunt läuft …" : (lp->tauntCooldown > 0 ? StrFormat("Taunt in %.0f s", ceilf(lp->tauntCooldown)) : "Taunt"),
           lp->tauntCooldown > 0 ? theme::Faint : kWhite, false);
      chip("T", "Liste", kWhite, tauntMenu_);
      if (st.phase == Phase::Hunting) {
        float maxT = st.phaseTimer <= 60 ? 20.0f : 45.0f;
        float f = Saturate(lp->autoTaunt / maxT);
        Rect tb = {p.x + p.w + ui_.S(12), p.y + ui_.S(48), ui_.S(330), ui_.S(36)};
        ui_.Panel(tb, ui_.S(12));
        Rect bar = {tb.x + ui_.S(12), tb.y + tb.h - ui_.S(10), tb.w - ui_.S(24), ui_.S(4)};
        ui_.Rect_(bar, Color(255, 255, 255, 26), ui_.S(2));
        ui_.RectGradH({bar.x, bar.y, std::max(bar.h, bar.w * f), bar.h}, f < 0.25f ? kRed : theme::Accent2, f < 0.25f ? kRed : kProp, ui_.S(2));
        ui_.Icon(tb.x + ui_.S(12), tb.y + ui_.S(5), icon::Timer, f < 0.25f ? kRed : theme::Dim);
        ui_.Text(FONT_SMALL, tb.x + ui_.S(36), tb.y + ui_.S(4), StrFormat("Zwangs-Taunt in %.0f s", ceilf(lp->autoTaunt)), kWhite);
      }
      // Hinweis zur Verwandlung
      int hp = view_.HighlightProp();
      if (hp >= 0 && hp < (int)w.props.size()) {
        const PropDef& d = props::Get(w.props[hp].type);
        std::string t = "Verwandeln in " + d.name;
        std::string hpS = StrFormat("%.0f HP", d.health);
        float kw = ui_.KeyCapWidth("E", ui_.S(24));
        float tw = ui_.Measure(FONT_MEDIUM, t).x, hw = ui_.Measure(FONT_LABEL, hpS).x;
        float rw = kw + tw + hw + ui_.S(44);
        Rect r = {W * 0.5f - rw * 0.5f, H * 0.5f + ui_.S(34), rw, ui_.S(40)};
        ui_.Panel(r, r.h * 0.5f);
        ui_.KeyCap(r.x + ui_.S(10), r.y + ui_.S(8), "E", ui_.S(24), theme::Accent);
        ui_.Text(FONT_MEDIUM, r.x + ui_.S(20) + kw, r.y + (r.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, t, kWhite);
        ui_.Text(FONT_LABEL, r.x + r.w - ui_.S(14), r.y + (r.h - ui_.LineHeight(FONT_LABEL)) * 0.5f, hpS, theme::Dim, ALIGN_RIGHT);
      }
    } else if (lp->team == TEAM_HUNTERS) {
      const WeaponInfo& wi = GetWeapon(lp->weapon);
      Rect a = {W - ui_.S(24) - ui_.S(320), H - ui_.S(112), ui_.S(320), ui_.S(88)};
      ui_.Panel(a, ui_.S(18));
      ui_.Label(a.x + ui_.S(18), a.y + ui_.S(13), wi.name, kHunter);
      int ammo = lp->ammo[lp->weapon];
      std::string mag = lp->weapon == WPN_GRENADE ? "" : StrFormat("/ %d", wi.magazine);
      float mw = ui_.Measure(FONT_MEDIUM, mag).x;
      ui_.Text(FONT_MEDIUM, a.x + a.w - ui_.S(18), a.y + ui_.S(44), mag, theme::Dim, ALIGN_RIGHT);
      ui_.Text(FONT_NUM, a.x + a.w - ui_.S(26) - mw, a.y + ui_.S(26), StrFormat("%d", ammo), ammo == 0 ? kRed : kWhite, ALIGN_RIGHT);
      // Waffenplätze
      const char* slots[3] = {"1", "2", "3"};
      float sx = a.x + ui_.S(18);
      for (int i = 0; i < 3; i++) {
        bool active = lp->weapon == i;
        bool avail = i < 2 || lp->ammo[WPN_GRENADE] > 0;
        float kw = ui_.KeyCap(sx, a.y + ui_.S(40), slots[i], ui_.S(24), active ? theme::Accent : (avail ? theme::Dim : theme::Faint));
        if (active) ui_.Rect_({sx, a.y + ui_.S(70), kw, ui_.S(3)}, theme::Accent, ui_.S(1.5f));
        sx += kw + ui_.S(6);
      }
      ui_.Text(FONT_SMALL, sx + ui_.S(4), a.y + ui_.S(43), StrFormat("Granaten %d", lp->ammo[WPN_GRENADE]), theme::Dim);
      if (lp->reloadTimer > 0) {
        float f = 1.0f - lp->reloadTimer / std::max(0.01f, wi.reload);
        Rect rb = {a.x + ui_.S(18), a.y + a.h - ui_.S(10), a.w - ui_.S(36), ui_.S(4)};
        ui_.Rect_(rb, Color(255, 255, 255, 26), ui_.S(2));
        ui_.RectGradH({rb.x, rb.y, std::max(rb.h, rb.w * f), rb.h}, theme::Accent2, theme::Accent, ui_.S(2));
        ui_.Label(a.x + a.w - ui_.S(18), a.y + ui_.S(13), "Lädt nach", theme::Accent, ALIGN_RIGHT);
      }
      // Sonar
      bool ready = lp->abilityTimer <= 0;
      Rect sb = {a.x, a.y - ui_.S(52), ui_.S(200), ui_.S(44)};
      ui_.Panel(sb, ui_.S(14));
      Vec2 sc = {sb.x + ui_.S(24), sb.y + sb.h * 0.5f};
      ui_.Ring(sc, ui_.S(13), ui_.S(3), Color(255, 255, 255, 30));
      ui_.Arc(sc, ui_.S(13), ui_.S(3), ready ? 1.0f : 1.0f - Saturate(lp->abilityTimer / 25.0f), ready ? Color::Hex(0x7fe7ff) : theme::Dim);
      ui_.IconIn(FONT_ICON, {sc.x - ui_.S(10), sc.y - ui_.S(10), ui_.S(20), ui_.S(20)}, icon::Wifi, ready ? Color::Hex(0x7fe7ff) : theme::Faint);
      float kx = sb.x + ui_.S(48);
      kx += ui_.KeyCap(kx, sb.y + ui_.S(10), "Q", ui_.S(24)) + ui_.S(8);
      ui_.Text(FONT_SMALL, kx, sb.y + (sb.h - ui_.LineHeight(FONT_SMALL)) * 0.5f,
               ready ? "Sonar bereit" : StrFormat("Sonar %.0f s", ceilf(lp->abilityTimer)), ready ? kWhite : theme::Dim);
    }
    if (!tauntMenu_ && !paused_) DrawCrosshair(*lp);
  } else if (lp && !lp->alive && st.phase != Phase::Waiting) {
    std::string t = freeCam_ ? "Freie Kamera" : "";
    if (!freeCam_ && spectate_ >= 0) t = st.players[spectate_].name;
    Rect r = {W * 0.5f - ui_.S(280), H - ui_.S(108), ui_.S(560), ui_.S(84)};
    ui_.Panel(r, ui_.S(18));
    ui_.Label(W * 0.5f, r.y + ui_.S(12), "Zuschauen", theme::Accent, ALIGN_CENTER);
    ui_.TextIn(FONT_MEDIUM, {r.x, r.y + ui_.S(24), r.w, ui_.S(30)}, t.empty() ? "–" : t, kWhite);
    float kx = r.x + ui_.S(70);
    kx += ui_.KeyCap(kx, r.y + ui_.S(54), "Maus L / R", ui_.S(22)) + ui_.S(8);
    kx += ui_.Text(FONT_SMALL, kx, r.y + ui_.S(56), "Spieler wechseln", theme::Dim) + ui_.S(18);
    kx += ui_.KeyCap(kx, r.y + ui_.S(54), "Leertaste", ui_.S(22)) + ui_.S(8);
    ui_.Text(FONT_SMALL, kx, r.y + ui_.S(56), freeCam_ ? "Spielern folgen" : "Freie Kamera", theme::Dim);
  }

  // Popups
  {
    float y = H * 0.5f + ui_.S(86);
    for (const Popup& pp : popups_) {
      float a = Saturate(pp.time / pp.life * 3.0f) * Saturate((pp.life - pp.time) * 8.0f);
      float rise = (1.0f - pp.time / pp.life) * ui_.S(14);
      Vec2 sz = ui_.Measure(FONT_NORMAL, pp.text);
      Rect r = {W * 0.5f - sz.x * 0.5f - ui_.S(18), y - rise, sz.x + ui_.S(36), ui_.S(34)};
      ui_.SetAlpha(a);
      ui_.Glass(r, r.h * 0.5f, 1.0f, Color::Hex(0x0b0f17, 170));
      ui_.Border(r, pp.color.Alpha(0.5f), r.h * 0.5f, ui_.S(1));
      ui_.TextIn(FONT_NORMAL, r, pp.text, pp.color);
      y += ui_.S(40);
    }
    ui_.SetAlpha(1);
  }

  // Ansage in der Mitte
  if (centerTime_ > 0 && !center_.empty()) {
    float a = Saturate(centerTime_ * 2.0f) * Saturate((centerDur_ - centerTime_) * 5.0f + 0.2f);
    float grow = Saturate((centerDur_ - centerTime_) * 3.0f);
    float y = H * 0.19f;
    ui_.SetAlpha(a);
    ui_.TextShadow(FONT_HUGE, W * 0.5f, y, UI::Upper(center_), centerCol_, ALIGN_CENTER, ui_.S(2));
    float bw = ui_.S(140) * grow;
    float by = y + ui_.LineHeight(FONT_HUGE) * 0.92f;
    ui_.RectGradH({W * 0.5f - bw * 0.5f, by, bw, ui_.S(4)}, centerCol_.Alpha(0.0f), centerCol_, ui_.S(2));
    ui_.RectGradH({W * 0.5f, by, bw * 0.5f, ui_.S(4)}, centerCol_, centerCol_.Alpha(0.0f), ui_.S(2));
    if (!centerSub_.empty()) {
      Vec2 sz = ui_.Measure(FONT_NORMAL, centerSub_);
      Rect r = {W * 0.5f - sz.x * 0.5f - ui_.S(20), by + ui_.S(18), sz.x + ui_.S(40), ui_.S(38)};
      ui_.Glass(r, r.h * 0.5f, 1.0f, Color::Hex(0x0b0f17, 160));
      ui_.TextIn(FONT_NORMAL, r, centerSub_, kWhite);
    }
    ui_.SetAlpha(1);
  }

  // Chat
  {
    float y = H - ui_.S(150);
    int shown = 0;
    for (auto it = chat_.rbegin(); it != chat_.rend() && shown < 7; ++it) {
      float age = (float)time_ - it->time;
      if (age > 12.0f && !chatOpen_) continue;
      float a = chatOpen_ ? 1.0f : Saturate(12.0f - age);
      float x = ui_.S(24);
      y -= ui_.S(28);
      ui_.SetAlpha(a);
      std::string who = it->system ? "" : it->who + "  ";
      float ww = ui_.Measure(FONT_SMALL, who).x, tw = ui_.Measure(FONT_SMALL, it->text).x;
      float rw = ww + tw + ui_.S(it->system ? 44 : 24);
      ui_.RectGradH({x, y, rw + ui_.S(40), ui_.S(24)}, Color(6, 8, 14, 150), Color(6, 8, 14, 0), ui_.S(6));
      float tx = x + ui_.S(10), ty = y + (ui_.S(24) - ui_.LineHeight(FONT_SMALL)) * 0.5f;
      if (it->system) {
        ui_.IconIn(FONT_ICON, {x + ui_.S(4), y, ui_.S(22), ui_.S(24)}, icon::Info, theme::Accent);
        ui_.Text(FONT_SMALL, tx + ui_.S(22), ty, it->text, Color(220, 224, 232));
      } else {
        tx += ui_.Text(FONT_SMALL, tx, ty, who, TeamColor(it->team));
        ui_.Text(FONT_SMALL, tx, ty, it->text, kWhite);
      }
      shown++;
    }
    ui_.SetAlpha(1);
    if (chatOpen_) {
      Rect cr = {ui_.S(24), H - ui_.S(150), ui_.S(540), ui_.S(40)};
      ui_.TextField("chat", cr, &chatInput_, 100, "Nachricht – Enter sendet, Esc bricht ab", icon::Chat);
    }
  }

  bool showScores = (InGameInput() && plat::KeyDown(VK_TAB)) || st.phase == Phase::RoundEnd || st.phase == Phase::MatchEnd || forceScores_;
  float sa = ui_.Anim("scores_in", showScores ? 1.0f : 0.0f, 14.0f);
  if (sa > 0.02f) DrawScoreboard(sa);
  if (tauntMenu_) DrawTauntMenu();
  if (help_) DrawHelp();
  if (st.phase == Phase::Waiting) {
    std::string t = "Warte auf weitere Spieler …";
    Vec2 sz = ui_.Measure(FONT_MEDIUM, t);
    Rect r = {W * 0.5f - sz.x * 0.5f - ui_.S(50), H * 0.45f, sz.x + ui_.S(80), ui_.S(52)};
    ui_.Panel(r, r.h * 0.5f);
    ui_.Arc({r.x + ui_.S(28), r.y + r.h * 0.5f}, ui_.S(10), ui_.S(3), 0.3f, theme::Accent, (float)time_ * 1.2f);
    ui_.Text(FONT_MEDIUM, r.x + ui_.S(50), r.y + (r.h - ui_.LineHeight(FONT_MEDIUM)) * 0.5f, t, kWhite);
  }
}

// ---------------------------------------------------------------------------------------------
// Demo-/Screenshot-Modus (für automatische Tests)
// ---------------------------------------------------------------------------------------------

void Game::Shot(const std::string& name) {
  std::vector<u8> px;
  int w, h;
  r_.ReadBackbuffer(px, w, h);
  WritePNG(shotDir_ + "/" + name + ".png", w, h, px.data());
  LOG("Screenshot %s", name.c_str());
}

void Game::Autopilot() {
  auto frames = [&](int n, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < n; i++) {
      plat::PumpEvents();
      Frame(dt);
    }
  };
  auto fastForward = [&](float seconds) {
    int ticks = (int)(seconds * TICK_RATE);
    for (int i = 0; i < ticks; i++) {
      session_->Update(TICK_DT);
      std::vector<GameEvent> evs;
      session_->TakeEvents(evs);
    }
  };
  IntroShot(0, 2.5, "intro_studio");
  IntroShot(1, 3.0, "intro_engine");
  IntroShot(2, 6.0, "intro_spiel");
  frames(90);
  Shot("menu");
  int startMap = cfg_.match.mapIndex;
  SelectMenuMap(startMap + 1);
  frames(90);
  Shot("menu_next_map");
  SelectMenuMap(startMap);
  update::Status us;
  us.state = update::Status::Downloading;
  us.latest = "1.0.1";
  us.progress = 0.42f;
  updater_.SetPreview(us);
  frames(4);
  Shot("update_download");
  us.state = update::Status::Installed;
  us.progress = 1;
  updater_.SetPreview(us);
  frames(4);
  Shot("update_ready");
  updater_.SetPreview(update::Status());
  screen_ = Screen::Play;
  hostSetup_ = false;
  frames(45);
  Shot("setup");
  screen_ = Screen::Join;
  frames(45);
  Shot("join");
  std::string savedJoin = joinAddr_;
  joinAddr_ = EncodeInvite((203u << 24) | (0u << 16) | (113u << 8) | 7u, GAME_PORT);
  frames(4);
  Shot("join_code");
  joinAddr_ = savedJoin;
  screen_ = Screen::Settings;
  frames(45);
  Shot("settings");
  screen_ = Screen::Play;
  cfg_.match.bots = 7;
  cfg_.match.hideTime = 8;
  cfg_.match.roundTime = 70;
  cfg_.match.botSkill = 1;
  StartLocal(false);
  frames(10);
  Shot("hide_hunter");
  fastForward(30);
  frames(40);
  Shot("hunt");
  weapon_ = WPN_SHOTGUN;
  frames(50);
  Shot("shotgun");
  weapon_ = WPN_GRENADE;
  frames(50);
  Shot("grenade");
  weapon_ = WPN_SMG;
  frames(30);
  pitch_ = -0.1f;
  forceScores_ = true;
  frames(25);
  Shot("scores");
  forceScores_ = false;
  // bis zur nächsten Runde vorspulen (dann ist der lokale Spieler ein Prop)
  auto& sim = static_cast<LocalSession*>(session_.get())->GetSim();
  for (int i = 0; i < 200 && sim.round < 2; i++) fastForward(1.0f);
  fastForward(2);
  frames(30);
  Shot("prop");
  help_ = true;
  frames(2);
  Shot("help");
  help_ = false;
  tauntMenu_ = true;
  frames(2);
  Shot("taunts");
  tauntMenu_ = false;
  paused_ = true;
  frames(30);
  Shot("pause");
  // Einladungskarte als Gastgeber – nur Anzeige: kein Router-Zugriff, kein Discord
  hosting_ = true;
  localIp_ = upnp::LocalIPv4();
  frames(20);
  Shot("invite");
  paused_ = false;
  joinRequests_.push_back({"0", "Kek-Freund", time_});
  frames(20);
  Shot("discord_request");
  joinRequests_.clear();
  hosting_ = false;
}

}  // namespace

int RunGame(int argc, char** argv) {
  Game g;
  return g.Run(argc, argv);
}
