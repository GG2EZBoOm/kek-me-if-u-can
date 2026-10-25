#include "game/Session.h"

// Echte Bewegungs-/Kollisionsschritte statt direkt aufgerufener Schadensfunktionen.
int RunImpactTests() {
  int fails = 0;
  auto check = [&](bool ok, const char* label) { LOG("%s %s", ok ? "[OK]  " : "[FAIL]", label); if (!ok) fails++; };
  World arena;
  arena.AddCollider(OBox({0, -1, 0}, {30, 1, 30}), COL_SOLID, MAT_CONCRETE);
  arena.Finalize();
  Sim sim;
  auto start = [&](const char* form = "sofa", World* world = nullptr) {
    sim.Init(world ? world : &arena, MatchSettings());
    for (int i = 0; i < 3; i++) {
      sim.AddPlayer(i == 0 ? "Abrissbirne" : "Sucher", false);
      Player& p = sim.players[i]; p.alive = true; p.team = i ? TEAM_HUNTERS : TEAM_PROPS;
      p.propType = i ? -1 : props::Find(form); p.health = p.maxHealth = i ? 100.0f : 150.0f;
      p.ms.pos = i == 0 ? Vec3() : i == 1 ? Vec3(0, 0, -5) : Vec3(20, 0, 20);
      p.ms.onGround = true; p.ms.rotLock = true; p.autoTaunt = 1000;
    }
    sim.phase = Phase::Hunting; sim.phaseTimer = 300; sim.events.clear();
  };
  auto move = [&](int n, u16 buttons, int id = 0) {
    for (int i = 0; i < n; i++) { UserCmd c; c.buttons = buttons; sim.QueueCmd(id, c); sim.Tick(); }
  };
  auto events = [&](u8 type, u16 cause) {
    int n = 0; for (const auto& e : sim.events) if (e.type == type && e.c == cause) n++; return n;
  };
  const u16 charge = BTN_FORWARD | BTN_SPRINT;
  start(); move(70, charge);
  check(sim.players[1].health == 30 && events(EV_HIT, KILL_RAM) == 1 && sim.players[1].ms.vel.z < -5,
        "Schweres Sofa rammt nach echtem Sprint-Anlauf: 70 Schaden und Rückstoß");
  move(180, charge);
  check(sim.players[1].health == 30 && events(EV_HIT, KILL_RAM) == 1,
        "Dauerdrücken gegen Sucher verursacht auch nach Abklingzeit keinen weiteren Schaden");
  start(); move(100, BTN_FORWARD);
  check(sim.players[1].health == 100, "Normales Laufen mit schweren Props bleibt ungefährlich");
  start("karton"); move(100, charge);
  check(sim.players[1].health == 100, "Kleine Props können Sucher nicht rammen");
  check(props::Get(props::Find("schirm_bw")).ramDamage == 0 && props::Get(props::Find("busch")).ramDamage == 0 &&
        props::Get(props::Find("rollcontainer")).ramDamage > 0, "Große leichte Dekoration ausgeschlossen, schwere Paket-Rollcontainer zugelassen");
  start(); sim.players[0].ms.pos.z = -3.9f; move(150, charge);
  check(sim.players[1].health == 100, "Ohne 1,5 Meter freien Anlauf kein Rammen aus nächster Nähe");
  start(); sim.phase = Phase::Hiding; move(100, charge);
  check(sim.players[1].health == 100, "Versteckphase schützt die wartenden Sucher");
  start(); sim.phase = Phase::RoundEnd; move(100, charge);
  check(sim.players[1].health == 100, "Nach Rundenende keine Aufprallschäden");
  start(); sim.players[1].team = TEAM_PROPS; sim.players[1].propType = props::Find("karton"); move(100, charge);
  check(sim.players[1].health == 100, "Kein Kollisionsschaden am eigenen Team");
  start(); sim.players[0].switchTimer = 10; move(100, charge);
  check(sim.players[1].health == 100, "Aufprall-Abklingzeit gilt serverseitig");
  start(); sim.players[0].disguiseTimer = 10; move(100, charge);
  check(sim.players[1].health == 100, "Formwechsel kann keinen unmittelbaren Angriff auslösen");
  start(); sim.players[0].ms.stamina = 0; move(100, charge);
  check(sim.players[1].health == 100, "Erschöpfte Props erreichen keine gefährliche Rammgeschwindigkeit");
  start(); sim.players[0].ms.colYaw = .7f; move(100, charge);
  check(sim.players[1].health == 30, "Auch gedrehte schwere Formen verwenden echte OBB-Kontakte");
  World wall;
  wall.AddCollider(OBox({0, -1, 0}, {30, 1, 30}), COL_SOLID, MAT_CONCRETE);
  wall.AddCollider(OBox({0, 3, -3}, {5, 3, .15f}), COL_SOLID, MAT_CONCRETE); wall.Finalize();
  start("sofa", &wall); move(180, charge);
  check(sim.players[1].health == 100 && sim.players[0].ms.pos.z > -3, "Wände verhindern Rammen durch Geometrie");
  start(); sim.decoys[0].active = true; sim.decoys[0].owner = 0; sim.decoys[0].propType = props::Find("sofa");
  sim.decoys[0].pos = {0, 0, -3}; move(120, charge);
  check(sim.players[1].health == 100, "Klone erzeugen keinen Kollisionsangriff");
  start("autoscooter"); sim.settings.infection = true; sim.players[0].health = 100; move(75, charge);
  check(!sim.players[1].alive && sim.players[0].kills == 1 && sim.players[1].deaths == 1 && sim.players[0].score >= 100 &&
        sim.players[0].health == 125 && events(EV_KILL, KILL_RAM) == 1 && sim.players[1].respawnTimer < 0,
        "Autoscooter überrollt: korrekte Punkte, Heilung, Kill-Ursache und kein Sucher-Infektionsrespawn");
  start("autoscooter"); sim.players[2].alive = false; move(75, charge);
  check(sim.phase == Phase::RoundEnd && sim.winner == TEAM_PROPS, "Letzter überrollter Sucher beendet die Runde zugunsten der Props");
  for (bool crouched : {false, true}) {
    start(); sim.players[0].ms.pos = {0, 6, -5}; sim.players[0].ms.onGround = false; sim.players[1].ms.crouching = crouched;
    move(60, 0);
    check(!sim.players[1].alive && events(EV_HIT, KILL_CRUSH) == 1 && events(EV_KILL, KILL_CRUSH) == 1,
          crouched ? "Fallendes schweres Prop erschlägt auch geduckte Sucher" : "Fallendes schweres Prop erschlägt Sucher von oben");
  }
  start("karton"); sim.players[0].ms.pos = {0, 6, -5}; sim.players[0].ms.onGround = false; move(60, 0);
  check(sim.players[1].health == 100, "Kleine fallende Props bleiben harmlos");
  start(); sim.players[0].ms.pos = {0, 1.9f, -5}; sim.players[0].ms.onGround = false; move(60, 0);
  check(sim.players[1].health == 100, "Sanftes Landen oder Stehen auf einem Sucher verursacht keinen Schaden");
  start(); sim.players[0].ms.pos = {0, 6, -5}; sim.players[0].ms.onGround = false; sim.mode = MODE_MOON; move(100, 0);
  check(!sim.players[1].alive && events(EV_KILL, KILL_CRUSH) == 1, "Erschlagen funktioniert auch in der Mondschicht");
  start(); sim.players[0].ms.pos = {0, 6, -5}; sim.players[0].ms.onGround = false; sim.players[1].health = 1000;
  move(60, 0); float health = sim.players[1].health;
  move(180, 0);
  check(health < 1000 && sim.players[1].health == health && events(EV_HIT, KILL_CRUSH) == 1,
        "Aufprall federt ab, löst aber keine Schadensschleife beim Landen aus");
  for(const char* form : {"auto_grau","auto_blau","polizeiauto","gabelstapler"}) {
    const int type=props::Find(form);
    World showroom;
    showroom.AddCollider(OBox({0,-1,0},{30,1,30}),COL_SOLID,MAT_CONCRETE);
    showroom.AddProp(type,{0,0,-4.5f},0);showroom.Finalize();
    start("paket_m",&showroom);
    sim.players[1].ms.pos={15,0,15};
    UserCmd use;use.buttons=BTN_USE;
    sim.QueueCmd(0,use);sim.Tick();
    std::vector<OBox> others;
    bool changed=sim.players[0].propType==type;
    check(changed && !BoxBlocked(sim.MakeEnv(0,others),PlayerBox(sim.players[0].ms,type),.02f),
          StrFormat("%s: E verwandelt am echten Weltfahrzeug ohne Überlappung",form).c_str());
    showroom.AddCollider(OBox({-1.6f,1.6f,.25f},{1.2f,1.6f,1.75f}),COL_SOLID,MAT_CONCRETE);
    showroom.AddCollider(OBox({1.6f,1.6f,.25f},{1.2f,1.6f,1.75f}),COL_SOLID,MAT_CONCRETE);
    showroom.Finalize();start("paket_m",&showroom);sim.players[1].ms.pos={15,0,15};
    sim.QueueCmd(0,use);sim.Tick();
    bool nofit=false;for(const auto& e:sim.events) nofit|=e.type==EV_NOFIT;
    check(nofit && sim.players[0].propType==props::Find("paket_m"),
          StrFormat("%s: zu schmale Stelle verhindert Verwandlung",form).c_str());
    start(form);sim.players[0].ms.colYaw=props::Get(type).lookYawOffset;
    sim.players[1].ms.pos={0,0,-9};move(140,charge);
    check(!sim.players[1].alive && events(EV_KILL,KILL_RAM)==1,
          StrFormat("%s: Sprint-Anlauf überrollt Sucher genau einmal",form).c_str());
    MoveEnv env;env.world=&arena;MoveState steer;steer.onGround=true;steer.yaw=0;steer.colYaw=0;steer.rotLock=false;
    for(int i=0;i<120;i++) UpdatePropRotation(env,steer,type);
    Vec3 front=RotateY(props::Get(type).id=="gabelstapler" ? Vec3(0,0,-1) : Vec3(1,0,0),steer.colYaw);
    check(Dot(front,Vec3(0,0,-1))>.99f,StrFormat("%s: entsperrte Modellfront folgt der Blickrichtung",form).c_str());
    env.world=&wall;steer.pos={0,0,-1};steer.colYaw=0;steer.yaw=0;
    // Start beside the wall in a fitting orientation, then turn toward it.
    if(form==std::string("gabelstapler")) {steer.pos={0,0,-.8f};steer.colYaw=PI*.5f;steer.yaw=0;}
    bool fits=!BoxBlocked(env,PlayerBox(steer,type),.02f);
    for(int i=0;i<180;i++) UpdatePropRotation(env,steer,type);
    check(fits && !BoxBlocked(env,PlayerBox(steer,type),.02f),StrFormat("%s: Drehen respektiert Wandkollision",form).c_str());
  }
  LOG("Aufpralltests beendet: %d Fehler", fails);
  return fails;
}
