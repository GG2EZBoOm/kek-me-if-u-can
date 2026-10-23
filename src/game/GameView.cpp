#include "game/GameView.h"
#include "game/Movement.h"
#include "game/Sim.h"

namespace {

void BuildMeshes(Mesh& torso, Mesh& head, Mesh& arm, Mesh& leg, Mesh& gun3p, Mesh& grenade) {
  MeshData d;
  MeshBuilder b(&d);
  // Bein (Hüfte im Ursprung, nach unten)
  b.Mat(MAT_FABRIC); b.ColorHex(0x1f2a44);
  b.Box({-0.08f, -0.78f, -0.09f}, {0.08f, 0.0f, 0.09f});
  b.Mat(MAT_LEATHER); b.ColorHex(0x141416);
  b.Box({-0.085f, -0.88f, -0.14f}, {0.085f, -0.76f, 0.09f});
  leg.Upload(d);
  d.Clear();
  // Rumpf (Hüfte im Ursprung)
  b.Mat(MAT_FABRIC); b.ColorHex(0x23304d);
  b.Box({-0.22f, 0.0f, -0.12f}, {0.22f, 0.56f, 0.12f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0xd8e030); b.Emissive(0.08f);
  b.Box({-0.226f, 0.2f, -0.126f}, {0.226f, 0.26f, 0.126f});
  b.Emissive(0);
  b.Mat(MAT_LEATHER); b.ColorHex(0x111111);
  b.Box({-0.226f, 0.0f, -0.126f}, {0.226f, 0.06f, 0.126f});
  b.Mat(MAT_GOLD); b.ColorHex(0xc9a24a);
  b.Box({0.07f, 0.38f, -0.13f}, {0.13f, 0.45f, -0.12f});
  b.Mat(MAT_FABRIC); b.ColorHex(0x1a2438);
  b.Box({-0.18f, 0.3f, 0.12f}, {0.18f, 0.5f, 0.17f});
  torso.Upload(d);
  d.Clear();
  // Kopf (Halsansatz im Ursprung)
  b.Mat(MAT_PLASTIC); b.ColorHex(0xd9a684);
  b.Box({-0.05f, 0.0f, -0.05f}, {0.05f, 0.06f, 0.05f});
  b.Box({-0.11f, 0.05f, -0.11f}, {0.11f, 0.29f, 0.11f});
  b.ColorHex(0x1b1b1b);
  b.Box({-0.07f, 0.17f, -0.112f}, {-0.03f, 0.2f, -0.105f});
  b.Box({0.03f, 0.17f, -0.112f}, {0.07f, 0.2f, -0.105f});
  b.Mat(MAT_FABRIC); b.ColorHex(0x1d2740);
  b.Box({-0.12f, 0.25f, -0.12f}, {0.12f, 0.34f, 0.12f});
  b.Box({-0.12f, 0.25f, -0.21f}, {0.12f, 0.27f, -0.11f});
  b.Mat(MAT_GOLD); b.ColorHex(0xc9a24a);
  b.Box({-0.03f, 0.28f, -0.125f}, {0.03f, 0.32f, -0.118f});
  head.Upload(d);
  d.Clear();
  // Arm (Schulter im Ursprung)
  b.Mat(MAT_FABRIC); b.ColorHex(0x23304d);
  b.Box({-0.06f, -0.55f, -0.06f}, {0.06f, 0.04f, 0.06f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0xd9a684);
  b.Box({-0.05f, -0.66f, -0.05f}, {0.05f, -0.55f, 0.05f});
  arm.Upload(d);
  d.Clear();
  // Waffe in dritter Person
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.Box({-0.03f, -0.05f, -0.4f}, {0.03f, 0.05f, 0.08f});
  b.Box({-0.02f, -0.18f, -0.22f}, {0.02f, -0.05f, -0.14f});
  b.CylinderZ({0, 0.02f, -0.48f}, 0.013f, 0.16f, 8);
  gun3p.Upload(d);
  d.Clear();
  b.Mat(MAT_METAL); b.ColorHex(0x3d4a2e);
  b.Sphere({0, 0, 0}, 0.06f, 10, 7);
  b.ColorHex(0x8d9299);
  b.Box({-0.01f, 0.04f, -0.02f}, {0.01f, 0.09f, 0.05f});
  grenade.Upload(d);
}

// Quader entlang einer Strecke (für Unterarme)
void Tube(MeshBuilder& b, Vec3 a, Vec3 c, float ra, float rc) {
  Vec3 y = c - a;
  float len = Length(y);
  y = y / len;
  Vec3 ref = fabsf(y.y) > 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
  Vec3 x = Normalize(Cross(ref, y));
  Vec3 z = Cross(x, y);
  Mat4 m = Mat4::Identity();
  m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
  m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
  m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
  m.m[12] = a.x; m.m[13] = a.y; m.m[14] = a.z;
  b.Push(m);
  // leicht verjüngter Ärmel aus zwei Abschnitten
  b.Box({-ra, 0, -ra}, {ra, len * 0.5f, ra});
  float rm = (ra + rc) * 0.5f;
  b.Box({-rm, len * 0.5f - 0.005f, -rm}, {rm, len, rm});
  b.Pop();
}

// Arme mit Handschuhen: rechte Hand am Griff, linke Hand vorne (z = linke Handposition)
void BuildArms(MeshBuilder& b, Vec3 rightHand, Vec3 leftHand, bool leftVisible) {
  const u32 glove = 0x1c1d20, sleeve = 0x23304d, cuff = 0x2c3a5c;
  b.Mat(MAT_LEATHER); b.ColorHex(glove);
  b.Push(Translation(rightHand) * RotationX(-0.25f));
  b.Box({-0.032f, -0.1f, -0.032f}, {0.035f, -0.005f, 0.04f});
  b.Box({-0.03f, -0.095f, -0.052f}, {0.03f, -0.035f, -0.03f});
  b.Box({0.028f, -0.04f, -0.03f}, {0.042f, 0.012f, 0.02f});
  b.Pop();
  b.Mat(MAT_FABRIC); b.ColorHex(cuff);
  Vec3 rw = rightHand + Vec3(0.008f, -0.1f, 0.05f);
  Tube(b, rw, rw + Vec3(0.02f, -0.03f, 0.05f), 0.036f, 0.038f);
  b.ColorHex(sleeve);
  Tube(b, rw + Vec3(0.015f, -0.02f, 0.04f), rw + Vec3(0.13f, -0.24f, 0.38f), 0.038f, 0.046f);
  if (!leftVisible) return;
  b.Mat(MAT_LEATHER); b.ColorHex(glove);
  b.Box(leftHand + Vec3(-0.04f, -0.045f, -0.05f), leftHand + Vec3(0.035f, -0.008f, 0.045f));
  b.Box(leftHand + Vec3(-0.048f, -0.02f, -0.04f), leftHand + Vec3(-0.032f, 0.03f, 0.03f));
  b.Box(leftHand + Vec3(0.03f, -0.03f, -0.045f), leftHand + Vec3(0.042f, 0.018f, 0.02f));
  b.Mat(MAT_FABRIC); b.ColorHex(cuff);
  Vec3 lw = leftHand + Vec3(-0.01f, -0.04f, 0.05f);
  Tube(b, lw, lw + Vec3(-0.03f, -0.03f, 0.05f), 0.035f, 0.037f);
  b.ColorHex(sleeve);
  Tube(b, lw + Vec3(-0.025f, -0.02f, 0.04f), lw + Vec3(-0.2f, -0.24f, 0.3f), 0.037f, 0.045f);
}

void BuildViewModels(Mesh& smg, Mesh& shotgun, Mesh& gren, Mesh* hands, Mesh& flash) {
  MeshData d;
  MeshBuilder b(&d);
  // --- MP (vorne = -Z, Ursprung über dem Griff) ---
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.Box({-0.027f, -0.034f, -0.3f}, {0.027f, 0.04f, 0.08f});
  b.ColorHex(0x1f2124);
  b.Box({-0.011f, 0.04f, -0.24f}, {0.011f, 0.051f, 0.03f});
  for (int i = 0; i < 9; i++) b.Box({-0.0115f, 0.051f, -0.23f + i * 0.028f}, {0.0115f, 0.055f, -0.218f + i * 0.028f});
  b.ColorHex(0x2a2c30);
  b.Box({-0.017f, 0.051f, -0.01f}, {-0.006f, 0.08f, 0.012f});
  b.Box({0.006f, 0.051f, -0.01f}, {0.017f, 0.08f, 0.012f});
  b.Box({-0.004f, 0.04f, -0.278f}, {0.004f, 0.076f, -0.268f});
  b.Box({-0.014f, 0.04f, -0.285f}, {0.014f, 0.08f, -0.282f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0x161719);
  b.Box({-0.033f, -0.06f, -0.3f}, {0.033f, 0.02f, -0.14f});
  b.ColorHex(0x0e0f10);
  for (int i = 0; i < 4; i++) b.Box({-0.0335f, -0.045f, -0.285f + i * 0.035f}, {0.0335f, -0.04f, -0.27f + i * 0.035f});
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.CylinderZ({0, 0.004f, -0.36f}, 0.011f, 0.13f, 10);
  b.CylinderZ({0, 0.004f, -0.44f}, 0.015f, 0.04f, 10);
  b.ColorHex(0x1b1c1f);
  b.Push(Translation({0, -0.034f, -0.1f}) * RotationX(0.2f));
  b.Box({-0.016f, -0.16f, -0.025f}, {0.016f, 0.0f, 0.025f});
  b.Pop();
  b.Push(Translation({0, -0.034f, 0.035f}) * RotationX(-0.25f));
  b.Box({-0.02f, -0.11f, -0.025f}, {0.02f, 0.0f, 0.022f});
  b.Pop();
  b.Box({-0.005f, -0.065f, -0.05f}, {0.005f, -0.058f, 0.02f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0xff3a2a); b.Emissive(1.0f);
  b.Box({-0.003f, 0.062f, -0.005f}, {0.003f, 0.068f, 0.0f});
  b.Emissive(0);
  smg.Upload(d);
  d.Clear();
  // --- Schrotflinte ---
  b.Mat(MAT_METAL); b.ColorHex(0x2e3034);
  b.Box({-0.03f, -0.04f, -0.17f}, {0.03f, 0.045f, 0.06f});
  b.CylinderZ({0, 0.022f, -0.48f}, 0.016f, 0.62f, 12);
  b.CylinderZ({0, -0.016f, -0.43f}, 0.013f, 0.5f, 10);
  b.Mat(MAT_GOLD); b.ColorHex(0xc9a24a);
  b.Sphere({0, 0.042f, -0.78f}, 0.005f, 6, 4);
  b.Mat(MAT_WOOD); b.ColorHex(0x6b4528);
  b.Box({-0.031f, -0.045f, -0.53f}, {0.031f, 0.006f, -0.36f});
  b.ColorHex(0x4e321c);
  for (int i = 0; i < 6; i++) b.Box({-0.0315f, -0.035f, -0.52f + i * 0.028f}, {0.0315f, -0.003f, -0.512f + i * 0.028f});
  b.ColorHex(0x6b4528);
  b.Push(Translation({0, -0.034f, 0.03f}) * RotationX(-0.25f));
  b.Box({-0.021f, -0.11f, -0.025f}, {0.021f, 0.0f, 0.03f});
  b.Pop();
  b.Push(Translation({0, -0.01f, 0.05f}) * RotationX(0.14f));
  b.Box({-0.023f, -0.06f, 0.0f}, {0.023f, 0.035f, 0.26f});
  b.Pop();
  shotgun.Upload(d);
  d.Clear();
  // --- Granate ---
  b.Mat(MAT_PLASTIC); b.ColorHex(0x4a5a32);
  b.Sphere({0, 0.01f, -0.06f}, 0.045f, 12, 8);
  b.ColorHex(0x36422a);
  for (int i = 0; i < 3; i++) b.Box({-0.046f, -0.02f + i * 0.02f, -0.065f}, {0.046f, -0.016f + i * 0.02f, -0.055f});
  b.Mat(MAT_METAL); b.ColorHex(0x8d9299);
  b.Box({-0.009f, 0.045f, -0.08f}, {0.009f, 0.075f, -0.04f});
  b.Box({0.005f, 0.04f, -0.05f}, {0.012f, 0.06f, 0.0f});
  b.Push(Translation({0.026f, 0.064f, -0.06f}) * RotationZ(PI * 0.5f));
  b.Torus({0, 0, 0}, 0.014f, 0.003f, 12, 5);
  b.Pop();
  gren.Upload(d);
  d.Clear();
  // Hände je Waffe
  BuildArms(b, Vec3(0, -0.034f, 0.035f), Vec3(0, -0.05f, -0.23f), true);
  hands[0].Upload(d);
  d.Clear();
  BuildArms(b, Vec3(0, -0.034f, 0.03f), Vec3(0, -0.04f, -0.45f), true);
  hands[1].Upload(d);
  d.Clear();
  BuildArms(b, Vec3(0, 0.05f, -0.03f), Vec3(), false);
  hands[2].Upload(d);
  d.Clear();
  b.Mat(MAT_PLASTIC); b.ColorHex(0xffc060); b.Emissive(1.0f);
  b.Box({-0.08f, -0.01f, -0.01f}, {0.08f, 0.01f, 0.01f});
  b.Box({-0.01f, -0.08f, -0.01f}, {0.01f, 0.08f, 0.01f});
  b.Push(RotationZ(PI * 0.25f));
  b.Box({-0.06f, -0.008f, -0.008f}, {0.06f, 0.008f, 0.008f});
  b.Box({-0.008f, -0.06f, -0.008f}, {0.008f, 0.06f, 0.008f});
  b.Pop();
  b.ColorHex(0xfff0c0);
  b.Sphere({0, 0, -0.02f}, 0.035f, 8, 6);
  b.Emissive(0);
  flash.Upload(d);
}

Vec4 V4(u32 hex, float a) {
  Vec3 c = Hex(hex);
  return Vec4(c, a);
}

}  // namespace

void GameView::Init(Renderer& r) {
  renderer_ = &r;
  wr_.Init(r);
  BuildMeshes(torso_, head_, arm_, leg_, gun3p_, grenade_);
  BuildViewModels(vmSmg_, vmShotgun_, vmGrenade_, vmHands_, flash_);
}

void GameView::LoadWorld(Renderer& r, const World& w) {
  wr_.Load(r, w);
  fx_.clear();
  lights_.clear();
}

void GameView::Burst(Vec3 pos, int n, float speed, Vec4 c0, Vec4 c1, float size, float life, bool additive, float gravity, float up) {
  for (int i = 0; i < n; i++) {
    Fx f;
    f.pos = pos;
    Vec3 d(rng_.Range(-1, 1), rng_.Range(-0.3f, 1.0f) + up, rng_.Range(-1, 1));
    f.vel = Normalize(d) * speed * rng_.Range(0.4f, 1.0f);
    f.maxLife = f.life = life * rng_.Range(0.6f, 1.2f);
    f.size0 = size;
    f.size1 = size * 0.3f;
    f.c0 = c0;
    f.c1 = c1;
    f.additive = additive;
    f.gravity = gravity;
    f.drag = 1.5f;
    fx_.push_back(f);
  }
}

void GameView::Confetti(Vec3 pos, int n) {
  const u32 cols[] = {0xff3b3b, 0xffd23b, 0x3bd16f, 0x3b8bff, 0xd13bff, 0xffffff, 0xff8a00};
  for (int i = 0; i < n; i++) {
    Fx f;
    f.pos = pos + Vec3(rng_.Range(-0.2f, 0.2f), rng_.Range(0, 0.4f), rng_.Range(-0.2f, 0.2f));
    f.vel = Vec3(rng_.Range(-3, 3), rng_.Range(2, 6), rng_.Range(-3, 3));
    f.maxLife = f.life = rng_.Range(1.5f, 2.8f);
    f.size0 = f.size1 = rng_.Range(0.025f, 0.05f);
    Vec3 c = Hex(cols[rng_.Int(0, 6)]);
    f.c0 = Vec4(c * 1.5f, 1);
    f.c1 = Vec4(c, 0);
    f.gravity = 4.0f;
    f.drag = 2.2f;
    fx_.push_back(f);
  }
}

void GameView::Smoke(Vec3 pos, int n, float size, Vec4 color) {
  for (int i = 0; i < n; i++) {
    Fx f;
    f.pos = pos + Vec3(rng_.Range(-0.3f, 0.3f), rng_.Range(0, 0.5f), rng_.Range(-0.3f, 0.3f)) * size * 2.0f;
    f.vel = Vec3(rng_.Range(-0.6f, 0.6f), rng_.Range(0.3f, 1.2f), rng_.Range(-0.6f, 0.6f));
    f.maxLife = f.life = rng_.Range(0.8f, 1.6f);
    f.size0 = size * 0.6f;
    f.size1 = size * 1.6f;
    f.c0 = color;
    f.c1 = Vec4(color.x, color.y, color.z, 0);
    f.drag = 1.0f;
    fx_.push_back(f);
  }
}

void GameView::AddLight(Vec3 pos, float radius, Vec3 color, float life) { lights_.push_back({pos, radius, color, life, life}); }

void GameView::OnEvent(const GameEvent& e, const ClientState& s, int localId, const World& w) {
  (void)w;
  switch (e.type) {
    case EV_SHOT:
      if (e.a == localId) {
        recoil_ = 1.0f;
        flashTimer_ = 0.05f;
        Shake(e.b == WPN_SHOTGUN ? 0.35f : 0.08f);
      } else {
        Vec3 muzzle = e.pos + e.dir * 0.7f - Vec3(0, 0.15f, 0);
        for (int i = 0; i < 14; i++) {
          Fx f;
          f.pos = muzzle + e.dir * (i * 1.5f);
          f.maxLife = f.life = 0.06f + i * 0.004f;
          f.size0 = 0.03f;
          f.size1 = 0.01f;
          f.c0 = Vec4(4.0f, 3.2f, 1.8f, 0.8f);
          f.c1 = Vec4(2.0f, 1.2f, 0.4f, 0.0f);
          f.additive = true;
          fx_.push_back(f);
        }
        Burst(muzzle, 4, 2.0f, Vec4(6, 4, 1.5f, 1), Vec4(2, 1, 0.2f, 0), 0.06f, 0.05f, true, 0);
      }
      AddLight(e.pos + e.dir * 0.8f, 5.0f, Vec3(4.0f, 2.8f, 1.4f), 0.06f);
      break;
    case EV_IMPACT: {
      bool metal = e.b == MAT_METAL || e.b == MAT_MIRROR || e.b == MAT_GOLD;
      if (e.c) Burst(e.pos, 7, 2.5f, V4(0x8a5a33, 1), V4(0x5a3a23, 0), 0.025f, 0.6f, false, 9.0f);
      else Burst(e.pos, 6, 1.5f, V4(0xb8b0a0, 0.8f), V4(0x8a8478, 0), 0.05f, 0.5f, false, 3.0f, 0.2f);
      if (metal) Burst(e.pos, 6, 4.0f, Vec4(6, 4, 1.5f, 1), Vec4(3, 1, 0.2f, 0), 0.015f, 0.25f, true, 12.0f);
      if (renderer_ && e.b != MAT_WATER) renderer_->AddDecal(e.pos, e.dir, e.b == MAT_GLASS ? 0.05f : 0.035f, e.b == MAT_GLASS ? Vec4(0.85f, 0.9f, 0.95f, 0.5f) : Vec4(0.04f, 0.04f, 0.04f, 0.92f));
      break;
    }
    case EV_HIT:
      Burst(e.pos, 10, 3.0f, V4(0xa87a4a, 1), V4(0x6b4a2e, 0), 0.03f, 0.7f, false, 9.0f);
      break;
    case EV_KILL: {
      const ClientPlayer& v = s.players[ClampI(e.b, 0, MAX_PLAYERS - 1)];
      if (v.team == TEAM_PROPS || e.f >= 0) {
        Confetti(e.pos + Vec3(0, 0.3f, 0), 60);
        Smoke(e.pos, 10, 0.5f, Vec4(0.9f, 0.9f, 0.9f, 0.6f));
        AddLight(e.pos + Vec3(0, 0.5f, 0), 6.0f, Vec3(3, 2.5f, 2), 0.3f);
      } else {
        Smoke(e.pos + Vec3(0, 0.8f, 0), 14, 0.6f, Vec4(0.4f, 0.4f, 0.45f, 0.7f));
      }
      break;
    }
    case EV_DISGUISE:
      Smoke(e.pos, 12, 0.4f, Vec4(0.95f, 0.95f, 1.0f, 0.55f));
      Burst(e.pos + Vec3(0, 0.3f, 0), 10, 2.0f, Vec4(3, 3, 3.5f, 1), Vec4(1, 1, 2, 0), 0.04f, 0.4f, true, 0);
      break;
    case EV_DECOY:
      Smoke(e.pos, 8, 0.35f, Vec4(0.95f, 0.95f, 1.0f, 0.5f));
      break;
    case EV_DECOY_BREAK:
      Confetti(e.pos + Vec3(0, 0.3f, 0), 25);
      Smoke(e.pos, 8, 0.4f, Vec4(0.8f, 0.8f, 0.8f, 0.6f));
      break;
    case EV_SONAR:
      for (int i = 0; i < 36; i++) {
        float a = TAU * i / 36;
        Fx f;
        f.pos = e.pos - Vec3(0, 1.2f, 0);
        f.vel = Vec3(cosf(a), 0, sinf(a)) * 9.0f;
        f.maxLife = f.life = 1.2f;
        f.size0 = 0.12f;
        f.size1 = 0.05f;
        f.c0 = Vec4(0.5f, 2.5f, 3.5f, 0.9f);
        f.c1 = Vec4(0.2f, 1.0f, 2.0f, 0);
        f.additive = true;
        fx_.push_back(f);
      }
      break;
    case EV_PING:
      Burst(e.pos, 8, 1.2f, Vec4(1.0f, 3.5f, 1.5f, 0.9f), Vec4(0.3f, 1.5f, 0.5f, 0), 0.06f, 0.6f, true, 0, 1.0f);
      AddLight(e.pos, 3.0f, Vec3(0.5f, 2.5f, 1.0f), 0.4f);
      break;
    case EV_EXPLOSION: {
      Burst(e.pos, 40, 9.0f, Vec4(8, 5, 2, 1), Vec4(3, 1, 0.2f, 0), 0.25f, 0.5f, true, 2.0f);
      Smoke(e.pos, 25, 1.4f, Vec4(0.25f, 0.24f, 0.22f, 0.8f));
      Burst(e.pos, 20, 7.0f, V4(0x3a3a3a, 1), V4(0x222222, 0), 0.05f, 1.0f, false, 12.0f);
      AddLight(e.pos + Vec3(0, 0.5f, 0), 14.0f, Vec3(12, 7, 3), 0.6f);
      if (renderer_) renderer_->AddDecal(e.pos - Vec3(0, 0.05f, 0), Vec3(0, 1, 0), 1.3f, Vec4(0.03f, 0.03f, 0.03f, 0.75f));
      if (localId >= 0) {
        float d = Distance(s.players[localId].pos, e.pos);
        if (d < 25) Shake(1.2f * (1.0f - d / 25.0f));
      }
      break;
    }
    case EV_JUMP:
      Burst(e.pos + Vec3(0, 0.05f, 0), 4, 0.8f, Vec4(0.7f, 0.68f, 0.62f, 0.4f), Vec4(0.6f, 0.6f, 0.55f, 0), 0.08f, 0.4f, false, 0, -0.2f);
      break;
    default:
      break;
  }
}

void GameView::Update(float dt) {
  time_ += dt;
  for (Fx& f : fx_) {
    f.life -= dt;
    f.vel.y -= f.gravity * dt;
    f.vel *= expf(-f.drag * dt);
    f.pos += f.vel * dt;
  }
  fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const Fx& f) { return f.life <= 0; }), fx_.end());
  if (fx_.size() > 4000) fx_.erase(fx_.begin(), fx_.begin() + (fx_.size() - 4000));
  for (FxLight& l : lights_) l.life -= dt;
  lights_.erase(std::remove_if(lights_.begin(), lights_.end(), [](const FxLight& l) { return l.life <= 0; }), lights_.end());
  recoil_ = Damp(recoil_, 0, 14.0f, dt);
  flashTimer_ = std::max(0.0f, flashTimer_ - dt);
  shake_ = Damp(shake_, 0, 6.0f, dt);
  switchAnim_ = std::max(0.0f, switchAnim_ - dt * 3.0f);
}

bool GameView::Project(Vec3 p, int width, int height, Vec2& out) const {
  Vec4 c = cam_.viewProj * Vec4(p, 1);
  if (c.w <= 0.05f) return false;
  out.x = (c.x / c.w * 0.5f + 0.5f) * width;
  out.y = (1.0f - (c.y / c.w * 0.5f + 0.5f)) * height;
  return true;
}

void GameView::DrawHunter(Renderer& r, const ClientPlayer& p, int id, float sky) {
  float speed = sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z);
  float amt = Saturate(speed / 4.0f);
  float ph = walkPhase_[id];
  float crouch = p.crouching ? 0.3f : 0.0f;
  float hipY = 0.9f - crouch;
  float swing = sinf(ph) * 0.6f * amt;
  Mat4 base = Translation(p.pos) * RotationY(p.yaw);
  DrawItem it;
  it.sky = sky;
  it.mesh = &leg_;
  it.model = base * Translation({-0.1f, hipY, 0}) * RotationX(swing + (p.crouching ? -0.8f : 0));
  r.Draw(it);
  it.model = base * Translation({0.1f, hipY, 0}) * RotationX(-swing + (p.crouching ? -0.8f : 0));
  r.Draw(it);
  Mat4 torsoM = base * Translation({0, hipY, 0}) * RotationX(p.crouching ? -0.25f : -0.05f * amt);
  it.mesh = &torso_;
  it.model = torsoM;
  r.Draw(it);
  it.mesh = &head_;
  it.model = torsoM * Translation({0, 0.56f, 0}) * RotationX(Clamp(p.pitch, -0.8f, 0.8f) * 0.6f);
  r.Draw(it);
  float aim = 1.45f + Clamp(p.pitch, -1.0f, 1.0f);
  it.mesh = &arm_;
  it.model = torsoM * Translation({0.27f, 0.5f, 0}) * RotationX(aim) * RotationZ(0.15f);
  r.Draw(it);
  it.model = torsoM * Translation({-0.27f, 0.5f, 0}) * RotationY(-0.5f) * RotationX(aim - 0.1f) * RotationZ(-0.25f);
  r.Draw(it);
  it.mesh = &gun3p_;
  it.model = torsoM * Translation({0.1f, 0.42f, -0.12f}) * RotationX(Clamp(p.pitch, -1.0f, 1.0f));
  r.Draw(it);
}

