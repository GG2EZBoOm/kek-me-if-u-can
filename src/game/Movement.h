#pragma once
// Gemeinsamer Bewegungscode (Server-Simulation und Client-Vorhersage).
#include "game/GameTypes.h"
#include "game/World.h"

struct MoveConfig {
  float maxSpeed = 4.6f;
  float sprintMul = 1.5f;
  float crouchMul = 0.5f;
  float accel = 11.0f;
  float airAccel = 1.6f;
  float friction = 7.0f;
  float stopSpeed = 1.5f;
  float jumpVel = 6.6f;
  float gravity = 20.0f;
  float stepHeight = 0.45f;
  float staminaDrain = 0.24f;  // pro Sekunde beim Sprinten
  float staminaRegen = 0.16f;
};

MoveConfig MoveConfigFor(int propType);

// Kollisionsquader eines Spielers (propType < 0: Jäger)
OBox PlayerBox(const MoveState& s, int propType);
float EyeHeight(const MoveState& s, int propType);
Vec3 EyePos(const MoveState& s, int propType);
// Kamera für Props (3rd Person), deterministisch auf Server und Client
Vec3 PropPivot(const MoveState& s, int propType);
float PropCamDistance(int propType);
Vec3 ThirdPersonCamera(const World& w, Vec3 pivot, float yaw, float pitch, float dist);

struct MoveEnv {
  const World* world = nullptr;
  const std::vector<OBox>* others = nullptr;  // andere Spieler, Köder
};

bool BoxBlocked(const MoveEnv& env, const OBox& b, float lift = 0.0f);
void PlayerMove(const MoveEnv& env, MoveState& s, const UserCmd& cmd, int propType, bool frozen, float dt);
// Prop-Drehung zur Blickrichtung (wenn nicht gesperrt und nicht blockiert)
void UpdatePropRotation(const MoveEnv& env, MoveState& s, int propType);
// Sucht eine freie Position für einen Quader in der Nähe (z. B. nach Verwandlung)
bool FindFreeSpot(const MoveEnv& env, MoveState& s, int propType);
// Kompletter Bewegungsschritt für einen Befehl (Rotationssperre, Bewegung, Prop-Drehung).
// Identisch auf Server und Client, damit die Vorhersage stimmt. Gibt true zurück, wenn gesprungen wurde.
bool ApplyMovement(const MoveEnv& env, MoveState& s, const UserCmd& cmd, int propType, bool frozen);
