// Karte "Oktoberfest" – Festzelt auf der Theresienwiese: Bierbänke, Musikpodium, Empore mit Boxen, Ausschank,
// Hendlküche, Biergarten, Wirtsbudenstraße und Fahrgeschäfte (Riesenrad, Kettenflieger, Autoscooter, Kinderkarussell).
#include "game/Maps.h"
#include "game/MapKit.h"
#include "game/Shapes.h"

namespace {

const float TX = 28.0f;     // Zelt: halbe Länge (Wandmitte)
const float TZ = 17.0f;     // halbe Breite
const float WT = 0.15f;     // halbe Wanddicke
const float WALL_H = 6.0f;  // Traufhöhe der Wände
const float EAVE_Y = 5.85f, EAVE_Z = 17.5f, RIDGE = 12.0f;
const float SLOPE = (RIDGE - EAVE_Y) / EAVE_Z;
const float GAL = 3.0f;     // Emporenboden
const float GAL_Z = 11.0f;  // Emporenkante
const float DOOR_H = 2.4f;
const float IN_SKY = 0.2f;

const u32 WOOD = 0x8a5e3a, WOOD_L = 0xb88a58, WOOD_D = 0x5a3a22, CANVAS = 0xf2ede2, BLUE = 0x2a6ebb, WHITE = 0xf4f4f0;

float RoofY(float z) { return RIDGE - SLOPE * fabsf(z); }

Mat4 Basis(Vec3 x, Vec3 y, Vec3 z, Vec3 t) {
  Mat4 m = Mat4::Identity();
  m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
  m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
  m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
  m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z;
  return m;
}

// Dünner Quader zwischen zwei Punkten (Seile, Ketten, Streben)
void Beam(MeshBuilder& b, Vec3 a, Vec3 c, float r) {
  Vec3 d = c - a;
  float len = Length(d);
  if (len < 1e-4f) return;
  Vec3 y = d / len;
  Vec3 ref = fabsf(y.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
  Vec3 x = Normalize(Cross(y, ref));
  Vec3 z = Cross(x, y);
  b.Push(Basis(x, y, z, a));
  b.Box(Vec3(-r, 0, -r), Vec3(r, len, r));
  b.Pop();
}

// Dreieck bzw. Viereck, das zur Seite n zeigt (Rückseiten werden weggeschnitten)
void TriN(MeshBuilder& b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 n) {
  if (Dot(Cross(p1 - p0, p2 - p0), n) < 0) std::swap(p1, p2);
  b.Tri(p0, p1, p2);
}
void QuadF(MeshBuilder& b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n) {
  if (Dot(Cross(p2 - p0, p3 - p1), n) < 0) std::swap(p1, p3);
  b.Quad(p0, p1, p2, p3);
}

Vec3 Sag(Vec3 a, Vec3 b, float sag, float t) { return Lerp(a, b, t) - Vec3(0, sag * 4 * t * (1 - t), 0); }

struct StallDef {
  float x;
  const char* sign;
  const char* zone;
  u32 awning;
  int kind;
};

struct Span {
  float a, b;
};

class Builder {
 public:
  explicit Builder(World& w) : w(w), k(w), rng(20261023) {}
  World& w;
  MapKit k;
  Rng rng;

  void Build() {
    SetupEnvironment();
    Ground();
    // Wände zuerst: die Türen werden registriert und halten Props fern
    TentShell();
    Gallery();
    Facade();
    Stage();
    BarAndKitchen();
    Roof();
    TentDecoration();
    Booths();
    HallTables();
    GalleryTables();
    Biergarten();
    Street();
    Stalls();
    FerrisWheel(Vec3(-36, 0, 68));
    ChainSwing(Vec3(25, 0, 66));
    Autoscooter();
    KidsCarousel(Vec3(47, 0, 62));
    RideArea();
    Yard();
    Boundary();
    Backdrop();
    Zones();
    Spawns();
  }

  // ------------------------------------------------------------------------------------------
  void SetupEnvironment() {
    auto& e = w.env;
    e.sunDir = Normalize(Vec3(-0.75f, 0.17f, 0.3f));
    e.sunColor = Vec3(2.7f, 1.45f, 0.75f);
    e.skyZenith = Hex(0x2a3a6e);
    e.skyHorizon = Hex(0xf2a060);
    e.groundColor = Hex(0x5a4e44);
    e.skyAmbient = Vec3(0.36f, 0.36f, 0.5f);
    e.groundAmbient = Vec3(0.26f, 0.2f, 0.16f);
    e.indoorAmbient = Vec3(0.27f, 0.21f, 0.16f);
    e.fogColor = Hex(0xd89a72);
    e.fogDensity = 0.0028f;
    e.clouds = 0.35f;
    e.exposure = 1.05f;
  }

  // ------------------------------------------------------------------------------------------
  // Bausteine
  void Garnitur(Vec3 c, float yaw, float stuff) {
    k.PropRel("biertisch", c, yaw, Vec3(0, 0, 0));
    k.PropRel("bierbank", c, yaw, Vec3(0, 0, 0.62f));
    k.PropRel("bierbank", c, yaw, Vec3(0, 0, -0.62f));
    if (!rng.Chance(stuff)) return;
    static const char* items[] = {"masskrug", "masskrug", "masskrug", "masskrug", "hendl",       "radi",
                                  "brezel",   "wiesnbrezn", "trachtenhut", "masstablett", "lebkuchenherz", "mandeln",
                                  "masskrug", "kerze"};
    const int count = (int)(sizeof(items) / sizeof(items[0]));
    int n = rng.Int(1, 3);
    for (int i = 0; i < n; i++) {
      float lx = -0.85f + (i + rng.Range(0.25f, 0.75f)) * (1.7f / n);
      const char* id = items[rng.Int(0, count - 1)];
      if (!strcmp(id, "kerze") && !rng.Chance(0.3f)) id = "masskrug";
      k.PropRel(id, c + Vec3(0, 0.765f, 0), yaw, Vec3(lx, 0, rng.Range(-0.1f, 0.1f)), rng.Range(-PI, PI));
    }
  }

  // Holzbrüstung mit Pfosten, Docken und Handlauf
  // avoid: Stützen auf der Linie – dort entfallen Pfosten und Docken
  void Balustrade(Vec3 a, Vec3 b, float h = 1.0f, const std::vector<Vec3>& avoid = {}) {
    Vec3 d = b - a;
    d.y = 0;
    float len = Length(d);
    if (len < 0.05f) return;
    float yaw = atan2f(d.x, d.z);
    Vec3 c = (a + b) * 0.5f;
    auto dist = [](Vec3 p, Vec3 q) { return Length(Vec3(p.x - q.x, 0, p.z - q.z)); };
    auto nearAvoid = [&](Vec3 p, float r) {
      for (const Vec3& q : avoid)
        if (dist(p, q) < r) return true;
      return false;
    };
    k.Style(MAT_WOOD, WOOD_D);
    int n = std::max(1, (int)(len / 1.5f));
    std::vector<Vec3> posts;
    for (int i = 0; i <= n; i++) {
      Vec3 p = Lerp(a, b, (float)i / n);
      if (nearAvoid(p, 0.2f)) continue;
      posts.push_back(p);
      k.Visual(p + Vec3(-0.05f, 0, -0.05f), p + Vec3(0.05f, h, 0.05f));
    }
    k.Style(MAT_WOOD, WOOD_L);
    int m = std::max(1, (int)(len / 0.22f));
    for (int i = 1; i < m; i++) {
      Vec3 p = Lerp(a, b, (float)i / m);
      // Docken nicht dicht an Pfosten oder Stützen – fast gleiche Flächen flackern
      bool clash = nearAvoid(p, 0.17f);
      for (const Vec3& q : posts) clash |= dist(p, q) < 0.085f;
      if (clash) continue;
      k.Visual(p + Vec3(-0.022f, 0.09f, -0.022f), p + Vec3(0.022f, h - 0.06f, 0.022f));
    }
    k.Style(MAT_WOOD, WOOD_D);
    k.VisualR(c + Vec3(0, h - 0.03f, 0), Vec3(0.055f, 0.035f, len * 0.5f), yaw);
    k.VisualR(c + Vec3(0, 0.07f, 0), Vec3(0.035f, 0.03f, len * 0.5f), yaw);
    k.ClipR(c + Vec3(0, h * 0.5f, 0), Vec3(0.06f, h * 0.5f, len * 0.5f), yaw, COL_PLAYER);
  }

  // Niedriger Holzzaun
  void LowFence(Vec3 a, Vec3 b, float h = 0.95f) {
    Vec3 d = b - a;
    float len = Length(d);
    if (len < 0.05f) return;
    float yaw = atan2f(d.x, d.z);
    Vec3 c = (a + b) * 0.5f;
    k.Style(MAT_WOOD, 0x6b4a2e);
    int n = std::max(1, (int)(len / 1.8f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = Lerp(a, b, (float)i / n);
      k.Visual(p + Vec3(-0.06f, 0, -0.06f), p + Vec3(0.06f, h + 0.05f, 0.06f));
    }
    k.Style(MAT_WOOD, 0x8a6440);
    k.VisualR(c + Vec3(0, h - 0.08f, 0), Vec3(0.03f, 0.06f, len * 0.5f), yaw);
    k.VisualR(c + Vec3(0, h * 0.45f, 0), Vec3(0.03f, 0.06f, len * 0.5f), yaw);
    k.ClipR(c + Vec3(0, h * 0.5f, 0), Vec3(0.07f, h * 0.5f, len * 0.5f), yaw, COL_PLAYER);
  }

  // Bauzaun an der Kartengrenze
  void SiteFence(Vec3 a, Vec3 b) {
    Vec3 d = b - a;
    float len = Length(d);
    float yaw = atan2f(d.x, d.z);
    int n = std::max(1, (int)(len / 3.5f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = Lerp(a, b, (float)i / n);
      k.Style(MAT_CONCRETE, 0x9a968e);
      k.VisualR(p + Vec3(0, 0.08f, 0), Vec3(0.18f, 0.08f, 0.35f), yaw);
      k.Style(MAT_METAL, 0xa8adb2);
      k.Visual(p + Vec3(-0.03f, 0, -0.03f), p + Vec3(0.03f, 2.0f, 0.03f));
    }
    Vec3 c = (a + b) * 0.5f;
    k.Style(MAT_METAL, 0xa8adb2);
    for (float y : {0.25f, 1.0f, 1.95f}) k.VisualR(c + Vec3(0, y, 0), Vec3(0.02f, 0.02f, len * 0.5f), yaw);
    k.Style(MAT_METAL, 0x8d9399);
    int m = std::max(1, (int)(len / 0.25f));
    for (int i = 0; i < m; i++) {
      float t = (i + 0.5f) / m;
      if (fabsf(t - roundf(t * n) / n) * len < 0.05f) continue;  // nicht am Pfosten (fast gleiche Flächen flackern)
      Vec3 p = Lerp(a, b, t);
      k.Visual(p + Vec3(-0.008f, 0.25f, -0.008f), p + Vec3(0.008f, 1.95f, 0.008f));
    }
  }

  // Zylinderförmige Plattform-Kollision aus sternförmig gedrehten Quadern
  void DiscClip(Vec3 c, float r, float h, u8 flags = COL_SOLID) {
    for (int i = 0; i < 6; i++)
      k.ClipR(c + Vec3(0, h * 0.5f, 0), Vec3(r * 0.966f, h * 0.5f, r * 0.259f), i * PI / 6, flags);
  }

  void RingFence(Vec3 c, float r, float gapA, float gapHalf) {
    const int n = 36;
    for (int i = 0; i < n; i++) {
      float a0 = TAU * i / n, a1 = TAU * (i + 1) / n, am = (a0 + a1) * 0.5f;
      if (fabsf(WrapAngle(am - gapA)) < gapHalf) continue;
      LowFence(c + Vec3(cosf(a0) * r, 0, sinf(a0) * r), c + Vec3(cosf(a1) * r, 0, sinf(a1) * r));
    }
  }

  void Bulb(MeshBuilder& b, Vec3 p, u32 col, float r = 0.06f) {
    b.Mat(MAT_PLASTIC);
    b.ColorHex(col);
    b.Emissive(1.0f);
    b.Sphere(p, r, 6, 4);
    b.Emissive(0);
  }

  // Lichterkette als Kettenlinie
  void BulbString(Vec3 a, Vec3 b, float sag, float spacing = 0.55f) {
    static const u32 cols[] = {0xffd890, 0xfff2d8, 0xff6a4a, 0xffd890, 0x7ad0ff, 0xfff2d8, 0x9aff8a};
    MeshBuilder& m = k.ob;
    int n = std::max(3, (int)(Length(b - a) / spacing));
    m.Mat(MAT_PLASTIC);
    m.ColorHex(0x222222);
    for (int i = 0; i < n; i++) Beam(m, Sag(a, b, sag, (float)i / n), Sag(a, b, sag, (float)(i + 1) / n), 0.01f);
    for (int i = 1; i < n; i++) Bulb(m, Sag(a, b, sag, (float)i / n) - Vec3(0, 0.06f, 0), cols[i % 7], 0.055f);
  }

  // Wimpelkette (blau-weiß)
  void Pennants(Vec3 a, Vec3 b, float sag) {
    MeshBuilder& m = k.ob;
    int n = std::max(4, (int)(Length(b - a) / 0.45f));
    m.Mat(MAT_FABRIC);
    m.ColorHex(0xdddddd);
    for (int i = 0; i < n; i++) Beam(m, Sag(a, b, sag, (float)i / n), Sag(a, b, sag, (float)(i + 1) / n), 0.008f);
    Vec3 side = Normalize(Cross(b - a, Vec3(0, 1, 0)));
    for (int i = 0; i < n; i++) {
      Vec3 p0 = Sag(a, b, sag, (i + 0.1f) / n), p1 = Sag(a, b, sag, (i + 0.9f) / n);
      Vec3 tip = (p0 + p1) * 0.5f - Vec3(0, 0.32f, 0);
      m.ColorHex(i % 2 ? WHITE : BLUE);
      TriN(m, p0, p1, tip, side);
      TriN(m, p0, p1, tip, side * -1.0f);
    }
  }

  // Hopfengirlande
  void Garland(Vec3 a, Vec3 b, float sag) {
    MeshBuilder& m = k.ob;
    m.Mat(MAT_LEAVES);
    int n = std::max(4, (int)(Length(b - a) / 0.24f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = Sag(a, b, sag, (float)i / n);
      m.ColorHex(i % 3 ? 0x3f7a2c : 0x86a850);
      m.Push(Translation(p) * RotationY(i * 0.9f));  // verdreht: sonst liegen Facetten der Nachbarn in einer Ebene
      m.Ellipsoid(Vec3(0, 0, 0), Vec3(0.13f, 0.1f, 0.13f), 6, 4);
      m.Pop();
    }
  }

  // Gestreifte Markise von (zBack, yBack) schräg nach vorn zu (zFront, yFront)
  void Awning(float x0, float x1, float zBack, float yBack, float zFront, float yFront, u32 col) {
    MeshBuilder& m = k.ob;
    m.Mat(MAT_FABRIC);
    Vec3 up = Normalize(Vec3(0, -(zFront - zBack), yFront - yBack));
    if (up.y < 0) up = up * -1.0f;
    int n = std::max(2, (int)((x1 - x0) / 0.5f));
    float sw = (x1 - x0) / n;
    for (int i = 0; i < n; i++) {
      float xa = x0 + i * sw, xb = xa + sw;
      m.ColorHex(i % 2 ? WHITE : col);
      Vec3 p0(xa, yBack, zBack), p1(xb, yBack, zBack), p2(xb, yFront, zFront), p3(xa, yFront, zFront);
      QuadF(m, p0, p1, p2, p3, up);
      QuadF(m, p0, p1, p2, p3, up * -1.0f);
      // Volant
      float zf = zFront + (zFront < zBack ? -0.01f : 0.01f);
      m.Box(Vec3(xa, yFront - 0.22f, std::min(zf, zFront)), Vec3(xb, yFront, std::max(zf, zFront)));
    }
  }

  void Mast(Vec3 p, float h = 6.6f) {
    k.Style(MAT_METAL, 0x2d3236);
    k.ob.Cylinder(p, 0.12f, 0.5f, 10);
    k.ob.Cylinder(p, 0.08f, h, 8);
    k.Style(MAT_GOLD, 0xc9a24a);
    k.ob.Sphere(p + Vec3(0, h + 0.1f, 0), 0.12f, 8, 5);
    k.Clip(p - Vec3(0.13f, 0, 0.13f), p + Vec3(0.13f, h, 0.13f), COL_SOLID);
  }

  void VisualTree(Vec3 p, float s) {
    MeshBuilder& b = k.ob;
    b.Mat(MAT_WOOD);
    b.ColorHex(0x5a4330);
    b.Frustum(p, 0.3f * s, 0.18f * s, 3.4f * s, 8);
    b.Mat(MAT_LEAVES);
    b.ColorHex(0x45702a);
    b.Ellipsoid(p + Vec3(0, 4.8f * s, 0), Vec3(2.6f, 2.2f, 2.6f) * s, 12, 7);
    b.ColorHex(0x527d30);
    b.Ellipsoid(p + Vec3(1.0f, 4.1f, 0.6f) * s, Vec3(1.6f, 1.4f, 1.6f) * s, 10, 6);
    b.Ellipsoid(p + Vec3(-0.9f, 5.6f, -0.4f) * s, Vec3(1.5f, 1.3f, 1.5f) * s, 10, 6);
  }

  // ------------------------------------------------------------------------------------------
  void Ground() {
    k.Clip(Vec3(-64, -3, -34), Vec3(64, 0, 88), COL_SOLID);
    k.FloorFinish(-64, -34, 64, 88, 0, MAT_ASPHALT, 0x57534e);
    k.FloorFinish(-28, TZ + WT, 28, 27.7f, 0.006f, MAT_CONCRETE, 0xbba98a);    // Kies im Biergarten
    k.FloorFinish(-42, -32, 42, -TZ - WT, 0.006f, MAT_CONCRETE, 0x8f8b84);    // Hinterhof
    k.FloorFinish(-62, 27.7f, 62, 28.0f, 0.008f, MAT_CONCRETE, 0xa8a49c);     // Bordstein
    k.FloorFinish(-62, 40.3f, 62, 40.6f, 0.008f, MAT_CONCRETE, 0xa8a49c);
  }

  // ------------------------------------------------------------------------------------------
  void TentShell() {
    k.FloorFinish(-TX, -TZ, TX, TZ, 0.012f, MAT_PARQUET, 0x8c6844);
    // Längswände (z = ±TZ): unten Holz, oben Plane mit Fenstern
    for (int s = -1; s <= 1; s += 2) {
      float z0 = s * TZ - WT, z1 = s * TZ + WT;
      std::vector<Hole> lower, upper;
      lower.push_back({-19.0f, -17.0f, 0, DOOR_H});
      lower.push_back({17.0f, 19.0f, 0, DOOR_H});
      if (s > 0) {
        lower.push_back({-3.0f, 3.0f, 0, 4.5f});  // Haupteingang
        upper.push_back({-3.0f, 3.0f, 0, 4.5f, true});
      } else {
        lower.push_back({-2.0f, 2.0f, 0, DOOR_H});  // Hintereingang zum Hof
      }
      for (float x = -25.5f; x < 25.6f; x += 3.0f) {
        if (s > 0 && fabsf(x) < 4.5f) continue;
        upper.push_back({x - 0.8f, x + 0.8f, 3.9f, 5.3f, true});
      }
      k.Style(MAT_WOOD, WOOD);
      k.WallX(-TX - WT, TX + WT, z0, z1, 0, 2.4f, lower);
      k.Style(MAT_FABRIC, CANVAS);
      k.WallX(-TX - WT, TX + WT, z0, z1, 2.4f, WALL_H, upper);
      for (const Hole& h : upper) {
        if (h.y0 < 1) continue;
        k.Glass(Vec3(h.a0, h.y0, s * TZ - 0.02f), Vec3(h.a1, h.y1, s * TZ + 0.02f), 0xffe2b8, 0.3f);
        k.Style(MAT_WOOD, WOOD_D);
        float mx = (h.a0 + h.a1) * 0.5f, my = (h.y0 + h.y1) * 0.5f;
        k.Visual(Vec3(mx - 0.03f, h.y0, z0 - 0.02f), Vec3(mx + 0.03f, h.y1, z1 + 0.02f));
        k.Visual(Vec3(h.a0, my - 0.03f, z0 - 0.02f), Vec3(h.a1, my + 0.03f, z1 + 0.02f));
        k.Visual(Vec3(h.a0 - 0.06f, h.y0 - 0.06f, z0 - 0.03f), Vec3(h.a1 + 0.06f, h.y0 + 0.02f, z1 + 0.03f));  // Fensterbank 2 cm über der Wandkrone
      }
      // Zierleiste zwischen Holz und Plane, Abschlussbrett am Wandkopf
      k.Style(MAT_WOOD, WOOD_D);
      k.Visual(Vec3(-TX, WALL_H - 0.15f, std::min(z0 - 0.02f, s * (TZ - WT - 0.06f))),
               Vec3(TX, WALL_H + 0.18f, std::max(z1 + 0.02f, s * (TZ - WT - 0.06f))));  // 2 cm vor der Wandfläche
      if (s > 0) {
        k.Visual(Vec3(-TX - WT - 0.02f, 2.35f, z0 - 0.03f), Vec3(-2.98f, 2.5f, z1 + 0.03f));
        k.Visual(Vec3(2.98f, 2.35f, z0 - 0.03f), Vec3(TX + WT + 0.02f, 2.5f, z1 + 0.03f));
      } else {
        k.Visual(Vec3(-TX - WT - 0.02f, 2.35f, z0 - 0.03f), Vec3(TX + WT + 0.02f, 2.5f, z1 + 0.03f));
      }
    }
    // Stirnwände (x = ±TX) mit Giebel
    for (int s = -1; s <= 1; s += 2) {
      float x0 = s * TX - WT, x1 = s * TX + WT;
      if (x0 > x1) std::swap(x0, x1);
      float dz = s * 13.75f;
      k.Style(MAT_WOOD, WOOD);
      k.WallZ(-TZ + WT, TZ - WT, x0, x1, 0, 2.4f, {{dz - 0.75f, dz + 0.75f, 0, DOOR_H}});
      k.Style(MAT_FABRIC, CANVAS);
      k.WallZ(-TZ + WT, TZ - WT, x0, x1, 2.4f, WALL_H);
      k.Style(MAT_WOOD, WOOD_D);
      k.Visual(Vec3(x0 - 0.03f, 2.35f, -TZ), Vec3(x1 + 0.03f, 2.5f, TZ));
      k.Style(MAT_FABRIC, CANVAS);
      Vec3 n((float)s, 0, 0);
      float xo = s * (TX + WT), xi = s * (TX - WT), zg = (RIDGE - WALL_H) / SLOPE;
      TriN(k.ob, Vec3(xo, WALL_H, -zg), Vec3(xo, WALL_H, zg), Vec3(xo, RIDGE, 0), n);
      TriN(k.ob, Vec3(xi, WALL_H, -zg), Vec3(xi, WALL_H, zg), Vec3(xi, RIDGE, 0), n * -1.0f);
      // Blau-weiße Rauten im Giebel (außen)
      for (int r = 0; r < 4; r++)
        for (int c = -r; c <= r; c++) {
          float y = RIDGE - 1.6f - r * 1.1f, z = c * 1.1f;
          k.ob.Push(Translation(Vec3(xo + s * 0.02f, y, z)) * RotationX(PI * 0.25f));
          k.Style(MAT_FABRIC, (r + c) % 2 ? WHITE : BLUE);
          k.ob.Box(Vec3(-0.02f, -0.36f, -0.36f), Vec3(0.02f, 0.36f, 0.36f));
          k.ob.Pop();
        }
    }
  }

  // ------------------------------------------------------------------------------------------
  void Roof() {
    MeshBuilder& b = k.ob;
    const float xi = TX - WT, xo = TX + 0.6f;
    for (int s = -1; s <= 1; s += 2) {
      Vec3 nOut = Normalize(Vec3(0, 1, SLOPE * s)), nIn = nOut * -1.0f;
      // Innen: blau-weißer Himmel in Bahnen parallel zum First
      b.Mat(MAT_FABRIC);
      const int bands = 12;
      const float zw = TZ - WT;
      for (int j = 0; j < bands; j++) {
        float za = s * zw * (1 - (float)j / bands), zb = s * zw * (1 - (float)(j + 1) / bands);
        b.ColorHex(j % 2 ? 0xf4f2ea : 0x8eb4de);
        QuadF(b, Vec3(-xi, RoofY(za), za), Vec3(xi, RoofY(za), za), Vec3(xi, RoofY(zb), zb), Vec3(-xi, RoofY(zb), zb), nIn);
      }
      // Dachüberstand von unten
      b.ColorHex(0x8eb4de);
      float ze = s * EAVE_Z, zw2 = s * (TZ + WT);
      QuadF(b, Vec3(-xo, EAVE_Y, ze), Vec3(xo, EAVE_Y, ze), Vec3(xo, RoofY(zw2), zw2), Vec3(-xo, RoofY(zw2), zw2), nIn);
      for (int sx = -1; sx <= 1; sx += 2) {
        float x0 = sx * (TX + WT), x1 = sx * xo;
        QuadF(b, Vec3(x0, EAVE_Y, ze), Vec3(x1, EAVE_Y, ze), Vec3(x1, RIDGE, 0), Vec3(x0, RIDGE, 0), nIn);
      }
      // Außen: Rautenmuster
      const int cx = 26, cu = 6;
      for (int i = 0; i < cx; i++)
        for (int j = 0; j < cu; j++) {
          float x0 = -xo + 2 * xo * i / cx, x1 = -xo + 2 * xo * (i + 1) / cx;
          float za = s * EAVE_Z * (1 - (float)j / cu), zb = s * EAVE_Z * (1 - (float)(j + 1) / cu);
          float ya = EAVE_Y + (RIDGE - EAVE_Y) * (float)j / cu + 0.06f, yb = EAVE_Y + (RIDGE - EAVE_Y) * (float)(j + 1) / cu + 0.06f;
          b.ColorHex((i + j) % 2 ? WHITE : BLUE);
          QuadF(b, Vec3(x0, ya, za), Vec3(x1, ya, za), Vec3(x1, yb, zb), Vec3(x0, yb, zb), nOut);
        }
      // Traufbrett
      k.Style(MAT_WOOD, WOOD_D);
      k.Visual(Vec3(-xo, EAVE_Y - 0.12f, s * EAVE_Z - 0.06f), Vec3(xo, EAVE_Y + 0.1f, s * EAVE_Z + 0.06f));
      for (int sx = -1; sx <= 1; sx += 2) Beam(b, Vec3(sx * xo, EAVE_Y, s * EAVE_Z), Vec3(sx * xo, RIDGE + 0.06f, 0), 0.07f);
    }
    // First mit Fahnen
    k.Style(MAT_WOOD, WOOD_D);
    k.Visual(Vec3(-xo, RIDGE - 0.05f, -0.12f), Vec3(xo, RIDGE + 0.18f, 0.12f));
    for (float x = -24; x <= 24.1f; x += 8) {
      k.Style(MAT_METAL, 0xd8d8d8);
      k.ob.Cylinder(Vec3(x, RIDGE, 0), 0.05f, 2.8f, 8);
      k.ob.Sphere(Vec3(x, RIDGE + 2.85f, 0), 0.08f, 6, 4);
      WindFlag f;
      f.attach = Vec3(x + 0.06f, RIDGE + 2.7f, 0);
      f.width = 1.6f;
      f.height = 0.85f;
      f.pattern = FLAG_BLUEWHITE;
      f.phase = x * 0.53f;
      w.flags.push_back(f);
    }
  }

  // ------------------------------------------------------------------------------------------
  void Gallery() {
    const float pillars[] = {-24.6f, -21.0f, -15.0f, -10.0f, -4.0f, 0.0f, 4.0f, 10.0f, 15.0f, 21.0f, 24.6f};
    for (int s = -1; s <= 1; s += 2) {
      float za = s > 0 ? GAL_Z : -(TZ - WT), zb = s > 0 ? TZ - WT : -GAL_Z;
      auto slab = [&](float x0, float x1) { k.Slab(x0, za, x1, zb, GAL, 0.25f, MAT_PARQUET, 0x7e5a3a, 0x5e4430); };
      if (s > 0) {
        slab(-TX + WT, -3.5f);
        slab(3.5f, TX - WT);
      } else {
        slab(-TX + WT, TX - WT);
      }
      // Stützen (unten und oben bis zum Dach)
      k.Style(MAT_WOOD, WOOD);
      for (float x : pillars) {
        if (s > 0 && fabsf(x) < 1) continue;
        float zc = s * GAL_Z;
        k.Solid(Vec3(x - 0.14f, 0, zc - 0.14f), Vec3(x + 0.14f, GAL - 0.45f, zc + 0.14f));  // Kappe sitzt obenauf
        k.Solid(Vec3(x - 0.12f, GAL, zc - 0.12f), Vec3(x + 0.12f, RoofY(zc), zc + 0.12f));
        k.Style(MAT_WOOD, WOOD_D);
        k.Visual(Vec3(x - 0.2f, GAL - 0.45f, zc - 0.2f), Vec3(x + 0.2f, GAL - 0.25f, zc + 0.2f));
        k.Style(MAT_WOOD, WOOD);
      }
      // Treppen an beiden Enden
      k.Style(MAT_WOOD, WOOD_L);
      for (int sx = -1; sx <= 1; sx += 2) k.Stairs(Vec3(sx * 26.9f, 0, s * 6.5f), s > 0 ? 2 : 3, 1.8f, 0.2f, 15, 0.3f, 0);
      // Brüstung
      float zr = s * GAL_Z;
      std::vector<Vec3> pil;
      for (float x : pillars) pil.push_back(Vec3(x, GAL, zr));
      if (s > 0) {
        Balustrade(Vec3(-26.0f, GAL, zr), Vec3(-3.5f, GAL, zr), 1.0f, pil);
        Balustrade(Vec3(3.5f, GAL, zr), Vec3(26.0f, GAL, zr), 1.0f, pil);
        for (float x : {-3.5f, 3.5f}) Balustrade(Vec3(x, GAL, GAL_Z), Vec3(x, GAL, TZ - WT));
      } else {
        Balustrade(Vec3(-26.0f, GAL, zr), Vec3(26.0f, GAL, zr), 1.0f, pil);
      }
      // Hopfengirlanden unter der Brüstung
      for (int i = 0; i + 1 < (int)(sizeof(pillars) / sizeof(pillars[0])); i++) {
        float xa = pillars[i], xb = pillars[i + 1];
        if (s > 0 && (fabsf(xa) < 1 || fabsf(xb) < 1)) continue;
        Garland(Vec3(xa, GAL - 0.35f, zr - s * 0.18f), Vec3(xb, GAL - 0.35f, zr - s * 0.18f), 0.45f);
      }
    }
  }

  // ------------------------------------------------------------------------------------------
  void Facade() {
    // Schaufassade über dem Haupteingang
    const float z0 = TZ + WT, z1 = TZ + WT + 0.3f;
    k.Style(MAT_WOOD, 0xe8dcc4);
    k.Solid(Vec3(-8.5f, 0, z0), Vec3(-3.0f, 7.5f, z1));
    k.Solid(Vec3(3.0f, 0, z0), Vec3(8.5f, 7.5f, z1));
    k.Solid(Vec3(-3.0f, 4.5f, z0), Vec3(3.0f, 7.5f, z1));
    ExtrudeConvex(k.ob, {{-8.5f, 7.5f}, {8.5f, 7.5f}, {5.0f, 10.4f}, {-5.0f, 10.4f}}, z0, z1);
    ExtrudeConvex(k.ob, {{-5.0f, 10.4f}, {5.0f, 10.4f}, {0.0f, 12.6f}}, z0, z1);
    // Holzsockel und Türrahmen
    k.Style(MAT_WOOD, WOOD_D);
    k.Visual(Vec3(-8.6f, 0, z1), Vec3(8.6f, 0.9f, z1 + 0.04f));
    k.Visual(Vec3(-3.25f, 0, z0 - 0.05f), Vec3(-2.98f, 4.7f, z1 + 0.06f));  // 2 cm in die Öffnung (sonst flackert die Laibung)
    k.Visual(Vec3(2.98f, 0, z0 - 0.05f), Vec3(3.25f, 4.7f, z1 + 0.06f));
    k.Visual(Vec3(-3.25f, 4.48f, z0 - 0.05f), Vec3(3.25f, 4.75f, z1 + 0.06f));
    // Rauten im Giebel
    for (int r = 0; r < 3; r++)
      for (int c = -3 + r; c <= 3 - r; c++) {
        Vec3 p(c * 1.2f, 8.2f + r * 1.0f, z1 + 0.02f);
        k.ob.Push(Translation(p) * RotationZ(PI * 0.25f));
        k.Style(MAT_FABRIC, (r + c) % 2 ? WHITE : BLUE);
        k.ob.Box(Vec3(-0.34f, -0.34f, 0), Vec3(0.34f, 0.34f, 0.03f));  // Diagonale < 1 m Reihenabstand
        k.ob.Pop();
      }
    // oberhalb des Traufbretts (bis 5,95 m)
    k.Sign("FESTZELT", Vec3(0, 6.45f, z1 + 0.01f), 0, 0.11f, 0xf2c230, 0.8f);
    k.Sign("ZUR WIESN", Vec3(-4.6f, 6.3f, z1 + 0.01f), 0, 0.045f, 0xffffff, 0.4f, false);
    k.Sign("O ZAPFT IS!", Vec3(4.6f, 6.3f, z1 + 0.01f), 0, 0.04f, 0xffffff, 0.4f, false);
    // Fahnen auf der Fassade
    for (float x : {-8.3f, 0.0f, 8.3f}) {
      float y0 = x == 0 ? 12.6f : 7.5f;
      k.Style(MAT_METAL, 0xd8d8d8);
      k.ob.Cylinder(Vec3(x, y0, z0 + 0.15f), 0.05f, 3.0f, 8);
      WindFlag f;
      f.attach = Vec3(x + 0.06f, y0 + 2.9f, z0 + 0.15f);
      f.width = 1.5f;
      f.height = 0.95f;
      f.pattern = FLAG_BAVARIA;
      f.phase = x * 0.71f + 1.0f;
      w.flags.push_back(f);
    }
    // Laternen am Eingang
    for (float x : {-3.8f, 3.8f}) {
      k.Style(MAT_METAL, 0x2a2c30);
      k.Visual(Vec3(x - 0.05f, 3.6f, z1), Vec3(x + 0.05f, 3.7f, z1 + 0.4f));
      Bulb(k.ob, Vec3(x, 3.45f, z1 + 0.35f), 0xffd890, 0.14f);
    }
    k.Light(Vec3(0, 4.2f, z1 + 2.0f), 11, Vec3(2.4f, 1.6f, 0.8f));
    BulbString(Vec3(-8.5f, 7.5f, z1 + 0.05f), Vec3(8.5f, 7.5f, z1 + 0.05f), 0.5f, 0.4f);
  }

  // ------------------------------------------------------------------------------------------
  void Stage() {
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(-4, 0, -3.5f), Vec3(4, 1.0f, 3.5f));
    k.FloorFinish(-4, -3.5f, 4, 3.5f, 1.006f, MAT_PARQUET, 0xa47a4c);
    // Blau-weiße Schürze
    for (int s = -1; s <= 1; s += 2) {
      for (int i = 0; i < 16; i++) {
        float x0 = -4 + i * 0.5f;
        k.Style(MAT_FABRIC, i % 2 ? WHITE : BLUE);
        float z = s * 3.5f;
        k.Visual(Vec3(x0, 0.12f, std::min(z, z + s * 0.03f)), Vec3(x0 + 0.5f, 0.9f, std::max(z, z + s * 0.03f)));
      }
      for (int i = 0; i < 14; i++) {
        float z0 = -3.5f + i * 0.5f;
        if (z0 + 0.5f > -0.8f && z0 < 0.8f) continue;
        k.Style(MAT_FABRIC, i % 2 ? WHITE : BLUE);
        float x = s * 4.0f;
        k.Visual(Vec3(std::min(x, x + s * 0.03f), 0.12f, z0), Vec3(std::max(x, x + s * 0.03f), 0.9f, z0 + 0.5f));
      }
    }
    k.Style(MAT_WOOD, WOOD_L);
    k.Stairs(Vec3(5.5f, 0, 0), 1, 1.6f, 0.2f, 5, 0.3f, 0);
    k.Stairs(Vec3(-5.5f, 0, 0), 0, 1.6f, 0.2f, 5, 0.3f, 0);
    // Geländer
    Balustrade(Vec3(-4, 1, 3.45f), Vec3(4, 1, 3.45f), 0.9f);
    Balustrade(Vec3(-4, 1, -3.45f), Vec3(4, 1, -3.45f), 0.9f);
    for (int s = -1; s <= 1; s += 2) {
      Balustrade(Vec3(s * 3.95f, 1, -3.45f), Vec3(s * 3.95f, 1, -0.8f), 0.9f);
      Balustrade(Vec3(s * 3.95f, 1, 0.8f), Vec3(s * 3.95f, 1, 3.45f), 0.9f);
    }
    // Baldachin
    k.Style(MAT_WOOD, WOOD_D);
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        k.Solid(Vec3(sx * 3.7f - 0.1f, 1, sz * 3.2f - 0.1f), Vec3(sx * 3.7f + 0.1f, 5.2f, sz * 3.2f + 0.1f));
    k.Style(MAT_WOOD, WOOD);
    k.Visual(Vec3(-4.3f, 5.2f, -3.8f), Vec3(4.3f, 5.5f, 3.8f));
    for (int i = 0; i < 4; i++) {
      // Zeltspitze in Blau-Weiß (vier Dreiecke)
      Vec3 apex(0, 7.0f, 0);
      Vec3 c[4] = {{-4.3f, 5.5f, -3.8f}, {4.3f, 5.5f, -3.8f}, {4.3f, 5.5f, 3.8f}, {-4.3f, 5.5f, 3.8f}};
      Vec3 a = c[i], b = c[(i + 1) % 4];
      int stripes = 6;
      for (int j = 0; j < stripes; j++) {
        Vec3 p0 = Lerp(a, b, (float)j / stripes), p1 = Lerp(a, b, (float)(j + 1) / stripes);
        Vec3 mid = (a + b) * 0.5f;
        Vec3 n = Normalize(Vec3(mid.x, 0, mid.z) * 0.25f + Vec3(0, 1, 0));
        k.Style(MAT_FABRIC, j % 2 ? WHITE : BLUE);
        TriN(k.ob, p0, p1, apex, n);
      }
    }
    k.Sign("O ZAPFT IS!", Vec3(0, 5.35f, 3.8f), 0, 0.04f, 0xf2c230, 0.7f, false);
    k.Sign("O ZAPFT IS!", Vec3(0, 5.35f, -3.8f), PI, 0.04f, 0xf2c230, 0.7f, false);
    // Scheinwerfer unter dem Baldachin
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2) {
        k.Style(MAT_METAL, 0x1d1e21);
        k.Visual(Vec3(sx * 2.0f - 0.12f, 5.0f, sz * 1.8f - 0.12f), Vec3(sx * 2.0f + 0.12f, 5.2f, sz * 1.8f + 0.12f));
        k.Style(MAT_PLASTIC, 0xfff0d0);
        k.Glow(1.0f);
        k.Visual(Vec3(sx * 2.0f - 0.08f, 4.98f, sz * 1.8f - 0.08f), Vec3(sx * 2.0f + 0.08f, 5.0f, sz * 1.8f + 0.08f));
        k.Glow(0);
      }
    k.Light(Vec3(0, 4.5f, 0), 9, Vec3(2.6f, 2.0f, 1.4f));
    // Blaskapelle
    const float y = 1.0f;
    k.Prop("pauke", Vec3(0, y, -2.5f), 0);
    k.Prop("tuba", Vec3(-1.7f, y, -2.3f), 0.3f);
    k.Prop("tuba", Vec3(1.8f, y, -2.2f), -0.4f);
    k.Prop("bierbank", Vec3(0, y, -1.35f), 0);
    k.Prop("bierbank", Vec3(0, y, 1.35f), PI);
    for (int i = 0; i < 5; i++) {
      float x = -2.0f + i * 1.0f;
      k.Prop("notenstaender", Vec3(x, y, -0.45f), PI + (x * 0.08f));
    }
    for (int i = 0; i < 3; i++) k.Prop("notenstaender", Vec3(-1.0f + i * 1.0f, y, 0.45f), 0.1f * (i - 1));
    k.Prop("lautsprecher", Vec3(-3.3f, y, 2.55f), -0.6f);
    k.Prop("lautsprecher", Vec3(3.3f, y, -2.55f), PI - 0.6f);
    k.Prop("masskrug", Vec3(-0.6f, y + 0.47f, -1.35f), 0.4f);
    k.Prop("masskrug", Vec3(1.1f, y + 0.47f, -1.3f), 2.0f);
    k.Prop("trachtenhut", Vec3(0.4f, y + 0.47f, 1.35f), 1.0f);
    k.Prop("masskrug", Vec3(2.8f, y, 2.4f), 0.0f);
  }

  // ------------------------------------------------------------------------------------------
  void BarAndKitchen() {
    // Ausschank (Osten)
    k.Style(MAT_WOOD, WOOD_D);
    k.Solid(Vec3(24.8f, 0, -4), Vec3(25.4f, 1.05f, 4));
    k.Style(MAT_WOOD, WOOD_L);
    k.Solid(Vec3(24.7f, 1.05f, -4.05f), Vec3(25.5f, 1.12f, 4.05f));
    k.Style(MAT_WOOD, 0x7a5232);
    for (float z = -3.5f; z <= 3.6f; z += 1.0f) k.Visual(Vec3(24.76f, 0.1f, z - 0.4f), Vec3(24.8f, 0.95f, z + 0.4f));
    // Fassregal an der Stirnwand
    k.Style(MAT_WOOD, WOOD);
    k.Solid(Vec3(26.9f, 0, -3.6f), Vec3(TX - WT, 0.9f, 3.6f));
    for (int i = 0; i < 5; i++) k.Prop("holzfass", Vec3(27.32f, 0.9f, -2.8f + i * 1.4f), PI);
    // Zapfhähne
    for (float z = -3.0f; z <= 3.1f; z += 2.0f) {
      k.Style(MAT_GOLD, 0xd8b24a);
      k.ob.Cylinder(Vec3(25.15f, 1.12f, z), 0.05f, 0.4f, 10);
      k.ob.CylinderX(Vec3(25.0f, 1.45f, z), 0.025f, 0.3f, 8);
      k.Style(MAT_PLASTIC, 0x1a1a1a);
      k.ob.Cylinder(Vec3(25.15f, 1.52f, z), 0.02f, 0.18f, 6);
    }
    for (int i = 0; i < 6; i++) k.Prop("masskrug", Vec3(25.05f, 1.12f, -2.5f + i * 1.0f + 0.25f), rng.Range(-PI, PI));
    k.Prop("masstablett", Vec3(25.1f, 1.12f, 3.4f), 0.3f);
    k.Prop("fass", Vec3(26.35f, 0, -3.3f), 0.4f);
    k.Prop("fass", Vec3(26.35f, 0, 3.3f), 1.2f);
    k.Sign("AUSSCHANK", Vec3(TX - WT - 0.01f, 4.2f, 0), -PI * 0.5f, 0.1f, 0xf2c230, 0.7f);
    k.Light(Vec3(25.8f, 3.0f, 0), 8, Vec3(2.3f, 1.55f, 0.8f));

    // Hendlküche (Westen)
    k.Style(MAT_METAL, 0xb8bcc2);
    k.Solid(Vec3(-25.4f, 0, -4), Vec3(-24.8f, 1.0f, 4));
    k.Style(MAT_METAL, 0xdadde2);
    k.Solid(Vec3(-25.5f, 1.0f, -4.05f), Vec3(-24.7f, 1.05f, 4.05f));
    k.Style(MAT_METAL, 0x2a2c30);
    k.Solid(Vec3(-TX + WT, 0, -3.6f), Vec3(-26.9f, 0.95f, 3.6f));
    k.Style(MAT_PLASTIC, 0xff5a1a);
    k.Glow(0.9f);
    k.Visual(Vec3(-27.75f, 0.95f, -3.5f), Vec3(-27.0f, 0.97f, 3.5f));
    k.Glow(0);
    // Drehspieße mit Hendln
    for (int r = 0; r < 3; r++) {
      float y = 1.25f + r * 0.32f;
      k.Style(MAT_METAL, 0x9aa0a8);
      k.ob.CylinderZ(Vec3(-27.4f, y, 0), 0.012f, 7.2f, 6);
      k.Style(MAT_PLASTIC, 0xb5651d);
      for (float z = -3.2f; z <= 3.25f; z += 0.4f) k.ob.Ellipsoid(Vec3(-27.4f, y, z), Vec3(0.09f, 0.1f, 0.12f), 8, 5);
    }
    k.Style(MAT_METAL, 0x8d9299);
    k.Visual(Vec3(-TX + WT, 2.4f, -3.8f), Vec3(-26.5f, 2.8f, 3.8f));
    for (int i = 0; i < 6; i++) k.Prop("hendl", Vec3(-25.1f, 1.05f, -3.0f + i * 1.2f), rng.Range(-PI, PI));
    k.Prop("kochtopf", Vec3(-26.2f, 0, -3.3f), 0);
    k.Prop("kochtopf", Vec3(-26.2f, 0, 3.3f), 1);
    k.Sign("HENDL", Vec3(-TX + WT + 0.01f, 4.2f, 0), PI * 0.5f, 0.12f, 0xf2c230, 0.7f);
    k.Light(Vec3(-25.8f, 3.0f, 0), 8, Vec3(2.4f, 1.4f, 0.7f));
    k.Light(Vec3(-27.0f, 1.4f, 0), 3.5f, Vec3(2.2f, 0.8f, 0.25f));
  }

  // ------------------------------------------------------------------------------------------
  void Chandelier(Vec3 c) {
    MeshBuilder& b = k.ob;
    k.Style(MAT_WOOD, WOOD_D);
    b.Torus(c, 1.25f, 0.07f, 28, 6);
    b.Cylinder(c - Vec3(0, 0.12f, 0), 0.18f, 0.24f, 10);
    for (int i = 0; i < 12; i++) {
      float a = TAU * i / 12;
      Beam(b, c, c + Vec3(cosf(a) * 1.25f, 0, sinf(a) * 1.25f), 0.03f);
    }
    k.Style(MAT_FABRIC, 0x6b5a40);
    float top = RoofY(c.z);
    for (int i = 0; i < 3; i++) {
      float a = TAU * i / 3 + 0.3f;
      Beam(b, c + Vec3(cosf(a) * 1.2f, 0, sinf(a) * 1.2f), Vec3(c.x, top, c.z), 0.015f);
    }
    for (int i = 0; i < 12; i++) {
      float a = TAU * (i + 0.5f) / 12;
      Vec3 p = c + Vec3(cosf(a) * 1.25f, 0.07f, sinf(a) * 1.25f);
      k.Style(MAT_PLASTIC, 0xf4f0e2);
      b.Cylinder(p, 0.03f, 0.14f, 6);
      Bulb(b, p + Vec3(0, 0.19f, 0), 0xffd890, 0.045f);
    }
    k.Light(c - Vec3(0, 0.3f, 0), 13, Vec3(2.6f, 1.75f, 0.95f));
  }

  void TentDecoration() {
    for (float x : {-19.0f, -9.5f, 9.5f, 19.0f})
      for (float z : {-4.6f, 4.6f}) Chandelier(Vec3(x, 6.6f, z));
    for (float x : {-24.0f, -14.25f, -4.75f, 4.75f, 14.25f, 24.0f}) Pennants(Vec3(x, 7.6f, -GAL_Z), Vec3(x, 7.6f, GAL_Z), 1.0f);
    for (float z : {-4.6f, 4.6f}) {
      const float xs[] = {-TX + WT, -19.0f, -9.5f, 9.5f, 19.0f, TX - WT};
      for (int i = 0; i < 5; i++) {
        if (i == 2) continue;  // über dem Podium frei lassen
        Garland(Vec3(xs[i], 7.3f, z), Vec3(xs[i + 1], 7.3f, z), 0.6f);
      }
    }
    // Girlanden entlang der Wandoberkante
    for (int s = -1; s <= 1; s += 2) Garland(Vec3(-TX + 0.3f, WALL_H - 0.2f, s * (TZ - 0.3f)), Vec3(TX - 0.3f, WALL_H - 0.2f, s * (TZ - 0.3f)), 0.0f);
  }

  // ------------------------------------------------------------------------------------------
  void Booths() {
    const Span segs[] = {{-25.8f, -19.5f}, {-16.5f, -3.5f}, {3.5f, 16.5f}, {19.5f, 25.8f}};
    for (int s = -1; s <= 1; s += 2) {
      for (const Span& sg : segs) {
        float len = sg.b - sg.a;
        int n = std::max(1, (int)(len / 3.05f));
        float bw = len / n;
        float zA = s * 12.6f, zB = s * (TZ - WT);
        for (int i = 0; i <= n; i++) {
          float x = sg.a + i * bw;
          k.Style(MAT_WOOD, WOOD_D);
          k.Solid(Vec3(x - 0.04f, 0, std::min(zA, zB)), Vec3(x + 0.04f, 1.3f, std::max(zA, zB)));
          k.Visual(Vec3(x - 0.07f, 0, zA - 0.07f), Vec3(x + 0.07f, 1.45f, zA + 0.07f));
          k.Style(MAT_WOOD, WOOD_L);
          k.Visual(Vec3(x - 0.06f, 1.3f, std::min(zA, zB)), Vec3(x + 0.06f, 1.35f, std::max(zA, zB)));
        }
        for (int i = 0; i < n; i++) {
          float cx = sg.a + (i + 0.5f) * bw;
          Garnitur(Vec3(cx, 0, s * 14.5f), 0, 0.85f);
          k.Style(MAT_PLASTIC, 0xfff0d0);
          k.Glow(1.0f);
          k.Visual(Vec3(cx - 0.18f, GAL - 0.36f, s * 14.5f - 0.18f), Vec3(cx + 0.18f, GAL - 0.25f, s * 14.5f + 0.18f), FACE_ALL & ~FACE_PY);
          k.Glow(0);
          k.Light(Vec3(cx, GAL - 0.6f, s * 14.5f), 4.5f, Vec3(1.7f, 1.1f, 0.6f));
        }
      }
    }
  }

  void HallTables() {
    const float inner[] = {7.6f, 9.9f, 13.8f, 16.1f, 20.0f, 22.3f};
    const float outer[] = {4.4f, 7.6f, 9.9f, 13.8f, 16.1f, 20.0f, 22.3f};
    for (int s = -1; s <= 1; s += 2) {
      for (float zr : {1.15f, 3.45f, -1.15f, -3.45f})
        for (float x : inner) Garnitur(Vec3(s * x, 0, zr), 0, 0.75f);
      for (float zr : {6.9f, 9.2f, -6.9f, -9.2f})
        for (float x : outer) Garnitur(Vec3(s * x, 0, zr), 0, 0.75f);
    }
  }

  void GalleryTables() {
    const float xs[] = {4.9f, 7.2f, 10.6f, 12.9f, 16.3f, 18.6f, 22.0f, 24.3f};
    for (int s = -1; s <= 1; s += 2) {
      for (int sx = -1; sx <= 1; sx += 2)
        for (float x : xs) Garnitur(Vec3(sx * x, GAL, s * 14.4f), 0, 0.6f);
      if (s < 0) {
        Garnitur(Vec3(-1.15f, GAL, -14.4f), 0, 0.6f);
        Garnitur(Vec3(1.15f, GAL, -14.4f), 0, 0.6f);
      }
      for (float x = -22.0f; x <= 22.1f; x += 5.5f) {
        if (s > 0 && fabsf(x + 1.0f) < 4.2f) continue;
        k.Prop("lampion", Vec3(x + 1.0f, GAL, s * 11.45f), 0);
      }
      k.Prop("fass", Vec3(-25.0f, GAL, s * 16.2f), 0);
      k.Prop("strohballen", Vec3(25.0f, GAL, s * 16.3f), 0);
    }
  }

  // ------------------------------------------------------------------------------------------
  void Maibaum(Vec3 p) {
    const float H = 26;
    MeshBuilder& b = k.ob;
    for (int i = 0; i < 26; i++) {
      b.Mat(MAT_PLASTIC);
      b.ColorHex(i % 2 ? WHITE : BLUE);
      b.Frustum(p + Vec3(0, (float)i, 0), 0.32f - i * 0.008f, 0.32f - (i + 1) * 0.008f, 1.0f, 12, i == 0 || i == 25);
    }
    k.Clip(p - Vec3(0.35f, 0, 0.35f), p + Vec3(0.35f, H, 0.35f), COL_SOLID);
    k.Style(MAT_LEAVES, 0x2f6a2a);
    b.Push(Translation(p + Vec3(0, H - 2.0f, 0)));
    b.Torus(Vec3(0), 0.9f, 0.18f, 20, 6);
    b.Pop();
    for (int i = 0; i < 4; i++) Beam(b, p + Vec3(cosf(i * PI * 0.5f) * 0.9f, H - 2.0f, sinf(i * PI * 0.5f) * 0.9f), p + Vec3(0, H - 0.6f, 0), 0.03f);
    // Zunftzeichen an Querbalken
    const u32 cols[] = {0xd8202a, 0xf2c230, 0x2fa04a, 0x8a5a2e};
    for (int lv = 0; lv < 4; lv++) {
      float y = 8.0f + lv * 4.0f;
      float a = lv * PI * 0.5f;
      Vec3 d(cosf(a), 0, sinf(a));
      k.Style(MAT_WOOD, WOOD_D);
      Beam(b, p + Vec3(0, y, 0) - d * 1.8f, p + Vec3(0, y, 0) + d * 1.8f, 0.05f);
      for (int sd = -1; sd <= 1; sd += 2) {
        Vec3 c = p + Vec3(0, y - 0.7f, 0) + d * (sd * 1.55f);
        Vec3 t = Cross(d, Vec3(0, 1, 0));
        k.Style(MAT_WOOD, 0xf0e6d0);
        k.ob.BoxR(c, Vec3(0.45f, 0.55f, 0.03f), atan2f(t.x, t.z) + PI * 0.5f);
        k.Style(MAT_PLASTIC, cols[(lv + sd + 1) % 4]);
        k.ob.BoxR(c, Vec3(0.25f, 0.3f, 0.04f), atan2f(t.x, t.z) + PI * 0.5f);
      }
    }
    k.Style(MAT_METAL, 0xc9a24a);
    b.Sphere(p + Vec3(0, H + 0.2f, 0), 0.25f, 10, 6);
    WindFlag f;
    f.attach = p + Vec3(0.2f, H - 0.3f, 0);
    f.width = 2.4f;
    f.height = 1.3f;
    f.pattern = FLAG_BAVARIA;
    f.phase = 2.0f;
    w.flags.push_back(f);
  }

  void Biergarten() {
    const float blocks[3][2] = {{5.1f, 7.5f}, {11.1f, 13.5f}, {21.5f, 23.9f}};
    for (int s = -1; s <= 1; s += 2)
      for (float zr : {19.6f, 22.9f, 26.2f})
        for (auto& bl : blocks)
          for (int i = 0; i < 2; i++) Garnitur(Vec3(s * bl[i], 0, zr), 0, 0.55f);
    for (int s = -1; s <= 1; s += 2)
      for (float zr : {21.25f, 24.55f}) {
        for (float x : {6.3f, 12.3f, 22.7f}) k.Prop("schirm_bw", Vec3(s * x, 0, zr), rng.Range(0, PI));
        k.Prop("baum", Vec3(s * 17.5f, 0, zr), zr * s);
      }
    const Span fence[] = {{-27.5f, -20.4f}, {-14.6f, -3.2f}, {3.2f, 14.6f}, {20.4f, 27.5f}};
    for (const Span& f : fence) LowFence(Vec3(f.a, 0, 27.7f), Vec3(f.b, 0, 27.7f));
    for (float x : {-3.2f, 3.2f}) {
      k.Prop("kuebel", Vec3(x * 1.15f, 0, 28.6f), 0);
    }
    Maibaum(Vec3(-34, 0, 22));
    // Lichterketten zwischen den Kastanien
    for (int s = -1; s <= 1; s += 2) {
      BulbString(Vec3(s * 3.4f, 4.2f, 18.0f), Vec3(s * 17.5f, 4.6f, 21.25f), 0.6f);
      BulbString(Vec3(s * 17.5f, 4.6f, 21.25f), Vec3(s * 17.5f, 4.6f, 24.55f), 0.3f);
      BulbString(Vec3(s * 17.5f, 4.6f, 24.55f), Vec3(s * 27.0f, 4.2f, 26.5f), 0.6f);
      k.Light(Vec3(s * 10.0f, 3.6f, 21.0f), 10, Vec3(2.0f, 1.4f, 0.8f));
      k.Light(Vec3(s * 21.0f, 3.6f, 25.0f), 9, Vec3(2.0f, 1.4f, 0.8f));
    }
  }

  // ------------------------------------------------------------------------------------------
  void Street() {
    for (float x = -54; x <= 54.1f; x += 12) {
      Mast(Vec3(x, 0, 28.3f));
      Mast(Vec3(x, 0, 38.8f));
      BulbString(Vec3(x, 6.4f, 28.3f), Vec3(x + 6, 6.4f, 38.8f), 0.9f);
      if (x + 12 <= 54.1f) BulbString(Vec3(x + 6, 6.4f, 38.8f), Vec3(x + 12, 6.4f, 28.3f), 0.9f);
      k.Light(Vec3(x + 3, 5.4f, 34), 12, Vec3(2.2f, 1.5f, 0.85f));
    }
    // Lichterketten längs der Straße
    for (float z : {28.3f, 38.8f})
      for (float x = -54; x < 54; x += 12) BulbString(Vec3(x, 6.2f, z), Vec3(x + 12, 6.2f, z), 0.6f);
    BreweryWagon(Vec3(-49, 0, 33.6f));
    // Bänke und Mülltonnen
    for (float x : {-30.0f, -8.0f, 24.0f, 46.0f}) {
      k.Prop("bierbank", Vec3(x, 0, 28.9f), 0);
      k.Prop("abfalleimer", Vec3(x + 1.6f, 0, 28.9f), 0);
    }
    k.Prop("strohballen", Vec3(-38.5f, 0, 29.1f), 0.1f);
    k.Prop("strohballen", Vec3(-38.4f, 0.45f, 29.1f), -0.05f);
    k.Prop("gartenzwerg", Vec3(-37.6f, 0.9f, 29.1f), 0.3f);
    k.Prop("lebkuchenherz", Vec3(-39.2f, 0.9f, 29.0f), PI);
  }

  void BreweryWagon(Vec3 c) {
    MeshBuilder& b = k.ob;
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(c + Vec3(-2.6f, 0.7f, -1.1f), c + Vec3(2.6f, 1.05f, 1.1f));
    k.Clip(c + Vec3(-2.4f, 0, -1.0f), c + Vec3(2.4f, 0.7f, 1.0f), COL_SOLID);
    k.Style(MAT_WOOD, 0x2a6ebb);
    k.Visual(c + Vec3(-2.6f, 1.05f, -1.1f), c + Vec3(2.6f, 1.35f, -1.04f));
    k.Visual(c + Vec3(-2.6f, 1.05f, 1.04f), c + Vec3(2.6f, 1.35f, 1.1f));
    k.Sign("WIESNBIER", c + Vec3(0, 0.88f, 1.11f), 0, 0.035f, 0xf2c230, 0.2f, false);
    k.Sign("WIESNBIER", c + Vec3(0, 0.88f, -1.11f), PI, 0.035f, 0xf2c230, 0.2f, false);
    for (float x : {-1.8f, 1.8f})
      for (float z : {-1.2f, 1.2f}) {
        k.Style(MAT_WOOD, 0x8a1c1c);
        b.CylinderZ(c + Vec3(x, 0.6f, z), 0.6f, 0.1f, 18);
        k.Style(MAT_METAL, 0x2a2c30);
        b.CylinderZ(c + Vec3(x, 0.6f, z), 0.62f, 0.06f, 18);
        k.Style(MAT_WOOD, 0xd8b24a);
        b.CylinderZ(c + Vec3(x, 0.6f, z), 0.12f, 0.16f, 10);
      }
    k.Style(MAT_WOOD, 0x6b4a2e);
    Beam(b, c + Vec3(2.6f, 0.8f, 0), c + Vec3(5.4f, 0.6f, 0), 0.06f);
    for (int i = 0; i < 4; i++) k.Prop("holzfass", c + Vec3(-1.8f + i * 1.2f, 1.05f, 0), PI * 0.5f);
  }

  // ------------------------------------------------------------------------------------------
  void Stalls() {
    const StallDef defs[] = {
        {-50, "LEBKUCHEN", "Lebkuchenstand", 0xc8202a, 0},  {-42, "SCHIESSBUDE", "Schießbude", 0x2a6ebb, 1},
        {-28, "HENDL", "Hendlbraterei", 0xd89a20, 2},       {-20, "MANDELN", "Mandelstand", 0x8a3a8a, 3},
        {-12, "STECKERLFISCH", "Fischbraterei", 0x2a8a4a, 4}, {8, "SOUVENIRS", "Souvenirstand", 0x2a6ebb, 5},
        {16, "ZUCKERWATTE", "Zuckerwattestand", 0xe060a8, 6}, {34, "BREZN", "Brezenstand", 0xc87a2a, 7},
        {42, "LOSE", "Losbude", 0xd8202a, 8},
    };
    for (const StallDef& d : defs) Stall(d);
  }

  void Stall(const StallDef& d) {
    const float x0 = d.x - 2.5f, x1 = d.x + 2.5f, zf = 40.6f, zb = 44.6f, H = 2.8f;
    k.FloorFinish(x0, zf, x1, zb, 0.02f, MAT_WOOD, 0x7a5a3a);
    k.Style(MAT_WOOD, 0x8a6a48);
    k.WallX(x0, x1, zb - 0.15f, zb, 0, H);
    k.WallZ(zf, zb - 0.15f, x0, x0 + 0.15f, 0, H);
    k.WallZ(zf, zb - 0.15f, x1 - 0.15f, x1, 0, H);
    // Theke mit Durchgang links
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(x0 + 1.05f, 0, zf), Vec3(x1 - 0.15f, 1.0f, zf + 0.5f));
    k.Style(MAT_WOOD, 0xc8a070);
    k.Solid(Vec3(x0 + 1.0f, 1.0f, zf - 0.05f), Vec3(x1 - 0.1f, 1.06f, zf + 0.55f));
    k.Style(MAT_FABRIC, d.awning);
    k.Visual(Vec3(x0 + 1.05f, 0.15f, zf - 0.02f), Vec3(x1 - 0.15f, 0.85f, zf));
    // Dach, Markise, Schild
    k.Style(MAT_WOOD, 0x5a3a22);
    k.Visual(Vec3(x0 - 0.1f, H, zf - 0.1f), Vec3(x1 + 0.1f, H + 0.15f, zb + 0.1f));
    k.Clip(Vec3(x0 - 0.1f, H, zf - 0.1f), Vec3(x1 + 0.1f, H + 0.15f, zb + 0.1f), COL_BULLET);
    Awning(x0 - 0.1f, x1 + 0.1f, zf - 0.1f, H + 0.05f, zf - 1.3f, H - 0.5f, d.awning);
    float px = std::min(0.075f, 4.4f / (6.0f * (float)strlen(d.sign)));
    k.Style(MAT_WOOD, 0x5a3a22);
    k.Visual(Vec3(d.x - 0.05f, H + 0.15f, zf - 0.05f), Vec3(d.x + 0.05f, H + 0.4f, zf + 0.05f));
    k.Sign(d.sign, Vec3(d.x, H + 0.72f, zf - 0.08f), 0, px, 0xfff2c0, 0.85f);
    for (int i = 0; i <= 12; i++) Bulb(k.ob, Vec3(x0 + i * 5.0f / 12, H - 0.75f, zf - 1.32f), i % 2 ? 0xffd890 : 0xff7a5a, 0.05f);
    k.Light(Vec3(d.x, 2.3f, 42.6f), 6, Vec3(2.0f, 1.5f, 0.9f));
    // Regale hinten
    k.Style(MAT_WOOD, 0x9a7a52);
    k.Solid(Vec3(x0 + 0.15f, 0, 43.85f), Vec3(x1 - 0.15f, 0.9f, zb - 0.15f));
    k.Visual(Vec3(x0 + 0.15f, 1.6f, 44.05f), Vec3(x1 - 0.15f, 1.65f, zb - 0.15f));
    StallGoods(d);
    k.Zone(Vec3(x0, -0.5f, zf), Vec3(x1, H, zb), 0.5f, d.zone, 2);
  }

  void StallGoods(const StallDef& d) {
    const float cy = 1.06f, sy = 0.9f, by = 1.65f, cz = 40.85f, sz = 44.15f, bz = 44.25f;
    auto counter = [&](const char* id, int n) {
      for (int i = 0; i < n; i++) k.Prop(id, Vec3(d.x - 1.2f + (i + 0.5f) * (3.5f / n), cy, cz), PI + rng.Range(-0.25f, 0.25f));
    };
    auto shelf = [&](const char* id, int n, float y, float z) {
      for (int i = 0; i < n; i++) k.Prop(id, Vec3(d.x - 2.0f + (i + 0.5f) * (4.0f / n), y, z), PI + rng.Range(-0.2f, 0.2f));
    };
    MeshBuilder& b = k.ob;
    switch (d.kind) {
      case 0:
        counter("lebkuchenherz", 5);
        shelf("lebkuchenherz", 6, sy, sz);
        shelf("lebkuchenherz", 5, by, bz);
        break;
      case 1:
        // Zielscheiben an der Rückwand
        for (int i = 0; i < 5; i++) {
          Vec3 c(d.x - 1.8f + i * 0.9f, 2.15f, 44.43f);
          for (int r = 0; r < 4; r++) {
            k.Style(MAT_PLASTIC, r % 2 ? WHITE : 0xd8202a);
            b.CylinderZ(c - Vec3(0, 0, r * 0.006f), 0.3f - r * 0.07f, 0.01f, 18);
          }
        }
        for (int i = 0; i < 3; i++) {
          k.Style(MAT_WOOD, 0x4a2e1a);
          b.BoxR(Vec3(d.x - 0.4f + i * 0.9f, cy + 0.04f, cz), Vec3(0.04f, 0.04f, 0.35f), PI * 0.5f + 0.2f);
          k.Style(MAT_METAL, 0x2a2c30);
          b.BoxR(Vec3(d.x - 0.1f + i * 0.9f, cy + 0.05f, cz - 0.02f), Vec3(0.015f, 0.015f, 0.3f), PI * 0.5f + 0.2f);
        }
        shelf("teddy", 4, sy, sz);
        k.Prop("ballons", Vec3(d.x + 1.95f, 0, 42.3f), 0);
        break;
      case 2:
        counter("hendl", 4);
        k.Style(MAT_PLASTIC, 0xff5a1a);
        k.Glow(0.8f);
        k.Visual(Vec3(d.x - 2.2f, sy, 43.9f), Vec3(d.x + 2.2f, sy + 0.015f, 44.4f));
        k.Glow(0);
        shelf("hendl", 3, sy + 0.02f, sz);
        break;
      case 3:
        counter("mandeln", 6);
        shelf("mandeln", 5, sy, sz);
        k.Style(MAT_GOLD, 0xc8783a);
        b.Ellipsoid(Vec3(d.x + 1.6f, by + 0.15f, bz), Vec3(0.2f, 0.15f, 0.2f), 12, 6);
        break;
      case 4:
        counter("steckerlfisch", 6);
        k.Style(MAT_PLASTIC, 0xff5a1a);
        k.Glow(0.8f);
        k.Visual(Vec3(d.x - 2.2f, sy, 43.9f), Vec3(d.x + 2.2f, sy + 0.015f, 44.4f));
        k.Glow(0);
        shelf("steckerlfisch", 5, sy + 0.02f, sz);
        break;
      case 5:
        counter("trachtenhut", 3);
        k.Prop("trachtenpuppe", Vec3(d.x + 1.6f, 0, 43.3f), PI);
        k.Prop("trachtenpuppe", Vec3(d.x + 0.4f, 0, 43.35f), PI + 0.3f);
        shelf("gartenzwerg", 3, sy, sz);
        shelf("lebkuchenherz", 4, by, bz);
        break;
      case 6:
        counter("zuckerwatte", 5);
        k.Prop("ballons", Vec3(d.x + 1.9f, 0, 41.9f), 0);
        k.Prop("ballons", Vec3(d.x + 1.9f, 0, 43.2f), 1);
        k.Style(MAT_METAL, 0xd8dade);
        b.Cylinder(Vec3(d.x - 1.0f, sy, sz), 0.35f, 0.3f, 18, true);
        break;
      case 7:
        counter("wiesnbrezn", 4);
        shelf("brezel", 6, sy, sz);
        shelf("wiesnbrezn", 3, by, bz);
        break;
      default:
        shelf("teddy", 3, sy, sz);
        k.Prop("ballons", Vec3(d.x + 1.95f, 0, 42.4f), 0);
        counter("gartenzwerg", 2);
        k.Style(MAT_PLASTIC, 0xf2c230);
        b.CylinderX(Vec3(d.x - 0.9f, cy + 0.35f, cz), 0.3f, 0.6f, 12);
        k.Style(MAT_METAL, 0x55595f);
        b.Box(Vec3(d.x - 1.25f, cy, cz - 0.05f), Vec3(d.x - 1.2f, cy + 0.35f, cz + 0.05f));
        b.Box(Vec3(d.x - 0.6f, cy, cz - 0.05f), Vec3(d.x - 0.55f, cy + 0.35f, cz + 0.05f));
        break;
    }
  }

  // ------------------------------------------------------------------------------------------
  void FerrisWheel(Vec3 c) {
    const float R = 14.0f, AX = 16.5f, speed = 0.075f;
    Vec3 P = c + Vec3(0, AX, 0);
    MeshBuilder& sb = k.ob;
    // A-Böcke vor und hinter dem Rad
    k.Style(MAT_METAL, 0xdfe3e8);
    for (float zs : {-1.0f, 1.0f}) {
      for (float sx : {-1.0f, 1.0f}) {
        Vec3 foot = c + Vec3(sx * 7.5f, 0, zs * 3.4f);
        Beam(sb, foot, P + Vec3(0, 0, zs * 2.1f), 0.22f);
        k.Style(MAT_CONCRETE, 0x9a968e);
        k.Solid(foot - Vec3(0.6f, 0, 0.6f), foot + Vec3(0.6f, 0.5f, 0.6f));
        k.Clip(foot - Vec3(0.45f, 0, 0.45f), foot + Vec3(0.45f, 3.0f, 0.45f), COL_SOLID);
        k.Style(MAT_METAL, 0xdfe3e8);
      }
      for (float hy : {5.0f, 10.5f}) {
        float half = 7.5f * (1 - hy / AX);
        float zz = zs * (3.4f - 1.3f * hy / AX);
        Beam(sb, c + Vec3(-half, hy, zz), c + Vec3(half, hy, zz), 0.12f);
      }
    }
    k.Style(MAT_METAL, 0x8a9098);
    sb.CylinderZ(P, 0.35f, 4.8f, 14);
    // Plattform, Kasse, Zaun, Portal
    k.Style(MAT_WOOD, 0x8a6440);
    k.Solid(Vec3(c.x - 4.5f, 0, c.z - 2.4f), Vec3(c.x + 4.5f, 0.2f, c.z + 2.4f));
    k.Style(MAT_WOOD, 0xd8202a);
    k.Solid(c + Vec3(2.8f, 0, -4.4f), c + Vec3(4.4f, 2.4f, -3.0f));
    k.Style(MAT_WOOD, WHITE);
    k.Visual(c + Vec3(2.7f, 2.4f, -4.5f), c + Vec3(4.5f, 2.6f, -2.9f));
    k.Sign("KASSE", c + Vec3(3.6f, 2.0f, -4.41f), PI, 0.04f, 0xffffff, 0.5f, false);
    for (int sx = -1; sx <= 1; sx += 2) {
      LowFence(c + Vec3(sx * 2.2f, 0, -4.6f), c + Vec3(sx * 9.5f, 0, -4.6f));
      LowFence(c + Vec3(sx * 9.5f, 0, -4.6f), c + Vec3(sx * 9.5f, 0, 4.6f));
    }
    LowFence(c + Vec3(-9.5f, 0, 4.6f), c + Vec3(9.5f, 0, 4.6f));
    k.Style(MAT_METAL, 0xd8b24a);
    for (int sx = -1; sx <= 1; sx += 2) k.Solid(c + Vec3(sx * 2.2f - 0.1f, 0, -4.7f), c + Vec3(sx * 2.2f + 0.1f, 4.0f, -4.5f));
    k.Style(MAT_PLASTIC, 0x1d2430);
    k.Visual(c + Vec3(-2.4f, 3.6f, -4.72f), c + Vec3(2.4f, 4.4f, -4.48f));
    k.Sign("RIESENRAD", c + Vec3(0, 4.0f, -4.73f), PI, 0.07f, 0xffd24a, 1.0f, false);
    k.Light(P, 22, Vec3(2.2f, 1.5f, 1.1f));
    k.Light(c + Vec3(0, 3.5f, -5.5f), 9, Vec3(2.2f, 1.6f, 1.0f));

    // Rad (dreht sich um Z)
    AnimatedPart wheel;
    wheel.pivot = P;
    wheel.axis = 2;
    wheel.speed = speed;
    {
      MeshBuilder b(&wheel.mesh);
      b.Mat(MAT_METAL);
      b.ColorHex(0xeceff2);
      for (float zo : {-1.1f, 1.1f}) {
        b.Push(Translation(P + Vec3(0, 0, zo)) * RotationX(PI * 0.5f));
        b.Torus(Vec3(0), R, 0.14f, 72, 6);
        b.Torus(Vec3(0), R * 0.6f, 0.08f, 56, 6);
        b.Pop();
        for (int i = 0; i < 16; i++) {
          float a = TAU * i / 16, a2 = TAU * (i + 0.5f) / 16;
          Vec3 hub = P + Vec3(0, 0, zo);
          Beam(b, hub + Vec3(cosf(a), sinf(a), 0) * 0.8f, hub + Vec3(cosf(a), sinf(a), 0) * R, 0.06f);
          Beam(b, hub + Vec3(cosf(a), sinf(a), 0) * (R * 0.6f), hub + Vec3(cosf(a2), sinf(a2), 0) * R, 0.04f);
        }
      }
      for (int i = 0; i < 16; i++) {
        float a = TAU * i / 16;
        b.CylinderZ(P + Vec3(cosf(a) * R, sinf(a) * R, 0), 0.07f, 2.4f, 8);
      }
      b.ColorHex(0xc8202a);
      b.CylinderZ(P, 0.9f, 2.6f, 20);
      for (float zo : {-1.25f, 1.25f}) {
        for (int i = 0; i < 96; i++) {
          float a = TAU * i / 96;
          Bulb(b, P + Vec3(cosf(a) * R, sinf(a) * R, zo), i % 2 ? 0xffd27a : 0xff7a5a, 0.09f);
        }
        for (int i = 0; i < 16; i++) {
          float a = TAU * i / 16;
          for (int j = 1; j <= 6; j++) {
            float r = 1.6f + j * 1.95f;
            Bulb(b, P + Vec3(cosf(a) * r, sinf(a) * r, zo * 1.06f), 0xfff2d0, 0.06f);
          }
        }
      }
    }
    w.anims.push_back(std::move(wheel));
    // Gondeln (hängen immer senkrecht)
    const u32 gcols[] = {0xd8202a, 0xf2c230, 0x2a6ebb, 0x2fa04a, 0xe060a8, 0xf08a24};
    for (int i = 0; i < 16; i++) {
      float a = TAU * i / 16;
      AnimatedPart g;
      g.pivot = P;
      g.axis = 2;
      g.speed = speed;
      g.orbit = true;
      g.offset = Vec3(cosf(a) * R, sinf(a) * R, 0);
      MeshBuilder b(&g.mesh);
      Vec3 h = P + g.offset;
      b.Mat(MAT_METAL);
      b.ColorHex(0x55595f);
      b.Box(h + Vec3(-0.05f, -0.6f, -0.05f), h + Vec3(0.05f, 0, 0.05f));
      b.Box(h + Vec3(-0.85f, -0.66f, -0.06f), h + Vec3(0.85f, -0.58f, 0.06f));
      for (int sx = -1; sx <= 1; sx += 2) b.Box(h + Vec3(sx * 0.85f - 0.03f, -0.9f, -0.03f), h + Vec3(sx * 0.85f + 0.03f, -0.6f, 0.03f));
      b.ColorHex(gcols[i % 6]);
      b.Mat(MAT_PLASTIC);
      b.Box(h + Vec3(-0.9f, -1.0f, -0.75f), h + Vec3(0.9f, -0.9f, 0.75f));
      b.Box(h + Vec3(-0.85f, -2.25f, -0.7f), h + Vec3(0.85f, -1.55f, 0.7f));
      for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
          b.Box(h + Vec3(sx * 0.8f - 0.05f, -1.55f, sz * 0.65f - 0.05f), h + Vec3(sx * 0.8f + 0.05f, -1.0f, sz * 0.65f + 0.05f));
      b.ColorHex(0xfff0d0);
      b.Emissive(0.35f);
      b.Box(h + Vec3(-0.75f, -1.55f, -0.6f), h + Vec3(0.75f, -1.05f, 0.6f));
      b.Emissive(0);
      b.ColorHex(WHITE);
      b.Box(h + Vec3(-0.87f, -1.75f, -0.72f), h + Vec3(0.87f, -1.68f, 0.72f));
      w.anims.push_back(std::move(g));
    }
  }

  // ------------------------------------------------------------------------------------------
  void ChainSwing(Vec3 c) {
    const float pr = 5.2f, py = 0.3f, cy = 8.6f;
    k.Style(MAT_WOOD, 0x8a5a3a);
    k.ob.Cylinder(c, pr, py, 36);
    DiscClip(c, pr, py);
    k.Style(MAT_METAL, 0xd8b24a);
    k.ob.Cylinder(c + Vec3(0, py, 0), 0.55f, cy - py, 16);
    k.Clip(c + Vec3(-0.5f, 0, -0.5f), c + Vec3(0.5f, cy, 0.5f), COL_SOLID);
    k.Style(MAT_PLASTIC, BLUE);
    k.ob.Frustum(c + Vec3(0, py, 0), 1.6f, 0.9f, 2.4f, 12);
    DiscClip(c, 1.5f, 2.7f);
    for (int i = 0; i < 12; i++) {
      float a = TAU * (i + 0.5f) / 12;
      k.Style(MAT_PLASTIC, i % 2 ? 0xf2c230 : 0xd8202a);
      k.ob.BoxR(c + Vec3(cosf(a) * 1.28f, 1.5f, sinf(a) * 1.28f), Vec3(0.3f, 0.5f, 0.02f), atan2f(cosf(a), sinf(a)));
    }
    RingFence(c, 8.6f, -PI * 0.5f, 0.2f);
    k.Style(MAT_METAL, 0xd8b24a);
    for (int sx = -1; sx <= 1; sx += 2) k.Solid(c + Vec3(sx * 2.6f - 0.1f, 0, -8.7f), c + Vec3(sx * 2.6f + 0.1f, 4.0f, -8.5f));
    k.Style(MAT_PLASTIC, 0x1d2430);
    k.Visual(c + Vec3(-2.8f, 3.55f, -8.72f), c + Vec3(2.8f, 4.35f, -8.48f));
    k.Sign("KETTENFLIEGER", c + Vec3(0, 3.95f, -8.73f), PI, 0.055f, 0x7ad0ff, 1.0f, false);
    k.Light(c + Vec3(0, cy + 0.5f, 0), 16, Vec3(2.2f, 1.6f, 1.2f));
    k.Light(c + Vec3(0, 3.0f, -9.5f), 7, Vec3(1.2f, 1.8f, 2.4f));

    AnimatedPart top;
    top.pivot = c;
    top.axis = 1;
    top.speed = 1.25f;
    {
      MeshBuilder b(&top.mesh);
      const int seg = 24;
      Vec3 apex = c + Vec3(0, cy + 1.7f, 0);
      for (int i = 0; i < seg; i++) {
        float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
        b.Mat(MAT_FABRIC);
        b.ColorHex(i % 2 ? WHITE : 0xd8202a);
        Vec3 p0 = c + Vec3(cosf(a0) * 4.6f, cy, sinf(a0) * 4.6f), p1 = c + Vec3(cosf(a1) * 4.6f, cy, sinf(a1) * 4.6f);
        TriN(b, apex, p0, p1, Vec3(0, 1, 0));
        TriN(b, apex, p0, p1, Vec3(0, -1, 0));
      }
      b.Mat(MAT_GOLD);
      b.ColorHex(0xd8b24a);
      b.Cylinder(c + Vec3(0, cy - 0.45f, 0), 4.65f, 0.45f, 36, false);
      b.Cylinder(c + Vec3(0, cy - 0.45f, 0), 0.75f, 0.5f, 12);
      for (int i = 0; i < 6; i++) {
        float a = TAU * i / 6;
        Beam(b, c + Vec3(0, cy - 0.2f, 0), c + Vec3(cosf(a) * 4.6f, cy - 0.2f, sinf(a) * 4.6f), 0.05f);
      }
      for (int i = 0; i < 48; i++) {
        float a = TAU * i / 48;
        Bulb(b, c + Vec3(cosf(a) * 4.72f, cy - 0.22f, sinf(a) * 4.72f), i % 3 == 0 ? 0xff6a4a : 0xffe0a0, 0.07f);
      }
      const int n = 20;
      const float tilt = 0.62f, len = 3.4f;
      const u32 seatCols[] = {0xd8202a, 0x2a6ebb, 0xf2c230, 0x2fa04a};
      for (int i = 0; i < n; i++) {
        float a = TAU * (i + 0.5f) / n;
        Vec3 out(cosf(a), 0, sinf(a)), tan(-sinf(a), 0, cosf(a));
        Vec3 T = c + out * 4.3f + Vec3(0, cy - 0.45f, 0);
        Vec3 dir = out * sinf(tilt) - Vec3(0, cosf(tilt), 0);
        Vec3 yAx = dir * -1.0f, zAx = tan, xAx = Cross(yAx, zAx);
        Vec3 O = T + dir * (len + 0.55f);
        b.Push(Basis(xAx, yAx, zAx, O));
        b.Mat(MAT_METAL);
        b.ColorHex(0x9aa0a8);
        for (float sd : {-0.22f, 0.22f}) b.Box(Vec3(sd - 0.008f, 0.5f, -0.008f), Vec3(sd + 0.008f, len + 0.55f, 0.008f));
        b.Mat(MAT_PLASTIC);
        b.ColorHex(seatCols[i % 4]);
        b.Box(Vec3(-0.24f, 0, -0.22f), Vec3(0.24f, 0.06f, 0.22f));
        b.Box(Vec3(-0.24f, 0.06f, -0.25f), Vec3(0.24f, 0.55f, -0.19f));
        b.ColorHex(0x2a2c30);
        b.Box(Vec3(-0.2f, -0.38f, 0.26f), Vec3(0.2f, -0.34f, 0.32f));
        b.Box(Vec3(-0.23f, 0.06f, -0.2f), Vec3(-0.2f, 0.5f, 0.24f));
        b.Box(Vec3(0.2f, 0.06f, -0.2f), Vec3(0.23f, 0.5f, 0.24f));
        b.Pop();
      }
    }
    w.anims.push_back(std::move(top));
    k.Zone(c - Vec3(8.8f, 0.5f, 8.8f), c + Vec3(8.8f, 12, 8.8f), 1.0f, "Kettenflieger", 1);
  }

  // ------------------------------------------------------------------------------------------
  void Autoscooter() {
    const float x0 = -14, x1 = 10, z0 = 57, z1 = 71, fy = 0.25f, RH = 4.8f;
    k.Style(MAT_METAL, 0x34373c);
    k.Solid(Vec3(x0, 0, z0), Vec3(x1, fy, z1));
    k.FloorFinish(x0 + 0.3f, z0 + 0.3f, x1 - 0.3f, z1 - 0.3f, fy + 0.012f, MAT_TILE, 0x2e3136);
    // Bande mit Durchgängen
    auto bande = [&](Vec3 mn, Vec3 mx) {
      k.Style(MAT_PLASTIC, 0x1d1d20);
      k.Solid(mn, mx);
      k.Style(MAT_PLASTIC, 0xf2c230);
      k.Visual(mn + Vec3(-0.01f, 0.2f, -0.01f), Vec3(mx.x + 0.01f, mn.y + 0.28f, mx.z + 0.01f));
    };
    const float bh = fy + 0.45f, t = 0.3f;
    const Span front[] = {{x0, -8.0f}, {-6.4f, 4.0f}, {5.6f, x1}};
    const Span back[] = {{x0, -6.4f}, {-4.8f, x1}};
    for (const Span& s : front) bande(Vec3(s.a, fy, z0), Vec3(s.b, bh, z0 + t));
    for (const Span& s : back) bande(Vec3(s.a, fy, z1 - t), Vec3(s.b, bh, z1));
    for (float xs : {x0, x1 - t}) {
      bande(Vec3(xs, fy, z0 + t), Vec3(xs + t, bh, 63.2f));
      bande(Vec3(xs, fy, 64.8f), Vec3(xs + t, bh, z1 - t));
    }
    // Pfosten und Dach
    k.Style(MAT_METAL, 0xd8b24a);
    for (float x : {x0 + 0.15f, -2.0f, x1 - 0.15f})
      for (float z : {z0 + 0.15f, z1 - 0.15f})
        k.Solid(Vec3(x - 0.12f, fy, z - 0.12f), Vec3(x + 0.12f, RH, z + 0.12f));
    k.Style(MAT_METAL, 0x2a2c30);
    k.Visual(Vec3(x0 - 0.4f, RH, z0 - 0.4f), Vec3(x1 + 0.4f, RH + 0.4f, z1 + 0.4f));
    k.Clip(Vec3(x0 - 0.4f, RH, z0 - 0.4f), Vec3(x1 + 0.4f, RH + 0.4f, z1 + 0.4f), COL_BULLET);
    // Deckengitter mit Leuchtfeldern
    const u32 neon[] = {0xff4fd8, 0x4fd8ff, 0xffe04f, 0x8aff6a};
    for (int i = 0; i < 12; i++)
      for (int j = 0; j < 7; j++) {
        float px = x0 + 1.0f + i * 2.0f, pz = z0 + 1.0f + j * 2.0f;
        k.Style(MAT_PLASTIC, neon[(i + j) % 4]);
        k.Glow((i + j) % 3 == 0 ? 1.0f : 0.25f);
        k.Visual(Vec3(px - 0.5f, RH - 0.04f, pz - 0.5f), Vec3(px + 0.5f, RH, pz + 0.5f), FACE_ALL & ~FACE_PY);
        k.Glow(0);
      }
    k.Style(MAT_METAL, 0x55595f);
    for (float x = x0; x <= x1 + 0.01f; x += 2.0f) k.Visual(Vec3(x - 0.03f, RH - 0.1f, z0), Vec3(x + 0.03f, RH, z1));
    for (float z = z0; z <= z1 + 0.01f; z += 2.0f) k.Visual(Vec3(x0, RH - 0.1f, z - 0.03f), Vec3(x1, RH, z + 0.03f));
    // Blende mit Schild
    k.Style(MAT_PLASTIC, 0x1d2430);
    k.Visual(Vec3(x0 - 0.4f, RH + 0.4f, z0 - 0.5f), Vec3(x1 + 0.4f, RH + 1.6f, z0 - 0.4f));
    k.Sign("AUTOSCOOTER", Vec3(-2, RH + 1.0f, z0 - 0.5f), PI, 0.13f, 0xff4fd8, 1.0f, false);
    for (int i = 0; i <= 40; i++) Bulb(k.ob, Vec3(x0 - 0.4f + i * (x1 - x0 + 0.8f) / 40, RH + 0.45f, z0 - 0.52f), i % 2 ? 0x4fd8ff : 0xffe04f, 0.05f);
    k.Light(Vec3(-8, 4.0f, 61), 10, Vec3(2.2f, 0.8f, 1.9f));
    k.Light(Vec3(4, 4.0f, 61), 10, Vec3(0.8f, 1.7f, 2.4f));
    k.Light(Vec3(-8, 4.0f, 67), 10, Vec3(0.9f, 1.9f, 2.2f));
    k.Light(Vec3(4, 4.0f, 67), 10, Vec3(2.3f, 1.6f, 0.7f));
    k.Light(Vec3(-2, 3.0f, 55.5f), 8, Vec3(2.2f, 1.0f, 2.0f));
    // Wagen
    const float cars[][3] = {{-11.0f, 60.0f, 0.4f}, {-7.5f, 66.5f, 2.2f}, {-3.8f, 60.8f, -0.9f}, {0.2f, 68.0f, 1.3f},
                             {3.2f, 62.4f, 2.8f},   {6.8f, 66.8f, -2.0f}, {-10.4f, 68.2f, 1.0f}, {7.4f, 59.6f, 0.9f},
                             {-1.0f, 64.6f, -1.6f}, {-12.0f, 63.9f, 3.0f}};
    for (auto& cr : cars) k.Prop("autoscooter", Vec3(cr[0], fy, cr[1]), cr[2]);
    // Kassenhäuschen
    k.Style(MAT_WOOD, 0x2a6ebb);
    k.Solid(Vec3(-17.2f, 0, 55.4f), Vec3(-15.4f, 2.4f, 57.2f));
    k.Style(MAT_WOOD, WHITE);
    k.Visual(Vec3(-17.3f, 2.4f, 55.3f), Vec3(-15.3f, 2.6f, 57.3f));
    k.Style(MAT_PLASTIC, 0xfff0d0);
    k.Glow(0.6f);
    k.Visual(Vec3(-16.9f, 1.1f, 55.38f), Vec3(-15.7f, 1.9f, 55.4f));
    k.Glow(0);
    k.Sign("KASSE", Vec3(-16.3f, 2.15f, 55.37f), PI, 0.035f, 0xffffff, 0.5f, false);
    k.Zone(Vec3(x0, -0.5f, z0), Vec3(x1, RH, z1), 0.55f, "Autoscooter", 2);
  }

  // ------------------------------------------------------------------------------------------
  void KidsCarousel(Vec3 c) {
    const float r = 4.6f, py = 0.3f;
    k.Style(MAT_WOOD, 0xb07a48);
    k.ob.Cylinder(c, r, py, 32);
    DiscClip(c, r, py);
    k.Style(MAT_PLASTIC, 0xd8202a);
    k.ob.Cylinder(c + Vec3(0, py, 0), 0.7f, 3.3f, 16);
    k.Clip(c + Vec3(-0.6f, 0, -0.6f), c + Vec3(0.6f, 4.0f, 0.6f), COL_SOLID);
    MeshBuilder& b = k.ob;
    const int seg = 24;
    Vec3 apex = c + Vec3(0, 5.4f, 0);
    for (int i = 0; i < seg; i++) {
      float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
      b.Mat(MAT_FABRIC);
      b.ColorHex(i % 2 ? WHITE : 0xd8202a);
      Vec3 p0 = c + Vec3(cosf(a0) * 5.0f, 3.6f, sinf(a0) * 5.0f), p1 = c + Vec3(cosf(a1) * 5.0f, 3.6f, sinf(a1) * 5.0f);
      TriN(b, apex, p0, p1, Vec3(0, 1, 0));
      TriN(b, apex, p0, p1, Vec3(0, -1, 0));
    }
    k.Style(MAT_GOLD, 0xd8b24a);
    b.Cylinder(c + Vec3(0, 3.3f, 0), 5.0f, 0.3f, 32, false);
    for (int i = 0; i < 40; i++) {
      float a = TAU * i / 40;
      Bulb(b, c + Vec3(cosf(a) * 5.06f, 3.45f, sinf(a) * 5.06f), i % 2 ? 0xffd890 : 0x7ad0ff, 0.05f);
    }
    k.Sign("KARUSSELL", c + Vec3(0, 3.45f, -5.08f), PI, 0.03f, 0xffffff, 0.6f);
    for (int i = 0; i < 8; i++) {
      float a = TAU * i / 8;
      k.Prop("holzpferd", c + Vec3(cosf(a) * 3.2f, py, sinf(a) * 3.2f), atan2f(-sinf(a), cosf(a)));
    }
    k.Light(c + Vec3(0, 3.0f, 0), 9, Vec3(2.4f, 1.8f, 1.2f));
    k.Prop("strohballen", c + Vec3(-5.6f, 0, -3.4f), 0.4f);
    k.Prop("gartenzwerg", c + Vec3(-5.3f, 0.45f, -3.5f), 2.5f);
    k.Zone(c - Vec3(r, 0.5f, r), c + Vec3(r, 3.3f, r), 0.7f, "Kinderkarussell", 2);
  }

  // ------------------------------------------------------------------------------------------
  void RideArea() {
    for (float x : {-52.0f, -20.0f, 12.0f, 37.0f, 56.0f}) k.Prop("laterne", Vec3(x, 0, 49.0f), 0);
    for (float x : {-46.0f, -20.0f, 4.0f, 30.0f, 52.0f}) k.Prop("laterne", Vec3(x, 0, 79.0f), 0);
    Garnitur(Vec3(-20.0f, 0, 54.0f), PI * 0.5f, 0.5f);
    Garnitur(Vec3(-17.2f, 0, 54.0f), PI * 0.5f, 0.5f);
    Garnitur(Vec3(-20.0f, 0, 59.5f), PI * 0.5f, 0.5f);
    for (int i = 0; i < 4; i++) k.Prop("dixi", Vec3(-4.0f + i * 1.35f, 0, 83.6f), PI);
    for (float x : {-56.0f, -40.0f, -24.0f, -12.0f, 14.0f, 38.0f, 56.0f}) k.Prop("baum", Vec3(x, 0, 83.0f), x);
    k.Prop("muelltonne", Vec3(2.2f, 0, 83.8f), 0);
    k.Prop("muelltonne", Vec3(3.0f, 0, 83.8f), 0.3f);
    k.Prop("ballons", Vec3(36.5f, 0, 55.0f), 0);
    k.Prop("teddy", Vec3(-29.0f, 0, 62.0f), -1.0f);
    k.Prop("abfalleimer", Vec3(14.5f, 0, 56.0f), 0);
    k.Prop("abfalleimer", Vec3(-24.0f, 0, 63.0f), 0);
    k.Prop("kiste", Vec3(57.5f, 0, 70.0f), 0.3f);
    k.Prop("kiste", Vec3(58.4f, 0, 71.2f), -0.2f);
    k.Prop("strohballen", Vec3(56.0f, 0, 66.5f), 1.57f);
  }

  // ------------------------------------------------------------------------------------------
  void Truck(Vec3 c) {
    MeshBuilder& b = k.ob;
    k.Clip(c + Vec3(-3.5f, 0, -1.1f), c + Vec3(5.0f, 0.5f, 1.1f), COL_SOLID);
    k.Style(MAT_METAL, 0x2a2c30);
    k.Solid(c + Vec3(-3.5f, 0.5f, -1.1f), c + Vec3(5.0f, 0.9f, 1.1f));
    k.Style(MAT_METAL, 0x2a6ebb);
    k.Solid(c + Vec3(3.2f, 0.9f, -1.2f), c + Vec3(5.0f, 2.9f, 1.2f));
    k.Style(MAT_MIRROR, 0x2a3440);
    k.Visual(c + Vec3(5.0f, 1.8f, -1.05f), c + Vec3(5.02f, 2.7f, 1.05f));
    k.Style(MAT_METAL, WHITE);
    k.Solid(c + Vec3(-3.5f, 0.9f, -1.25f), c + Vec3(3.0f, 3.6f, 1.25f));
    k.Style(MAT_FABRIC, BLUE);
    for (int i = 0; i < 8; i++) k.Visual(c + Vec3(-3.4f + i * 0.8f, 3.1f, -1.27f), c + Vec3(-3.0f + i * 0.8f, 3.5f, 1.27f));
    k.Sign("WIESNBIER", c + Vec3(-0.25f, 2.0f, 1.26f), 0, 0.07f, BLUE, 0.0f, false);
    k.Sign("WIESNBIER", c + Vec3(-0.25f, 2.0f, -1.26f), PI, 0.07f, BLUE, 0.0f, false);
    k.Style(MAT_PLASTIC, 0x1a1a1a);
    for (float x : {-2.4f, -1.2f, 4.0f})
      for (float z : {-1.0f, 1.0f}) b.CylinderZ(c + Vec3(x, 0.48f, z), 0.48f, 0.3f, 16);
  }

  void Yard() {
    for (int i = 0; i < 6; i++) k.Prop("dixi", Vec3(8.0f + i * 1.35f, 0, -29.8f), 0);
    // Kühlcontainer
    k.Style(MAT_METAL, 0xe4e6e8);
    k.Solid(Vec3(-36, 0, -31.0f), Vec3(-24, 2.6f, -28.6f));
    k.Style(MAT_METAL, 0xc8cacc);
    for (float x = -35.5f; x < -24; x += 0.6f) k.Visual(Vec3(x, 0.1f, -28.62f), Vec3(x + 0.08f, 2.5f, -28.55f));
    Truck(Vec3(-14.0f, 0, -26.0f));
    // Lager
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 2; j++) k.Prop("getraenkekiste", Vec3(-4.0f + i * 0.45f, j * 0.3f, -21.0f), 0);
    for (int i = 0; i < 5; i++) k.Prop("fass", Vec3(18.0f + i * 0.62f, 0, -21.0f), rng.Range(0, TAU));
    for (int i = 0; i < 4; i++) k.Prop("fass", Vec3(18.3f + i * 0.62f, 0, -21.6f), rng.Range(0, TAU));
    k.Prop("holzfass", Vec3(23.5f, 0, -24.0f), 0.3f);
    k.Prop("holzfass", Vec3(25.0f, 0, -25.5f), 1.6f);
    for (int i = 0; i < 3; i++) k.Prop("strohballen", Vec3(-28.0f + i * 1.15f, 0, -20.5f), 0);
    k.Prop("strohballen", Vec3(-27.4f, 0.45f, -20.5f), 0.1f);
    for (int i = 0; i < 4; i++) k.Prop("muelltonne", Vec3(30.0f + i * 0.9f, 0, -30.6f), 0);
    k.Prop("kiste", Vec3(4.5f, 0, -24.0f), 0.2f);
    k.Prop("kiste", Vec3(5.6f, 0, -24.3f), -0.4f);
    k.Prop("kiste", Vec3(5.0f, 0.6f, -24.1f), 0.1f);
    k.Prop("huetchen", Vec3(-4.0f, 0, -24.5f), 0);
    k.Prop("huetchen", Vec3(-6.0f, 0, -24.5f), 0);
    k.Prop("stapelstuehle", Vec3(-20.0f, 0, -19.5f), 0);
    k.Prop("bierbank", Vec3(12.0f, 0, -19.0f), 0);
    k.Prop("biertisch", Vec3(12.0f, 0, -19.8f), 0.05f);
    for (float x : {-30.0f, 0.0f, 30.0f}) k.Prop("laterne", Vec3(x, 0, -31.0f), 0);
  }

  // ------------------------------------------------------------------------------------------
  void Boundary() {
    const float X = 62, Z0 = -32, Z1 = 86;
    k.Clip(Vec3(-X - 1, 0, Z0 - 1), Vec3(-X, 10, Z1 + 1));
    k.Clip(Vec3(X, 0, Z0 - 1), Vec3(X + 1, 10, Z1 + 1));
    k.Clip(Vec3(-X, 0, Z0 - 1), Vec3(X, 10, Z0));
    k.Clip(Vec3(-X, 0, Z1), Vec3(X, 10, Z1 + 1));
    SiteFence(Vec3(-X, 0, Z0), Vec3(X, 0, Z0));
    SiteFence(Vec3(-X, 0, Z1), Vec3(X, 0, Z1));
    SiteFence(Vec3(-X, 0, Z0), Vec3(-X, 0, Z1));
    SiteFence(Vec3(X, 0, Z0), Vec3(X, 0, Z1));
    Rng r(77);
    for (float x = -66; x <= 66; x += 7) {
      VisualTree(Vec3(x + r.Range(-1, 1), 0, Z0 - 5 - r.Range(0, 3)), r.Range(0.9f, 1.3f));
      VisualTree(Vec3(x + r.Range(-1, 1), 0, Z1 + 5 + r.Range(0, 3)), r.Range(0.9f, 1.3f));
    }
    for (float z = Z0; z <= Z1; z += 7) {
      VisualTree(Vec3(-X - 5 - r.Range(0, 3), 0, z + r.Range(-1, 1)), r.Range(0.9f, 1.3f));
      VisualTree(Vec3(X + 5 + r.Range(0, 3), 0, z + r.Range(-1, 1)), r.Range(0.9f, 1.3f));
    }
  }

  void Backdrop() {
    MeshBuilder& b = k.ob;
    k.FloorFinish(-600, -600, 600, 600, -0.03f, MAT_GRASS, 0x5d7040);
    k.FloorFinish(-80, -50, 80, 100, -0.02f, MAT_ASPHALT, 0x4e4b47);
    // Bavaria mit Ruhmeshalle auf dem Hügel im Westen
    Vec3 bv(-165, 0, 20);
    k.Style(MAT_GRASS, 0x56703a);
    b.Ellipsoid(bv + Vec3(5, 0, 0), Vec3(55, 14, 95), 24, 8, 0.0f);
    const float H0 = 14.0f;
    k.Style(MAT_SANDSTONE, 0xd8c8a4);
    for (int i = 0; i <= 20; i++) b.Cylinder(Vec3(bv.x - 14, H0, bv.z - 30 + i * 3.0f), 0.7f, 9, 10);
    for (int s = -1; s <= 1; s += 2)
      for (int i = 0; i <= 8; i++) b.Cylinder(Vec3(bv.x - 14 + i * 3.0f, H0, bv.z + s * 30), 0.7f, 9, 10);
    k.Visual(Vec3(bv.x - 16, H0 + 9, bv.z - 32), Vec3(bv.x - 12, H0 + 11, bv.z + 32));
    for (int s = -1; s <= 1; s += 2) k.Visual(Vec3(bv.x - 16, H0 + 9, bv.z + s * 30 - 2), Vec3(bv.x + 11, H0 + 11, bv.z + s * 30 + 2));
    k.Visual(Vec3(bv.x - 18, H0 - 0.5f, bv.z - 34), Vec3(bv.x - 10, H0 + 9, bv.z + 34));
    k.Visual(Vec3(bv.x - 16, H0, bv.z - 33), Vec3(bv.x + 12, H0 + 0.6f, bv.z + 33));
    // Statue (Bronze)
    Vec3 st(bv.x + 4, H0, bv.z);
    k.Style(MAT_SANDSTONE, 0xcab890);
    k.Visual(st + Vec3(-4, 0.6f, -4), st + Vec3(4, 9, 4));  // auf der Plattform
    k.Style(MAT_METAL, 0x5f4e34);
    Vec3 base = st + Vec3(0, 9, 0);
    b.Frustum(base, 2.6f, 1.4f, 10, 16);
    b.Ellipsoid(base + Vec3(0, 11.5f, 0), Vec3(1.6f, 2.4f, 1.3f), 12, 8);
    b.Sphere(base + Vec3(0, 14.8f, 0), 1.15f, 12, 8);
    b.Ellipsoid(base + Vec3(0, 15.5f, -0.2f), Vec3(1.2f, 0.8f, 1.3f), 10, 6);
    Beam(b, base + Vec3(0, 13, 1.3f), base + Vec3(1.5f, 18.5f, 2.2f), 0.45f);
    b.Push(Translation(base + Vec3(1.6f, 19.2f, 2.3f)) * RotationZ(0.3f) * RotationX(PI * 0.5f));
    b.Torus(Vec3(0), 0.9f, 0.18f, 16, 6);
    b.Pop();
    Beam(b, base + Vec3(0, 13, -1.3f), base + Vec3(1.2f, 9.0f, -2.0f), 0.42f);
    b.Ellipsoid(base + Vec3(1.5f, 1.8f, -3.2f), Vec3(2.2f, 1.6f, 1.1f), 12, 7);
    b.Sphere(base + Vec3(3.4f, 3.0f, -3.2f), 1.0f, 10, 7);
    // Paulskirche im Osten
    Vec3 pk(150, 0, -30);
    k.Style(MAT_SANDSTONE, 0xb8a684);
    k.Visual(pk + Vec3(-8, 0, -30), pk + Vec3(8, 22, 10));
    b.Push(Translation(pk + Vec3(0, 22, -10)) * RotationY(PI * 0.25f));
    k.Style(MAT_METAL, 0x4a5a5e);
    b.Frustum(Vec3(0), 11.3f, 0.5f, 10, 4);
    b.Pop();
    k.Style(MAT_SANDSTONE, 0xb8a684);
    k.Visual(pk + Vec3(-6, 0, 10), pk + Vec3(6, 48, 22));
    b.Push(Translation(pk + Vec3(0, 48, 16)) * RotationY(PI * 0.25f));
    k.Style(MAT_METAL, 0x4a5a5e);
    b.Frustum(Vec3(0), 8.5f, 0.3f, 34, 4);
    b.Pop();
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = 0; sz <= 1; sz++) {
        Vec3 t = pk + Vec3(sx * 6.2f, 48, 10 + sz * 12);
        k.Style(MAT_METAL, 0x4a5a5e);
        b.Frustum(t, 1.2f, 0.1f, 8, 6);
      }
    // Frauenkirche in der Ferne
    Vec3 fk(260, 0, 190);
    for (int s = -1; s <= 1; s += 2) {
      k.Style(MAT_CONCRETE, 0x9a6a52);
      k.Visual(fk + Vec3(-6 + s * 9, 0, -6), fk + Vec3(6 + s * 9, 74, 6));
      k.Style(MAT_METAL, 0x4f7a5e);
      b.Ellipsoid(fk + Vec3(s * 9, 80, 0), Vec3(6.5f, 9.0f, 6.5f), 14, 8, -0.7f);
      b.Cylinder(fk + Vec3(s * 9, 88, 0), 0.8f, 6, 8);
    }
    k.Style(MAT_CONCRETE, 0x9a6a52);
    k.Visual(fk + Vec3(-14, 0, 6), fk + Vec3(14, 34, 90));
    k.Style(MAT_METAL, 0x6a3a2e);
    b.Push(Translation(fk + Vec3(0, 34, 0)));
    ExtrudeConvex(b, {{-14, 0}, {14, 0}, {0, 22}}, 6, 90);
    b.Pop();
    // Stadtsilhouette
    Rng r(4242);
    for (int i = 0; i < 70; i++) {
      float a = r.Range(-1.2f, 2.4f);
      float dist = r.Range(190, 340);
      Vec3 c(cosf(a) * dist, 0, 40 + sinf(a) * dist);
      float wdt = r.Range(10, 26), dep = r.Range(10, 26), hgt = r.Range(10, 30);
      k.Style(MAT_CONCRETE, r.Chance(0.5f) ? 0xd2c4b0 : 0xbfae98);
      k.Visual(Vec3(c.x - wdt, 0, c.z - dep), Vec3(c.x + wdt, hgt, c.z + dep));
      k.Style(MAT_PLASTER, 0x8a4a3a);
      k.ob.Wedge(Vec3(c.x - wdt, hgt, c.z - dep), Vec3(c.x + wdt, hgt, c.z), 0, dep * 0.4f);
      k.ob.Wedge(Vec3(c.x - wdt, hgt, c.z), Vec3(c.x + wdt, hgt, c.z + dep), dep * 0.4f, 0);
    }
    for (int i = 0; i < 90; i++) {
      float a = r.Range(0, TAU);
      float dist = r.Range(80, 150);
      Vec3 p(cosf(a) * dist, 0, 25 + sinf(a) * dist);
      if (p.x < -110) continue;
      VisualTree(p, r.Range(1.0f, 1.6f));
    }
  }