void GameView::DrawViewModel(Renderer& r, const ClientPlayer& me, const ViewInput& in, float sky) {
  if (me.weapon != lastWeapon_) {
    switchAnim_ = 1.0f;
    lastWeapon_ = me.weapon;
  }
  const Mesh* gun = me.weapon == WPN_SHOTGUN ? &vmShotgun_ : (me.weapon == WPN_GRENADE ? &vmGrenade_ : &vmSmg_);
  if (me.weapon == WPN_GRENADE && me.ammo[WPN_GRENADE] == 0) gun = nullptr;
  float speed = sqrtf(me.vel.x * me.vel.x + me.vel.z * me.vel.z);
  float bobAmt = me.onGround ? Saturate(speed / 5.0f) : 0.2f;
  Vec3 base = Lerp(Vec3(0.165f, -0.165f, -0.42f), Vec3(0.0f, -0.044f, -0.3f), aimBlend_);
  Vec3 bob(sinf(bobPhase_) * 0.012f * bobAmt, -fabsf(cosf(bobPhase_)) * 0.014f * bobAmt, 0);
  bob *= 1.0f - aimBlend_ * 0.7f;
  float reload = me.reloadTimer > 0 ? Saturate(std::min(me.reloadTimer, GetWeapon(me.weapon).reload - me.reloadTimer) * 3.0f) : 0.0f;
  float sw = std::max(switchAnim_, Saturate(me.switchTimer * 3));
  Vec3 off = base + bob + Vec3(0, -0.25f * reload - 0.3f * sw, 0.06f * recoil_);
  Mat4 camM = Translation(cam_.pos) * RotationY(cam_.yaw) * RotationX(cam_.pitch);
  Mat4 m = camM * Translation(off) * RotationX(0.09f * recoil_ + 0.5f * reload) * RotationZ(-0.4f * reload) * Scaling(Vec3(0.58f));
  DrawItem it;
  it.viewModel = true;
  it.castShadow = false;
  it.sky = sky;
  if (gun) {
    it.mesh = gun;
    it.model = m;
    r.Draw(it);
  }
  it.mesh = &vmHands_[ClampI(me.weapon, 0, 2)];
  it.model = m;
  r.Draw(it);
  if (flashTimer_ > 0 && me.weapon != WPN_GRENADE) {
    it.mesh = &flash_;
    float z = me.weapon == WPN_SHOTGUN ? -0.81f : -0.47f;
    it.model = m * Translation({0, 0.02f, z}) * RotationZ(rng_.Range(0, TAU)) * Scaling(Vec3(rng_.Range(0.8f, 1.3f)));
    r.Draw(it);
  }
}

