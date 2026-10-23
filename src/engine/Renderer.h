#pragma once
// Vorwärts-Renderer mit HDR, kaskadierten Sonnenschatten, Punktlichtern, SSAO, Bloom und FXAA.
#include "engine/Mesh.h"
#include "engine/Shader.h"

struct Camera {
  Vec3 pos;
  float yaw = 0, pitch = 0, roll = 0;
  float fovDeg = 80;
  float zNear = 0.05f, zFar = 500;
  Mat4 view, proj, viewProj, invViewProj;
  void Update(float aspect);
  Vec3 Forward() const { return DirFromYawPitch(yaw, pitch); }
  Vec3 Right() const { return RightFromYaw(yaw); }
};

struct PointLight {
  Vec3 pos;
  float radius = 8;
  Vec3 color = Vec3(1);  // inkl. Intensität
};

struct Environment {
  Vec3 sunDir = Normalize(Vec3(0.45f, 0.75f, 0.35f));  // zeigt zur Sonne
  Vec3 sunColor = Vec3(3.2f, 2.95f, 2.6f);
  Vec3 skyZenith = Hex(0x3a6fc4), skyHorizon = Hex(0xb9d3ec), groundColor = Hex(0x6b6a5e);
  Vec3 skyAmbient = Vec3(0.42f, 0.5f, 0.62f), groundAmbient = Vec3(0.25f, 0.23f, 0.2f);
  Vec3 indoorAmbient = Vec3(0.2f, 0.19f, 0.18f);
  Vec3 fogColor = Hex(0xb9cde2);
  float fogDensity = 0.004f;
  float exposure = 1.0f;
  float clouds = 0.5f;
  Vec3 worldMin = Vec3(-60, -5, -60), worldMax = Vec3(60, 40, 60);
};

struct PropInstance {
  Vec3 pos;
  float yaw = 0;
  float sky = 1;        // Himmelslicht-Anteil (innen < 1)
  float highlight = 0;  // Hervorhebung (Leuchtrand)
  float scale = 1;
  float alpha = 1;
  Vec3 tint = Vec3(1);
  float tintAmount = 0;
};

struct DrawItem {
  const Mesh* mesh = nullptr;
  Mat4 model = Mat4::Identity();
  float sky = 1;
  Vec3 tint = Vec3(1);
  float tintAmount = 0;
  float highlight = 0;
  bool viewModel = false;
  bool castShadow = true;
};

struct Particle {
  Vec3 pos;
  float size = 0.1f;
  Vec4 color = Vec4(1, 1, 1, 1);
  bool additive = false;
};

struct RenderSettings {
  int shadowSize = 4096;
  bool ssao = true;
  bool bloom = true;
  bool fxaa = true;
};

struct ScreenFX {
  float damage = 0;      // rote Ränder
  float saturation = 1;  // 0 = schwarzweiß
  float blackout = 0;    // 0..1 abgedunkelt (Versteckphase der Jäger)
  float vignette = 0.3f;
  Vec3 tint = Vec3(1);
  float viewModelFov = 68;
};

struct RenderStats {
  int drawCalls = 0;
  int instances = 0;
  int lights = 0;
  int triangles = 0;
};

class Renderer {
 public:
  bool Init(const RenderSettings& s);
  void Shutdown();
  void ApplySettings(const RenderSettings& s);
  const RenderSettings& Settings() const { return settings_; }

  void SetWorld(const MeshData& opaque, const MeshData& transparent);
  int AddPropMesh(const MeshData& d);
  void ClearPropMeshes();
  const Mesh* PropMesh(int id) const { return id >= 0 && id < (int)propMeshes_.size() ? propMeshes_[id].get() : nullptr; }

  void Begin(const Camera& cam, const Environment& env, int width, int height, float time);
  float Time() const { return time_; }
  void SetLights(const std::vector<PointLight>& lights);
  void Prop(int meshId, const PropInstance& inst);
  void Draw(const DrawItem& item);
  void AddParticle(const Particle& p) { particles_.push_back(p); }
  void AddDecal(Vec3 pos, Vec3 normal, float size, Vec4 color);
  void ClearDecals();
  void Render(const ScreenFX& fx);
  void ReadBackbuffer(std::vector<u8>& rgba, int& w, int& h);
  const RenderStats& Stats() const { return stats_; }
  const Camera& Cam() const { return cam_; }
  // Weichgezeichnete Kopie des letzten Bildes (für Milchglas in der Oberfläche)
  unsigned BackdropTexture() const { return back_[2].color[0]; }

 private:
  struct Target {
    GLuint fbo = 0;
    GLuint color[2] = {0, 0};
    GLuint depth = 0;
    int w = 0, h = 0;
  };
  struct InstanceData {
    float d[12];
  };
  void CreateTargets(int w, int h);
  void DestroyTargets();
  void CreateShadowMaps();
  void ComputeShadowMatrices();
  void DrawScene(Shader& sh, bool shadowPass);
  void DrawInstances(Shader& sh);
  void DrawItems(Shader& sh, bool viewModel, bool shadowPass);
  void SetCommonUniforms(Shader& sh);
  void SetConstInstance(float sky, Vec3 tint, float tintAmount, float highlight);
  void Fullscreen();
  void RebuildDecalMesh();

  RenderSettings settings_;
  Shader lit_, depth_, sky_, ssao_, blur_, bloomDown_, bloomUp_, composite_, fxaa_, particle_, kawase_;
  Mesh world_, glass_, decalMesh_;
  std::vector<std::unique_ptr<Mesh>> propMeshes_;
  std::vector<std::vector<PropInstance>> instances_;
  std::vector<DrawItem> items_;
  std::vector<Particle> particles_;
  struct DecalRec { Vec3 pos, normal; float size; Vec4 color; };
  std::vector<DecalRec> decals_;
  int decalHead_ = 0;
  bool decalsDirty_ = false;

  Camera cam_;
  Environment env_;
  float time_ = 0;
  int width_ = 0, height_ = 0;
  Target hdr_, ao_, aoBlur_, ldr_;
  Target bloom_[5];
  Target back_[3];  // unscharfer Bildhintergrund für Milchglas-Flächen der Oberfläche
  int bloomLevels_ = 0;
  GLuint shadowTex_[2] = {0, 0};
  GLuint shadowFbo_[2] = {0, 0};
  int shadowRes_[2] = {2048, 4096};
  Mat4 shadowMat_[2];
  Mat4 shadowViewProj_[2];
  GLuint emptyVao_ = 0;
  GLuint instanceVbo_ = 0;
  size_t instanceCap_ = 0;
  GLuint particleVao_ = 0, particleVbo_ = 0;
  GLuint lightUbo_ = 0;
  GLuint noiseTex_ = 0;
  int lightCount_ = 0;
  Vec3 ssaoKernel_[16];
  RenderStats stats_;
  ScreenFX fx_;
};
