#include "game/Duty.h"
#include "game/Sim.h"
#include "game/Sounds.h"

const Duty* MapDuty(const std::string& map) {
  static const Duty jobs[] = {
    {"zwinger","zwinger_cello","Als Violoncello zwischen die Instrumente auf der Hofbühne.",
     "Das Hofkonzert beginnt!",{10.8f,.72f,-10.4f},{11,2,-7.6f},2.3f,SND_DUTY_STRINGS},
    {"paketzentrum","paket_s","Als KEK-Päckchen auf den Auslauf von Sortierband B.",
     "Band B: Sendung zugestellt!",{6,.75f,9.5f},{6,.55f,12.65f},.84f,SND_DUTY_DISPATCH},
    {"bundestag","mikrofon","Als Tischmikrofon ans Rednerpult im Plenarsaal.",
     "Das Mikrofon hat um das Wort gebeten.",{5.35f,-1.62f,0},{6.4f,.2f,0},1.65f,SND_DUTY_SPEECH},
    {"oktoberfest","tuba","Als Tuba zur Blaskapelle auf dem Musikpodium.",
     "Die Blaskapelle spielt auf!",{2.6f,1,.2f},{0,2.0f,.9f},1.8f,SND_DUTY_BRASS},
    {"nudelfabrik","nudelkarton","Als Nudelkarton in den Versand. Zum Eingang des Versandlagers.",
     "Der Versand meldet: Nudeln auf dem Weg!",{16,0,41.5f},{16,1.0f,39.5f},2.2f,SND_DUTY_DISPATCH},
  };
  for (const Duty& job : jobs) if (map == job.map) return &job;
  return nullptr;
}

void Sim::UpdateDuty() {
  if (modeTimer > 0) {
    modeTimer = std::min(8.f,modeTimer+TICK_DT);
    return;
  }
  if (!duty) return;
  for (Player& p : players) {
    if (!p.active || !p.alive || p.team != TEAM_PROPS) continue;
    bool working = p.propType == dutyType && p.ms.onGround &&
                   fabsf(p.ms.pos.y-duty->spot.y)<.24f && DistanceXZ(p.ms.pos,duty->spot)<=duty->radius;
    p.modeMeter = working ? p.modeMeter+TICK_DT : 0;
    if (p.modeMeter+.0001f < DUTY_HOLD) continue;
    modeTimer = TICK_DT;
    Emit(EV_DUTY).pos = duty->speaker;
    float before = phaseTimer;
    phaseTimer = std::max(0.f,phaseTimer-DUTY_BONUS);
    if (before>60 && phaseTimer<=60) Emit(EV_COUNTDOWN).b=60;
    if (phaseTimer==0) EndRound(TEAM_PROPS);
    return;
  }
}