void GameView::Render(Renderer& r, const ClientState& s, int localId, const World& w, const ViewInput& in, float time,
                      int width, int height, ScreenFX& fx) {
  const ClientPlayer* me = (localId >= 0 && localId < MAX_PLAYERS) ? &s.players[localId] : nullptr;
  float dt = 1.0f / 60.0f;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const ClientPlayer& p = s.players[i];
    float sp = sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z);
    if (p.onGround) walkPhase_[i] += sp * dt * 2.4f;
  }
  highlight_ = -1;
  Camera cam;
  cam.fovDeg = in.fov;
  cam.yaw = in.yaw;
  cam.pitch = in.pitch;
  bool firstPerson = false;
  if (me && me->alive && me->team == TEAM_HUNTERS) {
    float target = me->crouching ? 1.0f : 1.62f;
    eyeH_ = Damp(eyeH_, target, 14.0f, dt);
    cam.pos = me->pos + Vec3(0, eyeH_, 0);
    aimBlend_ = Damp(aimBlend_, in.aiming && me->weapon != WPN_GRENADE ? 1.0f : 0.0f, 14.0f, dt);
    cam.fovDeg = Lerp(in.fov, in.fov * 0.62f, aimBlend_);
    float sp = sqrtf(me->vel.x * me->vel.x + me->vel.z * me->vel.z);
    if (me->onGround) bobPhase_ += sp * dt * 2.4f;
    firstPerson = true;
  } else if (me && me->alive && me->team == TEAM_PROPS) {
    MoveState ms;
    ms.pos = me->pos;
    ms.colYaw = me->colYaw;
    ms.yaw = in.yaw;
    ms.pitch = in.pitch;
    Vec3 pivot = PropPivot(ms, me->propType);
    cam.pos = ThirdPersonCamera(w, pivot, in.yaw, in.pitch, PropCamDistance(me->propType));
    highlight_ = FindDisguiseTarget(w, ms, me->propType);
  } else {
    if (in.freeCam) {
      cam.pos = in.freeCamPos;
    } else if (in.spectate >= 0 && s.players[in.spectate].active && s.players[in.spectate].alive) {
      const ClientPlayer& t = s.players[in.spectate];
      MoveState ms;
      ms.pos = t.pos;
      int pt = t.team == TEAM_PROPS ? t.propType : -1;
      Vec3 pivot = PropPivot(ms, pt);
      cam.pos = ThirdPersonCamera(w, pivot, in.yaw, in.pitch, PropCamDistance(pt) + 0.6f);
    } else {
      float a = time * 0.05f;
      cam.pos = Vec3(cosf(a) * 55.0f, 30.0f, sinf(a) * 55.0f);
      Vec3 d = Normalize(Vec3(0, 8, 0) - cam.pos);
      cam.yaw = YawFromDir(d);
      cam.pitch = asinf(d.y);
    }
  }
  if (shake_ > 0.001f) {
    cam.yaw += rng_.Range(-1, 1) * shake_ * 0.02f;
    cam.pitch += rng_.Range(-1, 1) * shake_ * 0.02f;
  }
  cam.pitch += recoil_ * 0.012f * (firstPerson ? 1.0f : 0.0f);

  r.Begin(cam, w.env, width, height, time);
  cam_ = r.Cam();
  Frustum fr;
  fr.FromMatrix(cam_.viewProj);
  float pulse = 0.35f + 0.25f * sinf(time * 6.0f);
  wr_.Submit(r, w, fr, cam_.pos, highlight_, pulse);

  for (int i = 0; i < MAX_PLAYERS; i++) {
    const ClientPlayer& p = s.players[i];
    if (!p.active || !p.alive) continue;
    float sky = w.SkyAt(p.pos + Vec3(0, 0.3f, 0));
    if (p.team == TEAM_PROPS) {
      if (p.propType < 0 || !wr_.PropVisible(p.propType, p.pos, p.colYaw, fr, cam_.pos)) continue;
      PropInstance inst;
      inst.pos = p.pos;
      inst.yaw = p.colYaw;
      inst.sky = sky;
      r.Prop(p.propType, inst);
    } else if (p.team == TEAM_HUNTERS) {
      if (i == localId && firstPerson) continue;
      if (Distance(p.pos, cam_.pos) > 120.0f) continue;
      DrawHunter(r, p, i, sky);
    }
  }
  for (const ClientDecoy& d : s.decoys) {
    if (!d.active || !wr_.PropVisible(d.propType, d.pos, d.yaw, fr, cam_.pos)) continue;
    PropInstance inst;
    inst.pos = d.pos;
    inst.yaw = d.yaw;
    inst.sky = w.SkyAt(d.pos + Vec3(0, 0.3f, 0));
    r.Prop(d.propType, inst);
  }
  for (const ClientGrenade& g : s.grenades) {
    if (!g.active) continue;
    DrawItem it;
    it.mesh = &grenade_;
    it.model = Translation(g.pos) * RotationY(time * 8.0f) * RotationX(time * 5.0f);
    it.sky = w.SkyAt(g.pos);
    r.Draw(it);
    Particle pt;
    pt.pos = g.pos + Vec3(0, 0.08f, 0);
    pt.size = 0.05f;
    pt.color = Vec4(4.0f, 0.6f, 0.4f, fmodf(time, 0.4f) < 0.2f ? 1.0f : 0.2f);
    pt.additive = true;
    r.AddParticle(pt);
  }
  if (firstPerson && me) DrawViewModel(r, *me, in, w.SkyAt(cam_.pos));

  for (const Fx& f : fx_) {
    float t = 1.0f - f.life / f.maxLife;
    Particle p;
    p.pos = f.pos;
    p.size = Lerp(f.size0, f.size1, t);
    p.color = f.c0 * (1.0f - t) + f.c1 * t;
    p.additive = f.additive;
    r.AddParticle(p);
  }
  lightBuf_ = w.lights;
  for (const FxLight& l : lights_) {
    float k = l.life / l.maxLife;
    lightBuf_.push_back({l.pos, l.radius, l.color * k});
  }
  if (flashTimer_ > 0 && firstPerson) lightBuf_.push_back({cam_.pos + cam_.Forward() * 0.8f, 5.0f, Vec3(3.5f, 2.5f, 1.2f)});
  r.SetLights(lightBuf_);

  fx.viewModelFov = 68;
  if (me) {
    fx.damage = Saturate(me->hurtTimer * 2.0f) * 0.6f;
    if (!me->alive && me->team != TEAM_SPECTATOR) fx.saturation = 0.35f;
    if (me->alive && me->team == TEAM_HUNTERS && s.phase == Phase::Hiding) fx.blackout = 0.985f;
  }
  r.Render(fx);
}

void GameView::RenderFlyby(Renderer& r, const World& w, float time, int width, int height) {
  const auto& path = w.cameraPath;
  Camera cam;
  cam.fovDeg = 70;
  int shots = (int)path.size() / 2;
  if (shots > 0) {
    float seg = 9.0f;
    int i = (int)(time / seg) % shots;
    float t = fmodf(time, seg) / seg;
    Vec3 p = path[i * 2], tgt = path[i * 2 + 1];
    Vec3 side = Normalize(Cross(tgt - p, Vec3(0, 1, 0)));
    cam.pos = p + side * ((t - 0.5f) * 3.0f) + Vec3(0, sinf(t * PI) * 0.4f, 0);
    Vec3 d = Normalize(tgt - cam.pos);
    cam.yaw = YawFromDir(d);
    cam.pitch = asinf(Clamp(d.y, -1.0f, 1.0f));
  }
  r.Begin(cam, w.env, width, height, time);
  cam_ = r.Cam();
  Frustum fr;
  fr.FromMatrix(cam_.viewProj);
  wr_.Submit(r, w, fr, cam_.pos);
  r.SetLights(w.lights);
  ScreenFX fx;
  fx.vignette = 0.5f;
  r.Render(fx);
}
