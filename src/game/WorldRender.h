#pragma once
// Bringt eine World auf den Renderer: statische Geometrie hochladen, Props und Lichter pro Frame einreichen.
#include "engine/Renderer.h"
#include "game/World.h"

struct Frustum {
  Vec4 planes[6];
  void FromMatrix(const Mat4& vp);
  bool Sphere(Vec3 c, float r) const;
};

class WorldRender {
 public:
  // Registriert alle Prop-Meshes (Mesh-ID == Prop-Typ)
  void Init(Renderer& r);
  void Load(Renderer& r, const World& w);
  void Submit(Renderer& r, const World& w, const Frustum& fr, Vec3 camPos, int highlightProp = -1, float highlight = 0);
  // Einheitliche Sichtbarkeitsregel für echte und verwandelte Props (keine Unterschiede, die Spieler verraten)
  bool PropVisible(int type, Vec3 pos, float yaw, const Frustum& fr, Vec3 camPos) const;
  const World* Loaded() const { return loaded_; }

 private:
  bool init_ = false;
  struct Anim {
    std::unique_ptr<Mesh> mesh;
    Vec3 center;
    float radius = 0;
  };
  std::vector<Anim> anims_;
  Mesh flagMesh_;
  MeshData flagData_;
  void BuildFlags(const World& w, const Frustum& fr, float t);
  const World* loaded_ = nullptr;
  std::vector<float> radius_;
};
