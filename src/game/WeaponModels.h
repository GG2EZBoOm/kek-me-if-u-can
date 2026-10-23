#pragma once
#include "engine/Renderer.h"
#include "game/Weapons.h"

struct WeaponPose {
  float reload = 0, bolt = 0, pump = 0, shell = 0;
  Vec3 magazineOffset, leftHand;
  float magazineRoll = 0;
  bool magazineVisible = true, shellVisible = false;
};
WeaponPose MakeWeaponPose(int weapon, u16 build, float reloadRemaining, float reloadDuration, float fireRemaining, bool empty = false);

// Shared geometry for the bench, first person and other players. Each socket is drawn separately.
class WeaponModels {
 public:
  void Init();
  void Draw(Renderer& r, int weapon, u16 build, const Mat4& model, float sky, bool viewModel,
            const WeaponPose& pose = WeaponPose(), int highlightSlot = -1) const;
 private:
  Mesh body_[FIREARM_COUNT], parts_[FIREARM_COUNT][WS_COUNT][4], bolt_[FIREARM_COUNT], shell_;
};
