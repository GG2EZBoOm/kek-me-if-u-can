#pragma once
#include "game/GameTypes.h"

struct Duty {
  const char* map;
  const char* form;
  const char* instruction;
  const char* announcement;
  Vec3 spot;
  Vec3 speaker;
  float radius;
  int sound;
};
const Duty* MapDuty(const std::string& map);
constexpr float DUTY_HOLD = 3, DUTY_BONUS = 10;
