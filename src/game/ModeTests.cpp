#include "game/Session.h"

// Kleine kontrollierte Arena: Regeln unabhängig von Bot-Zufall und Kartengeometrie prüfen.
int RunModeTests() {
  int fails = 0;
  auto check = [&](bool ok, const char* label) { LOG("%s %s", ok ? "[OK]  " : "[FAIL]", label); if (!ok) fails++; };
  World w;
  w.AddCollider(OBox({0, -1, 0}, {60, 1, 60}), COL_SOLID, MAT_CONCRETE);
  w.hunterSpawns.push_back({{15, .02f, 0}, 0});
  w.propSpawns.push_back({{0, .02f, 0}, 0});
  w.AddProp(props::Find("paket_s"), {30, 0, 0}, 0);
  w.AddProp(props::Find("paket_m"), {33, 0, 0}, 0);
  w.Finalize();
  Sim sim;
  int prop = -1, hunter = -1;
  auto start = [&](GameMode mode) {
    MatchSettings ms; ms.mode = mode; ms.rounds = 30; ms.hideTime = 30; ms.roundTime = 300;
    sim.Init(&w, ms);
    sim.AddPlayer("A", false); sim.AddPlayer("B", false);
    sim.phaseTimer = 0; sim.Tick();
    for (int i = 0; i < 2; i++) {
      if (sim.players[i].team == TEAM_PROPS) prop = i; else hunter = i;
      sim.players[i].prevButtons = 0;
      sim.players[i].autoTaunt = 1000;
    }
    sim.phase = Phase::Hunting; sim.phaseTimer = 300;
    sim.events.clear();
  };
  auto count = [&](u8 type) { int n = 0; for (auto& e : sim.events) if (e.type == type) n++; return n; };
  auto press = [&](int id, u16 button, float yaw = 0) {
    sim.players[id].prevButtons = 0;
    UserCmd cmd; cmd.buttons = button; cmd.yaw = yaw;
    sim.QueueCmd(id, cmd); sim.Tick();
  };

  start(MODE_ROULETTE);
  auto& rp = sim.players[prop];
  int oldType = rp.propType; Vec3 oldPos = rp.ms.pos;
  rp.health = rp.maxHealth * .4f;
  sim.modeTimer = 5 + TICK_DT * .5f; sim.Tick();
  check(count(EV_MODE_WARNING) == 1, "Roulette warnt einmal fünf Sekunden vor dem Wechsel");
  sim.events.clear(); sim.modeTimer = TICK_DT * .5f; sim.Tick();
  std::vector<OBox> others;
  check(rp.propType != oldType && count(EV_MODE_PULSE) == 1 && Distance(oldPos, rp.ms.pos) < .001f &&
        !BoxBlocked(sim.MakeEnv(prop, others), PlayerBox(rp.ms, rp.propType), .02f), "Roulette verwandelt passend ohne Teleport durch Wände");
  check(fabsf(rp.health / rp.maxHealth - .4f) < .001f, "Roulette erhält den Lebensanteil");
  sim.events.clear(); rp.disguiseTimer = 0; press(prop, BTN_USE);
  check(count(EV_NOFIT) == 1 && count(EV_DISGUISE) == 0, "Manueller Formwechsel während Roulette-Jagd gesperrt");
  sim.phase = Phase::RoundEnd; float timer = sim.modeTimer; sim.Tick();
  check(sim.modeTimer == timer, "Spezialregeln pausieren nach Rundenende");

  start(MODE_HOT);
  auto& hp = sim.players[prop];
  hp.modeMeter = HOT_LIMIT - TICK_DT * .5f; hp.aliveAcc = 0; int score = hp.score;
  sim.Tick();
  check(count(EV_TAUNT) == 1 && count(EV_PING) == 1 && hp.score == score, "Stillstehen löst Hupe und Ping ohne Bonuspunkte aus");
  hp.ms.pos.x += 1.1f; sim.Tick();
  check(hp.modeMeter == 0 && sim.players[hunter].modeMeter == 0, "Ein Meter Ortswechsel setzt Alarm zurück; Sucher bleiben verschont");

  start(MODE_CLONES);
  auto& cp = sim.players[prop];
  for (float yaw : {0.0f, 2.1f, 4.2f}) press(prop, BTN_DECOY, yaw);
  int active = 0; for (auto& d : sim.decoys) if (d.active) active++;
  check(active == 3 && cp.decoys == 0, "Drei Klone mit Q frei platzierbar");
  cp.modeMeter = CLONE_RECHARGE - TICK_DT * .5f; sim.Tick();
  check(cp.decoys == 1, "Klon-Nachschub nach zwölf Sekunden");
  press(prop, BTN_DECOY, 1.0f);
  active = 0; for (auto& d : sim.decoys) if (d.active) active++;
  check(active == 3 && cp.decoys == 1, "Vierter aktiver Klon abgewiesen, Ladung erhalten");
  sim.events.clear(); press(prop, BTN_TAUNT);
  check(count(EV_TAUNT) == 4, "Alle drei Klone ahmen den Taunt ihres Besitzers nach");
  cp.abilityTimer = 0; cp.health = cp.maxHealth * .35f;
  oldPos = cp.ms.pos; sim.events.clear(); press(prop, BTN_ABILITY);
  check(count(EV_CLONE_SWAP) == 1 && Distance(cp.ms.pos, oldPos) > 1 && cp.abilityTimer > 9 &&
        fabsf(cp.health / cp.maxHealth - .35f) < .001f, "G tauscht Positionen ohne Heilung und startet Abklingzeit");
  oldPos = cp.ms.pos; press(prop, BTN_ABILITY);
  check(Distance(cp.ms.pos, oldPos) < .05f, "Klon-Tausch kann nicht während der Abklingzeit wiederholt werden");
  sim.players[hunter].abilityTimer = 0; press(hunter, BTN_ABILITY);
  bool sonar = false; for (auto& d : sim.decoys) if (d.active && d.pingTimer > 0) sonar = true;
  check(sonar, "Klon reagiert auf Sucher-Sonar");
  for (auto& d : sim.decoys) if (d.active) d.life = TICK_DT * .5f;
  sim.Tick(); active = 0; for (auto& d : sim.decoys) if (d.active) active++;
  check(active == 0, "Abgelaufene Klone geben Plätze wieder frei");
  cp.abilityTimer = 0; sim.events.clear(); press(prop, BTN_ABILITY);
  check(count(EV_CLONE_SWAP) == 0 && cp.abilityTimer == 0, "Tausch ohne eigenen Klon verbraucht keine Fähigkeit");

  float peaks[2] = {};
  for (int m = 0; m < 2; m++) {
    MoveEnv env; env.world = &w; env.mode = m ? MODE_MOON : MODE_CLASSIC;
    MoveState state; state.pos = {0, .01f, 0}; state.onGround = true;
    for (int frame = 0; frame < 180; frame++) {
      UserCmd cmd; cmd.buttons = frame == 0 ? BTN_JUMP : 0;
      ApplyMovement(env, state, cmd, -1, false);
      peaks[m] = std::max(peaks[m], state.pos.y);
    }
  }
  check(peaks[0] > .5f && peaks[1] > peaks[0] * 2, "Mondschicht erzeugt deutlich höhere Sprünge");
  float fall[2] = {};
  for (int m = 0; m < 2; m++) {
    start(m ? MODE_MOON : MODE_CLASSIC);
    auto& g = sim.grenades[0]; g.active = true; g.pos = {0, 20, 0}; g.fuse = 5;
    for (int i = 0; i < 30; i++) sim.Tick();
    fall[m] = 20 - g.pos.y;
  }
  check(fall[1] < fall[0] * .45f && fall[1] > fall[0] * .35f, "Auch Granaten folgen der Mondschwerkraft");

  start(MODE_SHUFFLE);
  bool shuffled = true;
  for (int i = 0; i < 20; i++) {
    GameMode previous = sim.mode;
    sim.phase = Phase::RoundEnd; sim.phaseTimer = 0; sim.Tick();
    shuffled &= sim.mode >= MODE_ROULETTE && sim.mode <= MODE_MOON && sim.mode != previous;
  }
  check(shuffled, "Wundertüte spielt alle Runden ohne direkte Moduswiederholung");
  GameMode current = sim.mode; sim.settings.mode = MODE_CLASSIC; sim.Tick();
  check(sim.mode == current, "Geplante Regeländerung verändert die laufende Runde nicht");
  sim.phase = Phase::RoundEnd; sim.phaseTimer = 0; sim.Tick();
  check(sim.mode == MODE_CLASSIC && sim.modeTimer == 0, "Nächste Runde übernimmt Klassisch ohne Spezialtimer");
  return fails;
}
