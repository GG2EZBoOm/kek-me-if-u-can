#pragma once
// Statische Spielwelt: Kollisionskörper, platzierte Props, Lichter, Zonen, Raumgitter für Abfragen.
#include "engine/Mesh.h"
#include "engine/Renderer.h"
#include "game/PropCatalog.h"

enum ColFlags : u8 {
  COL_PLAYER = 1,  // blockiert Spieler
  COL_BULLET = 2,  // blockiert Schüsse
  COL_GLASS = 4,   // Glas (Treffer-Effekt)
  COL_PROP = 8,    // gehört zu einem platzierten Prop
  COL_SOLID = COL_PLAYER | COL_BULLET,
};

// Um die Y-Achse gedrehter Quader
struct OBox {
  Vec3 c, h;
  float yaw = 0, cs = 1, sn = 0;
  OBox() = default;
  OBox(Vec3 center, Vec3 half, float y = 0) : c(center), h(half) { SetYaw(y); }
  void SetYaw(float y) { yaw = y; cs = cosf(y); sn = sinf(y); }
  Vec3 ToLocal(Vec3 p) const {
    Vec3 d = p - c;
    return {cs * d.x - sn * d.z, d.y, sn * d.x + cs * d.z};
  }
  Vec3 DirToLocal(Vec3 d) const { return {cs * d.x - sn * d.z, d.y, sn * d.x + cs * d.z}; }
  Vec3 DirToWorld(Vec3 d) const { return {cs * d.x + sn * d.z, d.y, -sn * d.x + cs * d.z}; }
  Vec3 AxisX() const { return {cs, 0, -sn}; }
  Vec3 AxisZ() const { return {sn, 0, cs}; }
  void Bounds(Vec3& mn, Vec3& mx) const {
    float ex = fabsf(cs) * h.x + fabsf(sn) * h.z;
    float ez = fabsf(sn) * h.x + fabsf(cs) * h.z;
    mn = c - Vec3(ex, h.y, ez);
    mx = c + Vec3(ex, h.y, ez);
  }
};

bool RayAABB(Vec3 o, Vec3 d, Vec3 mn, Vec3 mx, float maxT, float& t, Vec3& n);
bool RayOBox(Vec3 o, Vec3 d, const OBox& b, float maxT, float& t, Vec3& n);
// Trennachsentest; mtv schiebt a aus b heraus
bool OverlapOBox(const OBox& a, const OBox& b, Vec3* mtv);

struct Collider {
  OBox box;
  u8 flags = COL_SOLID;
  u8 surface = MAT_PLAIN;
  i32 prop = -1;
};

struct WorldProp {
  int type = 0;
  Vec3 pos;
  float yaw = 0;
  float sky = 1;
  int collider = -1;
};

struct Zone {
  Vec3 mn, mx;
  float sky = 1;
  std::string name;
  int priority = 0;
  std::vector<Vec4> planes;  // optionale Schräge (z. B. Zeltdach): Punkt muss n·p + d <= 0 erfüllen
  bool Contains(Vec3 p) const;
};

// Türöffnung (Mitte auf Bodenhöhe, in der Wandmitte); alongX: Wand verläuft entlang X, Durchgang entlang Z
struct DoorInfo {
  Vec3 center;
  bool alongX = true;
  float width = 1, depth = 0.3f;
};

// Bewegtes Deko-Teil (Fahrgeschäfte). Mesh in Weltkoordinaten; dreht sich um pivot.
// orbit: nur die Lage kreist um pivot, das Teil selbst bleibt aufrecht (Riesenrad-Gondeln).
struct AnimatedPart {
  MeshData mesh;
  Vec3 pivot;
  int axis = 1;        // 0 = X, 1 = Y, 2 = Z
  float speed = 0.3f;  // rad/s
  float phase = 0;
  bool orbit = false;
  Vec3 offset;         // bei orbit: Aufhängepunkt relativ zu pivot (Startlage)
  Mat4 Transform(float t) const;
};

// Fahne, die im Wind weht (wird jedes Bild neu verformt). attach = obere Ecke am Mast,
// die Fahne zeigt entlang yaw (lokal +X), wind = Stärke (innen klein).
enum FlagPattern : u8 { FLAG_GERMANY = 0, FLAG_EUROPE, FLAG_BAVARIA, FLAG_BLUEWHITE };
struct WindFlag {
  Vec3 attach;
  float yaw = 0;
  float width = 2, height = 1.2f;
  u8 pattern = FLAG_GERMANY;
  float wind = 1;
  float phase = 0;
};

struct SpawnPoint {
  Vec3 pos;
  float yaw = 0;
};

struct RayHit {
  bool hit = false;
  float t = 0;
  Vec3 pos, normal;
  int collider = -1;
  int prop = -1;
  u8 surface = 0;
  u8 flags = 0;
};

class World {
 public:
  std::string id, name, description;
  Environment env;
  std::vector<Collider> cols;
  std::vector<WorldProp> props;
  std::vector<PointLight> lights;
  std::vector<Zone> zones;
  std::vector<SpawnPoint> hunterSpawns, propSpawns;
  std::vector<DoorInfo> doors;
  std::vector<AnimatedPart> anims;
  std::vector<WindFlag> flags;
  std::vector<Vec3> cameraPath;  // Kamerafahrt fürs Menü
  MeshData opaque, glass;
  Vec3 bmin, bmax;
  float killY = -40;

  void Clear();
  int AddCollider(const OBox& b, u8 flags, u8 surface, int prop = -1);
  int AddProp(int type, Vec3 pos, float yaw);
  void Finalize();

  float SkyAt(Vec3 p) const;
  const Zone* ZoneAt(Vec3 p) const;
  // Kollisionskörper, deren AABB die Box schneidet
  void Query(Vec3 mn, Vec3 mx, std::vector<int>& out) const;
  // Strahltest. mask: welche Flags treffen. exactProps: Props über Einzelteile testen.
  RayHit Raycast(Vec3 o, Vec3 dir, float maxDist, u8 mask, bool exactProps = true) const;
  bool LineOfSight(Vec3 a, Vec3 b) const;
  // Strahl gegen ein Prop (Einzelteile) in Weltkoordinaten
  static bool RayProp(Vec3 o, Vec3 d, int type, Vec3 pos, float yaw, float maxT, float& t, Vec3& n);

 private:
  void Insert(int idx);
  bool TestCollider(int idx, Vec3 o, Vec3 d, float maxT, bool exactProps, float& t, Vec3& n) const;
  float cell_ = 2.0f;
  int gw_ = 0, gh_ = 0;
  Vec3 gmin_;
  std::vector<std::vector<int>> grid_;
  mutable std::vector<u32> stamp_;
  mutable u32 stampId_ = 0;
};