  // ------------------------------------------------------------------------------------------
  void Zones() {
    k.Zone(Vec3(-70, -5, -40), Vec3(70, 60, 95), 1.0f, "Theresienwiese", -1);
    k.Zone(Vec3(-28, -1, TZ + WT), Vec3(28, 20, 27.7f), 1.0f, "Biergarten", 0);
    k.Zone(Vec3(-62, -1, 27.7f), Vec3(62, 20, 40.6f), 1.0f, "Wirtsbudenstraße", 0);
    k.Zone(Vec3(-62, -1, -32), Vec3(62, 20, -TZ - WT), 1.0f, "Hinterhof", 0);
    k.Zone(Vec3(-50, -1, 60), Vec3(-22, 34, 76), 1.0f, "Riesenrad", 0);
    // Zelt: Dachschräge begrenzt den Innenbereich
    k.Zone(Vec3(-TX, -1, -TZ), Vec3(TX, RIDGE + 0.1f, TZ), IN_SKY, "Festzelt", 1);
    w.zones.back().planes = {Vec4(0, 1, SLOPE, -RIDGE), Vec4(0, 1, -SLOPE, -RIDGE)};
    for (int s = -1; s <= 1; s += 2) {
      float za = s > 0 ? GAL_Z : -TZ, zb = s > 0 ? TZ : -GAL_Z;
      k.Zone(Vec3(-TX, -1, za), Vec3(TX, GAL - 0.25f, zb), 0.16f, "Boxen", 2);
      k.Zone(Vec3(-TX, GAL - 0.25f, za), Vec3(TX, WALL_H, zb), 0.2f, "Empore", 2);
    }
    k.Zone(Vec3(-3.5f, -1, GAL_Z), Vec3(3.5f, WALL_H, TZ), 0.28f, "Haupteingang", 3);
    k.Zone(Vec3(-4, 0.9f, -3.5f), Vec3(4, 5.5f, 3.5f), 0.2f, "Musikpodium", 3);
    k.Zone(Vec3(24.6f, -1, -4.5f), Vec3(TX, 5, 4.5f), 0.18f, "Ausschank", 3);
    k.Zone(Vec3(-TX, -1, -4.5f), Vec3(-24.6f, 5, 4.5f), 0.18f, "Hendlküche", 3);
  }

