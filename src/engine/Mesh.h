#pragma once
// Prozedurale Geometrie: Vertexformat, Mesh-Baukasten und GPU-Meshes.
#include "engine/Common.h"
#include "engine/Math.h"
#include "engine/GL.h"

// Prozedurale Materialien (werden im Shader aus der Objektposition erzeugt)
enum Material : u8 {
  MAT_PLAIN = 0,
  MAT_WOOD,       // Holzmaserung
  MAT_TILE,       // Fliesen
  MAT_GRASS,
  MAT_SANDSTONE,  // Quadermauerwerk (Reichstag-Fassade)
  MAT_CARPET,
  MAT_METAL,
  MAT_FABRIC,
  MAT_CONCRETE,
  MAT_GLASS,      // transparent
  MAT_SCREEN,     // leuchtender Bildschirm
  MAT_LEAVES,
  MAT_MARBLE,
  MAT_PLASTIC,
  MAT_PAPER,
  MAT_WATER,
  MAT_MIRROR,     // Spiegel/Chrom
  MAT_ASPHALT,
  MAT_PARQUET,
  MAT_PLASTER,    // Putz
  MAT_GOLD,
  MAT_LEATHER,
  MAT_COUNT
};

struct Vertex {
  Vec3 pos;
  Vec3 normal;
  u8 color[4];
  u8 aux[4];  // x = Material, y = Himmelslicht (0..255), z = Leuchtkraft, w = frei
};

struct MeshData {
  std::vector<Vertex> verts;
  std::vector<u32> indices;
  void Clear() { verts.clear(); indices.clear(); }
  bool Empty() const { return indices.empty(); }
  void Append(const MeshData& o);
  void Bounds(Vec3& mn, Vec3& mx) const;
};

enum BoxFace : u32 {
  FACE_PX = 1, FACE_NX = 2, FACE_PY = 4, FACE_NY = 8, FACE_PZ = 16, FACE_NZ = 32, FACE_ALL = 63
};

class MeshBuilder {
 public:
  explicit MeshBuilder(MeshData* out) : out_(out) {}
  void SetTarget(MeshData* out) { out_ = out; }
  MeshData* Target() const { return out_; }

  // Pinsel-Zustand
  void Color(Vec3 c, float a = 1.0f);
  void ColorHex(u32 hex, float a = 1.0f) { Color(Hex(hex), a); }
  void Mat(Material m) { material_ = m; }
  void Emissive(float e) { emissive_ = (u8)(Saturate(e) * 255); }
  void SkyLight(float s) { sky_ = (u8)(Saturate(s) * 255); }
  Material CurMat() const { return (Material)material_; }

  // Transformationsstapel
  void Push(const Mat4& m);
  void Pop();
  void Reset();
  const Mat4& Xf() const { return xf_; }

  void Tri(Vec3 a, Vec3 b, Vec3 c);
  void Quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d);  // gegen den Uhrzeigersinn
  void QuadN(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n);
  void Box(Vec3 mn, Vec3 mx, u32 faces = FACE_ALL);
  void BoxC(Vec3 center, Vec3 half, u32 faces = FACE_ALL) { Box(center - half, center + half, faces); }
  // Quader um Y gedreht
  void BoxR(Vec3 center, Vec3 half, float yaw, u32 faces = FACE_ALL);
  void Cylinder(Vec3 base, float r, float h, int segs = 16, bool caps = true) { Frustum(base, r, r, h, segs, caps); }
  void Frustum(Vec3 base, float r0, float r1, float h, int segs = 16, bool caps = true);
  // Zylinder entlang X bzw. Z (z. B. liegende Flaschen, Rollen)
  void CylinderX(Vec3 center, float r, float len, int segs = 16);
  void CylinderZ(Vec3 center, float r, float len, int segs = 16);
  void Sphere(Vec3 c, float r, int segs = 16, int rings = 10) { Ellipsoid(c, Vec3(r), segs, rings); }
  void Ellipsoid(Vec3 c, Vec3 radii, int segs = 16, int rings = 10, float yCut = -1.0f);
  void Torus(Vec3 c, float R, float r, int segs = 20, int sides = 10);
  // Ring/Scheibe in XZ-Ebene (z. B. Kuppelringe)
  void Ring(Vec3 c, float r0, float r1, int segs, bool up = true);
  // Rampe: Quader, dessen Oberseite von h0 (bei z=mn.z) nach h1 (bei z=mx.z) ansteigt
  void Wedge(Vec3 mn, Vec3 mx, float h0, float h1);

 private:
  u32 AddVertex(Vec3 p, Vec3 n);
  MeshData* out_;
  Mat4 xf_ = Mat4::Identity();
  Mat4 nxf_ = Mat4::Identity();
  bool identity_ = true;
  std::vector<Mat4> stack_;
  u8 color_[4] = {255, 255, 255, 255};
  u8 material_ = MAT_PLAIN;
  u8 emissive_ = 0;
  u8 sky_ = 255;
};

// GPU-Mesh
class Mesh {
 public:
  void Upload(const MeshData& d, bool dynamic = false);
  void Destroy();
  void Draw() const;
  void DrawInstanced(int count) const;
  bool Valid() const { return indexCount_ > 0; }
  GLuint Vao() const { return vao_; }
  int IndexCount() const { return indexCount_; }

 private:
  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  int indexCount_ = 0;
  size_t vboCap_ = 0, iboCap_ = 0;
};
