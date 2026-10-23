#pragma once
// Gemeinsame Spieltypen für Simulation, Netzwerk, Bots und Darstellung.
#include "engine/Common.h"
#include "engine/Math.h"

constexpr int MAX_PLAYERS = 16;
constexpr int TICK_RATE = 60;
constexpr float TICK_DT = 1.0f / TICK_RATE;
constexpr int MAX_DECOYS = 24;
constexpr int MAX_GRENADES = 8;
constexpr int NAME_LEN = 24;
constexpr u16 GAME_PORT = 27015;
constexpr u32 PROTOCOL_VERSION = 3;

enum Team : u8 { TEAM_SPECTATOR = 0, TEAM_HUNTERS = 1, TEAM_PROPS = 2 };

enum class Phase : u8 { Waiting = 0, Hiding, Hunting, RoundEnd, MatchEnd };

enum Buttons : u16 {
  BTN_FORWARD = 1 << 0,
  BTN_BACK = 1 << 1,
  BTN_LEFT = 1 << 2,
  BTN_RIGHT = 1 << 3,
  BTN_JUMP = 1 << 4,
  BTN_SPRINT = 1 << 5,
  BTN_CROUCH = 1 << 6,
  BTN_FIRE = 1 << 7,
  BTN_AIM = 1 << 8,
  BTN_RELOAD = 1 << 9,
  BTN_USE = 1 << 10,
  BTN_TAUNT = 1 << 11,
  BTN_ABILITY = 1 << 12,
  BTN_LOCK = 1 << 13,
  BTN_DECOY = 1 << 14,
  BTN_ALT = 1 << 15,  // Zuschauer: nächstes Ziel / Freiflug
};

enum Weapon : u8 { WPN_SMG = 0, WPN_SHOTGUN = 1, WPN_GRENADE = 2, WPN_COUNT };

struct UserCmd {
  u32 seq = 0;
  float yaw = 0, pitch = 0;
  u16 buttons = 0;
  u8 weapon = 0;  // gewünschte Waffe
  u8 taunt = 0;   // 0 = zufällig, sonst Taunt-Nummer + 1
};

// Bewegungszustand (wird vom Client vorhergesagt)
struct MoveState {
  Vec3 pos;
  Vec3 vel;
  float yaw = 0, pitch = 0;
  float colYaw = 0;  // Drehung des Kollisionsquaders (Props)
  float stamina = 1;
  bool onGround = false;
  bool crouching = false;
  bool rotLock = false;
  u16 lastButtons = 0;
};

enum EventType : u8 {
  EV_NONE = 0,
  EV_SHOT,          // a=Schütze, b=Waffe, pos=Mündung, dir=Richtung
  EV_IMPACT,        // pos, dir=Normale, b=Material, c=1 wenn Prop
  EV_HIT,           // a=Angreifer, b=Opfer, f=Schaden, pos
  EV_PENALTY,       // a=Jäger, f=Schaden, pos=getroffenes Objekt
  EV_KILL,          // a=Täter (255 = Welt), b=Opfer, c=Ursache
  EV_TAUNT,         // a=Spieler, b=Taunt, pos
  EV_DISGUISE,      // a=Spieler, c=Prop-Typ, pos
  EV_DECOY,         // a=Besitzer, pos
  EV_DECOY_BREAK,   // pos, c=Prop-Typ
  EV_SONAR,         // a=Jäger, pos
  EV_PING,          // a=Prop, pos
  EV_PHASE,         // b=Phase
  EV_ROUND_END,     // b=Siegerteam
  EV_JUMP,          // a=Spieler, pos
  EV_RELOAD,        // a=Spieler, b=Waffe
  EV_EMPTY,         // a=Spieler
  EV_GRENADE,       // a=Spieler, pos
  EV_EXPLOSION,     // pos
  EV_CHAT,          // a=Spieler, text
  EV_JOIN,          // a=Spieler
  EV_LEAVE,         // a=Spieler
  EV_COUNTDOWN,     // b=Sekunden
  EV_LOCK,          // a=Spieler, b=an/aus
  EV_NOFIT,         // a=Spieler (Verwandlung passt nicht)
  EV_TEAMSWITCH,    // a=Spieler, b=Team
};

enum KillCause : u8 { KILL_SMG = 0, KILL_SHOTGUN, KILL_GRENADE, KILL_PENALTY, KILL_FALL, KILL_INFECT };

struct GameEvent {
  u32 id = 0;
  u8 type = EV_NONE;
  u8 a = 0, b = 0;
  u16 c = 0;
  Vec3 pos, dir;
  float f = 0;
  std::string text;
};

struct MatchSettings {
  int mapIndex = 0;
  int rounds = 6;
  float hideTime = 30;
  float roundTime = 240;
  int bots = 7;           // Gesamtzahl Spieler wird mit Bots aufgefüllt
  int botSkill = 1;       // 0 leicht, 1 normal, 2 schwer
  bool infection = false; // gefundene Props werden zu Jägern
  bool teamSwap = true;
};

inline const char* TeamName(Team t) {
  switch (t) {
    case TEAM_HUNTERS: return "Jäger";
    case TEAM_PROPS: return "Props";
    default: return "Zuschauer";
  }
}
