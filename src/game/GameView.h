#pragma once
// 3D-Darstellung des Spiels: Kamera, Spielermodelle, Waffe, Effekte.
#include "engine/Renderer.h"
#include "game/ClientState.h"
#include "game/WorldRender.h"
#include "game/WeaponModels.h"

struct ViewInput {
  float yaw = 0, pitch = 0;
  bool aiming = false;
  float fov = 85;
  int spectate = -1;    // Ziel des Zuschauermodus
  bool freeCam = false;
  Vec3 freeCamPos;
};

class GameView {
 public:
  void Init(Renderer& r);
  void LoadWorld(Renderer& r, const World& w);
  void OnEvent(const GameEvent& e, const ClientState& s, int localId, const World& w);
  void Update(float dt);
  void Render(Renderer& r, const ClientState& s, int localId, const World& w, const ViewInput& in, float time, int width,
              int height, ScreenFX& fx);
  // Menü-Hintergrund: Kamerafahrt
  void RenderFlyby(Renderer& r, const World& w, float time, int width, int height);
  void RenderWorkbench(Renderer& r, int weapon, u16 build, float angle, float elevation, int selectedSlot,
                       float time, int width, int height);

  const Camera& Cam() const { return cam_; }
  int HighlightProp() const { return highlight_; }
  // Projektion für Namensschilder (false, wenn hinter der Kamera)
  bool Project(Vec3 p, int width, int height, Vec2& out) const;
  void Shake(float amount) { shake_ = std::max(shake_, amount); }
  WorldRender& Worlds() { return wr_; }
  // Für Vorschaubilder (Kamera-Viewer): Jäger in einer Pose zeichnen
  void DrawHunterPreview(Renderer& r, Vec3 pos, float yaw, float pitch, float walkPhase, float speed, bool crouch, int weapon,
                         float sky);

 private:
  struct Fx {
    Vec3 pos, vel;
    float life = 0, maxLife = 1;
    float size0 = 0.1f, size1 = 0.1f;
    Vec4 c0, c1;
    bool additive = false;
    float gravity = 0, drag = 0;
  };
  struct FxLight {
    Vec3 pos;
    float radius;
    Vec3 color;
    float life, maxLife;
  };
  void Burst(Vec3 pos, int n, float speed, Vec4 c0, Vec4 c1, float size, float life, bool additive, float gravity, float up = 0.5f);
  void Confetti(Vec3 pos, int n);
  void Smoke(Vec3 pos, int n, float size, Vec4 color);
  void AddLight(Vec3 pos, float radius, Vec3 color, float life);
  void DrawHunter(Renderer& r, const ClientPlayer& p, int id, float sky);
  void DrawViewModel(Renderer& r, const ClientPlayer& me, const ViewInput& in, float sky);

  Renderer* renderer_ = nullptr;
  WorldRender wr_;
  Camera cam_;
  std::vector<Fx> fx_;
  std::vector<FxLight> lights_;
  std::vector<PointLight> lightBuf_;
  Mesh torso_, head_, thigh_, shin_, upperArm_, foreArm_, grenade_;
  Mesh vmGrenade_, grenadeHands_, flash_;
  WeaponModels weapons_;
  Mesh rightHand_, leftHand_, bench_, casing_;
  struct Casing { Vec3 pos, vel; float life, delay, spin; };
  std::vector<Casing> casings_;
  float walkPhase_[MAX_PLAYERS] = {};
  float eyeH_ = 1.62f;
  float recoil_ = 0, flashTimer_ = 0, shake_ = 0, bobPhase_ = 0;
  float aimBlend_ = 0;
  float switchAnim_ = 0;
  int lastWeapon_ = -1;
  int highlight_ = -1;
  float time_ = 0;
  float frameDt_ = TICK_DT, sprintBlend_ = 0, lastYaw_ = 0, lastPitch_ = 0;
  Vec2 sway_, cameraRecoil_;
  Rng rng_{4242};
};
