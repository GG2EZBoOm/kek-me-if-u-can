// Karte "Bundestag" – das Reichstagsgebäude in Berlin, für Prop Hunt neu interpretiert.
#include "game/Maps.h"
#include "game/MapKit.h"
#include "game/Shapes.h"

namespace {

const float GY = -1.2f;    // Platz der Republik
const float E0C = 4.6f;    // Decke Erdgeschoss (Unterkante Platte)
const float E1 = 5.0f;     // 1. Obergeschoss
const float E1C = 9.6f;    // Decke 1. OG
const float RF = 10.0f;    // Dachterrasse
const float WELL = -1.62f; // Boden vor dem Rednerpult
const Vec2 HC(7.0f, 0.0f); // Fokus der Sitzreihen

const u32 SAND = 0xc9b48e, SAND_D = 0xae9672, PLASTER = 0xd9d3c8, STONE = 0xd9d3c6, STEEL = 0xdfe2e5;

float DomeR(float h) {
  h = Clamp(h, 0.0f, 11.0f);
  return 12.0f * sqrtf(std::max(0.0f, 1.0f - (h / 11.0f) * (h / 11.0f)));
}

Mat4 Basis(Vec3 x, Vec3 y, Vec3 z, Vec3 t) {
  Mat4 m = Mat4::Identity();
  m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
  m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
  m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
  m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z;
  return m;
}

float FaceYaw(Vec3 from, Vec3 to) {
  Vec3 d = to - from;
  return atan2f(d.x, d.z);
}

// Fensterpositionen, die keine Trennwände treffen
std::vector<float> WindowCenters(float a0, float a1, float step, const std::vector<float>& walls, float clear) {
  std::vector<float> out;
  for (float c = a0; c <= a1 + 0.01f; c += step) {
    bool ok = true;
    for (float wv : walls) if (fabsf(c - wv) < clear) ok = false;
    if (ok) out.push_back(c);
  }
  return out;
}

class Builder {
 public:
  explicit Builder(World& w) : w(w), k(w), rng(20251022) {}
  World& w;
  MapKit k;
  Rng rng;

  void Build() {
    SetupEnvironment();
    Outside();
    Plinth();
    Portico();
    Facades();
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2) Tower((float)sx, (float)sz);
    GroundFloor();
    UpperFloor();
    Hall();
    Roof();
    Dome();
    Restaurant();
    FurnishGroundFloor();
    FurnishUpperFloor();
    FurnishRoof();
    Zones();
    Spawns();
  }

  // ------------------------------------------------------------------------------------------
  void SetupEnvironment() {
    auto& e = w.env;
    e.sunDir = Normalize(Vec3(-0.62f, 0.44f, 0.36f));
    e.sunColor = Vec3(3.0f, 2.4f, 1.75f);
    e.skyZenith = Hex(0x3a6db6);
    e.skyHorizon = Hex(0xe9d6b6);
    e.groundColor = Hex(0x6b6458);
    e.skyAmbient = Vec3(0.42f, 0.48f, 0.6f);
    e.groundAmbient = Vec3(0.27f, 0.23f, 0.19f);
    e.indoorAmbient = Vec3(0.2f, 0.19f, 0.18f);
    e.fogColor = Hex(0xd8ccb8);
    e.fogDensity = 0.0024f;
    e.clouds = 0.45f;
    e.exposure = 0.8f;
  }

