#include "game/Session.h"
#include "game/WeaponModels.h"
#include "game/Settings.h"

int RunWeaponTests() {
  int fails=0;
  auto check=[&](bool ok,const char* label){LOG("%s %s",ok?"[OK]  ":"[FAIL]",label); if(!ok) fails++;};
  bool valid=true;
  for(int w=0;w<FIREARM_COUNT;w++) for(unsigned raw=0;raw<65536;raw++) {
    u16 b=ValidateWeaponBuild(w,(u16)raw); auto s=GetWeaponStats(w,b);
    valid &= b==ValidateWeaponBuild(w,b) && b<4096 && s.weight>.4f && s.weight<7 && s.adsTime>=.14f && s.adsTime<=.55f &&
             s.magazine>0 && s.magazine<=50 && s.recoil>.4f && s.recoil<1.5f && s.spreadAim>0 && s.reload<8;
    for(int slot=0;slot<WS_COUNT;slot++) valid &= WeaponPart(b,slot)<WeaponPartCount(slot) && !WeaponPartBlock(w,b,slot,WeaponPart(b,slot));
  }
  check(valid,"Alle 196608 Waffen-Bitmuster werden serverseitig auf gültige, begrenzte Builds reduziert");
  u16 shortMP=WithWeaponPart(0,0,WS_BARREL,1), shortSG=WithWeaponPart(1,0,WS_BARREL,1);
  check(WithWeaponPart(0,shortMP,WS_GRIP,1)==shortMP && WithWeaponPart(1,shortSG,WS_MAGAZINE,2)==shortSG,
        "Unpassender Vertikalgriff und überlange Magazinröhre werden abgelehnt");
  auto compact=GetWeaponStats(0,WeaponPreset(0,1)), stable=GetWeaponStats(0,WeaponPreset(0,2)), quiet=GetWeaponStats(0,WeaponPreset(0,3));
  check(compact.adsTime<stable.adsTime && compact.weight<stable.weight && compact.recoil>stable.recoil && compact.magazine==20 && stable.magazine==50,
        "Kompakt- und Kontroll-Build haben echte, gegensätzliche Handhabungswerte");
  check(quiet.suppressed && quiet.soundRadius<GetWeaponStats(0,0).soundRadius && quiet.weight>GetWeaponStats(0,0).weight,
        "Schalldämpfer reduziert Hörweite mit zusätzlichem Gewicht");
  bool pose=true;
  for(int w=0;w<FIREARM_COUNT;w++) for(int preset=0;preset<4;preset++) for(int i=0;i<=120;i++) {
    u16 b=WeaponPreset(w,preset); float duration=GetWeaponStats(w,b).reload;
    auto p=MakeWeaponPose(w,b,duration*i/120,duration,.85f*i/120);
    pose &= std::isfinite(p.leftHand.x+p.leftHand.y+p.leftHand.z) && p.reload>=0 && p.reload<=1 && p.pump>=0 && p.pump<.09f;
  }
  check(pose,"Hand-, Magazin- und Pumpbewegungen bleiben während aller Nachladephasen endlich und begrenzt");
  World arena; arena.AddCollider(OBox({0,-1,0},{100,1,100}),COL_SOLID,MAT_CONCRETE); arena.Finalize();
  Sim sim; UserCmd cmd;
  auto start=[&](int weapon=0,u16 build=0){
    sim.Init(&arena,MatchSettings()); sim.AddPlayer("Werkbank",false); sim.AddPlayer("Prop",false);
    for(int i=0;i<2;i++) { auto& p=sim.players[i]; p.alive=true; p.team=i?TEAM_PROPS:TEAM_HUNTERS;
      p.ms.pos=i?Vec3(80,0,80):Vec3(); p.ms.onGround=true; p.propType=i?props::Find("karton"):-1; p.health=p.maxHealth=100; p.autoTaunt=1000;
    }
    auto& p=sim.players[0]; p.weapon=(u8)weapon; p.builds[weapon]=p.nextBuilds[weapon]=build;
    for(int w=0;w<WPN_COUNT;w++) p.ammo[w]=(u8)GetWeaponStats(w,w<FIREARM_COUNT?p.builds[w]:0).magazine;
    sim.phase=Phase::Hunting; sim.phaseTimer=300; cmd=UserCmd(); cmd.weapon=(u8)weapon; cmd.builds[weapon]=build;
  };
  auto ticks=[&](int n,u16 buttons){cmd.buttons=buttons; for(int i=0;i<n;i++){cmd.seq++;sim.QueueCmd(0,cmd);sim.Tick();}};
  start(); sim.phase=Phase::Hiding; cmd.builds[0]=WeaponPreset(0,2); ticks(1,0);
  check(sim.players[0].builds[0]==cmd.builds[0] && sim.players[0].ammo[0]==50,"Werkbank vor der Jagd setzt Build und korrekte Magazinkapazität");
  sim.phase=Phase::Hunting; sim.players[0].ammo[0]=7; cmd.builds[0]=WeaponPreset(0,1); ticks(2,0);
  check(sim.players[0].builds[0]==WeaponPreset(0,2) && sim.players[0].ammo[0]==7 && sim.players[0].nextBuilds[0]==cmd.builds[0],
        "Umbau während der Jagd wird vorgemerkt; weder Werte noch Munition lassen sich austauschen");
  sim.phase=Phase::RoundEnd; sim.phaseTimer=0; ticks(2,0);
  check(sim.players[0].builds[0]==WeaponPreset(0,1) && sim.players[0].ammo[0]==20,"Vorgemerkter Build wird mit der nächsten Runde angelegt");
  start(0,WeaponPreset(0,2)); sim.players[0].ammo[0]=3; ticks(1,BTN_RELOAD);
  check(sim.players[0].reloadDuration>2.4f && sim.players[0].ammo[0]==3,"Trommelmagazin verlängert Nachladen ohne sofortiges Auffüllen");
  ticks(155,0); check(sim.players[0].ammo[0]==50 && sim.players[0].reloadTimer==0,"Abgeschlossenes Nachladen füllt die konfigurierte Trommel");
  start(); sim.players[0].ammo[0]=2; ticks(1,BTN_RELOAD); ticks(30,0); cmd.weapon=WPN_SHOTGUN; ticks(1,0); ticks(150,0);
  check(sim.players[0].ammo[0]==2 && sim.players[0].reloadTimer==0,"Waffenwechsel bricht MP-Nachladen ohne kostenlose Munition ab");
  start(1); sim.players[0].ammo[1]=0; ticks(1,BTN_RELOAD); ticks(35,0);
  check(sim.players[0].ammo[1]==0,"Schrotpatrone erscheint erst nach dem Einführen");
  ticks(25,0); check(sim.players[0].ammo[1]==1 && sim.players[0].reloadTimer>0,"Schrotflinte lädt einzelne Patronen statt des gesamten Rohres");
  ticks(1,BTN_FIRE); check(sim.players[0].reloadTimer==0 && sim.players[0].ammo[1]==1,"Feuer unterbricht Schrot-Nachladen mit geladener Patrone");
  ticks(15,BTN_FIRE); check(sim.players[0].ammo[1]==0,"Nach kurzer Rückkehr zum Griff lässt sich die einzelne Patrone abfeuern");
  start(1,WithWeaponPart(1,0,WS_MAGAZINE,2)); sim.players[0].ammo[1]=0; ticks(1,BTN_RELOAD); ticks(320,0);
  check(sim.players[0].ammo[1]==8 && sim.players[0].reloadTimer==0,"Verlängerte Schrot-Röhre wird mit exakt acht Patronen geladen");
  start(); ticks(1,BTN_AIM); check(sim.players[0].aimProgress<.2f,"Zielen erreicht nicht sofort maximale Präzision");
  ticks(30,BTN_AIM); check(sim.players[0].aimProgress==1,"Anschlag wird mit der konfigurierten Dauer aufgebaut");
  ticks(13,BTN_AIM|BTN_FIRE);
  float kick=sim.players[0].recoilPitch; bool upward=false;
  for(const auto& e:sim.events) if(e.type==EV_SHOT && e.dir.y>.005f) upward=true;
  check(kick>.01f && upward,"Rückstoß beeinflusst autoritative Schussrichtung und Kamerazustand");
  ticks(60,0); check(sim.players[0].recoilPitch<kick*.01f,"Waffe kehrt nach der Feuerpause kontrolliert zurück");
  start(); ticks(30,BTN_FORWARD|BTN_SPRINT); int ammo=sim.players[0].ammo[0]; ticks(1,BTN_FIRE|BTN_AIM);
  check(sim.players[0].ammo[0]==ammo && sim.players[0].sprintRecover>0,"Sprint verlangt vor dem Schuss das Anheben der Waffe");
  ticks(20,BTN_FIRE|BTN_AIM); check(sim.players[0].ammo[0]<ammo,"Nach der Sprint-Erholung ist Feuern wieder möglich");
  u16 pistolBuild=WeaponPreset(WPN_PISTOL,2);
  check(GetWeaponStats(WPN_PISTOL,0).magazine==15 && GetWeaponStats(WPN_PISTOL,pistolBuild).magazine==20 &&
        GetWeaponStats(WPN_PISTOL,WeaponPreset(WPN_PISTOL,1)).magazine==10 &&
        WeaponPart(pistolBuild,WS_OPTIC)==1 && WeaponPart(pistolBuild,WS_STOCK)==0 &&
        WithWeaponPart(WPN_PISTOL,0,WS_OPTIC,3)==0 && WithWeaponPart(WPN_PISTOL,0,WS_STOCK,2)==0,
        "Pistole bietet 10/15/20 Schuss und lehnt große Optiken sowie Schulterstützen ab");
  start(WPN_PISTOL); ticks(90,BTN_FIRE);
  check(sim.players[0].ammo[WPN_PISTOL]==14,"Gehaltene Feuertaste löst bei der Pistole genau einen Schuss aus");
  ticks(1,0); ticks(1,BTN_FIRE);
  check(sim.players[0].ammo[WPN_PISTOL]==13,"Erneuter Klick gibt den nächsten Pistolenschuss ab");
  ticks(1,0); ticks(1,BTN_FIRE);
  check(sim.players[0].ammo[WPN_PISTOL]==13,"Schnelle Klicks umgehen die Feuerpause der Pistole nicht");
  ticks(30,0); ticks(1,BTN_RELOAD); ticks(30,BTN_FIRE);
  check(sim.players[0].ammo[WPN_PISTOL]==13 && sim.players[0].reloadTimer>0,"Pistole kann während des Magazinwechsels nicht schießen");
  ticks(80,BTN_FIRE);
  check(sim.players[0].ammo[WPN_PISTOL]==15,"Nachladen füllt das Pistolenmagazin; gehaltenes Feuer erzeugt keinen Folgeschuss");
  for(int preset=1;preset<=2;preset++) {
    start(WPN_PISTOL,WeaponPreset(WPN_PISTOL,preset)); sim.players[0].ammo[WPN_PISTOL]=0;
    ticks(1,BTN_RELOAD); ticks(120,0);
    check(sim.players[0].ammo[WPN_PISTOL]==(preset==1?10:20),"Pistolen-Nachladen respektiert das gewählte Magazin");
  }
  start(WPN_PISTOL); sim.players[0].ammo[WPN_PISTOL]=3; ticks(1,BTN_RELOAD); ticks(30,0);
  cmd.weapon=WPN_GRENADE; ticks(120,0);
  check(sim.players[0].weapon==WPN_GRENADE && sim.players[0].ammo[WPN_PISTOL]==3,"Slot 4 wählt die Granate und bricht Pistolen-Nachladen ohne Munition ab");
  auto locked=MakeWeaponPose(WPN_PISTOL,0,0,0,0,true), ready=MakeWeaponPose(WPN_PISTOL,0,0,0,0,false);
  auto closing=MakeWeaponPose(WPN_PISTOL,0,.05f,1.45f,0,true);
  check(locked.bolt>.03f && ready.bolt==0 && closing.bolt<.001f,"Leere Pistole hält den Schlitten offen und schließt ihn nach dem Magazinwechsel");
  start(WPN_PISTOL); sim.players[1].ms.pos={0,EyePos(sim.players[0].ms,-1).y-props::Get(sim.players[1].propType).colHalf.y,-5}; sim.players[1].health=20;
  ticks(1,BTN_FIRE); bool pistolKill=false;
  for(const auto& e:sim.events) if(e.type==EV_KILL && e.a==0 && e.b==1 && e.c==KILL_PISTOL) pistolKill=true;
  check(pistolKill,"Pistolentreffer verursachen Schaden und melden die richtige Abschussursache");
  start(WPN_PISTOL,pistolBuild); ticks(1,BTN_FIRE);
  ClientState client; FillClientState(sim,client,1);
  check(client.players[0].builds[WPN_PISTOL]==pistolBuild && client.players[0].ammo[WPN_PISTOL]==19 &&
        client.players[0].recoilPitch==sim.players[0].recoilPitch,
        "Lokaler Client erhält dieselben Module und Rückstoßwerte wie die Simulation");
  return fails;
}