  void Spawns() {
    for (float x : {50.0f, 52.5f, 55.0f, 57.5f})
      for (float z : {30.5f, 32.8f, 35.1f, 37.4f}) w.hunterSpawns.push_back({Vec3(x, 0, z), PI * 0.5f});
    for (float x : {-22.0f, -16.0f, -10.0f, -5.8f, 5.8f, 10.0f, 16.0f, 22.0f}) {
      w.propSpawns.push_back({Vec3(x, 0, 5.2f), 0});
      w.propSpawns.push_back({Vec3(x, 0, -5.2f), PI});
    }
    w.cameraPath = {Vec3(-38, 7, 31),     Vec3(8, 3, 44),     Vec3(6, 4.5f, 31),  Vec3(0, 6.5f, 17),
                    Vec3(-22, 6.2f, 8.2f), Vec3(0, 2.5f, 0),   Vec3(23, 5.4f, 13.5f), Vec3(0, 3, -2),
                    Vec3(-8, 9, 46),      Vec3(-36, 15, 68),  Vec3(12, 3.5f, 52), Vec3(25, 7, 66),
                    Vec3(-2, 2.8f, 52.5f), Vec3(-2, 1, 64)};
  }
};

}  // namespace

void BuildOktoberfest(World& w) {
  props::BuildCatalog();
  Builder b(w);
  b.Build();
}