  // ------------------------------------------------------------------------------------------
  void Fence(Vec3 a, Vec3 b, float height) {
    k.Style(MAT_METAL, 0x2e3236);
    Vec3 d = b - a;
    float len = Length(d);
    int n = std::max(1, (int)(len / 1.6f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = a + d * ((float)i / n);
      k.Visual(p + Vec3(-0.04f, 0, -0.04f), p + Vec3(0.04f, height, 0.04f));
    }
    float yaw = atan2f(d.x, d.z);
    Vec3 c = (a + b) * 0.5f;
    k.VisualR(c + Vec3(0, height, 0), Vec3(0.035f, 0.035f, len * 0.5f), yaw);
    k.VisualR(c + Vec3(0, height * 0.45f, 0), Vec3(0.02f, 0.02f, len * 0.5f), yaw);
  }

  void BigFlag(Vec3 base, float poleH, float fw, float fh, bool eu, float yaw) {
    k.Style(MAT_METAL, 0xd8d8d8);
    k.ob.Cylinder(base, 0.09f, poleH, 10);
    k.ob.Sphere(base + Vec3(0, poleH + 0.1f, 0), 0.14f, 8, 6);
    k.Clip(base - Vec3(0.12f, 0, 0.12f), base + Vec3(0.12f, poleH, 0.12f), COL_SOLID);
    WindFlag f;
    f.attach = base + Vec3(0, poleH - 0.2f, 0) + RotateY(Vec3(0.1f, 0, 0), yaw);
    f.yaw = yaw;
    f.width = fw;
    f.height = fh;
    f.pattern = eu ? FLAG_EUROPE : FLAG_GERMANY;
    f.wind = poleH < 4 ? 0.12f : 1.0f;  // Standfahnen im Saal hängen fast still
    f.phase = base.x * 0.37f + base.z * 0.61f;
    w.flags.push_back(f);
  }

  void Outside() {
    // Boden
    // Boden ohne Gebäudegrundfläche (der Plenarsaal liegt tiefer als der Platz)
    k.Clip(Vec3(-92, -4, -35), Vec3(-30, GY, 50), COL_SOLID);
    k.Clip(Vec3(30, -4, -35), Vec3(52, GY, 50), COL_SOLID);
    k.Clip(Vec3(-30, -4, -35), Vec3(30, GY, -20), COL_SOLID);
    k.Clip(Vec3(-30, -4, 20), Vec3(30, GY, 50), COL_SOLID);
    k.FloorFinish(-88, -33, -44, 33, GY, MAT_GRASS, 0x5f8f3a);
    k.FloorFinish(-88, -2.5f, -44, 2.5f, GY + 0.015f, MAT_CONCRETE, 0xcabfa6);
    k.FloorFinish(-88, -20, -44, -17, GY + 0.015f, MAT_CONCRETE, 0xcabfa6);
    k.FloorFinish(-88, 17, -44, 20, GY + 0.015f, MAT_CONCRETE, 0xcabfa6);
    k.FloorFinish(-67.5f, -33, -64.5f, 33, GY + 0.02f, MAT_CONCRETE, 0xcabfa6);
    const u32 plaza = 0xb5afa3;
    k.FloorFinish(-92, -35, -88, 50, GY, MAT_TILE, plaza);
    k.FloorFinish(-88, -35, -44, -33, GY, MAT_TILE, plaza);
    k.FloorFinish(-88, 33, -44, 50, GY, MAT_TILE, plaza);
    k.FloorFinish(-44, -35, -30, 50, GY, MAT_TILE, plaza);
    k.FloorFinish(30, -35, 38, 50, GY, MAT_TILE, plaza);
    k.FloorFinish(-30, -35, 30, -20, GY, MAT_TILE, plaza);
    k.FloorFinish(-30, 20, 30, 50, GY, MAT_TILE, plaza);
    k.FloorFinish(38, -35, 46, 50, GY, MAT_ASPHALT, 0x38393b);
    k.FloorFinish(46, -35, 52, 50, GY, MAT_CONCRETE, 0xa9a59d);
    k.Style(MAT_CONCRETE, 0x9a968e);
    k.Solid(Vec3(37.8f, GY, -35), Vec3(38.0f, GY + 0.12f, -3));  // an der Osttreppe unterbrochen
    k.Solid(Vec3(37.8f, GY, 3), Vec3(38.0f, GY + 0.12f, 50));
    k.Solid(Vec3(46.0f, GY, -35), Vec3(46.2f, GY + 0.12f, 50));
    k.Style(MAT_PLASTIC, 0xf2f2f2);
    for (float z = -33; z < 48; z += 6) k.Visual(Vec3(41.9f, GY, z), Vec3(42.1f, GY + 0.012f, z + 3));

    // Fernboden, Spree, gegenüberliegendes Ufer
    const u32 farCol = 0x7b8a5e;
    k.FloorFinish(-600, -35, -92, 600, GY - 0.02f, MAT_GRASS, farCol);
    k.FloorFinish(52, -35, 600, 600, GY - 0.02f, MAT_CONCRETE, 0x9c9a92);
    k.FloorFinish(-92, 50, 52, 600, GY - 0.02f, MAT_GRASS, farCol);
    k.FloorFinish(-600, -600, 600, -160, GY - 0.02f, MAT_GRASS, farCol);
    k.FloorFinish(-600, -160, 600, -76, GY, MAT_TILE, 0xaaa69c);
    k.FloorFinish(-600, -75, 600, -35.6f, -2.6f, MAT_WATER, 0x2c4c5c);
    k.Style(MAT_SANDSTONE, SAND_D);
    k.Solid(Vec3(-92, -3.4f, -35.6f), Vec3(52, GY, -35));
    k.Visual(Vec3(-600, -3.4f, -76), Vec3(600, GY, -75));
    k.Clip(Vec3(-92, GY, -35.6f), Vec3(52, 30, -35.0f));
    Fence(Vec3(-92, GY, -35.3f), Vec3(52, GY, -35.3f), 1.05f);
    // Paul-Löbe-Haus (Kulisse)
    k.Style(MAT_CONCRETE, 0xd9d5cc);
    k.Visual(Vec3(-70, GY, -108), Vec3(50, 21, -84));
    k.Style(MAT_MIRROR, 0x2b3c4a);
    for (int f = 0; f < 5; f++) k.Visual(Vec3(-69, GY + 1.2f + f * 4.2f, -83.95f), Vec3(49, GY + 3.6f + f * 4.2f, -83.9f));
    k.Style(MAT_CONCRETE, 0xe4e0d8);
    k.Visual(Vec3(-72, 21, -110), Vec3(52, 21.8f, -82));
    k.ob.Cylinder(Vec3(-78, GY, -96), 9, 24, 32);
    for (float x = -80; x <= 40; x += 9) k.Prop("baum", Vec3(x, GY, -79), 0);
    // Skyline
    Rng r(99);
    for (int i = 0; i < 40; i++) {
      float a = r.Range(-0.6f, 2.6f);
      float dist = r.Range(220, 360);
      Vec3 c(80 + cosf(a) * dist, GY, 30 + sinf(a) * dist);
      if (c.z < -60 && c.z > -200) continue;
      float wdt = r.Range(15, 40), dep = r.Range(15, 40), hgt = r.Range(12, 45);
      k.Style(MAT_CONCRETE, r.Chance(0.5f) ? 0xcfc8bb : 0xb9b2a6);
      k.Visual(Vec3(c.x - wdt, GY, c.z - dep), Vec3(c.x + wdt, GY + hgt, c.z + dep));
    }
    // Fernsehturm
    Vec3 ft(430, GY, -80);
    k.Style(MAT_CONCRETE, 0xd8d6d0);
    k.ob.Frustum(ft, 9, 4, 190, 20);
    k.Style(MAT_METAL, 0x9aa3ad);
    k.ob.Sphere(ft + Vec3(0, 200, 0), 17, 24, 16);
    k.Style(MAT_PLASTIC, 0xd02020);
    k.ob.Frustum(ft + Vec3(0, 214, 0), 1.6f, 0.6f, 70, 10);
    // Brandenburger Tor
    Vec3 bt(70, GY, 270);
    k.Style(MAT_SANDSTONE, 0xd8c8a4);
    for (int i = 0; i < 6; i++) k.ob.Cylinder(bt + Vec3(-15 + i * 6, 0, 0), 1.0f, 14, 14);
    k.Visual(bt + Vec3(-18, 14, -3), bt + Vec3(18, 18, 3));
    k.Visual(bt + Vec3(-6, 18, -2), bt + Vec3(6, 21, 2));
    k.Style(MAT_METAL, 0x4f7a5e);
    k.Visual(bt + Vec3(-3, 21, -1.5f), bt + Vec3(3, 24, 1.5f));

    // Bäume, Laternen, Bänke, Poller
    for (float x = -86; x <= -46; x += 5) {
      k.Prop("baum", Vec3(x, GY, -31.5f), x * 0.7f);
      k.Prop("baum", Vec3(x, GY, 31.5f), x * 1.3f);
    }
    for (float x = -38; x <= 32; x += 7) k.Prop("baum", Vec3(x, GY, 41), x);
    for (float x = -40; x <= 34; x += 8.5f) k.Prop("baum", Vec3(x, GY, -31), x * 0.4f);
    for (float x = -84; x <= -46; x += 10) {
      k.Prop("laterne", Vec3(x, GY, 3.6f), 0);
      k.Prop("laterne", Vec3(x, GY, -3.6f), 0);
    }
    for (float x = -36; x <= 34; x += 12) k.Prop("laterne", Vec3(x, GY, -33.5f), 0);
    for (float z = -28; z <= 46; z += 12) k.Prop("laterne", Vec3(-44.5f, GY, z), 0);
    for (float z = -30; z <= 46; z += 14) k.Prop("laterne", Vec3(46.8f, GY, z), 0);
    for (float x = -81; x <= -51; x += 10) {
      k.Prop("parkbank", Vec3(x, GY, 4.7f), PI);
      k.Prop("parkbank", Vec3(x + 4, GY, -4.7f), 0);
      k.Prop("abfalleimer", Vec3(x + 1.6f, GY, 4.9f), 0);
    }
    for (float x = -30; x <= 24; x += 9) k.Prop("parkbank", Vec3(x, GY, -27.5f), 0);
    for (float z = -18; z <= 18; z += 3) k.Prop("poller", Vec3(-43.5f, GY, z), 0);
    BigFlag(Vec3(-46, GY, -12), 14, 3.6f, 2.2f, false, 0);
    BigFlag(Vec3(-46, GY, 12), 14, 3.6f, 2.2f, true, 0);
    // Absperrgitter vor der Treppe
    for (int i = 0; i < 4; i++) {
      k.Prop("gitter", Vec3(-44.5f + i * 0.0f, GY, -8 - i * 2.1f), PI * 0.5f);
      k.Prop("gitter", Vec3(-44.5f, GY, 8 + i * 2.1f), PI * 0.5f);
    }
    // Besucherzentrum (Container)
    for (int c = 0; c < 2; c++) {
      float x0 = -38 + c * 9.0f, x1 = x0 + 7, z0 = 25, z1 = 28.2f;
      k.Style(MAT_METAL, c == 0 ? 0xe9e9e4 : 0xd6dde4);
      float cx = (x0 + x1) * 0.5f;
      k.WallX(x0, x1, z0, z0 + 0.12f, GY, GY + 2.7f, {{cx - 0.7f, cx + 0.7f, GY, GY + 2.2f}, {x0 + 0.8f, x0 + 2.2f, GY + 1.0f, GY + 2.0f}});
      k.WallX(x0, x1, z1 - 0.12f, z1, GY, GY + 2.7f, {{x1 - 2.4f, x1 - 0.8f, GY + 1.0f, GY + 2.0f}});
      k.WallZ(z0, z1, x0, x0 + 0.12f, GY, GY + 2.7f);
      k.WallZ(z0, z1, x1 - 0.12f, x1, GY, GY + 2.7f);
      k.Solid(Vec3(x0 - 0.1f, GY + 2.7f, z0 - 0.1f), Vec3(x1 + 0.1f, GY + 2.9f, z1 + 0.1f));
      k.FloorFinish(x0, z0, x1, z1, GY + 0.02f, MAT_CARPET, 0x55606e);
      k.Glass(Vec3(x0 + 0.8f, GY + 1.0f, z0 + 0.04f), Vec3(x0 + 2.2f, GY + 2.0f, z0 + 0.08f), 0xc4dbe8, 0.25f);
      k.Glass(Vec3(x1 - 2.4f, GY + 1.0f, z1 - 0.08f), Vec3(x1 - 0.8f, GY + 2.0f, z1 - 0.04f), 0xc4dbe8, 0.25f);
      k.CeilingLight(Vec3(cx, GY + 2.7f, (z0 + z1) * 0.5f), 6, Vec3(2.2f, 2.2f, 2.3f), 0.4f);
      k.Style(MAT_WOOD, 0xc8a27a);
      k.Solid(Vec3(x0 + 0.5f, GY, z1 - 0.9f), Vec3(x0 + 3.0f, GY + 1.0f, z1 - 0.3f));
      k.Prop("monitor", Vec3(x0 + 1.2f, GY + 1.0f, z1 - 0.6f), PI);
      k.Prop("kordel", Vec3(x1 - 1.2f, GY, z0 + 1.0f), 0);
      k.Prop("kordel", Vec3(x1 - 2.2f, GY, z0 + 1.0f), 0);
      k.Prop(c == 0 ? "koffer" : "schirmstaender", Vec3(x1 - 0.6f, GY, z1 - 0.6f), 0.3f);
      k.Prop("papierkorb", Vec3(x0 + 3.5f, GY, z1 - 0.4f), 0);
    }
    // Straße: Autos
    k.Prop("auto_grau", Vec3(44.4f, GY, -22), PI * 0.5f);
    k.Prop("auto_blau", Vec3(44.4f, GY, -10), PI * 0.5f);
    k.Prop("polizeiauto", Vec3(40.2f, GY, 7), -PI * 0.5f);
    k.Prop("auto_grau", Vec3(44.4f, GY, 22), PI * 0.5f);
    k.Prop("auto_blau", Vec3(44.4f, GY, 35), PI * 0.5f);
    k.Prop("huetchen", Vec3(39.0f, GY, 11.5f), 0);
    k.Prop("huetchen", Vec3(39.6f, GY, 12.6f), 0.4f);
    k.Prop("muelltonne", Vec3(47.2f, GY, -2), PI * 0.5f);
    k.Prop("muelltonne", Vec3(47.2f, GY, -0.9f), PI * 0.5f);
    k.Prop("fahrrad", Vec3(47.0f, GY, 14), 0.2f);
    k.Prop("fahrrad", Vec3(47.0f, GY, 15.2f), -0.1f);
    k.Prop("busch", Vec3(-40, GY, 33), 0);
    k.Prop("busch", Vec3(-36.5f, GY, 33.5f), 1);
    k.Prop("kuebel", Vec3(-41, GY, -11.5f), 0);
    k.Prop("kuebel", Vec3(-41, GY, 11.5f), 0);
    // Grenzen
    // West- und Nordgrenze beginnen an der Vorderkante der Randhecke: Die Heckenkugeln sind nur Optik,
    // sonst könnten sich Props darin verstecken.
    k.Clip(Vec3(-93, GY, -36), Vec3(-90.25f, 40, 51));
    k.Clip(Vec3(52, GY, -36), Vec3(53, 40, 51));
    k.Clip(Vec3(-93, GY, 48.25f), Vec3(53, 40, 51));
    k.Style(MAT_LEAVES, 0x3d6b2a);
    for (float x = -91; x < 52; x += 2.2f) k.ob.Ellipsoid(Vec3(x, GY + 1.0f, 49.3f), Vec3(1.5f, 1.3f, 1.0f), 10, 6);
    for (float z = -34; z < 50; z += 2.2f) k.ob.Ellipsoid(Vec3(-91.3f, GY + 1.0f, z), Vec3(1.0f, 1.3f, 1.5f), 10, 6);
  }

  // ------------------------------------------------------------------------------------------
  void Plinth() {
    k.Style(MAT_SANDSTONE, SAND_D);
    const u32 sides = FACE_PX | FACE_NX | FACE_PZ | FACE_NZ;
    k.Solid(Vec3(-30, GY, -20), Vec3(-12.6f, 0, 20), sides);
    k.Solid(Vec3(12.6f, GY, -20), Vec3(30, 0, 20), sides);
    k.Solid(Vec3(-12.6f, GY, -20), Vec3(12.6f, 0, -12.6f), sides);
    k.Solid(Vec3(-12.6f, GY, 12.6f), Vec3(12.6f, 0, 20), sides);
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        k.Solid(Vec3(sx * 30 - 4.5f, GY, sz * 20 - 4.5f), Vec3(sx * 30 + 4.5f, 0, sz * 20 + 4.5f), sides);
    // Grundboden unter Durchgängen
    const u32 base = 0xc9c2b4;
    k.FloorFinish(-29, -19, -12.6f, 19, -0.006f, MAT_MARBLE, base);
    k.FloorFinish(12.6f, -19, 29, 19, -0.006f, MAT_MARBLE, base);
    k.FloorFinish(-12.6f, -19, 12.6f, -12.6f, -0.006f, MAT_MARBLE, base);
    k.FloorFinish(-12.6f, 12.6f, 12.6f, 19, -0.006f, MAT_MARBLE, base);
    // Ost-Eingang
    k.Solid(Vec3(30, GY, -3), Vec3(32, 0, 3), sides);
    k.FloorFinish(29, -3, 32, 3, 0, MAT_MARBLE, STONE);
    k.Style(MAT_SANDSTONE, SAND);
    k.Stairs(Vec3(38, GY, 0), 1, 6, 0.2f, 6, 1.0f, GY);
  }

  void Portico() {
    k.Style(MAT_SANDSTONE, SAND_D);
    k.Solid(Vec3(-35.5f, GY, -9.5f), Vec3(-30, 0, 9.5f), FACE_PX | FACE_NX | FACE_PZ | FACE_NZ);
    k.FloorFinish(-35.5f, -9.5f, -29, 9.5f, 0, MAT_MARBLE, STONE);
    k.Style(MAT_SANDSTONE, SAND);
    k.Stairs(Vec3(-41.5f, GY, 0), 0, 19, 0.2f, 6, 1.0f, GY);
    for (int i = 0; i < 6; i++) {
      float z = -7.5f + i * 3.0f;
      k.Style(MAT_SANDSTONE, SAND_D);
      k.Solid(Vec3(-34.35f, 0, z - 0.75f), Vec3(-32.85f, 0.5f, z + 0.75f));
      k.Style(MAT_SANDSTONE, SAND);
      k.ob.Cylinder(Vec3(-33.6f, 0.5f, z), 0.55f, 7.3f, 24, false);
      k.Clip(Vec3(-34.1f, 0.5f, z - 0.5f), Vec3(-33.1f, 7.8f, z + 0.5f), COL_SOLID);
      k.Style(MAT_SANDSTONE, SAND_D);
      k.Solid(Vec3(-34.4f, 7.8f, z - 0.8f), Vec3(-32.8f, 8.4f, z + 0.8f));
    }
    k.Style(MAT_SANDSTONE, SAND);
    k.Solid(Vec3(-35.5f, 8.4f, -9.5f), Vec3(-30, 9.6f, 9.5f));
    k.Style(MAT_SANDSTONE, SAND_D);
    k.Solid(Vec3(-35.9f, 9.6f, -9.9f), Vec3(-30, 10.0f, 9.9f));
    // Giebel
    k.Style(MAT_SANDSTONE, SAND);
    Vec3 A0(-35.7f, 10, -9.7f), A1(-30, 10, -9.7f), B0(-35.7f, 10, 9.7f), B1(-30, 10, 9.7f);
    Vec3 C0(-35.7f, 12.6f, 0), C1(-30, 12.6f, 0);
    k.ob.Tri(A0, B0, C0);
    k.Style(MAT_SANDSTONE, SAND_D);
    k.ob.Quad(A0, C0, C1, A1);
    k.ob.Quad(B0, B1, C1, C0);
    k.Clip(Vec3(-35.7f, 10, -8.1f), Vec3(-30, 10.87f, 8.1f), COL_SOLID);
    k.Clip(Vec3(-35.7f, 10.87f, -4.85f), Vec3(-30, 11.73f, 4.85f), COL_SOLID);
    k.Clip(Vec3(-35.7f, 11.73f, -1.6f), Vec3(-30, 12.6f, 1.6f), COL_SOLID);
    // Inschrift "DEM DEUTSCHEN VOLKE"
    const char* text = "DEM DEUTSCHEN VOLKE";
    float px = 0.11f;
    float width = PixelTextWidth(text, px);
    k.Style(MAT_GOLD, 0x8a6a2a);
    k.ob.Push(Translation(Vec3(-35.5f, 8.62f, -width * 0.5f)) * RotationY(-PI * 0.5f));
    BuildPixelText(k.ob, text, px, 0.06f);
    k.ob.Pop();
  }

  void WindowSet(bool alongX, float fixed0, float fixed1, const std::vector<float>& centers, float y0, float y1,
                 float outward, std::vector<Hole>& holes) {
    float mid = (fixed0 + fixed1) * 0.5f;
    for (float c : centers) {
      holes.push_back({c - 0.7f, c + 0.7f, y0, y1, true});
      if (alongX) {
        k.Glass(Vec3(c - 0.7f, y0, mid - 0.04f), Vec3(c + 0.7f, y1, mid + 0.04f));
        k.Style(MAT_METAL, 0x3a3d42);
        k.Visual(Vec3(c - 0.03f, y0, mid - 0.06f), Vec3(c + 0.03f, y1, mid + 0.06f));
        k.Visual(Vec3(c - 0.7f, (y0 + y1) * 0.5f + 0.3f, mid - 0.06f), Vec3(c + 0.7f, (y0 + y1) * 0.5f + 0.36f, mid + 0.06f));
        float face = outward < 0 ? fixed0 : fixed1;
        k.Style(MAT_SANDSTONE, SAND_D);
        k.Visual(Vec3(c - 0.9f, y0 - 0.18f, std::min(face, face + outward * 0.25f)), Vec3(c + 0.9f, y0, std::max(face, face + outward * 0.25f)));
        k.Visual(Vec3(c - 0.95f, y1, std::min(face, face + outward * 0.18f)), Vec3(c + 0.95f, y1 + 0.4f, std::max(face, face + outward * 0.18f)));
      } else {
        k.Glass(Vec3(mid - 0.04f, y0, c - 0.7f), Vec3(mid + 0.04f, y1, c + 0.7f));
        k.Style(MAT_METAL, 0x3a3d42);
        k.Visual(Vec3(mid - 0.06f, y0, c - 0.03f), Vec3(mid + 0.06f, y1, c + 0.03f));
        k.Visual(Vec3(mid - 0.06f, (y0 + y1) * 0.5f + 0.3f, c - 0.7f), Vec3(mid + 0.06f, (y0 + y1) * 0.5f + 0.36f, c + 0.7f));
        float face = outward < 0 ? fixed0 : fixed1;
        k.Style(MAT_SANDSTONE, SAND_D);
        k.Visual(Vec3(std::min(face, face + outward * 0.25f), y0 - 0.18f, c - 0.9f), Vec3(std::max(face, face + outward * 0.25f), y0, c + 0.9f));
        k.Visual(Vec3(std::min(face, face + outward * 0.18f), y1, c - 0.95f), Vec3(std::max(face, face + outward * 0.18f), y1 + 0.4f, c + 0.95f));
      }
    }
  }

  void Facades() {
    std::vector<float> nWalls = {-18, -15.5f, -10, -2, -1, 6, 13.5f, 14};
    std::vector<float> sWalls = {-18, -15.5f, -6, -1, 6, 13.5f, 14};
    auto nC = WindowCenters(-24, 23.5f, 2.5f, nWalls, 1.25f);
    auto sC = WindowCenters(-24, 23.5f, 2.5f, sWalls, 1.25f);
    // Nord
    {
      std::vector<Hole> h;
      WindowSet(true, -20, -19, nC, 1.0f, 3.8f, -1, h);
      WindowSet(true, -20, -19, nC, 6.0f, 8.8f, -1, h);
      k.Style(MAT_SANDSTONE, SAND);
      k.WallX(-25.5f, 25.5f, -20, -19, 0, RF, h);
    }
    // Süd
    {
      std::vector<Hole> h;
      WindowSet(true, 19, 20, sC, 1.0f, 3.8f, 1, h);
      WindowSet(true, 19, 20, sC, 6.0f, 8.8f, 1, h);
      k.Style(MAT_SANDSTONE, SAND);
      k.WallX(-25.5f, 25.5f, 19, 20, 0, RF, h);
    }
    // West mit drei Portalen
    {
      std::vector<Hole> h = {{-6.3f, -3.7f, 0, 4.2f}, {-1.3f, 1.3f, 0, 4.2f}, {3.7f, 6.3f, 0, 4.2f}};
      WindowSet(false, -30, -29, {-13, -9.5f, 9.5f, 13}, 1.2f, 3.8f, -1, h);
      WindowSet(false, -30, -29, {-13, -10, -5, 0, 5, 10, 13}, 6.0f, 8.6f, -1, h);
      k.Style(MAT_SANDSTONE, SAND);
      k.WallZ(-15.5f, 15.5f, -30, -29, 0, RF, h);
      k.Style(MAT_WOOD, 0x4a3020);
      for (float zc : {-5.0f, 0.0f, 5.0f}) {
        // 2 cm in die Öffnung vorstehend, damit Rahmen und Laibung nicht in einer Ebene liegen
        k.Visual(Vec3(-30.05f, 0, zc - 1.42f), Vec3(-28.95f, 4.3f, zc - 1.28f));
        k.Visual(Vec3(-30.05f, 0, zc + 1.28f), Vec3(-28.95f, 4.3f, zc + 1.42f));
        k.Visual(Vec3(-30.05f, 4.18f, zc - 1.42f), Vec3(-28.95f, 4.36f, zc + 1.42f));
      }
    }
    // Ost mit Ostportal
    {
      std::vector<Hole> h = {{-1.5f, 1.5f, 0, 3.2f}};
      WindowSet(false, 29, 30, {-13, -9, -5, 5, 9, 13}, 1.0f, 3.8f, 1, h);
      WindowSet(false, 29, 30, {-13, -9, -5, 5, 9, 13}, 6.0f, 8.8f, 1, h);
      k.Style(MAT_SANDSTONE, SAND);
      k.WallZ(-15.5f, 15.5f, 29, 30, 0, RF, h);
    }
    // Gesimse und Brüstung
    k.Style(MAT_SANDSTONE, SAND_D);
    for (int s = -1; s <= 1; s += 2) {
      float zf = s * 20.0f;
      k.Visual(Vec3(-25.5f, 4.7f, std::min(zf, zf + s * 0.25f)), Vec3(25.5f, 5.0f, std::max(zf, zf + s * 0.25f)));
      k.Visual(Vec3(-25.5f, 9.6f, std::min(zf, zf + s * 0.45f)), Vec3(25.5f, 10.1f, std::max(zf, zf + s * 0.45f)));
      float xf = s * 30.0f;
      k.Visual(Vec3(std::min(xf, xf + s * 0.25f), 4.7f, -15.5f), Vec3(std::max(xf, xf + s * 0.25f), 5.0f, 15.5f));
      k.Visual(Vec3(std::min(xf, xf + s * 0.45f), 9.6f, -15.5f), Vec3(std::max(xf, xf + s * 0.45f), 10.1f, 15.5f));
    }
    k.Style(MAT_SANDSTONE, SAND);
    k.Solid(Vec3(-25.5f, RF, -20), Vec3(25.5f, RF + 1.1f, -19.5f));
    k.Solid(Vec3(-25.5f, RF, 19.5f), Vec3(25.5f, RF + 1.1f, 20));
    k.Solid(Vec3(-30, RF, -15.5f), Vec3(-29.5f, RF + 1.1f, 15.5f));
    k.Solid(Vec3(29.5f, RF, -15.5f), Vec3(30, RF + 1.1f, 15.5f));
  }

  // ------------------------------------------------------------------------------------------
  // Eckturm mit Treppenhaus. sx/sz = Vorzeichen der Ecke; lokal liegt das Gebäude Richtung -lx/-lz.
  void Tower(float sx, float sz) {
    float cx = sx * 30, cz = sz * 20;
    auto WX = [&](float lx) { return cx + sx * lx; };
    auto WZ = [&](float lz) { return cz + sz * lz; };
    auto box = [&](float lx0, float lz0, float lx1, float lz1, float y0, float y1, bool solid = true, u32 faces = FACE_ALL) {
      Vec3 mn(std::min(WX(lx0), WX(lx1)), y0, std::min(WZ(lz0), WZ(lz1)));
      Vec3 mx(std::max(WX(lx0), WX(lx1)), y1, std::max(WZ(lz0), WZ(lz1)));
      if (solid) k.Solid(mn, mx, faces);
      else k.Visual(mn, mx, faces);
    };
    auto holesZ = [&](std::vector<std::pair<float, float>> lz, std::vector<std::pair<float, float>> ys, bool win = false) {
      std::vector<Hole> out;
      for (auto& a : lz)
        for (auto& y : ys) {
          float z0 = WZ(a.first), z1 = WZ(a.second);
          out.push_back({std::min(z0, z1), std::max(z0, z1), y.first, y.second, win});
        }
      return out;
    };
    auto holesX = [&](std::vector<std::pair<float, float>> lx, std::vector<std::pair<float, float>> ys, bool win = false) {
      std::vector<Hole> out;
      for (auto& a : lx)
        for (auto& y : ys) {
          float x0 = WX(a.first), x1 = WX(a.second);
          out.push_back({std::min(x0, x1), std::max(x0, x1), y.first, y.second, win});
        }
      return out;
    };
    const float TOP = 16.0f;
    std::vector<std::pair<float, float>> doors = {{0, 2.4f}, {5, 7.4f}, {10, 12.4f}};
    std::vector<std::pair<float, float>> wins = {{1.2f, 3.6f}, {6.2f, 8.6f}, {11.5f, 14.5f}};
    k.Style(MAT_SANDSTONE, SAND);
    // -lx Wand (zum Gebäude) mit Türen
    {
      float x0 = std::min(WX(-4.5f), WX(-3.7f)), x1 = std::max(WX(-4.5f), WX(-3.7f));
      k.WallZ(std::min(WZ(-4.5f), WZ(4.5f)), std::max(WZ(-4.5f), WZ(4.5f)), x0, x1, 0, TOP - 0.4f, holesZ({{-3.5f, -2.3f}}, doors));
    }
    // +lx Wand mit Fenstern
    {
      float x0 = std::min(WX(3.7f), WX(4.5f)), x1 = std::max(WX(3.7f), WX(4.5f));
      auto h = holesZ({{0.9f, 2.3f}}, wins, true);
      k.WallZ(std::min(WZ(-4.5f), WZ(4.5f)), std::max(WZ(-4.5f), WZ(4.5f)), x0, x1, 0, TOP - 0.4f, h);
      for (auto& hh : h) k.Glass(Vec3((x0 + x1) * 0.5f - 0.04f, hh.y0, hh.a0), Vec3((x0 + x1) * 0.5f + 0.04f, hh.y1, hh.a1));
      k.Style(MAT_SANDSTONE, SAND);
    }
    // -lz Wand mit Türen
    {
      float z0 = std::min(WZ(-4.5f), WZ(-3.7f)), z1 = std::max(WZ(-4.5f), WZ(-3.7f));
      k.WallX(std::min(WX(-3.7f), WX(3.7f)), std::max(WX(-3.7f), WX(3.7f)), z0, z1, 0, TOP - 0.4f, holesX({{-3.5f, -2.3f}}, doors));
    }
    // +lz Wand mit Fenstern
    {
      float z0 = std::min(WZ(3.7f), WZ(4.5f)), z1 = std::max(WZ(3.7f), WZ(4.5f));
      auto h = holesX({{-0.7f, 0.7f}}, wins, true);
      k.WallX(std::min(WX(-3.7f), WX(3.7f)), std::max(WX(-3.7f), WX(3.7f)), z0, z1, 0, TOP - 0.4f, h);
      for (auto& hh : h) k.Glass(Vec3(hh.a0, hh.y0, (z0 + z1) * 0.5f - 0.04f), Vec3(hh.a1, hh.y1, (z0 + z1) * 0.5f + 0.04f));
      k.Style(MAT_SANDSTONE, SAND);
    }
    // Dach, Brüstung, Fahne
    k.Style(MAT_SANDSTONE, SAND_D);
    box(-4.8f, -4.8f, 4.8f, 4.8f, TOP - 0.4f, TOP);
    box(-4.8f, -4.8f, 4.8f, -4.3f, TOP, TOP + 0.9f);
    box(-4.8f, 4.3f, 4.8f, 4.8f, TOP, TOP + 0.9f);
    box(-4.8f, -4.3f, -4.3f, 4.3f, TOP, TOP + 0.9f);
    box(4.3f, -4.3f, 4.8f, 4.3f, TOP, TOP + 0.9f);
    box(-4.7f, -4.7f, 4.7f, 4.7f, 9.6f, 10.0f, false, FACE_PX | FACE_NX | FACE_PZ | FACE_NZ);
    BigFlag(Vec3(cx, TOP, cz), 7, 2.6f, 1.6f, false, sx > 0 ? PI : 0);
    // Boden EG
    k.FloorFinish(std::min(WX(-3.7f), WX(3.7f)), std::min(WZ(-3.7f), WZ(3.7f)), std::max(WX(-3.7f), WX(3.7f)),
                  std::max(WZ(-3.7f), WZ(3.7f)), 0, MAT_MARBLE, 0xcfc8ba);
    // Geschossplatten mit Treppenloch (u = lx+3.7, v = lz+3.7)
    for (float y : {E1, RF}) {
      Vec3 mn(std::min(WX(-3.7f), WX(-2.1f)), y - 0.4f, std::min(WZ(-3.7f), WZ(3.7f)));
      Vec3 mx(std::max(WX(-3.7f), WX(-2.1f)), y, std::max(WZ(-3.7f), WZ(3.7f)));
      k.Slab(mn.x, mn.z, mx.x, mx.z, y, 0.4f, MAT_MARBLE, 0xcfc8ba);
      Vec3 mn2(std::min(WX(-2.1f), WX(3.7f)), y - 0.4f, std::min(WZ(-0.1f), WZ(3.7f)));
      Vec3 mx2(std::max(WX(-2.1f), WX(3.7f)), y, std::max(WZ(-0.1f), WZ(3.7f)));
      k.Slab(mn2.x, mn2.z, mx2.x, mx2.z, y, 0.4f, MAT_MARBLE, 0xcfc8ba);
      k.Railing(Vec3(WX(-2.1f), y, WZ(-0.1f)), Vec3(WX(3.7f), y, WZ(-0.1f)), 1.0f, false);
    }
    // Treppenläufe
    int dirPlus = sx > 0 ? 0 : 1, dirMinus = sx > 0 ? 1 : 0;
    const float rise = 2.5f / 12.0f, run = 0.35f;
    for (float y0 : {0.0f, E1}) {
      k.Style(MAT_MARBLE, 0xd6d0c4);
      k.Stairs(Vec3(WX(-2.1f), y0, WZ(-2.85f)), dirPlus, 1.7f, rise, 12, run, y0);
      box(2.1f, -3.7f, 3.7f, -0.1f, y0 + 2.2f, y0 + 2.5f);
      k.Stairs(Vec3(WX(2.1f), y0 + 2.5f, WZ(-0.95f)), dirMinus, 1.7f, rise, 12, run, y0 + 2.2f);
      k.Style(MAT_PLASTER, PLASTER);
      box(-2.1f, -2.0f, 2.1f, -1.8f, y0, y0 + 5.0f);
    }
    for (float y : {4.4f, 9.4f, 15.3f}) k.Light(Vec3(cx, y, cz), 8, Vec3(1.8f, 1.7f, 1.5f));
    k.Zone(Vec3(std::min(WX(-3.7f), WX(3.7f)), -0.5f, std::min(WZ(-3.7f), WZ(3.7f))),
           Vec3(std::max(WX(-3.7f), WX(3.7f)), TOP, std::max(WZ(-3.7f), WZ(3.7f))), 0.12f, "Treppenturm", 6);
  }

  // ------------------------------------------------------------------------------------------
  void GroundFloor() {
    k.Style(MAT_PLASTER, PLASTER);
    auto door = [](float c, float w = 1.4f, float h = 2.4f) { return Hole{c - w * 0.5f, c + w * 0.5f, 0, h}; };
    // Innenwände der Flügel
    k.WallX(-25.5f, 25.5f, -15.5f, -15.2f, 0, E0C, {door(-21.5f), door(-12.5f), door(-6), door(2), door(10), door(20)});
    k.WallX(-25.5f, 25.5f, 15.2f, 15.5f, 0, E0C, {door(-21.5f), door(-12), door(0), door(10), door(20)});
    for (float x : {-18.0f, -10.0f, -2.0f, 6.0f, 14.0f}) k.WallZ(-19, -15.5f, x - 0.125f, x + 0.125f, 0, E0C);
    for (float x : {-18.0f, -6.0f, 6.0f, 14.0f}) k.WallZ(15.5f, 19, x - 0.125f, x + 0.125f, 0, E0C);
    // Ringwände
    k.WallZ(-15.2f, 15.2f, -15.5f, -15.2f, 0, E0C, {{-10, -7, 0, 3.5f}, {-1.5f, 1.5f, 0, 3.5f}, {7, 10, 0, 3.5f}});
    k.WallZ(-15.2f, 15.2f, 15.2f, 15.5f, 0, E0C, {door(-8), door(-1), door(9)});
    // Kantine / Küche
    k.WallX(15.5f, 29, 3, 3.25f, 0, E0C, {door(22), {17, 20, 0.95f, 2.0f}});
    k.Style(MAT_METAL, 0xb9bdc3);
    k.Solid(Vec3(17, 0.9f, 2.85f), Vec3(20, 0.96f, 3.4f));

    // Böden
    k.FloorFinish(-29, -15.5f, -15.5f, 15.5f, 0, MAT_MARBLE, 0xe0dacd);
    const u32 corr = 0xc9c2b4;
    k.FloorFinish(-15.2f, -15.2f, -12.6f, 15.2f, 0, MAT_MARBLE, corr);
    k.FloorFinish(-12.6f, -15.2f, 15.2f, -12.6f, 0, MAT_MARBLE, corr);
    k.FloorFinish(-12.6f, 12.6f, 15.2f, 15.2f, 0, MAT_MARBLE, corr);
    k.FloorFinish(12.6f, -12.6f, 15.2f, 12.6f, 0, MAT_MARBLE, corr);
    struct RoomF { float x0, x1; Material m; u32 c; };
    RoomF north[] = {{-25.5f, -18, MAT_PARQUET, 0xb08458}, {-18, -10, MAT_CARPET, 0x6f7480}, {-10, -2, MAT_PARQUET, 0x8a6240},
                     {-2, 6, MAT_TILE, 0xb9b8b2}, {6, 14, MAT_CARPET, 0x6f7480}, {14, 25.5f, MAT_TILE, 0xe8e8e4}};
    for (auto& r : north) k.FloorFinish(r.x0, -19, r.x1, -15.5f, 0, r.m, r.c);
    RoomF south[] = {{-25.5f, -18, MAT_TILE, 0xb0b4b8}, {-18, -6, MAT_CARPET, 0x707a6c}, {-6, 6, MAT_PARQUET, 0xc49a6c},
                     {6, 14, MAT_TILE, 0xa8acb0}, {14, 25.5f, MAT_CONCRETE, 0xa09a90}};
    for (auto& r : south) k.FloorFinish(r.x0, 15.5f, r.x1, 19, 0, r.m, r.c);
    k.FloorFinish(15.5f, -15.5f, 29, 3, 0, MAT_TILE, 0xe4ded2);
    k.FloorFinish(15.5f, 3, 29, 15.5f, 0, MAT_TILE, 0xd2d8d8);

    // Decken-Leuchten
    for (float z : {-12.0f, -6.0f, 0.0f, 6.0f, 12.0f}) k.CeilingLight(Vec3(-13.9f, E0C, z), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
    for (float x : {-9.0f, -3.0f, 3.0f, 9.0f, 14.0f}) {
      k.CeilingLight(Vec3(x, E0C, -13.9f), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
      k.CeilingLight(Vec3(x, E0C, 13.9f), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
    }
    for (float z : {-8.0f, -2.0f, 4.0f, 9.0f}) k.CeilingLight(Vec3(13.9f, E0C, z), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
    float nCenters[] = {-21.75f, -14, -6, 2, 10, 19.75f};
    for (float x : nCenters) k.CeilingLight(Vec3(x, E0C, -17.25f), 7.5f, Vec3(1.8f, 1.65f, 1.4f));
    float sCenters[] = {-21.75f, -12, 0, 10, 19.75f};
    for (float x : sCenters) k.CeilingLight(Vec3(x, E0C, 17.25f), 7.5f, Vec3(1.8f, 1.65f, 1.4f));
    for (float x : {18.5f, 22.5f, 26.5f})
      for (float z : {-11.0f, -4.0f}) k.CeilingLight(Vec3(x, E0C, z), 8, Vec3(2.4f, 2.3f, 2.1f));
    for (float x : {19.0f, 25.0f})
      for (float z : {6.5f, 12.0f}) k.CeilingLight(Vec3(x, E0C, z), 8, Vec3(2.3f, 2.3f, 2.4f));
    for (float x : {-26.0f, -21.0f})
      for (float z : {-10.0f, 0.0f, 10.0f}) k.CeilingLight(Vec3(x, E1C, z), 12, Vec3(2.4f, 2.2f, 1.9f), 0.6f);

    // Große Treppe in der Eingangshalle mit rotem Teppich
    k.Style(MAT_MARBLE, 0xe4dfd4);
    k.Stairs(Vec3(-26.0f, 0, 0), 0, 5.0f, 0.2f, 25, 0.3f, 0);
    k.Style(MAT_CARPET, 0x8e1b22);
    for (int i = 0; i < 25; i++) {
      float x0 = -26.0f + i * 0.3f, top = 0.2f * (i + 1);
      k.Visual(Vec3(x0, top, -1.25f), Vec3(x0 + 0.3f, top + 0.012f, 1.25f), FACE_PY | FACE_NX);
    }
    for (int i = 0; i < 25; i += 2) {
      float x0 = -26.0f + i * 0.3f, top = 0.2f * (i + 1);
      for (float zs : {-2.55f, 2.55f}) k.Railing(Vec3(x0, top, zs), Vec3(x0 + 0.6f, top, zs), 0.95f, false);
    }
  }

  // ------------------------------------------------------------------------------------------
  void UpperFloor() {
    // Platten
    k.Slab(-15.5f, -19, -1, -12.6f, E1, 0.4f, MAT_CARPET, 0x4b5a86);
    k.Slab(-1, -19, 13.5f, -12.6f, E1, 0.4f, MAT_CARPET, 0x4f6b58);
    k.Slab(13.5f, -19, 25.5f, -12.6f, E1, 0.4f, MAT_PARQUET, 0xa47a50);
    k.Slab(-25.5f, -19, -15.5f, -15.5f, E1, 0.4f, MAT_PARQUET, 0xb08458);
    k.Slab(-25.5f, 15.5f, -15.5f, 19, E1, 0.4f, MAT_PARQUET, 0xb08458);
    k.Slab(-29, -15.5f, -15.5f, -13.5f, E1, 0.4f, MAT_MARBLE, 0xe0dacd);
    k.Slab(-29, 13.5f, -15.5f, 15.5f, E1, 0.4f, MAT_MARBLE, 0xe0dacd);
    k.Slab(-18.5f, -13.5f, -15.5f, 13.5f, E1, 0.4f, MAT_MARBLE, 0xe0dacd);
    k.Slab(-15.5f, -12.6f, -12.6f, 12.6f, E1, 0.4f, MAT_MARBLE, 0xc9c2b4);
    k.Slab(-15.5f, 12.6f, -1, 19, E1, 0.4f, MAT_CARPET, 0x6a6e78);
    k.Slab(-1, 12.6f, 13.5f, 19, E1, 0.4f, MAT_CARPET, 0xa89a80);
    k.Slab(13.5f, 12.6f, 25.5f, 19, E1, 0.4f, MAT_PARQUET, 0xa47a50);
    k.Slab(12.6f, -12.6f, 15.5f, 12.6f, E1, 0.4f, MAT_MARBLE, 0xc9c2b4);
    k.Slab(15.5f, -12.6f, 29, 0, E1, 0.4f, MAT_PARQUET, 0x9a6a42);
    k.Slab(25.5f, -15.5f, 29, -12.6f, E1, 0.4f, MAT_PARQUET, 0x9a6a42);
    k.Slab(15.5f, 0, 29, 12.6f, E1, 0.4f, MAT_CARPET, 0x7a4e46);
    k.Slab(25.5f, 12.6f, 29, 15.5f, E1, 0.4f, MAT_CARPET, 0x7a4e46);
    // Galerien im Plenarsaal
    k.Slab(-12, -12, 6, -9.6f, E1, 0.4f, MAT_CARPET, 0x55607a);
    k.Slab(-12, 9.6f, 6, 12, E1, 0.4f, MAT_CARPET, 0x55607a);
    k.Slab(-12, -9.6f, -9.6f, 9.6f, E1, 0.4f, MAT_CARPET, 0x55607a);
    k.Railing(Vec3(-9.6f, E1, -9.6f), Vec3(6, E1, -9.6f));
    k.Railing(Vec3(6, E1, -12), Vec3(6, E1, -9.6f));
    k.Railing(Vec3(-9.6f, E1, 9.6f), Vec3(6, E1, 9.6f));
    k.Railing(Vec3(6, E1, 9.6f), Vec3(6, E1, 12));
    k.Railing(Vec3(-9.6f, E1, -9.6f), Vec3(-9.6f, E1, 9.6f));
    // Geländer um den Luftraum der Eingangshalle
    k.Railing(Vec3(-26.3f, E1, -13.5f), Vec3(-18.5f, E1, -13.5f));
    k.Railing(Vec3(-26.3f, E1, 13.5f), Vec3(-18.5f, E1, 13.5f));
    k.Railing(Vec3(-29, E1, -13.5f), Vec3(-26.3f, E1, -13.5f));
    k.Railing(Vec3(-29, E1, 13.5f), Vec3(-26.3f, E1, 13.5f));
    k.Railing(Vec3(-18.5f, E1, -13.5f), Vec3(-18.5f, E1, -2.5f));
    k.Railing(Vec3(-18.5f, E1, 2.5f), Vec3(-18.5f, E1, 13.5f));

    k.Style(MAT_PLASTER, PLASTER);
    auto door = [](float c, float w = 1.4f) { return Hole{c - w * 0.5f, c + w * 0.5f, E1, E1 + 2.4f}; };
    k.WallZ(-15.5f, 15.5f, -15.5f, -15.2f, E1, E1C, {door(-6.3f), door(6.3f)});
    for (float x : {-1.0f, 13.5f}) {
      k.WallZ(-19, -12.6f, x - 0.125f, x + 0.125f, E1, E1C, {door(-15.9f)});
      k.WallZ(12.6f, 19, x - 0.125f, x + 0.125f, E1, E1C, {door(15.9f)});
    }
    k.WallZ(-19, -15.5f, -15.5f, -15.2f, E1, E1C, {door(-17.2f)});
    k.WallZ(15.5f, 19, -15.5f, -15.2f, E1, E1C, {door(17.2f)});
    k.WallX(-25.5f, -15.5f, -15.8f, -15.5f, E1, E1C, {door(-17.4f)});
    k.WallX(-25.5f, -15.5f, 15.5f, 15.8f, E1, E1C, {door(-17.4f)});
    k.WallX(12.6f, 25.5f, -12.85f, -12.6f, E1, E1C, {door(13.9f)});
    k.WallX(12.6f, 25.5f, 12.6f, 12.85f, E1, E1C, {door(13.9f)});
    k.WallZ(-15.5f, -12.6f, 25.25f, 25.5f, E1, E1C);
    k.WallZ(12.6f, 15.5f, 25.25f, 25.5f, E1, E1C);
    k.WallZ(-12.6f, 12.6f, 15.2f, 15.5f, E1, E1C, {door(-6), door(6)});
    k.WallX(15.5f, 29, -0.125f, 0.125f, E1, E1C, {door(22.5f)});

    // Leuchten
    for (float x : {-12.0f, -6.0f, 9.0f}) {
      k.CeilingLight(Vec3(x, E1C, -15.8f), 8, Vec3(2.3f, 2.1f, 1.8f));
      k.CeilingLight(Vec3(x, E1C, 15.8f), 8, Vec3(2.3f, 2.1f, 1.8f));
    }
    for (float x : {3.0f, 19.5f}) {
      k.CeilingLight(Vec3(x, E1C, -15.8f), 8, Vec3(2.3f, 2.1f, 1.8f));
      k.CeilingLight(Vec3(x, E1C, 15.8f), 8, Vec3(2.3f, 2.1f, 1.8f));
    }
    k.CeilingLight(Vec3(-20.5f, E1C, -17.25f), 7, Vec3(2.2f, 1.9f, 1.5f));
    k.CeilingLight(Vec3(-20.5f, E1C, 17.25f), 7, Vec3(2.2f, 1.9f, 1.5f));
    for (float z : {-9.0f, 0.0f, 9.0f}) {
      k.CeilingLight(Vec3(-13.9f, E1C, z), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
      k.CeilingLight(Vec3(13.9f, E1C, z), 7, Vec3(1.9f, 1.8f, 1.6f), 0.35f);
    }
    for (float z : {-8.0f, 6.5f}) {
      k.CeilingLight(Vec3(19.5f, E1C, z), 8, Vec3(2.4f, 2.1f, 1.7f));
      k.CeilingLight(Vec3(25.5f, E1C, z * 0.8f), 8, Vec3(2.4f, 2.1f, 1.7f));
    }
  }

  // ------------------------------------------------------------------------------------------
  float HallL(float phi) const {
    float dx = -cosf(phi), dz = sinf(phi);
    float t = 1e9f;
    if (dx < -1e-4f) t = std::min(t, (-12.0f - HC.x) / dx);
    if (dx > 1e-4f) t = std::min(t, (12.0f - HC.x) / dx);
    if (dz < -1e-4f) t = std::min(t, (-12.0f - HC.y) / dz);
    if (dz > 1e-4f) t = std::min(t, (12.0f - HC.y) / dz);
    return t;
  }
  Vec3 HallP(float r, float phi, float y) const { return Vec3(HC.x - r * cosf(phi), y, HC.y + r * sinf(phi)); }

  void Hall() {
    k.Style(MAT_PLASTER, 0xd8d2c6);
    auto door0 = [](float c) { return Hole{c - 0.7f, c + 0.7f, 0, 2.6f}; };
    auto door1 = [](float c) { return Hole{c - 0.7f, c + 0.7f, E1, E1 + 2.4f}; };
    k.WallZ(-12.6f, 12.6f, -12.6f, -12, WELL, E1C, {door0(-6), door0(0), door0(6), door1(-5), door1(5)});
    k.WallX(-12.6f, 12.6f, -12.6f, -12, WELL, E1C, {door0(-10), door0(10), door1(-8), door1(2)});
    k.WallX(-12.6f, 12.6f, 12, 12.6f, WELL, E1C, {door0(-10), door0(10), door1(-8), door1(2)});
    k.WallZ(-12.6f, 12.6f, 12, 12.6f, WELL, E1C, {door0(-10.6f), door0(10.6f)});
    // Lichtband oben
    k.Style(MAT_PLASTIC, 0xfff4e0);
    k.Glow(0.7f);
    k.Visual(Vec3(-12, 9.2f, -12), Vec3(12, 9.35f, -11.9f));
    k.Visual(Vec3(-12, 9.2f, 11.9f), Vec3(12, 9.35f, 12));
    k.Visual(Vec3(-12, 9.2f, -12), Vec3(-11.9f, 9.35f, 12));
    k.Glow(0);
    // Boden vor dem Pult
    k.Style(MAT_CARPET, 0x3f4862);
    k.Solid(Vec3(-12, WELL - 0.4f, -12), Vec3(12, WELL, 12), FACE_PY);
    // Sitzstufen (Ringsegmente um HC)
    const int SEG = 72;
    const float dphi = PI / SEG;
    for (int ring = 1; ring <= 9; ring++) {
      float r0 = 4.8f + 1.2f * ring;
      float r1 = ring == 9 ? 30.0f : r0 + 1.2f;
      float h = WELL + 0.18f * ring;
      for (int s = 0; s < SEG; s++) {
        float pa = -PI * 0.5f + s * dphi, pb = pa + dphi, pm = (pa + pb) * 0.5f;
        float La = HallL(pa), Lb = HallL(pb), Lm = HallL(pm);
        if (r0 >= std::max(La, Lb)) continue;
        // Außenkante genau an der Wand enden lassen – sonst liegt der Teppich auf den Türschwellen und flackert.
        // Nur Segmente über einer Saalecke brauchen Überstand, damit keine Lücke bleibt.
        Vec3 wa = HallP(La, pa, 0), wb = HallP(Lb, pb, 0);
        bool sameWall = fabsf(wa.x - wb.x) < 1e-3f || fabsf(wa.z - wb.z) < 1e-3f;
        float ext = sameWall ? 0.0f : 0.5f;
        float ra = std::min(r1, La + ext), rb = std::min(r1, Lb + ext);
        k.Style(MAT_CARPET, ring == 9 ? 0x55607a : 0x5d6987);
        Vec3 I0 = HallP(r0, pa, h), I1 = HallP(r0, pb, h), O0 = HallP(ra, pa, h), O1 = HallP(rb, pb, h);
        k.ob.Quad(I0, O0, O1, I1);
        k.Style(MAT_WOOD, 0x6b4a2e);
        Vec3 Ba = HallP(r0, pa, h - 0.18f), Bb = HallP(r0, pb, h - 0.18f);
        k.ob.Quad(Bb, Ba, I0, I1);
        float rc1 = std::min(r1, Lm + 0.3f);
        float rc = (r0 + rc1) * 0.5f;
        Vec3 dir(-cosf(pm), 0, sinf(pm));
        float yaw = atan2f(dir.x, dir.z);
        Vec3 c = HallP(rc, pm, (h + WELL - 0.1f) * 0.5f);
        Vec3 half((rc1 * dphi) * 0.55f + 0.02f, (h - (WELL - 0.1f)) * 0.5f, (rc1 - r0) * 0.5f + 0.01f);
        w.AddCollider(OBox(c, half, yaw), COL_SOLID, MAT_CARPET);
      }
    }
    // Abgeordnetensitze
    float aisles[] = {0, 28 * DEG2RAD, -28 * DEG2RAD, 56 * DEG2RAD, -56 * DEG2RAD};
    const char* desk[] = {"ordner_blau", "ordner_rot", "laptop", "wasserflasche", "aktenstapel", "namensschild"};
    Rng seatRng(77);
    int seats = 0;
    for (int row = 0; row <= 8; row++) {
      float rs = 4.8f + 1.2f * row + 0.55f;
      float h = WELL + 0.18f * row;
      float dp = 0.72f / rs;
      for (float phi = -85 * DEG2RAD + dp * 0.5f; phi < 85 * DEG2RAD; phi += dp) {
        bool skip = false;
        for (float a : aisles) if (fabsf(phi - a) * rs < 0.75f) skip = true;
        if (skip) continue;
        Vec3 p = HallP(rs, phi, h);
        if (fabsf(p.z) > 11.3f || p.x < -11.3f || rs + 0.45f > HallL(phi)) continue;
        float yaw = FaceYaw(p, Vec3(HC.x, h, HC.y));
        k.Prop("sitz", p, yaw);
        seats++;
        if (seatRng.Chance(0.12f)) {
          const char* id = desk[seatRng.Int(0, 5)];
          k.Prop(id, p + RotateY(Vec3(0, 0.53f, 0.05f), yaw), yaw + seatRng.Range(-0.4f, 0.4f));
        }
      }
    }
    LOG("Plenarsaal: %d Sitze", seats);

    // Präsidium
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(9.2f, WELL, -5), Vec3(12, -0.4f, 5));
    k.Solid(Vec3(9.2f, WELL, -6), Vec3(12, -0.2f, -5));
    k.Solid(Vec3(9.2f, WELL, 5), Vec3(12, -0.2f, 6));
    k.FloorFinish(9.2f, -6, 12, 6, -0.39f, MAT_CARPET, 0x3f4862);
    k.Stairs(Vec3(7.24f, WELL, 0), 0, 4, (-0.4f - WELL) / 7.0f, 7, 0.28f, WELL);
    k.Style(MAT_WOOD, 0x5a3d26);
    k.Solid(Vec3(9.6f, -0.4f, -3.6f), Vec3(10.3f, 0.35f, 3.6f));
    k.Solid(Vec3(9.5f, -0.4f, -3.7f), Vec3(9.62f, 0.78f, 3.7f));
    k.Style(MAT_GOLD, 0xc9a24a);
    k.ob.Push(Translation(Vec3(9.49f, 0.25f, 0)) * RotationY(-PI * 0.5f) * Scaling(Vec3(0.18f)));
    BuildEagle(k.ob);
    k.ob.Pop();
    for (float z : {-1.2f, 0.0f, 1.2f}) {
      k.Prop("sitz", Vec3(10.95f, -0.4f, z), -PI * 0.5f);
      k.Prop("mikrofon", Vec3(10.0f, 0.35f, z), -PI * 0.5f);
      k.Prop("namensschild", Vec3(9.8f, 0.35f, z + 0.35f), -PI * 0.5f);
    }
    k.Prop("glocke", Vec3(10.0f, 0.35f, -0.6f), 0);
    k.Prop("wasserglas", Vec3(10.05f, 0.35f, 0.6f), 0);
    k.Prop("aktenstapel", Vec3(10.0f, 0.35f, 2.5f), 0.2f);
    k.Prop("laptop", Vec3(10.0f, 0.35f, -2.5f), -PI * 0.5f);
    k.Prop("flagge", Vec3(11.3f, -0.4f, -4.4f), -PI * 0.5f);
    BigFlag(Vec3(11.3f, -0.4f, 4.4f), 2.4f, 0.9f, 0.6f, true, -PI * 0.5f);
    // Rednerpult und Stenografen
    k.Prop("rednerpult", Vec3(6.4f, WELL, 0), -PI * 0.5f);
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(4.0f, WELL, -1.6f), Vec3(4.8f, WELL + 0.74f, 1.6f));
    k.Prop("kantinenstuhl", Vec3(3.55f, WELL, -0.8f), PI * 0.5f);
    k.Prop("kantinenstuhl", Vec3(3.55f, WELL, 0.8f), PI * 0.5f);
    k.Prop("laptop", Vec3(4.4f, WELL + 0.74f, -0.8f), PI * 0.5f);
    k.Prop("aktenstapel", Vec3(4.4f, WELL + 0.74f, 0.8f), 0.1f);
    // Bundesadler an der Ostwand
    k.Style(MAT_PLASTER, 0xe2dacb);
    k.Visual(Vec3(11.9f, 1.6f, -4.8f), Vec3(12.0f, 9.0f, 4.8f), FACE_NX);
    k.Style(MAT_METAL, 0x5a5d63);
    k.ob.Push(Translation(Vec3(11.95f, 5.4f, 0)) * RotationY(-PI * 0.5f) * Scaling(Vec3(3.0f)));
    BuildEagle(k.ob);
    k.ob.Pop();
    // Regierungsbank (Nord) und Bundesratsbank (Süd)
    for (int s = -1; s <= 1; s += 2) {
      float zs = (float)s;
      k.Style(MAT_WOOD, 0x6b4a2e);
      k.Solid(Vec3(7.5f, WELL, std::min(zs * 6, zs * 12)), Vec3(12, 0, std::max(zs * 6, zs * 12)));
      k.FloorFinish(7.5f, std::min(zs * 6, zs * 12), 12, std::max(zs * 6, zs * 12), 0.01f, MAT_CARPET, 0x3f4862);
      k.Stairs(Vec3(8.35f, WELL, zs * 3.76f), s < 0 ? 3 : 2, 1.7f, 0.2025f, 8, 0.28f, WELL);
      k.Style(MAT_WOOD, 0x5a3d26);
      for (float zr : {9.2f, 7.2f}) {
        float zz = zs * zr;
        k.Solid(Vec3(9.3f, 0, std::min(zz, zz + zs * 0.3f)), Vec3(11.2f, 0.75f, std::max(zz, zz + zs * 0.3f)));
        for (float x = 9.6f; x <= 10.95f; x += 0.65f) {
          Vec3 p(x, 0, zz + zs * 0.85f);
          k.Prop("sitz", p, FaceYaw(p, Vec3(0, 0, 0)));
        }
      }
      k.Prop("mikrofon", Vec3(10.5f, 0.75f, zs * 7.35f), s < 0 ? PI : 0);
      k.Prop("wasserflasche", Vec3(11.0f, 0.75f, zs * 9.35f), 0);
      k.Prop("laptop", Vec3(9.9f, 0.75f, zs * 9.35f), s < 0 ? PI : 0);
    }
    // Licht im Saal
    for (int i = 0; i < 8; i++) {
      float a = TAU * i / 8;
      k.Light(Vec3(cosf(a) * 7.5f, 8.2f, sinf(a) * 7.5f), 15, Vec3(2.6f, 2.6f, 2.8f));
    }
    k.Light(Vec3(10.5f, 3.0f, 0), 7, Vec3(2.0f, 1.9f, 1.7f));
    k.Light(Vec3(0, 7.5f, 0), 14, Vec3(3.0f, 3.0f, 3.2f));

    // Galeriebestuhlung
    for (float x = -10.6f; x <= 4.6f; x += 1.8f) {
      k.Prop("sitzbank", Vec3(x, E1, -10.4f), 0);
      k.Prop("sitzbank", Vec3(x, E1, -11.35f), 0);
      k.Prop("sitzbank", Vec3(x, E1, 10.4f), PI);
      k.Prop("sitzbank", Vec3(x, E1, 11.35f), PI);
    }
    k.Style(MAT_WOOD, 0x5a3d26);
    k.Solid(Vec3(-10.6f, E1, -8.5f), Vec3(-10.0f, E1 + 0.75f, 8.5f));
    for (float z = -7.5f; z <= 7.5f; z += 1.5f) {
      k.Prop("buerostuhl", Vec3(-11.15f, E1, z), PI * 0.5f);
      if (seatRng.Chance(0.6f)) k.Prop(seatRng.Chance(0.5f) ? "laptop" : "aktenstapel", Vec3(-10.3f, E1 + 0.75f, z), PI * 0.5f);
    }
    k.Prop("tvkamera", Vec3(-11.0f, E1, -9.0f), FaceYaw(Vec3(-11, 0, -9), Vec3(6, 0, 0)));
    k.Prop("tvkamera", Vec3(-11.0f, E1, 9.0f), FaceYaw(Vec3(-11, 0, 9), Vec3(6, 0, 0)));

    // Glasdecke / Kuppelboden
    k.Style(MAT_PLASTER, 0xe9e6df);
    const int RS = 64;
    for (int i = 0; i < RS; i++) {
      float a0 = TAU * i / RS, a1 = TAU * (i + 1) / RS;
      auto sq = [](float a) {
        float c = cosf(a), s = sinf(a);
        float t = 12.6f / std::max(fabsf(c), fabsf(s));
        return Vec2(c * t, s * t);
      };
      Vec2 o0 = sq(a0), o1 = sq(a1);
      float ri = 11.6f;
      Vec3 I0t(cosf(a0) * ri, RF, sinf(a0) * ri), I1t(cosf(a1) * ri, RF, sinf(a1) * ri);
      Vec3 O0t(o0.x, RF, o0.y), O1t(o1.x, RF, o1.y);
      k.Style(MAT_TILE, 0xb8b4ac);
      k.ob.Quad(I0t, I1t, O1t, O0t);
      k.Style(MAT_PLASTER, 0xe9e6df);
      Vec3 dn(0, -0.4f, 0);
      k.ob.Quad(I0t + dn, O0t + dn, O1t + dn, I1t + dn);
      k.ob.Quad(I0t + dn, I1t + dn, I1t, I0t);
    }
    w.AddCollider(OBox(Vec3(0, RF - 0.2f, 0), Vec3(12.6f, 0.2f, 12.6f)), COL_SOLID, MAT_GLASS);
    k.gb.Mat(MAT_GLASS);
    k.gb.ColorHex(0xcfe4ee, 0.18f);
    k.gb.Ring(Vec3(0, RF, 0), 1.0f, 11.6f, 64, true);
    k.gb.Ring(Vec3(0, RF - 0.4f, 0), 1.0f, 11.6f, 64, false);
    k.Style(MAT_METAL, STEEL);
    for (int i = 0; i < 24; i++) {
      float a = TAU * i / 24;
      Vec3 d(cosf(a), 0, sinf(a));
      k.VisualR(d * 6.4f + Vec3(0, RF - 0.5f, 0), Vec3(0.06f, 0.09f, 5.2f), atan2f(d.x, d.z));
    }
    for (float r : {4.0f, 8.0f, 11.4f}) {
      for (int i = 0; i < 48; i++) {
        float a = TAU * (i + 0.5f) / 48;
        Vec3 d(cosf(a), 0, sinf(a));
        k.VisualR(d * r + Vec3(0, RF - 0.5f, 0), Vec3(r * TAU / 96 + 0.02f, 0.09f, 0.06f), atan2f(d.x, d.z));
      }
    }
  }

  // ------------------------------------------------------------------------------------------
  void Roof() {
    auto slab = [&](float x0, float z0, float x1, float z1) { k.Slab(x0, z0, x1, z1, RF, 0.4f, MAT_TILE, 0xb8b4ac); };
    slab(-29, -15.5f, -12.6f, 15.5f);
    slab(-25.5f, -19, -12.6f, -15.5f);
    slab(-25.5f, 15.5f, -12.6f, 19);
    slab(-12.6f, -19, 12.6f, -12.6f);
    slab(-12.6f, 12.6f, 12.6f, 19);
    slab(12.6f, -19, 25.5f, 19);
    slab(25.5f, -15.5f, 29, 15.5f);
    // Die Mauerkronen hinter der Brüstung bleiben Sandstein (eine Fliesenauflage darauf würde flackern)
  }

  void Dome() {
    const int SEG = 48, RINGS = 16;
    const float HT = 10.3f;
    // Türen nur dort, wo beide Rampen genug Kopfhöhe lassen (Rampe A startet bei 0°, B bei 180°)
    float doorA[2] = {160 * DEG2RAD, 340 * DEG2RAD};
    auto isDoor = [&](float a, float h1) {
      for (float d : doorA) if (fabsf(AngleDiff(a, d)) < 0.1f && h1 <= 2.7f) return true;
      return false;
    };
    auto P = [&](float r, float a, float h) { return Vec3(cosf(a) * r, RF + h, sinf(a) * r); };
    // Glas
    k.gb.Mat(MAT_GLASS);
    k.gb.ColorHex(0xd4e6ef, 0.16f);
    for (int j = 0; j < RINGS; j++) {
      float h0 = HT * j / RINGS, h1 = HT * (j + 1) / RINGS;
      float r0 = DomeR(h0), r1 = DomeR(h1);
      for (int i = 0; i < SEG; i++) {
        float a0 = TAU * i / SEG, a1 = TAU * (i + 1) / SEG;
        if (isDoor((a0 + a1) * 0.5f, h1)) continue;
        k.gb.Quad(P(r0, a0, h0), P(r0, a1, h0), P(r1, a1, h1), P(r1, a0, h1));
      }
    }
    // Stahlrippen
    k.Style(MAT_METAL, STEEL);
    for (int i = 0; i < 24; i++) {
      float a = TAU * i / 24;
      Vec3 tang(-sinf(a), 0, cosf(a));
      for (int j = 0; j < RINGS; j++) {
        float h0 = HT * j / RINGS, h1 = HT * (j + 1) / RINGS;
        Vec3 p0 = P(DomeR(h0), a, h0), p1 = P(DomeR(h1), a, h1);
        Vec3 y = p1 - p0;
        float len = Length(y);
        y = y / len;
        Vec3 z = Normalize(Cross(tang, y));
        k.ob.Push(Basis(tang, y, z, (p0 + p1) * 0.5f));
        k.ob.Box(Vec3(-0.06f, -len * 0.5f - 0.02f, -0.1f), Vec3(0.06f, len * 0.5f + 0.02f, 0.1f));
        k.ob.Pop();
      }
    }
    for (int j = 1; j <= RINGS; j++) {
      float h = HT * j / RINGS, r = DomeR(h);
      float t = j == RINGS ? 0.22f : 0.05f;
      for (int i = 0; i < SEG; i++) {
        float a = TAU * (i + 0.5f) / SEG;
        if (isDoor(a, h + 0.01f) && h < 2.7f) continue;
        Vec3 d(cosf(a), 0, sinf(a));
        k.VisualR(d * r + Vec3(0, RF + h, 0), Vec3(r * TAU / SEG * 0.5f + 0.02f, t, 0.08f + t * 0.5f), atan2f(d.x, d.z));
      }
    }
    // Sockelring und Kollision der Kuppelhaut
    k.Style(MAT_CONCRETE, 0xcfcac0);
    for (int i = 0; i < SEG; i++) {
      float a = TAU * (i + 0.5f) / SEG;
      Vec3 d(cosf(a), 0, sinf(a));
      float yaw = atan2f(d.x, d.z);
      bool door = isDoor(a, 0.5f);
      if (!door) k.SolidR(d * 12.05f + Vec3(0, RF + 0.22f, 0), Vec3(12.05f * TAU / SEG * 0.5f + 0.03f, 0.22f, 0.3f), yaw);
      // Haut nur in Bodennähe kollidierbar; weiter oben halten die hohen Außengeländer der Rampen
      for (int b = 0; b < 3; b++) {
        if (door && b < 2) continue;
        float r = DomeR(b + 0.5f) + 0.12f;
        w.AddCollider(OBox(d * r + Vec3(0, RF + b + 0.5f, 0), Vec3(r * TAU / SEG * 0.55f + 0.05f, 0.5f, 0.12f), yaw),
                      COL_SOLID | COL_GLASS, MAT_GLASS);
      }
    }
    // Spiralrampen
    const int N = 144;
    // Rampenbreite 1.6 m, Kopfhöhe 2.1 m unter dem Glas; letzte 8 % sind ein flaches Podest an der Plattform
    auto ro = [&](float h) { return DomeR(std::min(h + 2.1f, 10.9f)) - 0.1f; };
    auto ri = [&](float h) { return ro(h) - 1.6f; };
    auto rampH = [&](float t) { return 0.05f + 5.95f * std::min(t / 0.92f, 1.0f); };
    for (int ramp = 0; ramp < 2; ramp++) {
      float as = ramp * PI;
      for (int s = 0; s < N; s++) {
        float t0 = (float)s / N, t1 = (float)(s + 1) / N;
        float h0 = rampH(t0), h1 = rampH(t1);
        float a0 = as + TAU * t0, a1 = as + TAU * t1;
        Vec3 I0 = P(ri(h0), a0, h0), I1 = P(ri(h1), a1, h1), O0 = P(ro(h0), a0, h0), O1 = P(ro(h1), a1, h1);
        Vec3 dn(0, -0.25f, 0);
        k.Style(MAT_METAL, 0xe8eaec);
        k.ob.Quad(I0, I1, O1, O0);
        k.ob.Quad(I0 + dn, O0 + dn, O1 + dn, I1 + dn);
        k.ob.Quad(I0 + dn, I1 + dn, I1, I0);
        k.ob.Quad(O1 + dn, O0 + dn, O0, O1);
        float am = (a0 + a1) * 0.5f, hm = (h0 + h1) * 0.5f;
        Vec3 d(cosf(am), 0, sinf(am));
        float yaw = atan2f(d.x, d.z);
        float rm = (ri(hm) + ro(hm)) * 0.5f;
        w.AddCollider(OBox(d * rm + Vec3(0, RF + hm - 0.15f, 0), Vec3(ro(hm) * (TAU / N) * 0.58f, 0.15f, 0.82f), yaw), COL_SOLID, MAT_METAL);
        // Geländer innen und außen
        Vec3 up(0, 1.0f, 0);
        k.gb.Mat(MAT_GLASS);
        k.gb.ColorHex(0xc4dbe8, 0.2f);
        bool landing = t0 >= 0.9f;
        if (!landing) k.gb.Quad(I0, I1, I1 + up, I0 + up);
        k.gb.Quad(O1, O0, O0 + up, O1 + up);
        k.Style(MAT_METAL, 0x9aa0a8);
        for (int side = landing ? 1 : 0; side < 2; side++) {
          Vec3 a = (side == 0 ? I0 : O0) + up, b2 = (side == 0 ? I1 : O1) + up;
          Vec3 ax = b2 - a;
          float len = Length(ax);
          ax = ax / len;
          Vec3 az = Normalize(Cross(ax, Vec3(0, 1, 0)));
          Vec3 ay = Cross(az, ax);
          k.ob.Push(Basis(ax, ay, az, (a + b2) * 0.5f));
          k.ob.Box(Vec3(-len * 0.5f - 0.01f, -0.025f, -0.03f), Vec3(len * 0.5f + 0.01f, 0.025f, 0.03f));
          k.ob.Pop();
        }
        if (s % 4 == 0) {
          float t2 = std::min(1.0f, (s + 4.0f) / N);
          float hm4 = rampH((t0 + t2) * 0.5f);
          float am4 = as + TAU * (t0 + t2) * 0.5f;
          Vec3 d4(cosf(am4), 0, sinf(am4));
          float yaw4 = atan2f(d4.x, d4.z);
          for (int side = t0 >= 0.88f ? 1 : 0; side < 2; side++) {
            float r = side == 0 ? ri(hm4) - 0.05f : ro(hm4) + 0.05f;
            float hh = side == 0 ? 0.6f : 1.2f;  // außen unsichtbar erhöht: kein Sprung durchs Kuppelglas
            w.AddCollider(OBox(d4 * r + Vec3(0, RF + hm4 + hh - 0.1f, 0), Vec3(r * (TAU / N) * 2.1f, hh, 0.05f), yaw4),
                          COL_SOLID | COL_GLASS, MAT_GLASS);
          }
        }
      }
    }
    // Aussichtsplattform
    float PY = RF + 6.0f;
    const float PR0 = 2.5f, PR1 = 6.4f;
    k.Style(MAT_METAL, 0xe8eaec);
    k.ob.Ring(Vec3(0, PY, 0), PR0, PR1, 48, true);
    k.ob.Ring(Vec3(0, PY - 0.3f, 0), PR0, PR1, 48, false);
    k.ob.Frustum(Vec3(0, PY - 0.3f, 0), PR1, PR1, 0.3f, 48, false);
    for (int i = 0; i < 32; i++) {
      float a = TAU * (i + 0.5f) / 32;
      Vec3 d(cosf(a), 0, sinf(a));
      w.AddCollider(OBox(d * ((PR0 + PR1) * 0.5f) + Vec3(0, PY - 0.15f, 0), Vec3(PR1 * TAU / 64 * 1.1f, 0.15f, (PR1 - PR0) * 0.5f), atan2f(d.x, d.z)), COL_SOLID, MAT_METAL);
      bool nearEnd = false;
      for (float e : {TAU, PI}) {
        float diff = AngleDiff(a, e);
        if (diff > -4 * DEG2RAD && diff < 36 * DEG2RAD) nearEnd = true;
      }
      if (nearEnd) continue;
      float a0 = TAU * i / 32, a1 = TAU * (i + 1) / 32;
      Vec3 p0(cosf(a0) * PR1, PY, sinf(a0) * PR1), p1(cosf(a1) * PR1, PY, sinf(a1) * PR1);
      k.Railing(p0, p1, 1.0f, true);
    }
    // Spiegelkegel
    const float CY0 = 9.0f, CY1 = 20.3f, CR0 = 0.9f, CR1 = 3.2f;
    k.Style(MAT_MIRROR, 0xdfe6ee);
    const int CB = 12, CS = 24;
    for (int b = 0; b < CB; b++) {
      float y0 = Lerp(CY0, CY1, (float)b / CB), y1 = Lerp(CY0, CY1, (float)(b + 1) / CB);
      float r0 = Lerp(CR0, CR1, (float)b / CB), r1 = Lerp(CR0, CR1, (float)(b + 1) / CB);
      for (int i = 0; i < CS; i++) {
        float a0 = TAU * i / CS, a1 = TAU * (i + 1) / CS;
        Vec3 B0(cosf(a0) * r0, y0, sinf(a0) * r0), B1(cosf(a1) * r0, y0, sinf(a1) * r0);
        Vec3 T0(cosf(a0) * r1, y1, sinf(a0) * r1), T1(cosf(a1) * r1, y1, sinf(a1) * r1);
        k.ob.Quad(B1, B0, T0, T1);
      }
      float rm = (r0 + r1) * 0.5f * 0.78f;
      w.AddCollider(OBox(Vec3(0, (y0 + y1) * 0.5f, 0), Vec3(rm, (y1 - y0) * 0.5f, rm), (b % 2) * PI * 0.25f), COL_SOLID, MAT_MIRROR);
    }
    k.Style(MAT_METAL, STEEL);
    k.ob.Ring(Vec3(0, CY1, 0), 0.2f, CR1 + 0.1f, CS, true);
    k.Style(MAT_PLASTIC, 0xfff8e8);
    k.Glow(0.6f);
    k.ob.Ring(Vec3(0, CY0, 0), 0.0f, CR0, CS, false);
    k.Glow(0);
    k.Light(Vec3(0, 8.0f, 0), 12, Vec3(3.0f, 3.0f, 3.1f));
    // Bänke in der Kuppel
    for (int i = 0; i < 6; i++) {
      float a = TAU * i / 6 + 0.3f;
      Vec3 p(cosf(a) * 4.2f, RF, sinf(a) * 4.2f);
      k.Prop("sitzbank", p, FaceYaw(p, Vec3(0, RF, 0)) + PI);
    }
  }

  void Restaurant() {
    float x0 = 17, x1 = 27, z0 = -8, z1 = 8, top = RF + 3.0f;
    k.Glass(Vec3(x0 - 0.05f, RF, z0), Vec3(x0 + 0.05f, top, -1.2f));
    k.Glass(Vec3(x0 - 0.05f, RF, 1.2f), Vec3(x0 + 0.05f, top, z1));
    k.Glass(Vec3(x1 - 0.05f, RF, z0), Vec3(x1 + 0.05f, top, z1));
    k.Glass(Vec3(x0, RF, z0 - 0.05f), Vec3(x1, top, z0 + 0.05f));
    k.Glass(Vec3(x0, RF, z1 - 0.05f), Vec3(x1, top, z1 + 0.05f));
    k.Style(MAT_METAL, 0x3a3d42);
    for (float x = x0; x <= x1 + 0.01f; x += 2) {
      k.Solid(Vec3(x - 0.06f, RF, z0 - 0.06f), Vec3(x + 0.06f, top, z0 + 0.06f));
      k.Solid(Vec3(x - 0.06f, RF, z1 - 0.06f), Vec3(x + 0.06f, top, z1 + 0.06f));
    }
    for (float z = z0 + 2; z <= z1 - 1.9f; z += 2) {
      if (fabsf(z) > 1.3f) k.Solid(Vec3(x0 - 0.06f, RF, z - 0.06f), Vec3(x0 + 0.06f, top, z + 0.06f));
      k.Solid(Vec3(x1 - 0.06f, RF, z - 0.06f), Vec3(x1 + 0.06f, top, z + 0.06f));
    }
    k.Style(MAT_PLASTER, 0xf2f0ea);
    k.Solid(Vec3(x0 - 0.5f, top, z0 - 0.5f), Vec3(x1 + 0.5f, top + 0.3f, z1 + 0.5f));
    k.FloorFinish(x0, z0, x1, z1, RF + 0.015f, MAT_PARQUET, 0xa47a50);
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(25, RF, -5), Vec3(26.2f, RF + 1.05f, 5));
    k.CeilingLight(Vec3(21, top, -4), 8, Vec3(2.4f, 2.1f, 1.7f));
    k.CeilingLight(Vec3(21, top, 4), 8, Vec3(2.4f, 2.1f, 1.7f));
    k.Prop("kaffeemaschine", Vec3(25.6f, RF + 1.05f, -3.5f), -PI * 0.5f);
    k.Prop("tablett", Vec3(25.6f, RF + 1.05f, -1.0f), -PI * 0.5f);
    k.Prop("mate", Vec3(25.5f, RF + 1.05f, 0.5f), 0);
    k.Prop("mate", Vec3(25.7f, RF + 1.05f, 0.65f), 0);
    k.Prop("brezel", Vec3(25.6f, RF + 1.05f, 2.0f), 0.3f);
    k.Prop("currywurst", Vec3(25.6f, RF + 1.05f, 3.5f), 0.5f);
    k.Prop("ficus", Vec3(26.3f, RF, -7.3f), 0);
    k.Prop("ficus", Vec3(26.3f, RF, 7.3f), 0);
    for (Vec3 t : {Vec3(19.4f, RF, -5.5f), Vec3(19.4f, RF, 5.5f), Vec3(22.6f, RF, -5.8f), Vec3(22.6f, RF, -2.2f),
                   Vec3(22.6f, RF, 2.2f), Vec3(22.6f, RF, 5.8f)})
      TableSet(t, true);
  }

  // ------------------------------------------------------------------------------------------
  void DeskSet(Vec3 p, float yaw, bool twoMonitors = false) {
    k.PropRel("schreibtisch", p, yaw, Vec3(0, 0, 0));
    k.PropRel("buerostuhl", p, yaw, Vec3(rng.Range(-0.2f, 0.2f), 0, 0.72f), PI + rng.Range(-0.4f, 0.4f));
    k.PropRel("monitor", p, yaw, Vec3(twoMonitors ? -0.3f : 0.0f, 0.76f, -0.12f), 0);
    if (twoMonitors) k.PropRel("monitor", p, yaw, Vec3(0.32f, 0.76f, -0.12f), -0.2f);
    k.PropRel("pc", p, yaw, Vec3(-0.55f, 0, 0.0f), 0);
    if (rng.Chance(0.7f)) k.PropRel("telefon", p, yaw, Vec3(0.55f, 0.76f, 0.1f), -0.3f);
    if (rng.Chance(0.6f)) k.PropRel("kaffeebecher", p, yaw, Vec3(0.3f, 0.76f, 0.22f), rng.Range(0, TAU));
    if (rng.Chance(0.5f)) k.PropRel("aktenstapel", p, yaw, Vec3(-0.5f, 0.76f, 0.1f), rng.Range(-0.3f, 0.3f));
    if (rng.Chance(0.4f)) k.PropRel("tischlampe", p, yaw, Vec3(0.65f, 0.76f, -0.25f), 0);
    if (rng.Chance(0.3f)) k.PropRel("papierkorb", p, yaw, Vec3(0.95f, 0, 0.1f), 0);
  }

  void TableSet(Vec3 p, bool food) {
    float yaw = rng.Chance(0.5f) ? 0 : PI;
    k.PropRel("kantinentisch", p, yaw, Vec3(0, 0, 0));
    for (int i = 0; i < 4; i++) {
      if (rng.Chance(0.15f)) continue;
      float sx = (i & 1) ? 0.3f : -0.3f;
      float sz = (i & 2) ? 0.68f : -0.68f;
      k.PropRel("kantinenstuhl", p, yaw, Vec3(sx + rng.Range(-0.08f, 0.08f), 0, sz + rng.Range(-0.05f, 0.1f) * Sign(sz)),
                (sz > 0 ? PI : 0) + rng.Range(-0.3f, 0.3f));
    }
    if (!food) return;
    const char* items[] = {"tablett", "currywurst", "mate", "kaffeebecher", "brezel", "wasserglas"};
    int n = rng.Int(0, 3);
    for (int i = 0; i < n; i++)
      k.PropRel(items[rng.Int(0, 5)], p, yaw, Vec3(rng.Range(-0.4f, 0.4f), 0.76f, rng.Range(-0.2f, 0.2f)), rng.Range(0, TAU));
  }

  void FurnishGroundFloor() {
    // --- Eingangshalle ---
    for (int s = -1; s <= 1; s += 2) {
      float zs = (float)s;
      k.Prop("detektor", Vec3(-26.2f, 0, zs * 5.0f), PI * 0.5f);
      k.Prop("roentgen", Vec3(-26.2f, 0, zs * 7.4f), PI * 0.5f);
      k.Prop("kordel", Vec3(-28.3f, 0, zs * 3.3f), 0);
      k.Prop("kordel", Vec3(-27.3f, 0, zs * 3.3f), 0);
      k.Prop("kordel", Vec3(-24.4f, 0, zs * 3.4f), 0);
      k.Prop("bueste", Vec3(-26.6f, 0, zs * 3.4f), -PI * 0.5f);
      k.Style(MAT_WOOD, 0x5a3d26);
      k.Solid(Vec3(-21.5f, 0, std::min(zs * 8.5f, zs * 11.5f)), Vec3(-20, 1.05f, std::max(zs * 8.5f, zs * 11.5f)));
      k.Style(MAT_MARBLE, 0xe9e5dc);
      k.Solid(Vec3(-21.6f, 1.05f, std::min(zs * 8.4f, zs * 11.6f)), Vec3(-19.9f, 1.1f, std::max(zs * 8.4f, zs * 11.6f)));
      k.Prop("monitor", Vec3(-20.6f, 1.1f, zs * 10.0f), PI * 0.5f);
      k.Prop("telefon", Vec3(-20.5f, 1.1f, zs * 9.0f), PI * 0.5f);
      k.Prop("buerostuhl", Vec3(-19.3f, 0, zs * 10.0f), -PI * 0.5f);
      k.Prop("pflanze", Vec3(-20.7f, 1.1f, zs * 11.1f), 0);
      for (float x : {-24.3f, -18.6f}) k.Prop("sitzbank", Vec3(x, 0, zs * 15.0f), s < 0 ? 0 : PI);
      k.Prop("ficus", Vec3(-28.3f, 0, zs * 14.8f), 0);
      k.Prop("ficus", Vec3(-16.2f, 0, zs * 14.8f), 0);
      k.Prop("ficus", Vec3(-16.1f, 0, zs * 3.2f), 0);
      k.Prop("garderobe", Vec3(-28.4f, 0, zs * 8.6f), 0);
      k.Prop("feuerloescher", Vec3(-15.75f, 0, zs * 5.0f), -PI * 0.5f);
      k.Prop("abfalleimer", Vec3(-28.4f, 0, zs * 11.0f), 0);
      k.Prop("flagge", Vec3(-17.3f, E1, zs * 2.9f), -PI * 0.5f);
    }
    k.Prop("schirmstaender", Vec3(-28.5f, 0, -10.2f), 0);
    k.Prop("koffer", Vec3(-25.0f, 0, -9.2f), 0.4f);
    k.Prop("koffer", Vec3(-24.4f, 0, 9.6f), -0.8f);
    k.Prop("adlerfigur", Vec3(-17.0f, 0, -12.0f), -PI * 0.5f);
    k.Prop("wahlurne", Vec3(-17.0f, 0, 12.0f), -PI * 0.5f);
    k.Sign("PLENARSAAL", Vec3(-15.56f, 3.95f, 0), -PI * 0.5f, 0.055f, 0xd9b54a);
    k.Sign("AUSGANG", Vec3(-28.94f, 4.6f, 0), PI * 0.5f, 0.045f, 0x3be36b, 1.0f);

    // --- Umgänge ---
    for (int s = -1; s <= 1; s += 2) {
      float zs = (float)s;
      k.Prop("ficus", Vec3(-14.6f, 0, zs * 14.6f), 0);
      k.Prop("sitzbank", Vec3(-14.95f, 0, zs * 4.3f), PI * 0.5f);
      k.Prop("ficus", Vec3(14.6f, 0, zs * 14.6f), 0);
    }
    k.Prop("wasserspender", Vec3(-15.0f, 0, 12.2f), PI * 0.5f);
    k.Prop("feuerloescher", Vec3(-12.75f, 0, -9.0f), -PI * 0.5f);
    for (float x : {-8.2f, -3.5f, 4.5f, 7.5f, 12.5f}) k.Prop("bueste", Vec3(x, 0, -14.85f), 0);
    k.Prop("sitzbank", Vec3(0, 0, -12.95f), PI);
    k.Prop("automat", Vec3(-3.8f, 0, 14.72f), PI);
    k.Prop("automat", Vec3(-2.7f, 0, 14.72f), PI);
    k.Prop("sitzbank", Vec3(3.5f, 0, 12.95f), 0);
    k.Prop("staffelei", Vec3(-8.8f, 0, 14.6f), PI);
    k.Prop("staffelei", Vec3(12.4f, 0, 14.6f), PI);
    k.Prop("papierkorb", Vec3(-1.8f, 0, 14.9f), 0);
    k.Prop("kopierer", Vec3(14.78f, 0, 4.0f), -PI * 0.5f);
    k.Prop("wasserspender", Vec3(14.9f, 0, -4.5f), -PI * 0.5f);
    k.Prop("aktenschrank", Vec3(14.95f, 0, 11.0f), -PI * 0.5f);
    k.Prop("feuermelder", Vec3(14.98f, 1.4f, -10.5f), -PI * 0.5f);
    k.Sign("KANTINE", Vec3(15.16f, 3.3f, -4.6f), -PI * 0.5f, 0.05f, 0xd9b54a);

    // --- Nordflügel ---
    // Garderobe
    k.Style(MAT_WOOD, 0x6b4a2e);
    k.Solid(Vec3(-23.5f, 0, -17.6f), Vec3(-19.5f, 1.05f, -17.0f));
    for (float x = -24.0f; x <= -18.6f; x += 1.1f) k.Prop("garderobe", Vec3(x, 0, -18.5f), rng.Range(0, TAU));
    k.Prop("koffer", Vec3(-23.2f, 0, -17.4f), 0.5f);
    k.Prop("koffer", Vec3(-19.5f, 0, -17.6f), -0.3f);
    k.Prop("schirmstaender", Vec3(-18.6f, 0, -16.0f), 0);
    k.Prop("sitzbank", Vec3(-24.3f, 0, -15.95f), PI);
    // Büro Nord 1
    DeskSet(Vec3(-16.4f, 0, -18.55f), 0);
    DeskSet(Vec3(-12.4f, 0, -18.55f), 0);
    k.Prop("aktenschrank", Vec3(-17.65f, 0, -16.4f), PI * 0.5f);
    k.Prop("regal", Vec3(-10.3f, 0, -17.2f), -PI * 0.5f);
    k.Prop("ficus", Vec3(-10.9f, 0, -15.95f), 0);
    // Bibliothek
    for (float x = -9.35f; x <= -2.6f; x += 1.0f) k.Prop("regal", Vec3(x, 0, -18.65f), 0);
    k.Prop("kantinentisch", Vec3(-8.4f, 0, -16.9f), 0);
    k.Prop("kantinenstuhl", Vec3(-8.7f, 0, -16.2f), PI);
    k.Prop("kantinenstuhl", Vec3(-8.0f, 0, -16.2f), PI + 0.3f);
    k.Prop("tischlampe", Vec3(-8.8f, 0.76f, -17.1f), 0);
    k.Prop("aktenstapel", Vec3(-8.0f, 0.76f, -17.0f), 0.4f);
    k.Prop("globus", Vec3(-4.4f, 0, -16.0f), 0.5f);
    k.Prop("sessel", Vec3(-3.4f, 0, -16.4f), -PI * 0.5f);
    k.Prop("stehlampe", Vec3(-2.7f, 0, -15.85f), 0);
    // Poststelle
    k.Style(MAT_WOOD, 0x9a7a52);
    k.Solid(Vec3(-1.6f, 0, -18.8f), Vec3(1.6f, 0.8f, -18.0f));
    for (int i = 0; i < 4; i++) k.Prop("paket", Vec3(-1.2f + i * 0.8f, 0.8f, -18.4f), rng.Range(-0.3f, 0.3f));
    k.Prop("kiste", Vec3(5.2f, 0, -18.4f), 0);
    k.Prop("kiste", Vec3(5.2f, 0.7f, -18.4f), 0.1f);
    k.Prop("karton", Vec3(4.2f, 0, -18.5f), 0);
    k.Prop("karton", Vec3(4.2f, 0.36f, -18.5f), 0.2f);
    k.Prop("karton", Vec3(3.4f, 0, -18.45f), -0.1f);
    k.Prop("paket", Vec3(3.5f, 0, -16.6f), 0.7f);
    k.Prop("paket", Vec3(4.6f, 0, -16.3f), 0.2f);
    k.Prop("aktenschrank", Vec3(-1.6f, 0, -16.1f), PI * 0.5f);
    k.Prop("getraenkekiste", Vec3(0.4f, 0, -16.2f), 0);
    // Büro Nord 2
    DeskSet(Vec3(8.0f, 0, -18.55f), 0);
    DeskSet(Vec3(11.9f, 0, -18.55f), 0);
    k.Prop("kopierer", Vec3(13.45f, 0, -16.4f), -PI * 0.5f);
    k.Prop("ficus", Vec3(6.6f, 0, -15.95f), 0);
    // Sanitätsraum
    k.Prop("sofa", Vec3(16.8f, 0, -18.45f), 0);
    DeskSet(Vec3(20.6f, 0, -18.55f), 0);
    k.Prop("aktenschrank", Vec3(23.4f, 0, -18.7f), 0);
    k.Prop("wasserspender", Vec3(25.0f, 0, -18.7f), 0);
    k.Prop("feuerloescher", Vec3(14.4f, 0, -16.0f), PI * 0.5f);
    k.Prop("koffer", Vec3(24.6f, 0, -16.2f), 1.2f);

    // --- Südflügel ---
    // Sicherheitszentrale
    DeskSet(Vec3(-23.6f, 0, 18.55f), PI, true);
    DeskSet(Vec3(-20.4f, 0, 18.55f), PI, true);
    k.Style(MAT_MIRROR, 0x101418);
    k.Visual(Vec3(-25.0f, 2.0f, 18.95f), Vec3(-19.0f, 3.6f, 19.0f));
    k.Style(MAT_SCREEN, 0x4a7ab0);
    for (int i = 0; i < 6; i++) {
      float x = -24.8f + i * 1.0f;
      k.Visual(Vec3(x, 2.1f, 18.9f), Vec3(x + 0.9f, 2.75f, 18.95f));
      k.Visual(Vec3(x, 2.85f, 18.9f), Vec3(x + 0.9f, 3.5f, 18.95f));
    }
    k.Style(MAT_METAL, 0xb9bdc3);
    k.Solid(Vec3(-19.6f, 0, 15.8f), Vec3(-18.9f, 0.9f, 17.2f));
    k.Prop("kaffeemaschine", Vec3(-19.25f, 0.9f, 16.2f), -PI * 0.5f);
    k.Prop("kaffeebecher", Vec3(-19.2f, 0.9f, 16.9f), 0);
    k.Prop("aktenschrank", Vec3(-25.0f, 0, 15.95f), PI * 0.5f);
    // Fraktionsbüro
    DeskSet(Vec3(-16.2f, 0, 18.55f), PI);
    DeskSet(Vec3(-12.4f, 0, 18.55f), PI);
    DeskSet(Vec3(-8.6f, 0, 18.55f), PI);
    k.Prop("kopierer", Vec3(-6.75f, 0, 16.2f), -PI * 0.5f);
    k.Prop("ficus", Vec3(-17.4f, 0, 15.95f), 0);
    k.Prop("aktenschrank", Vec3(-14.5f, 0, 15.85f), PI);
    // Andachtsraum
    k.Style(MAT_MARBLE, 0xeeeae2);
    k.Solid(Vec3(-5.6f, 0, 16.4f), Vec3(-4.8f, 0.95f, 18.2f));
    k.Prop("vase", Vec3(-5.2f, 0.95f, 16.9f), 0);
    k.Prop("pflanze", Vec3(-5.2f, 0.95f, 17.7f), 0);
    for (float x = -3.2f; x <= 4.0f; x += 1.45f) k.Prop("sitzbank", Vec3(x, 0, 17.3f), -PI * 0.5f);
    k.Light(Vec3(-5.0f, 2.2f, 17.3f), 4, Vec3(1.6f, 1.2f, 0.8f));
    // Technikraum
    for (float x = 6.7f; x <= 13.0f; x += 0.82f) k.Prop("aktenschrank", Vec3(x, 0, 18.75f), PI);
    DeskSet(Vec3(12.4f, 0, 16.1f), 0);
    k.Prop("klima", Vec3(7.3f, 0, 16.2f), 0);
    // Lager
    {
      Rng r(5);
      const char* stuff[] = {"kiste", "karton", "getraenkekiste", "stapelstuehle", "karton", "paket", "kiste"};
      for (float x = 15.0f; x <= 24.8f; x += 1.1f) {
        const char* id = stuff[r.Int(0, 6)];
        int pi = k.Prop(id, Vec3(x, 0, 18.5f), r.Range(-0.2f, 0.2f));
        if (pi >= 0 && (strcmp(id, "kiste") == 0 || strcmp(id, "karton") == 0) && r.Chance(0.6f))
          k.Prop(r.Chance(0.5f) ? "karton" : "paket", Vec3(x, strcmp(id, "kiste") == 0 ? 0.7f : 0.36f, 18.5f), r.Range(-0.4f, 0.4f));
      }
      k.Prop("gitter", Vec3(22.0f, 0, 16.0f), 0);
      k.Prop("huetchen", Vec3(15.2f, 0, 16.0f), 0);
      k.Prop("huetchen", Vec3(15.6f, 0, 16.3f), 0);
      k.Prop("kochtopf", Vec3(24.0f, 0, 16.2f), 0);
      k.Prop("feuerloescher", Vec3(25.2f, 0, 15.9f), PI);
    }

    // --- Kantine ---
    k.Style(MAT_METAL, 0xb9bdc3);
    k.Solid(Vec3(26.6f, 0, -13.5f), Vec3(28.0f, 1.0f, -2.6f));
    k.Glass(Vec3(26.6f, 1.0f, -13.0f), Vec3(26.66f, 1.42f, -3.0f));
    k.Prop("tablett", Vec3(27.3f, 1.0f, -12.4f), -PI * 0.5f);
    k.Prop("tablett", Vec3(27.3f, 1.03f, -12.4f), -PI * 0.5f);
    for (int i = 0; i < 3; i++) k.Prop("currywurst", Vec3(27.25f, 1.0f, -10.8f + i * 0.5f), -PI * 0.5f);
    for (int i = 0; i < 3; i++) k.Prop("brezel", Vec3(27.3f, 1.0f, -8.6f + i * 0.35f), i * 0.7f);
    for (int i = 0; i < 4; i++) k.Prop("mate", Vec3(27.2f + (i % 2) * 0.12f, 1.0f, -6.6f + i * 0.13f), 0);
    k.Prop("kaffeemaschine", Vec3(27.4f, 1.0f, -3.4f), -PI * 0.5f);
    k.Prop("kaffeebecher", Vec3(27.0f, 1.0f, -2.4f), 0);
    k.Prop("kaffeebecher", Vec3(27.1f, 1.0f, -2.0f), 1);
    k.Prop("wasserglas", Vec3(27.0f, 1.0f, -4.4f), 0);
    for (float x : {17.6f, 20.6f, 23.6f})
      for (float z : {-12.6f, -9.0f, -5.4f, -1.6f}) TableSet(Vec3(x, 0, z), true);
    k.Prop("automat", Vec3(17.0f, 0, -14.72f), 0);
    k.Prop("automat", Vec3(18.15f, 0, -14.72f), 0);
    k.Prop("abfalleimer", Vec3(25.3f, 0, -14.9f), 0);
    k.Prop("abfalleimer", Vec3(16.0f, 0, 2.4f), 0);
    k.Prop("ficus", Vec3(28.3f, 0, 2.3f), 0);
    k.Prop("wasserspender", Vec3(25.9f, 0, 2.6f), PI);
    k.Prop("stapelstuehle", Vec3(24.4f, 0, 2.4f), 0.2f);
    // Küche
    k.Style(MAT_METAL, 0xb9bdc3);
    k.Solid(Vec3(27.6f, 0, 4.0f), Vec3(29, 0.92f, 15.2f));
    k.Solid(Vec3(23.5f, 0, 3.3f), Vec3(27.6f, 0.92f, 4.0f));
    k.Solid(Vec3(19.5f, 0, 8.0f), Vec3(24.5f, 0.92f, 9.4f));
    for (int i = 0; i < 4; i++) k.Prop("kochtopf", Vec3(28.3f, 0.92f, 5.0f + i * 1.2f), i * 0.5f);
    k.Prop("mikrowelle", Vec3(28.3f, 0.92f, 10.6f), -PI * 0.5f);
    k.Prop("mikrowelle", Vec3(28.3f, 0.92f, 11.8f), -PI * 0.5f);
    k.Prop("kaffeemaschine", Vec3(26.4f, 0.92f, 3.65f), 0);
    k.Prop("kochtopf", Vec3(21.0f, 0.92f, 8.7f), 0);
    k.Prop("tablett", Vec3(22.8f, 0.92f, 8.7f), 0.3f);
    k.Prop("brezel", Vec3(23.9f, 0.92f, 8.5f), 0.3f);
    k.Prop("aktenschrank", Vec3(16.0f, 0, 14.9f), PI);
    k.Prop("aktenschrank", Vec3(16.85f, 0, 14.9f), PI);
    k.Prop("getraenkekiste", Vec3(18.4f, 0, 14.9f), 0);
    k.Prop("getraenkekiste", Vec3(18.4f, 0.3f, 14.9f), 0.1f);
    k.Prop("kiste", Vec3(16.3f, 0, 4.2f), 0);
    k.Prop("karton", Vec3(17.4f, 0, 4.1f), 0.3f);
  }

  void FurnishUpperFloor() {
    const char* tableItems[] = {"mikrofon", "wasserflasche", "wasserglas", "namensschild", "laptop", "aktenstapel"};
    // Fraktionssaal A: Tischreihen Richtung Osten
    for (float x : {-13.0f, -10.0f, -7.0f, -4.6f}) {
      k.Style(MAT_WOOD, 0x8a6440);
      k.Solid(Vec3(x - 0.35f, E1, -18.4f), Vec3(x + 0.35f, E1 + 0.75f, -13.6f));
      for (float z = -17.8f; z <= -14.0f; z += 1.2f) {
        k.Prop("buerostuhl", Vec3(x - 0.75f, E1, z), PI * 0.5f + rng.Range(-0.3f, 0.3f));
        if (rng.Chance(0.7f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x - 0.1f, E1 + 0.75f, z), PI * 0.5f);
      }
    }
    k.Prop("rednerpult", Vec3(-2.2f, E1, -18.2f), -PI * 0.5f);
    k.Prop("flagge", Vec3(-1.7f, E1, -18.3f), -PI * 0.5f);
    k.Style(MAT_SCREEN, 0x6fa0d8);
    k.Visual(Vec3(-1.16f, 6.0f, -18.2f), Vec3(-1.13f, 8.4f, -13.6f));
    // Fraktionssaal B: Reihen Richtung Norden
    for (float z : {-16.9f, -14.9f}) {
      k.Style(MAT_WOOD, 0x8a6440);
      k.Solid(Vec3(1.0f, E1, z - 0.3f), Vec3(11.5f, E1 + 0.75f, z + 0.3f));
      for (float x = 1.6f; x <= 11.0f; x += 1.2f) {
        k.Prop("buerostuhl", Vec3(x, E1, z + 0.75f), PI + rng.Range(-0.3f, 0.3f));
        if (rng.Chance(0.7f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x, E1 + 0.75f, z + 0.1f), PI);
      }
    }
    k.Prop("rednerpult", Vec3(6.2f, E1, -18.4f), 0);
    k.Prop("flagge", Vec3(8.2f, E1, -18.5f), 0);
    k.Prop("ficus", Vec3(-0.4f, E1, -18.4f), 0);
    k.Prop("ficus", Vec3(12.9f, E1, -13.2f), 0);
    // Sitzungssaal
    k.Style(MAT_WOOD, 0x4a3020);
    k.Solid(Vec3(15.5f, E1, -17.3f), Vec3(23.5f, E1 + 0.75f, -14.3f));
    for (float x = 16.0f; x <= 23.1f; x += 1.4f) {
      k.Prop("buerostuhl", Vec3(x, E1, -17.95f), 0);
      k.Prop("buerostuhl", Vec3(x, E1, -13.65f), PI);
      if (rng.Chance(0.6f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x, E1 + 0.75f, -16.9f), 0);
      if (rng.Chance(0.6f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x, E1 + 0.75f, -14.7f), PI);
    }
    k.Prop("buerostuhl", Vec3(14.8f, E1, -15.8f), PI * 0.5f);
    k.Prop("buerostuhl", Vec3(24.2f, E1, -15.8f), -PI * 0.5f);
    k.Prop("glocke", Vec3(15.9f, E1 + 0.75f, -15.8f), 0);
    k.Prop("adlerfigur", Vec3(14.2f, E1, -18.4f), 0);
    k.Prop("flagge", Vec3(24.9f, E1, -18.5f), 0);
    // Vorraum Nord
    k.Prop("sofa", Vec3(-21.8f, E1, -18.45f), 0);
    k.Prop("couchtisch", Vec3(-21.8f, E1, -17.1f), 0);
    k.Prop("sessel", Vec3(-23.6f, E1, -17.1f), PI * 0.5f);
    k.Prop("sessel", Vec3(-19.4f, E1, -17.1f), -PI * 0.5f);
    k.Prop("vase", Vec3(-21.6f, E1 + 0.42f, -17.0f), 0);
    k.Prop("ficus", Vec3(-16.2f, E1, -18.4f), 0);
    k.Prop("stehlampe", Vec3(-25.0f, E1, -18.5f), 0);
    k.Prop("garderobe", Vec3(-16.2f, E1, -16.3f), 0);
    // Pressevorraum Süd
    k.Prop("automat", Vec3(-24.9f, E1, 18.45f), PI);
    k.Style(MAT_METAL, 0xb9bdc3);
    k.Solid(Vec3(-23.8f, E1, 18.3f), Vec3(-22.0f, E1 + 0.9f, 19.0f));
    k.Prop("kaffeemaschine", Vec3(-23.2f, E1 + 0.9f, 18.65f), PI);
    k.Prop("kaffeebecher", Vec3(-22.5f, E1 + 0.9f, 18.6f), 0);
    k.Prop("sitzbank", Vec3(-20.0f, E1, 16.1f), 0);
    k.Prop("papierkorb", Vec3(-16.2f, E1, 18.5f), 0);
    // Pressezentrum
    for (float x : {-13.2f, -9.8f, -6.4f, -3.0f}) DeskSet(Vec3(x, E1, 18.55f), PI, rng.Chance(0.5f));
    k.Style(MAT_PLASTIC, 0xe8e8e4);
    k.Solid(Vec3(-13.5f, E1, 14.6f), Vec3(-3.0f, E1 + 0.74f, 15.4f));
    for (float x = -13.0f; x <= -3.6f; x += 1.3f) {
      k.Prop("kantinenstuhl", Vec3(x, E1, 14.0f), rng.Range(-0.4f, 0.4f));
      if (rng.Chance(0.7f)) k.Prop(rng.Chance(0.6f) ? "laptop" : "kaffeebecher", Vec3(x, E1 + 0.74f, 14.9f), PI);
    }
    k.Prop("tvkamera", Vec3(-14.6f, E1, 13.4f), 0.7f);
    k.Prop("tvkamera", Vec3(-2.0f, E1, 13.3f), -0.6f);
    k.Prop("stapelstuehle", Vec3(-1.7f, E1, 17.0f), 0);
    // Lounge
    for (float x : {4.4f, 9.6f}) {
      k.Prop("sofa", Vec3(x, E1, 18.4f), PI);
      k.Prop("sofa", Vec3(x, E1, 14.2f), 0);
      k.Prop("couchtisch", Vec3(x, E1, 16.3f), 0);
      if (x > 5) k.Prop("sessel", Vec3(x - 1.75f, E1, 16.3f), PI * 0.5f);
      k.Prop("sessel", Vec3(x + 1.75f, E1, 16.3f), -PI * 0.5f);
      k.Prop(rng.Chance(0.5f) ? "vase" : "kaffeebecher", Vec3(x + 0.2f, E1 + 0.42f, 16.2f), 0);
    }
    k.Prop("stehlampe", Vec3(-0.5f, E1, 18.6f), 0);
    k.Prop("stehlampe", Vec3(12.9f, E1, 18.6f), 0);
    k.Prop("ficus", Vec3(5.6f, E1, 18.5f), 0);
    k.Prop("ficus", Vec3(-0.4f, E1, 13.3f), 0);
    k.Prop("globus", Vec3(12.8f, E1, 13.4f), 0.4f);
    k.Prop("staffelei", Vec3(5.6f, E1, 13.4f), 0.2f);
    // Bundesrat-Saal
    k.Style(MAT_WOOD, 0x5a3d26);
    k.Solid(Vec3(16.0f, E1, 14.6f), Vec3(23.0f, E1 + 0.75f, 17.2f));
    for (float x = 16.6f; x <= 22.5f; x += 1.3f) {
      k.Prop("sitz", Vec3(x, E1, 13.95f), 0);
      k.Prop("sitz", Vec3(x, E1, 17.85f), PI);
      if (rng.Chance(0.5f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x, E1 + 0.75f, 15.2f), 0);
    }
    k.Prop("wahlurne", Vec3(22.4f, E1 + 0.75f, 15.9f), -PI * 0.5f);
    k.Prop("bueste", Vec3(25.0f, E1, 18.5f), PI);
    k.Prop("flagge", Vec3(14.2f, E1, 18.5f), PI);
    // Ostflur
    k.Prop("ficus", Vec3(12.95f, E1, -11.9f), 0);
    k.Prop("ficus", Vec3(12.95f, E1, 11.9f), 0);
    k.Prop("sitzbank", Vec3(14.95f, E1, -2.0f), -PI * 0.5f);
    k.Prop("staffelei", Vec3(14.7f, E1, 2.2f), -PI * 0.5f);
    // Westflur
    k.Prop("staffelei", Vec3(-14.8f, E1, -9.5f), PI * 0.5f);
    k.Prop("staffelei", Vec3(-14.8f, E1, 0.0f), PI * 0.5f);
    k.Prop("staffelei", Vec3(-14.8f, E1, 9.5f), PI * 0.5f);
    k.Prop("sitzbank", Vec3(-12.95f, E1, -2.5f), -PI * 0.5f);
    // Balkon Eingangshalle
    k.Prop("sitzbank", Vec3(-16.0f, E1, -9.0f), PI * 0.5f);
    k.Prop("sitzbank", Vec3(-16.0f, E1, 9.0f), PI * 0.5f);
    k.Prop("ficus", Vec3(-16.1f, E1, -12.8f), 0);
    k.Prop("ficus", Vec3(-16.1f, E1, 12.8f), 0);
    k.Prop("bueste", Vec3(-28.4f, E1, -14.9f), PI * 0.5f);
    k.Prop("bueste", Vec3(-28.4f, E1, 14.9f), PI * 0.5f);
    // Präsidialbüro
    k.PropRel("schreibtisch", Vec3(24.0f, E1, -6.0f), PI * 0.5f, Vec3(0, 0, 0));
    k.PropRel("buerostuhl", Vec3(24.0f, E1, -6.0f), PI * 0.5f, Vec3(0, 0, 0.75f), PI);
    k.PropRel("monitor", Vec3(24.0f, E1, -6.0f), PI * 0.5f, Vec3(-0.3f, 0.76f, -0.12f), 0);
    k.PropRel("telefon", Vec3(24.0f, E1, -6.0f), PI * 0.5f, Vec3(0.5f, 0.76f, 0.1f), 0);
    k.PropRel("glocke", Vec3(24.0f, E1, -6.0f), PI * 0.5f, Vec3(0.3f, 0.76f, -0.2f), 0);
    k.Prop("flagge", Vec3(26.9f, E1, -7.6f), -PI * 0.5f);
    k.Prop("flagge", Vec3(26.9f, E1, -4.4f), -PI * 0.5f);
    for (float x = 16.6f; x <= 20.0f; x += 1.05f) k.Prop("regal", Vec3(x, E1, -12.4f), 0);
    k.Prop("globus", Vec3(27.9f, E1, -1.0f), 0.6f);
    k.Prop("sofa", Vec3(18.6f, E1, -0.8f), PI);
    k.Prop("couchtisch", Vec3(18.6f, E1, -2.2f), 0);
    k.Prop("sessel", Vec3(16.4f, E1, -2.2f), PI * 0.5f);
    k.Prop("bueste", Vec3(28.2f, E1, -14.8f), -PI * 0.75f);
    k.Prop("stehlampe", Vec3(28.3f, E1, -11.9f), 0);
    // Ältestenrat
    k.Style(MAT_WOOD, 0x4a3020);
    k.Solid(Vec3(18.0f, E1, 5.2f), Vec3(27.0f, E1 + 0.75f, 7.6f));
    for (float x = 18.6f; x <= 26.5f; x += 1.3f) {
      k.Prop("buerostuhl", Vec3(x, E1, 4.55f), 0);
      k.Prop("buerostuhl", Vec3(x, E1, 8.25f), PI);
      if (rng.Chance(0.5f)) k.Prop(tableItems[rng.Int(0, 5)], Vec3(x, E1 + 0.75f, 6.4f), 0);
    }
    k.Prop("ficus", Vec3(28.3f, E1, 12.0f), 0);
    k.Prop("aktenschrank", Vec3(28.5f, E1, 1.4f), -PI * 0.5f);
    k.Prop("aktenschrank", Vec3(28.5f, E1, 2.25f), -PI * 0.5f);
  }

  void FurnishRoof() {
    for (float x = -24.0f; x <= -14.0f; x += 2.0f) {
      k.Prop("liegestuhl", Vec3(x, RF, 17.6f), PI + rng.Range(-0.3f, 0.3f));
      k.Prop("liegestuhl", Vec3(x + 1.0f, RF, -17.6f), rng.Range(-0.3f, 0.3f));
    }
    k.Prop("sonnenschirm", Vec3(-19.0f, RF, 15.0f), 0);
    k.Prop("sonnenschirm", Vec3(-19.0f, RF, -15.0f), 0);
    k.Prop("sonnenschirm", Vec3(14.0f, RF, 12.5f), 0);
    for (float x = -26.0f; x <= 24.0f; x += 7.0f) {
      k.Prop("kuebel", Vec3(x, RF, 18.7f), 0);
      k.Prop("kuebel", Vec3(x, RF, -18.7f), 0);
    }
    k.Prop("klima", Vec3(-6.0f, RF, -16.5f), 0);
    k.Prop("klima", Vec3(6.0f, RF, -16.5f), 0);
    k.Prop("klima", Vec3(-22.0f, RF, 4.0f), PI * 0.5f);
    for (int i = 0; i < 4; i++) {
      float a = PI * 0.25f + i * PI * 0.5f;
      Vec3 p(cosf(a) * 15.5f, RF, sinf(a) * 15.5f);
      k.Prop("parkbank", p, FaceYaw(p, Vec3(0, RF, 0)));
      k.Prop("abfalleimer", p + Vec3(1.3f, 0, 0), 0);
    }
    k.Prop("tvkamera", Vec3(-26.5f, RF, 0.0f), -PI * 0.5f);
    BigFlag(Vec3(-27.8f, RF, 0), 9, 4.0f, 2.4f, false, 0);
  }

  // ------------------------------------------------------------------------------------------
  void Zones() {
    k.Zone(Vec3(-95, -10, -40), Vec3(55, 40, 52), 1.0f, "Platz der Republik", -1);
    k.Zone(Vec3(-95, -10, -36), Vec3(55, 40, -25), 1.0f, "Spreeufer", 0);
    k.Zone(Vec3(37, -10, -36), Vec3(52, 8, 52), 1.0f, "Scheidemannstraße", 0);
    k.Zone(Vec3(-41.5f, -2, -10), Vec3(-30, 10, 10), 0.7f, "Westportal", 1);
    k.Zone(Vec3(-38.2f, -2, 24.9f), Vec3(-22, 2, 28.4f), 0.4f, "Besucherzentrum", 2);
    k.Zone(Vec3(-29, -0.5f, -15.5f), Vec3(-15.5f, 9.6f, 15.5f), 0.24f, "Eingangshalle", 2);
    k.Zone(Vec3(-15.5f, -0.5f, -15.5f), Vec3(15.5f, 4.6f, 15.5f), 0.14f, "Umgang", 1);
    k.Zone(Vec3(-12, -2, -12), Vec3(12, 9.6f, 12), 0.55f, "Plenarsaal", 3);
    k.Zone(Vec3(-12, 4.6f, -12), Vec3(6, 8.5f, -9.6f), 0.5f, "Besuchertribüne", 4);
    k.Zone(Vec3(-12, 4.6f, 9.6f), Vec3(6, 8.5f, 12), 0.5f, "Besuchertribüne", 4);
    k.Zone(Vec3(-12, 4.6f, -9.6f), Vec3(-9.6f, 8.5f, 9.6f), 0.5f, "Pressetribüne", 4);
    struct R { float x0, x1; const char* n; };
    R north[] = {{-25.5f, -18, "Garderobe"}, {-18, -10, "Büro"}, {-10, -2, "Bibliothek"}, {-2, 6, "Poststelle"}, {6, 14, "Büro"}, {14, 25.5f, "Sanitätsraum"}};
    for (auto& r : north) k.Zone(Vec3(r.x0, -0.5f, -19), Vec3(r.x1, 4.6f, -15.5f), 0.12f, r.n, 2);
    R south[] = {{-25.5f, -18, "Sicherheitszentrale"}, {-18, -6, "Fraktionsbüro"}, {-6, 6, "Andachtsraum"}, {6, 14, "Technikraum"}, {14, 25.5f, "Lager"}};
    for (auto& r : south) k.Zone(Vec3(r.x0, -0.5f, 15.5f), Vec3(r.x1, 4.6f, 19), 0.12f, r.n, 2);
    k.Zone(Vec3(15.5f, -0.5f, -15.5f), Vec3(29, 4.6f, 3), 0.15f, "Kantine", 2);
    k.Zone(Vec3(15.5f, -0.5f, 3), Vec3(29, 4.6f, 15.5f), 0.12f, "Küche", 2);
    k.Zone(Vec3(-29, 4.6f, -15.5f), Vec3(-15.5f, 9.6f, -13.5f), 0.24f, "Hallenbalkon", 3);
    k.Zone(Vec3(-29, 4.6f, 13.5f), Vec3(-15.5f, 9.6f, 15.5f), 0.24f, "Hallenbalkon", 3);
    k.Zone(Vec3(-18.5f, 4.6f, -13.5f), Vec3(-15.5f, 9.6f, 13.5f), 0.24f, "Hallenbalkon", 3);
    k.Zone(Vec3(-25.5f, 4.6f, -19), Vec3(-15.5f, 9.6f, -15.5f), 0.12f, "Fraktionsvorraum", 3);
    k.Zone(Vec3(-25.5f, 4.6f, 15.5f), Vec3(-15.5f, 9.6f, 19), 0.12f, "Pressevorraum", 3);
    k.Zone(Vec3(-15.5f, 4.6f, -19), Vec3(-1, 9.6f, -12.6f), 0.12f, "Fraktionssaal A", 2);
    k.Zone(Vec3(-1, 4.6f, -19), Vec3(13.5f, 9.6f, -12.6f), 0.12f, "Fraktionssaal B", 2);
    k.Zone(Vec3(13.5f, 4.6f, -19), Vec3(25.5f, 9.6f, -12.6f), 0.12f, "Sitzungssaal", 2);
    k.Zone(Vec3(-15.5f, 4.6f, 12.6f), Vec3(-1, 9.6f, 19), 0.12f, "Pressezentrum", 2);
    k.Zone(Vec3(-1, 4.6f, 12.6f), Vec3(13.5f, 9.6f, 19), 0.12f, "Abgeordneten-Lounge", 2);
    k.Zone(Vec3(13.5f, 4.6f, 12.6f), Vec3(25.5f, 9.6f, 19), 0.12f, "Bundesrat-Saal", 2);
    k.Zone(Vec3(-15.5f, 4.6f, -12.6f), Vec3(-12.6f, 9.6f, 12.6f), 0.12f, "Westflur", 2);
    k.Zone(Vec3(12.6f, 4.6f, -12.6f), Vec3(15.5f, 9.6f, 12.6f), 0.12f, "Ostflur", 2);
    k.Zone(Vec3(15.5f, 4.6f, -15.5f), Vec3(29, 9.6f, 0), 0.12f, "Präsidialbüro", 2);
    k.Zone(Vec3(15.5f, 4.6f, 0), Vec3(29, 9.6f, 15.5f), 0.12f, "Ältestenrat", 2);
    k.Zone(Vec3(-30.5f, 9.9f, -20.5f), Vec3(30.5f, 30, 20.5f), 1.0f, "Dachterrasse", 2);
    k.Zone(Vec3(-11.2f, 9.9f, -11.2f), Vec3(11.2f, 25, 11.2f), 1.0f, "Kuppel", 3);
    k.Zone(Vec3(17, 9.9f, -8), Vec3(27, 13, 8), 0.65f, "Dachrestaurant", 4);
  }

  void Spawns() {
    for (float x : {-78.0f, -75.0f, -72.0f, -69.0f})
      for (float z : {-6.0f, -2.0f, 2.0f, 6.0f}) w.hunterSpawns.push_back({Vec3(x, GY, z), -PI * 0.5f});
    Vec3 ps[] = {{-27.5f, 0, -8.5f}, {-27.5f, 0, 8.5f}, {-23, 0, -6}, {-23, 0, 6}, {-20, 0, -5}, {-20, 0, 5},
                 {-17, 0, -10}, {-17, 0, 10}, {-27.5f, 0, -12}, {-27.5f, 0, 12}, {-23.5f, 0, -12}, {-23.5f, 0, 12},
                 {-19, 0, -13}, {-19, 0, 13}, {-16.5f, 0, -6.5f}, {-16.5f, 0, 6.5f}};
    for (Vec3 p : ps) w.propSpawns.push_back({p, -PI * 0.5f});
    w.cameraPath = {Vec3(-64, 3.5f, -20), Vec3(-30, 7, 0),  Vec3(19, 13.2f, 13.5f), Vec3(0, 15, 0),
                    Vec3(-10.5f, 7.2f, -10.5f), Vec3(8, -0.5f, 2), Vec3(9.5f, 11.8f, 1), Vec3(0, 16, 0),
                    Vec3(-28, 3.2f, -13), Vec3(-18, 4, 2)};
  }
};

}  // namespace

void BuildBundestag(World& w) {
  props::BuildCatalog();
  Builder b(w);
  b.Build();
}
