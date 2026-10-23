// Karte "Nudelfabrik" – alte Backsteinfabrik mit Sheddach: Produktionshalle mit Rührwerken, Nudelpresse und
// Spaghetti-Straße, Laufstege, Trockenraum, Mehllager, Werkstatt, Verpackung mit Lkw an der Rampe, Bürotrakt mit
// Kantine – draußen Mehlsilos, Werkshof, Parkplatz, Pförtnerhaus und Hinterhof mit Schornstein.
#include "game/MapKit.h"
#include "game/Maps.h"
#include "game/Shapes.h"

namespace {

// Gebäude (Außenkanten der Außenwände)
const float BX0 = -36, BX1 = 36, BZ0 = -22, BZ1 = 22;
const float WT = 0.35f;        // Außenwanddicke
const float WALL_H = 9.0f;     // Traufhöhe
const float ROOF_T = 0.3f;     // Flachdach über den Anbauten
const float IX = 20.0f;        // Hallenwände bei x = ±IX
const float HZ1 = 10.0f;       // Hallen-Südwand bei z = HZ1
const float IWH = 0.15f;       // halbe Innenwanddicke
const float SHED_LO = 9.0f, SHED_HI = 11.5f;
const float CAT = 4.0f;        // Laufsteghöhe
const float OG = 4.0f;         // Bürogeschoss
const float OGC = 3.7f;        // Decke Erdgeschoss Bürotrakt (Unterseite der Platte)

const u32 BRICK = 0x9a4a32, BRICK_D = 0x7a3a28, STEEL = 0x3c4046, STEEL_L = 0x8a9096, MGREEN = 0x4f6e62,
          SSTEEL = 0xc8cdd2, YELLOW = 0xf2c418, CONC = 0x8e948c, PLASTER = 0xe6e2d8, HALLPAINT = 0x6e8a7a;

Mat4 Basis(Vec3 x, Vec3 y, Vec3 z, Vec3 t) {
  Mat4 m = Mat4::Identity();
  m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
  m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
  m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
  m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z;
  return m;
}

// Dünner Quader zwischen zwei Punkten (Streben, Rohre)
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

// Rundes Rohr zwischen zwei Punkten
void Pipe(MeshBuilder& b, Vec3 a, Vec3 c, float r, int segs = 10) {
  Vec3 d = c - a;
  float len = Length(d);
  if (len < 1e-4f) return;
  Vec3 y = d / len;
  Vec3 ref = fabsf(y.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
  Vec3 x = Normalize(Cross(y, ref));
  Vec3 z = Cross(x, y);
  b.Push(Basis(x, y, z, a));
  b.Frustum(Vec3(0, 0, 0), r, r, len, segs);
  b.Pop();
}

// Viereck, das zur Seite n zeigt
void QuadF(MeshBuilder& b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n) {
  if (Dot(Cross(p2 - p0, p3 - p1), n) < 0) std::swap(p1, p3);
  b.Quad(p0, p1, p2, p3);
}
void TriN(MeshBuilder& b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 n) {
  if (Dot(Cross(p1 - p0, p2 - p0), n) < 0) std::swap(p1, p2);
  b.Tri(p0, p1, p2);
}

// Rechteck [w0,w1] x [y0,y1] ohne die Öffnungen, zerlegt in Teilrechtecke
void SplitRect(float w0, float w1, float y0, float y1, const std::vector<Hole>& holes,
               const std::function<void(float, float, float, float)>& emit) {
  std::vector<float> cuts = {w0, w1};
  for (const Hole& h : holes) {
    if (h.a1 <= w0 || h.a0 >= w1 || h.y1 <= y0 || h.y0 >= y1) continue;
    cuts.push_back(Clamp(h.a0, w0, w1));
    cuts.push_back(Clamp(h.a1, w0, w1));
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end(), [](float a, float b) { return fabsf(a - b) < 0.0005f; }), cuts.end());
  for (size_t i = 0; i + 1 < cuts.size(); i++) {
    float s0 = cuts[i], s1 = cuts[i + 1];
    std::vector<std::pair<float, float>> gaps;
    for (const Hole& h : holes)
      if (h.a0 <= s0 + 0.0005f && h.a1 >= s1 - 0.0005f && h.y1 > y0 && h.y0 < y1) gaps.push_back({std::max(h.y0, y0), std::min(h.y1, y1)});
    std::sort(gaps.begin(), gaps.end());
    float cur = y0;
    for (auto& g : gaps) {
      if (g.first > cur + 0.001f) emit(s0, s1, cur, g.first);
      cur = std::max(cur, g.second);
    }
    if (cur < y1 - 0.001f) emit(s0, s1, cur, y1);
  }
}

struct Win {
  bool alongX;
  float a0, a1, y0, y1, f0, f1;
  int out;        // Außenseite: +1 / -1 entlang der Wandnormale, 0 = Innenfenster
};

class Builder {
 public:
  explicit Builder(World& w) : w(w), k(w), rng(20261024) {}
  World& w;
  MapKit k;
  Rng rng;
  std::vector<Win> wins;
  // Öffnungen der Wände (für Verkleidungen)
  std::vector<Hole> hN, hS, hW, hE, hXm, hXp, hZh, hZw, hZo, hZu;

  void Build() {
    SetupEnvironment();
    Ground();
    // Wände zuerst: die Türen werden registriert und halten Props fern
    Shell();
    InteriorWalls();
    BuildWindows();
    Cladding();
    Roofs();
    HallStructure();
    LineA();
    LineB();
    Catwalks();
    HallProps();
    DryingRoom();
    FlourStore();
    Workshop();
    Packaging();
    Truck();
    OfficeWing();
    Outside();
    Silos();
    Fence();
    Backdrop();
    Zones();
    Spawns();
  }

  // ------------------------------------------------------------------------------------------
  void SetupEnvironment() {
    auto& e = w.env;
    e.sunDir = Normalize(Vec3(0.38f, 0.6f, 0.7f));  // Sonne im Süden: scheint durch die Sheddach-Verglasung
    e.sunColor = Vec3(3.0f, 2.6f, 2.05f);
    e.skyZenith = Hex(0x467ac2);
    e.skyHorizon = Hex(0xc9d9e8);
    e.groundColor = Hex(0x6a665a);
    e.skyAmbient = Vec3(0.4f, 0.46f, 0.56f);
    e.groundAmbient = Vec3(0.24f, 0.22f, 0.19f);
    e.indoorAmbient = Vec3(0.25f, 0.24f, 0.23f);
    e.fogColor = Hex(0xc4d0dc);
    e.fogDensity = 0.0032f;
    e.clouds = 0.55f;
    e.exposure = .82f;
  }

  // ------------------------------------------------------------------------------------------
  // Boden: begehbare Grundfläche, Innenböden bis zur Außenkante der Wände, außen ringsum
  void Ground() {
    k.Clip(Vec3(-75, -2, -55), Vec3(75, 0, 75), COL_SOLID);
    // Halle: Industriebeton, Anbauten je nach Nutzung
    k.FloorFinish(-IX, BZ0, IX, HZ1, 0, MAT_CONCRETE, CONC);
    k.FloorFinish(BX0, BZ0, -IX, 4.0f, 0, MAT_CONCRETE, 0x9a9c94);       // Mehllager
    k.FloorFinish(BX0, 4.0f, -IX, BZ1, 0, MAT_CONCRETE, 0x86887f);       // Werkstatt
    k.FloorFinish(-IX, HZ1, IX, BZ1, 0, MAT_CONCRETE, 0x949890);          // Verpackung
    k.FloorFinish(IX, BZ0, BX1, -2.0f, 0, MAT_TILE, 0xc8c0b0);           // Trockenraum
    k.FloorFinish(IX, -2.0f, BX1, HZ1, 0, MAT_TILE, 0xb8bcc0);           // Kantine
    k.FloorFinish(IX, HZ1, BX1, BZ1, 0, MAT_TILE, 0xa8aca8);             // Umkleide
    // Gelbe Gehweg-Markierungen in der Halle (12 mm aufgetragen)
    k.Style(MAT_PLASTIC, YELLOW);
    for (float z : {-10.3f, -0.6f, 8.8f}) k.FloorFinish(-19.4f, z - 0.06f, 19.4f, z + 0.06f, 0.012f, MAT_PLASTIC, YELLOW);
    for (float x : {-17.2f, 16.8f}) {
      k.FloorFinish(x - 0.06f, -10.24f, x + 0.06f, -0.66f, 0.012f, MAT_PLASTIC, YELLOW);
      k.FloorFinish(x - 0.06f, -0.54f, x + 0.06f, 8.74f, 0.012f, MAT_PLASTIC, YELLOW);
    }
    // Außen
    k.FloorFinish(-60, BZ1, 60, 58, 0, MAT_ASPHALT, 0x3c3d3f);          // Werkshof
    k.FloorFinish(-60, -40, 60, BZ0, 0, MAT_CONCRETE, 0x8a8a84);        // Hinterhof
    k.FloorFinish(-60, BZ0, BX0, BZ1, 0, MAT_CONCRETE, 0x9a9890);       // Siloplatz
    k.FloorFinish(BX1, BZ0, 60, BZ1, 0, MAT_ASPHALT, 0x404143);         // Parkplatz
    // Parkplatz-Linien und Laderampen-Markierung
    for (float z = -18; z <= 18.1f; z += 3.0f) k.FloorFinish(40, z - 0.05f, 46, z + 0.05f, 0.012f, MAT_PLASTIC, 0xe8e8e2);
    for (float z = -18; z <= 18.1f; z += 3.0f) k.FloorFinish(50, z - 0.05f, 56, z + 0.05f, 0.012f, MAT_PLASTIC, 0xe8e8e2);
    for (float x : {-12.5f, -7.5f, -0.5f, 4.5f}) k.FloorFinish(x - 0.06f, BZ1 + 0.3f, x + 0.06f, BZ1 + 14.0f, 0.012f, MAT_PLASTIC, YELLOW);
  }

  // ------------------------------------------------------------------------------------------
  // Fenster: Öffnung merken, Glas/Sprossen/Sims nach dem Wandbau
  void AddWindows(std::vector<Hole>& h, bool alongX, float f0, float f1, int out, std::initializer_list<float> centers,
                  float width, float y0, float y1) {
    for (float c : centers) {
      h.push_back({c - width * 0.5f, c + width * 0.5f, y0, y1, true});
      wins.push_back({alongX, c - width * 0.5f, c + width * 0.5f, y0, y1, f0, f1, out});
    }
  }

  void BuildWindows() {
    for (const Win& wd : wins) {
      auto P = [&](float a, float y, float f) { return wd.alongX ? Vec3(a, y, f) : Vec3(f, y, a); };
      auto Box = [&](float a0, float y0, float fa, float a1, float y1, float fb) {
        Vec3 p = P(a0, y0, fa), q = P(a1, y1, fb);
        k.Visual(Min(p, q), Max(p, q));
      };
      float fm = (wd.f0 + wd.f1) * 0.5f;
      Vec3 g0 = P(wd.a0, wd.y0, fm - 0.008f), g1 = P(wd.a1, wd.y1, fm + 0.008f);
      k.Glass(Min(g0, g1), Max(g0, g1), 0xb4c4cc, 0.28f, true);
      // Stahlsprossen (kleine Scheiben) und Rahmen, alle in derselben Farbe
      k.Style(MAT_METAL, 0x2a2d31);
      int nv = std::max(1, (int)lroundf((wd.a1 - wd.a0) / 0.55f));
      for (int i = 1; i < nv; i++) {
        float a = wd.a0 + (wd.a1 - wd.a0) * i / nv;
        Box(a - 0.018f, wd.y0, fm - 0.03f, a + 0.018f, wd.y1, fm + 0.03f);
      }
      int nh = std::max(1, (int)lroundf((wd.y1 - wd.y0) / 0.6f));
      for (int j = 1; j < nh; j++) {
        float y = wd.y0 + (wd.y1 - wd.y0) * j / nh;
        Box(wd.a0, y - 0.018f, fm - 0.03f, wd.a1, y + 0.018f, fm + 0.03f);
      }
      Box(wd.a0, wd.y0, fm - 0.035f, wd.a0 + 0.05f, wd.y1, fm + 0.035f);
      Box(wd.a1 - 0.05f, wd.y0, fm - 0.035f, wd.a1, wd.y1, fm + 0.035f);
      Box(wd.a0 + 0.05f, wd.y0, fm - 0.035f, wd.a1 - 0.05f, wd.y0 + 0.05f, fm + 0.035f);
      Box(wd.a0 + 0.05f, wd.y1 - 0.05f, fm - 0.035f, wd.a1 - 0.05f, wd.y1, fm + 0.035f);
      if (wd.out == 0) continue;
      // Außen: Betonsims (3 cm über der Brüstung) und Ziegelsturz, beide nur vor der Wand
      float outer = wd.out > 0 ? std::max(wd.f0, wd.f1) : std::min(wd.f0, wd.f1);
      k.Style(MAT_CONCRETE, 0xb8b2a6);
      Box(wd.a0 - 0.08f, wd.y0 - 0.1f, outer, wd.a1 + 0.08f, wd.y0 + 0.03f, outer + wd.out * 0.07f);
      k.Style(MAT_BRICK, BRICK_D);
      Box(wd.a0 - 0.12f, wd.y1, outer, wd.a1 + 0.12f, wd.y1 + 0.3f, outer + wd.out * 0.03f);
    }
  }

  // Wandverkleidung (Farbe, Fliesen, Putz) 12 mm vor der Wandfläche, Öffnungen ausgespart
  void Clad(bool alongX, float face, int dir, float a0, float a1, float y0, float y1, const std::vector<Hole>& holes, Material m,
            u32 col) {
    k.Style(m, col);
    SplitRect(a0, a1, y0, y1, holes, [&](float s0, float s1, float ya, float yb) {
      float f0 = face, f1 = face + dir * 0.012f;
      if (alongX) k.Visual(Vec3(s0, ya, std::min(f0, f1)), Vec3(s1, yb, std::max(f0, f1)));
      else k.Visual(Vec3(std::min(f0, f1), ya, s0), Vec3(std::max(f0, f1), yb, s1));
    });
  }

  // Stahlzarge 2 cm in die Öffnung, über die Wanddicke hinaus
  void DoorFrame(bool alongX, float f0, float f1, float a0, float a1, float y1, float y0 = 0) {
    k.Style(MAT_METAL, 0x50565c);
    auto Box = [&](float aa, float ya, float ab, float yb) {
      if (alongX) k.Visual(Vec3(aa, ya, f0 - 0.02f), Vec3(ab, yb, f1 + 0.02f));
      else k.Visual(Vec3(f0 - 0.02f, ya, aa), Vec3(f1 + 0.02f, yb, ab));
    };
    Box(a0, y0, a0 + 0.06f, y1);
    Box(a1 - 0.06f, y0, a1, y1);
    Box(a0 + 0.06f, y1 - 0.06f, a1 - 0.06f, y1);
  }

  // Rolltor (offen): Wickelkasten innen über der Öffnung, Führungsschienen in der Laibung
  void RollerDoor(bool alongX, float f0, float f1, int in, float a0, float a1, float y1) {
    k.Style(MAT_METAL, 0x6a7078);
    float inner = in > 0 ? std::max(f0, f1) : std::min(f0, f1);
    auto Box = [&](float aa, float ya, float fa, float ab, float yb, float fb) {
      if (alongX) k.Visual(Vec3(aa, ya, std::min(fa, fb)), Vec3(ab, yb, std::max(fa, fb)));
      else k.Visual(Vec3(std::min(fa, fb), ya, aa), Vec3(std::max(fa, fb), yb, ab));
    };
    Box(a0 - 0.15f, y1 + 0.02f, inner, a1 + 0.15f, y1 + 0.7f, inner + in * 0.55f);
    k.Style(MAT_PLASTIC, 0xc8281e);
    Box(a0 - 0.15f, y1 + 0.3f, inner + in * 0.55f, a1 + 0.15f, y1 + 0.42f, inner + in * 0.56f);
    k.Style(MAT_METAL, 0x50565c);
    Box(a0 + 0.02f, 0, inner - in * 0.02f, a0 + 0.1f, y1, inner + in * 0.12f);
    Box(a1 - 0.1f, 0, inner - in * 0.02f, a1 - 0.02f, y1, inner + in * 0.12f);
    // gelb-schwarze Warnstreifen an der Laibung (außen)
    float outer = in > 0 ? std::min(f0, f1) : std::max(f0, f1);
    for (int i = 0; i < 6; i++) {
      k.Style(MAT_PLASTIC, i % 2 ? 0x1a1a1a : YELLOW);
      for (float a : {a0 - 0.2f, a1 + 0.02f})
        Box(a, i * 0.25f, outer, a + 0.18f, i * 0.25f + 0.25f, outer - in * 0.015f);
    }
  }

  // ------------------------------------------------------------------------------------------
  void Shell() {
    const float n0 = BZ0, n1 = BZ0 + WT, s0 = BZ1 - WT, s1 = BZ1, w0 = BX0, w1 = BX0 + WT, e0 = BX1 - WT, e1 = BX1;
    // Nord: Hallenfenster, Hintertür Halle, Tür Trockenraum, Lagerfenster
    AddWindows(hN, true, n0, n1, -1, {-17.0f, -12.0f, -7.0f, 7.0f, 12.0f, 17.0f}, 2.6f, 2.6f, 7.4f);
    AddWindows(hN, true, n0, n1, -1, {-31.0f, -25.0f}, 2.4f, 3.0f, 7.0f);
    AddWindows(hN, true, n0, n1, -1, {24.5f, 34.0f}, 2.0f, 3.2f, 6.4f);
    hN.push_back({-1.0f, 1.0f, 0, 2.6f});      // Hintertür Halle
    hN.push_back({29.6f, 31.2f, 0, 2.4f});     // Hintertür Trockenraum
    // Süd: Laderampen-Tore, Personaltür, Fenster Verpackung, Werkstatt, Büro
    hS.push_back({-12.0f, -8.0f, 0, 4.0f});
    hS.push_back({0.0f, 4.0f, 0, 4.0f});
    hS.push_back({10.0f, 11.6f, 0, 2.5f});
    AddWindows(hS, true, s0, s1, 1, {-16.5f, -4.5f, 7.0f, 15.5f}, 2.2f, 4.8f, 7.6f);
    AddWindows(hS, true, s0, s1, 1, {-31.0f, -25.0f}, 2.4f, 2.8f, 6.8f);
    AddWindows(hS, true, s0, s1, 1, {23.5f, 34.0f}, 2.0f, 1.0f, 2.8f);
    AddWindows(hS, true, s0, s1, 1, {23.5f, 29.5f, 33.5f}, 2.0f, 5.4f, 7.6f);
    // West: Rolltor Mehllager, Werkstatt-Tür, Fenster
    hW.push_back({-14.0f, -9.0f, 0, 4.5f});
    hW.push_back({17.0f, 18.6f, 0, 2.5f});
    AddWindows(hW, false, w0, w1, -1, {-19.0f, -4.0f, 1.5f}, 2.4f, 3.0f, 7.0f);
    AddWindows(hW, false, w0, w1, -1, {8.0f, 13.0f}, 2.4f, 2.8f, 6.8f);
    // Ost: Trockenraum, Haupteingang Büro, Bürofenster
    AddWindows(hE, false, e0, e1, 1, {-18.0f, -12.0f, -6.0f}, 2.0f, 3.4f, 6.4f);
    hE.push_back({14.0f, 15.8f, 0, 2.6f});
    AddWindows(hE, false, e0, e1, 1, {1.5f, 6.5f, 12.0f, 19.0f}, 2.0f, 1.0f, 2.8f);
    AddWindows(hE, false, e0, e1, 1, {1.5f, 6.5f, 12.5f, 16.5f, 20.0f}, 2.0f, 5.4f, 7.6f);

    k.Style(MAT_BRICK, BRICK);
    k.WallX(BX0, BX1, n0, n1, 0, WALL_H, hN);
    k.WallX(BX0, BX1, s0, s1, 0, WALL_H, hS);
    k.WallZ(n1, s0, w0, w1, 0, WALL_H, hW);
    k.WallZ(n1, s0, e0, e1, 0, WALL_H, hE);
    // Sockel aus Klinker (dunkler) und Gesims unter der Traufe, beide vor der Wand
    k.Style(MAT_BRICK, 0x5e3024);
    auto band = [&](float y0, float y1, float d) {
      SplitRect(BX0, BX1, y0, y1, hN, [&](float a, float b, float ya, float yb) { k.Visual(Vec3(a, ya, n0 - d), Vec3(b, yb, n0)); });
      SplitRect(BX0, BX1, y0, y1, hS, [&](float a, float b, float ya, float yb) { k.Visual(Vec3(a, ya, s1), Vec3(b, yb, s1 + d)); });
      SplitRect(n0 - d, s1 + d, y0, y1, hW, [&](float a, float b, float ya, float yb) { k.Visual(Vec3(w0 - d, ya, a), Vec3(w0, yb, b)); });
      SplitRect(n0 - d, s1 + d, y0, y1, hE, [&](float a, float b, float ya, float yb) { k.Visual(Vec3(e1, ya, a), Vec3(e1 + d, yb, b)); });
    };
    band(0, 0.6f, 0.04f);
    k.Style(MAT_BRICK, BRICK_D);
    band(8.4f, 8.7f, 0.06f);
    band(8.75f, 9.0f, 0.1f);
    // Türzargen der Außentüren
    DoorFrame(true, n0, n1, -1.0f, 1.0f, 2.6f);
    DoorFrame(true, n0, n1, 29.6f, 31.2f, 2.4f);
    DoorFrame(true, s0, s1, 10.0f, 11.6f, 2.5f);
    DoorFrame(false, w0, w1, 17.0f, 18.6f, 2.5f);
    DoorFrame(false, e0, e1, 14.0f, 15.8f, 2.6f);
    RollerDoor(false, w0, w1, 1, -14.0f, -9.0f, 4.5f);
    RollerDoor(true, s0, s1, -1, -12.0f, -8.0f, 4.0f);
    RollerDoor(true, s0, s1, -1, 0.0f, 4.0f, 4.0f);
  }

  void InteriorWalls() {
    // Hallenwand West (x = -20): Durchfahrt zum Mehllager, Türen zur Werkstatt
    hXm = {{-20.0f, -16.0f, 0, 3.6f}, {6.0f, 7.6f, 0, 2.4f}, {15.0f, 16.6f, 0, 2.4f}};
    // Hallenwand Ost (x = +20): Vortrockner, Trockenraum, Kantine, Umkleide, Laufsteg-Tür ins Büro, Bürofenster
    hXp = {{-15.2f, -11.0f, 0, 3.6f}, {-6.0f, -4.4f, 0, 2.4f}, {7.5f, 9.1f, 0, 2.4f}, {14.0f, 15.6f, 0, 2.4f},
           {-1.5f, -0.1f, OG, OG + 2.3f}};
    AddWindows(hXp, false, IX - IWH, IX + IWH, 0, {4.6f, 7.6f}, 2.6f, 5.0f, 6.8f);
    // Hallen-Südwand (z = 10): Durchgänge zur Verpackung
    hZh = {{-14.0f, -10.0f, 0, 3.0f}, {2.0f, 4.0f, 0, 2.4f}, {12.0f, 14.0f, 0, 3.6f}};
    // Mehllager / Werkstatt (z = 4), Trockenraum / Kantine (z = -2), Kantine / Umkleide (z = 10, EG)
    hZw = {{-30.0f, -28.4f, 0, 2.4f}};
    hZo = {{30.0f, 31.6f, 0, 2.4f}};
    hZu = {{24.0f, 25.6f, 0, 2.4f}};
    const float n1 = BZ0 + WT, s0 = BZ1 - WT;
    k.Style(MAT_BRICK, BRICK);
    k.WallZ(n1, s0, -IX - IWH, -IX + IWH, 0, WALL_H, hXm);
    k.WallZ(n1, s0, IX - IWH, IX + IWH, 0, WALL_H, hXp);
    k.WallX(-IX + IWH, IX - IWH, HZ1 - IWH, HZ1 + IWH, 0, WALL_H, hZh);
    k.WallX(BX0 + WT, -IX - IWH, 4.0f - IWH, 4.0f + IWH, 0, WALL_H, hZw);
    k.WallX(IX + IWH, BX1 - WT, -2.0f - IWH, -2.0f + IWH, 0, WALL_H, hZo);
    k.Style(MAT_PLASTER, PLASTER);
    k.WallX(IX + IWH, BX1 - WT, HZ1 - IWH, HZ1 + IWH, 0, OGC, hZu);
    // Bürogeschoss: Trennwände
    k.WallZ(-2.0f + IWH, HZ1 - IWH, 28.0f - IWH, 28.0f + IWH, OG, WALL_H, {{4.0f, 5.6f, OG, OG + 2.3f}});
    k.WallX(IX + IWH, BX1 - WT, HZ1 - IWH, HZ1 + IWH, OG, WALL_H, {{24.0f, 25.6f, OG, OG + 2.3f}});
    DoorFrame(false, 28.0f - IWH, 28.0f + IWH, 4.0f, 5.6f, OG + 2.3f, OG);
    DoorFrame(true, HZ1 - IWH, HZ1 + IWH, 24.0f, 25.6f, OG + 2.3f, OG);
    DoorFrame(false, IX - IWH, IX + IWH, -1.5f, -0.1f, OG + 2.3f, OG);
    // Zargen der Innentüren
    for (const Hole& h : hXm) if (h.y1 < 3) DoorFrame(false, -IX - IWH, -IX + IWH, h.a0, h.a1, h.y1);
    for (const Hole& h : hXp)
      if (!h.window && h.y1 < 3) DoorFrame(false, IX - IWH, IX + IWH, h.a0, h.a1, h.y1);
    DoorFrame(true, HZ1 - IWH, HZ1 + IWH, 2.0f, 4.0f, 2.4f);
    DoorFrame(true, 4.0f - IWH, 4.0f + IWH, -30.0f, -28.4f, 2.4f);
    DoorFrame(true, -2.0f - IWH, -2.0f + IWH, 30.0f, 31.6f, 2.4f);
    DoorFrame(true, HZ1 - IWH, HZ1 + IWH, 24.0f, 25.6f, 2.4f);
  }

  // Verkleidungen innen (Farbe in der Halle, Fliesen und Putz im Bürotrakt)
  void Cladding() {
    const float n1 = BZ0 + WT, s0 = BZ1 - WT, w1 = BX0 + WT, e0 = BX1 - WT;
    const float hw = 1.4f;
    // Halle: graugrüner Sockelanstrich
    Clad(true, n1, 1, -IX + IWH, IX - IWH, 0, hw, hN, MAT_PLASTER, HALLPAINT);
    Clad(true, HZ1 - IWH, -1, -IX + IWH, IX - IWH, 0, hw, hZh, MAT_PLASTER, HALLPAINT);
    Clad(false, -IX + IWH, 1, n1, HZ1 - IWH, 0, hw, hXm, MAT_PLASTER, HALLPAINT);
    Clad(false, IX - IWH, -1, n1, HZ1 - IWH, 0, hw, hXp, MAT_PLASTER, HALLPAINT);
    // Trockenraum: weiße Fliesen bis 2,2 m
    Clad(true, n1, 1, IX + IWH, e0, 0, 2.2f, hN, MAT_TILE, 0xeef0ec);
    Clad(true, -2.0f - IWH, -1, IX + IWH, e0, 0, 2.2f, hZo, MAT_TILE, 0xeef0ec);
    Clad(false, IX + IWH, 1, n1, -2.0f - IWH, 0, 2.2f, hXp, MAT_TILE, 0xeef0ec);
    Clad(false, e0, -1, n1, -2.0f - IWH, 0, 2.2f, hE, MAT_TILE, 0xeef0ec);
    // Kantine: Fliesen bis 1,6 m, darüber Putz bis zur Decke
    Clad(true, -2.0f + IWH, 1, IX + IWH, e0, 0, 1.6f, hZo, MAT_TILE, 0xd8e2e6);
    Clad(true, -2.0f + IWH, 1, IX + IWH, e0, 1.6f, OGC, hZo, MAT_PLASTER, PLASTER);
    Clad(false, IX + IWH, 1, -2.0f + IWH, HZ1 - IWH, 0, 1.6f, hXp, MAT_TILE, 0xd8e2e6);
    Clad(false, IX + IWH, 1, -2.0f + IWH, HZ1 - IWH, 1.6f, OGC, hXp, MAT_PLASTER, PLASTER);
    Clad(false, e0, -1, -2.0f + IWH, HZ1 - IWH, 0, 1.6f, hE, MAT_TILE, 0xd8e2e6);
    Clad(false, e0, -1, -2.0f + IWH, HZ1 - IWH, 1.6f, OGC, hE, MAT_PLASTER, PLASTER);
    // Umkleide: Putz
    Clad(false, IX + IWH, 1, HZ1 + IWH, s0, 0, OGC, hXp, MAT_PLASTER, 0xdcdcd4);
    Clad(false, e0, -1, HZ1 + IWH, s0, 0, OGC, hE, MAT_PLASTER, 0xdcdcd4);
    Clad(true, s0, -1, IX + IWH, e0, 0, OGC, hS, MAT_PLASTER, 0xdcdcd4);
    // Bürogeschoss: Putz (Meisterbüro, Labor, Archiv)
    std::vector<Hole> wallX28 = {{4.0f, 5.6f, OG, OG + 2.3f}};
    std::vector<Hole> wallZ10 = {{24.0f, 25.6f, OG, OG + 2.3f}};
    Clad(false, IX + IWH, 1, -2.0f + IWH, HZ1 - IWH, OG, WALL_H, hXp, MAT_PLASTER, PLASTER);
    Clad(false, 28.0f - IWH, -1, -2.0f + IWH, HZ1 - IWH, OG, WALL_H, wallX28, MAT_PLASTER, PLASTER);
    Clad(false, 28.0f + IWH, 1, -2.0f + IWH, HZ1 - IWH, OG, WALL_H, wallX28, MAT_PLASTER, 0xe8ecee);
    Clad(false, e0, -1, -2.0f + IWH, HZ1 - IWH, OG, WALL_H, hE, MAT_PLASTER, 0xe8ecee);
    Clad(true, -2.0f + IWH, 1, IX + IWH, 28.0f - IWH, OG, WALL_H, hZo, MAT_PLASTER, PLASTER);
    Clad(true, -2.0f + IWH, 1, 28.0f + IWH, e0, OG, WALL_H, hZo, MAT_PLASTER, 0xe8ecee);
    Clad(true, HZ1 - IWH, -1, IX + IWH, 28.0f - IWH, OG, WALL_H, wallZ10, MAT_PLASTER, PLASTER);
    Clad(true, HZ1 - IWH, -1, 28.0f + IWH, e0, OG, WALL_H, wallZ10, MAT_PLASTER, 0xe8ecee);
    Clad(true, HZ1 + IWH, 1, IX + IWH, e0, OG, WALL_H, wallZ10, MAT_PLASTER, 0xdedad0);
    Clad(false, IX + IWH, 1, HZ1 + IWH, s0, OG, WALL_H, hXp, MAT_PLASTER, 0xdedad0);
    Clad(false, e0, -1, HZ1 + IWH, s0, OG, WALL_H, hE, MAT_PLASTER, 0xdedad0);
    Clad(true, s0, -1, IX + IWH, e0, OG, WALL_H, hS, MAT_PLASTER, 0xdedad0);
    // Werkstatt: grauer Sockel
    Clad(true, 4.0f + IWH, 1, w1, -IX - IWH, 0, 1.6f, hZw, MAT_PLASTER, 0x8a8e88);
    Clad(true, s0, -1, w1, -IX - IWH, 0, 1.6f, hS, MAT_PLASTER, 0x8a8e88);
    Clad(false, w1, 1, 4.0f + IWH, s0, 0, 1.6f, hW, MAT_PLASTER, 0x8a8e88);
    Clad(false, -IX - IWH, -1, 4.0f + IWH, s0, 0, 1.6f, hXm, MAT_PLASTER, 0x8a8e88);
  }

  // ------------------------------------------------------------------------------------------
  // Dächer: Flachdach über den Anbauten, Sheddach über der Halle mit Giebeln
  void Roofs() {
    auto flat = [&](float x0, float z0, float x1, float z1) {
      k.Slab(x0, z0, x1, z1, WALL_H + ROOF_T, ROOF_T, MAT_CONCRETE, 0x5a5c5e, 0xd8d8d2);
    };
    flat(BX0, BZ0, -IX - IWH, BZ1);
    flat(IX + IWH, BZ0, BX1, BZ1);
    flat(-IX - IWH, HZ1 + IWH, IX + IWH, BZ1);
    // Dachrand (Attika) aus Klinker auf den Anbauten
    k.Style(MAT_BRICK, BRICK_D);
    const float ty = WALL_H + ROOF_T, th = 0.5f;
    k.Solid(Vec3(BX0, ty, BZ0), Vec3(-IX - IWH, ty + th, BZ0 + 0.3f));
    k.Solid(Vec3(IX + IWH, ty, BZ0), Vec3(BX1, ty + th, BZ0 + 0.3f));
    k.Solid(Vec3(BX0, ty, BZ1 - 0.3f), Vec3(BX1, ty + th, BZ1));
    k.Solid(Vec3(BX0, ty, BZ0 + 0.3f), Vec3(BX0 + 0.3f, ty + th, BZ1 - 0.3f));
    k.Solid(Vec3(BX1 - 0.3f, ty, BZ0 + 0.3f), Vec3(BX1, ty + th, BZ1 - 0.3f));
    // Sheddach: 4 Zähne à 8 m, Verglasung senkrecht nach Süden
    MeshBuilder& b = k.ob;
    const float x0 = -IX, x1 = IX;
    for (int j = 0; j < 4; j++) {
      float za = BZ0 + j * 8.0f, zb = za + 8.0f;
      float zaO = j == 0 ? za - 0.3f : za;  // Dachüberstand im Norden
      float yaO = SHED_LO - (j == 0 ? 0.3f * (SHED_HI - SHED_LO) / 8.0f : 0);
      Vec3 nUp = Normalize(Vec3(0, 8.0f, -(SHED_HI - SHED_LO)));
      b.Mat(MAT_CORRUGATED);
      b.ColorHex(0x5c6066);
      const float xo0 = x0 - IWH, xo1 = x1 + IWH;
      QuadF(b, Vec3(xo0, yaO + 0.15f, zaO), Vec3(xo1, yaO + 0.15f, zaO), Vec3(xo1, SHED_HI + 0.15f, zb), Vec3(xo0, SHED_HI + 0.15f, zb), nUp);
      b.ColorHex(0xb4b8bc);
      QuadF(b, Vec3(x0, SHED_LO, za), Vec3(x1, SHED_LO, za), Vec3(x1, SHED_HI, zb), Vec3(x0, SHED_HI, zb), nUp * -1.0f);
      if (j == 0) QuadF(b, Vec3(xo0, yaO, zaO), Vec3(xo1, yaO, zaO), Vec3(xo1, SHED_LO, za), Vec3(xo0, SHED_LO, za), nUp * -1.0f);
      // Firstblech und Verglasung
      k.Style(MAT_METAL, 0x50565c);
      k.Visual(Vec3(x0 - IWH - 0.02f, SHED_HI - 0.05f, zb - 0.06f), Vec3(x1 + IWH + 0.02f, SHED_HI + 0.2f, zb + 0.04f));
      k.Glass(Vec3(x0 + 0.15f, SHED_LO, zb - 0.01f), Vec3(x1 - 0.15f, SHED_HI - 0.05f, zb + 0.01f), 0xc4d4dc, 0.22f, false);
      k.Style(MAT_METAL, 0x2a2d31);
      for (float x = x0 + 0.15f; x <= x1 - 0.1f; x += 2.0f) k.Visual(Vec3(x, SHED_LO, zb - 0.04f), Vec3(x + 0.06f, SHED_HI - 0.05f, zb + 0.03f));
      k.Visual(Vec3(x0 + 0.15f, (SHED_LO + SHED_HI) * 0.5f - 0.03f, zb - 0.035f), Vec3(x1 - 0.15f, (SHED_LO + SHED_HI) * 0.5f + 0.03f, zb + 0.025f));
      // Giebel über den Hallenwänden (Ziegel)
      for (float gx : {-IX, IX}) {
        float xa = gx - IWH, xb = gx + IWH;
        b.Mat(MAT_BRICK);
        b.ColorHex(BRICK);
        // Außenseite bis zur Dachoberkante, Innenseite bis zur Untersicht
        float xOut = gx < 0 ? xa : xb, xIn = gx < 0 ? xb : xa, sgn = gx < 0 ? -1.0f : 1.0f;
        QuadF(b, Vec3(xOut, SHED_LO, za), Vec3(xOut, SHED_LO, zb), Vec3(xOut, SHED_HI + 0.15f, zb), Vec3(xOut, SHED_LO + 0.15f, za),
              Vec3(sgn, 0, 0));
        TriN(b, Vec3(xIn, SHED_LO, za), Vec3(xIn, SHED_LO, zb), Vec3(xIn, SHED_HI, zb), Vec3(-sgn, 0, 0));
        QuadF(b, Vec3(xa, SHED_LO, zb), Vec3(xb, SHED_LO, zb), Vec3(xb, SHED_HI + 0.15f, zb), Vec3(xa, SHED_HI + 0.15f, zb), Vec3(0, 0, 1));
      }
    }
    // Dach nicht verlassbar (Kugeln und Granaten bleiben drin)
    k.Clip(Vec3(-IX, SHED_LO, BZ0), Vec3(IX, SHED_LO + 0.2f, HZ1), COL_SOLID);
  }

  // ------------------------------------------------------------------------------------------
  // Hallentragwerk, Stützen, Kranbahn, Pendelleuchten
  void HallStructure() {
    MeshBuilder& b = k.ob;
    // Shedbinder unter den Glaskanten
    for (float z : {-14.0f, -6.0f, 2.0f}) {
      k.Style(MAT_METAL, 0x5a6470);
      k.Visual(Vec3(-IX + IWH, SHED_LO - 0.42f, z - 0.1f), Vec3(IX - IWH, SHED_LO - 0.38f, z + 0.1f));
      k.Visual(Vec3(-IX + IWH, SHED_LO - 0.04f, z - 0.1f), Vec3(IX - IWH, SHED_LO, z + 0.1f));
      k.Visual(Vec3(-IX + IWH, SHED_LO - 0.38f, z - 0.02f), Vec3(IX - IWH, SHED_LO - 0.04f, z + 0.02f));
      for (float x = -18.0f; x <= 18.1f; x += 2.0f) Beam(b, Vec3(x, SHED_LO - 0.4f, z), Vec3(x + 1.0f, SHED_LO - 0.02f, z), 0.025f);
    }
    // Pfetten unter der Dachschräge
    k.Style(MAT_METAL, 0x5a6470);
    for (int j = 0; j < 4; j++) {
      float za = BZ0 + j * 8.0f, zb = za + 8.0f;
      for (float x = -16.0f; x <= 16.1f; x += 4.0f)
        Beam(b, Vec3(x, SHED_LO - 0.12f, za + 0.05f), Vec3(x, SHED_HI - 0.12f, zb - 0.05f), 0.05f);
    }
    // Stützen (I-Profil) mit Anfahrschutz, unter den Bindern bei z = -6 und z = 2
    for (float z : {-6.0f, 2.0f})
      for (float x : {-12.0f, -4.0f, 4.0f, 12.0f}) {
        k.Style(MAT_METAL, 0x5a6470);
        k.Solid(Vec3(x - 0.15f, 0.9f, z - 0.15f), Vec3(x + 0.15f, SHED_LO - 0.42f, z - 0.12f));
        k.Solid(Vec3(x - 0.15f, 0.9f, z + 0.12f), Vec3(x + 0.15f, SHED_LO - 0.42f, z + 0.15f));
        k.Visual(Vec3(x - 0.02f, 0.9f, z - 0.12f), Vec3(x + 0.02f, SHED_LO - 0.42f, z + 0.12f));
        for (int i = 0; i < 4; i++) {
          k.Style(MAT_PLASTIC, i % 2 ? 0x1a1a1a : YELLOW);
          k.Solid(Vec3(x - 0.22f, i * 0.225f, z - 0.22f), Vec3(x + 0.22f, i * 0.225f + 0.225f, z + 0.22f));
        }
        k.Prop("feuerloescher", Vec3(x, 0, z + 0.32f), 0);
      }
    // Kranbahn mit Brückenkran
    k.Style(MAT_METAL, 0x2f3338);
    for (float z : {-21.3f, 9.5f}) k.Visual(Vec3(-IX + IWH, 7.0f, z - 0.15f), Vec3(IX - IWH, 7.3f, z + 0.15f));
    k.Style(MAT_METAL, YELLOW);
    k.Visual(Vec3(5.4f, 7.3f, -21.5f), Vec3(5.9f, 7.9f, 9.7f));
    k.Visual(Vec3(6.5f, 7.3f, -21.5f), Vec3(7.0f, 7.9f, 9.7f));
    for (float z : {-21.5f, 9.2f}) k.Visual(Vec3(5.2f, 7.3f, z), Vec3(7.2f, 7.85f, z + 0.5f));
    k.Style(MAT_METAL, 0x30353a);
    k.Visual(Vec3(5.6f, 7.05f, -4.0f), Vec3(6.8f, 7.55f, -2.9f));  // Laufkatze
    k.Style(MAT_METAL, 0x9aa0a6);
    Beam(b, Vec3(6.0f, 7.05f, -3.45f), Vec3(6.0f, 5.3f, -3.45f), 0.012f);
    Beam(b, Vec3(6.4f, 7.05f, -3.45f), Vec3(6.4f, 5.3f, -3.45f), 0.012f);
    k.Style(MAT_METAL, YELLOW);
    k.Visual(Vec3(5.9f, 4.95f, -3.6f), Vec3(6.5f, 5.35f, -3.3f));  // Unterflasche
    k.Style(MAT_METAL, 0x2a2c30);
    b.Push(Translation(Vec3(6.2f, 4.75f, -3.45f)) * RotationX(PI * 0.5f));
    b.Torus(Vec3(0), 0.12f, 0.035f, 14, 6);
    b.Pop();
    k.Sign("10 T", Vec3(6.2f, 7.6f, -21.0f), 0, 0.05f, 0x1a1a1a, 0, false);
    // Pendelleuchten
    for (float x : {-15.0f, -5.0f, 5.0f, 15.0f})
      for (float z : {-18.0f, -10.0f, -2.0f, 6.0f}) {
        k.Style(MAT_METAL, 0x9aa0a6);
        Beam(b, Vec3(x, 6.9f, z), Vec3(x, SHED_LO, z), 0.008f);
        k.Style(MAT_METAL, 0x3d5a4a);
        b.Frustum(Vec3(x, 6.7f, z), 0.38f, 0.1f, 0.22f, 16, false);
        k.Style(MAT_PLASTIC, 0xfff2d8);
        k.Glow(1.0f);
        b.Ring(Vec3(x, 6.7f, z), 0.0f, 0.36f, 16, false);
        k.Glow(0);
        k.Light(Vec3(x, 6.4f, z), 14, Vec3(2.3f, 2.2f, 1.95f));
      }
  }

  // ------------------------------------------------------------------------------------------
  // Linie A: Mehltrichter, Teigmischer auf der Bühne, Nudelpresse, Band, Schneidwerk, Vortrockner
  void LineA() {
    MeshBuilder& b = k.ob;
    const float zc = -14.0f;
    // Mehltrichter
    {
      Vec3 c(-17.6f, 0, zc);
      k.Style(MAT_METAL, STEEL);
      for (float sx : {-0.75f, 0.75f})
        for (float sz : {-0.75f, 0.75f}) k.Solid(c + Vec3(sx - 0.08f, 0, sz - 0.08f), c + Vec3(sx + 0.08f, 2.3f, sz + 0.08f));
      k.Style(MAT_METAL, SSTEEL);
      b.Frustum(c + Vec3(0, 2.0f, 0), 0.25f, 1.0f, 1.2f, 20);
      b.Cylinder(c + Vec3(0, 3.2f, 0), 1.0f, 1.0f, 20);
      Pipe(b, c + Vec3(0, 4.2f, 0), c + Vec3(0, 8.2f, 0), 0.16f);
      Pipe(b, c + Vec3(0, 8.2f, 0), Vec3(BX0 + WT, 8.2f, zc), 0.16f);
      Pipe(b, c + Vec3(0, 2.0f, 0), Vec3(-14.4f, 3.05f, zc), 0.09f);
      k.Clip(c + Vec3(-1.0f, 2.0f, -1.0f), c + Vec3(1.0f, 4.25f, 1.0f), COL_SOLID);
      k.Sign("MEHL", c + Vec3(0, 3.7f, 1.01f), 0, 0.05f, 0xf2f2ea, 0, true);
    }
    // Mischerbühne
    const float px0 = -16.0f, px1 = -9.0f, pz0 = -16.5f, pz1 = -11.5f, ph = 1.2f;
    k.Style(MAT_METAL, 0x56635c);
    k.Solid(Vec3(px0, 0, pz0), Vec3(px1, ph, pz1));
    k.Style(MAT_METAL, 0x6a7078);
    k.FloorFinish(px0, pz0, px1, pz1, ph + 0.012f, MAT_METAL, 0x6a7078);
    k.Style(MAT_METAL, STEEL);
    k.Stairs(Vec3(-15.0f, 0, -9.1f), 3, 1.2f, 0.2f, 6, 0.4f, 0);
    k.Railing(Vec3(px0, ph, pz0), Vec3(px1, ph, pz0), 1.0f, false);
    k.Railing(Vec3(px0, ph, pz0), Vec3(px0, ph, pz1), 1.0f, false);
    k.Railing(Vec3(px1, ph, pz0), Vec3(px1, ph, pz1), 1.0f, false);
    k.Railing(Vec3(-14.35f, ph, pz1), Vec3(px1, ph, pz1), 1.0f, false);
    // Teigmischer mit Knethaken (drehen sich)
    for (float tx : {-14.4f, -10.6f}) {
      Vec3 c(tx, ph, zc);
      k.Style(MAT_METAL, SSTEEL);
      b.Frustum(c, 1.05f, 1.05f, 1.3f, 28, false);
      b.Mat(MAT_METAL);
      b.ColorHex(0xb4b9be);
      for (int i = 0; i < 28; i++) {
        float a0 = TAU * i / 28, a1 = TAU * (i + 1) / 28;
        Vec3 p0 = c + Vec3(cosf(a0), 0, sinf(a0)) * 1.0f, p1 = c + Vec3(cosf(a1), 0, sinf(a1)) * 1.0f;
        Vec3 n = Vec3(-cosf((a0 + a1) * 0.5f), 0, -sinf((a0 + a1) * 0.5f));
        QuadF(b, p0 + Vec3(0, 1.16f, 0), p1 + Vec3(0, 1.16f, 0), p1 + Vec3(0, 1.3f, 0), p0 + Vec3(0, 1.3f, 0), n);
      }
      b.ColorHex(0xd4d8dc);
      b.Ring(c + Vec3(0, 1.3f, 0), 1.0f, 1.05f, 28, true);
      k.Style(MAT_PLASTIC, 0xf0dca8);  // Teigoberfläche
      b.Cylinder(c + Vec3(0, 1.0f, 0), 1.0f, 0.16f, 28);
      // Ausleger mit Getriebe
      k.Style(MAT_METAL, MGREEN);
      k.Solid(c + Vec3(-0.12f, 0, -1.4f), c + Vec3(0.12f, 2.6f, -1.15f));
      k.Visual(c + Vec3(-0.12f, 2.3f, -1.15f), c + Vec3(0.12f, 2.55f, 0.1f));
      k.Visual(c + Vec3(-0.3f, 2.55f, -0.3f), c + Vec3(0.3f, 3.0f, 0.3f));
      b.CylinderX(c + Vec3(0.55f, 2.78f, 0), 0.18f, 0.5f, 14);
      k.Clip(c + Vec3(-0.95f, 0, -0.95f), c + Vec3(0.95f, 1.3f, 0.95f), COL_SOLID);
      AnimatedPart hook;
      hook.pivot = Vec3(tx, 0, zc);
      hook.axis = 1;
      hook.speed = 1.4f;
      hook.phase = tx;
      {
        MeshBuilder hb(&hook.mesh);
        hb.Mat(MAT_METAL);
        hb.ColorHex(0xd8dce0);
        Vec3 top = c + Vec3(0, 2.3f, 0);
        Pipe(hb, top, c + Vec3(0, 0.9f, 0), 0.05f, 10);
        for (int i = 0; i < 18; i++) {
          float t0 = i / 18.0f, t1 = (i + 1) / 18.0f;
          Vec3 a = c + Vec3(cosf(t0 * TAU * 1.3f) * 0.42f, 1.05f + t0 * 0.95f, sinf(t0 * TAU * 1.3f) * 0.42f);
          Vec3 d = c + Vec3(cosf(t1 * TAU * 1.3f) * 0.42f, 1.05f + t1 * 0.95f, sinf(t1 * TAU * 1.3f) * 0.42f);
          Pipe(hb, a, d, 0.035f, 8);
        }
        Pipe(hb, c + Vec3(0, 2.0f, 0), c + Vec3(0.42f, 2.0f, 0), 0.03f, 8);
      }
      w.anims.push_back(std::move(hook));
    }
    k.Sign("TEIGMISCHER", Vec3(-12.5f, 0.6f, pz1 + 0.01f), 0, 0.035f, 0xf2f2ea, 0, true);
    // Nudelpresse (Extruder)
    {
      k.Style(MAT_METAL, MGREEN);
      k.Solid(Vec3(-8.2f, 0, zc - 1.0f), Vec3(-2.2f, 0.9f, zc + 1.0f));
      k.Visual(Vec3(-8.2f, 0.9f, zc - 0.7f), Vec3(-6.6f, 2.1f, zc + 0.7f));  // Getriebe
      b.CylinderX(Vec3(-7.4f, 2.45f, zc), 0.3f, 1.2f, 16);              // Motor
      k.Style(MAT_METAL, SSTEEL);
      b.CylinderX(Vec3(-4.4f, 1.45f, zc), 0.32f, 4.4f, 20);             // Schneckengehäuse
      k.Style(MAT_METAL, 0x2a2d31);
      for (float x = -6.0f; x <= -2.9f; x += 0.8f) b.CylinderX(Vec3(x, 1.45f, zc), 0.345f, 0.12f, 20);  // Heizbänder
      k.Style(MAT_METAL, SSTEEL);
      b.Frustum(Vec3(-6.0f, 1.75f, zc), 0.15f, 0.45f, 0.6f, 16);         // Einfülltrichter
      Pipe(b, Vec3(-6.0f, 2.35f, zc), Vec3(-10.6f, 3.0f, zc), 0.08f);
      b.CylinderX(Vec3(-2.05f, 1.45f, zc), 0.42f, 0.3f, 20);            // Matrizenkopf
      k.Clip(Vec3(-8.2f, 0.9f, zc - 0.7f), Vec3(-1.85f, 2.75f, zc + 0.7f), COL_SOLID);
      // Bedienpult mit Bildschirm
      k.Style(MAT_METAL, 0x3a4450);
      k.Solid(Vec3(-3.4f, 0, zc + 1.05f), Vec3(-2.4f, 1.15f, zc + 1.45f));
      k.Style(MAT_SCREEN, 0x4ad08a);
      k.Visual(Vec3(-3.3f, 0.75f, zc + 1.45f), Vec3(-2.5f, 1.08f, zc + 1.465f));
      k.Style(MAT_PLASTIC, 0xd8281e);
      b.CylinderZ(Vec3(-2.6f, 0.55f, zc + 1.47f), 0.04f, 0.04f, 12);   // Not-Aus
      k.Sign("NUDELPRESSE", Vec3(-5.2f, 0.5f, zc + 1.01f), 0, 0.035f, 0xf2f2ea, 0, true);
    }
    // Förderband mit laufenden Nudeln
    {
      const float x0 = -1.85f, x1 = 11.0f;
      k.Style(MAT_METAL, STEEL_L);
      k.Solid(Vec3(x0, 0.6f, zc - 0.45f), Vec3(x1, 0.85f, zc + 0.45f));
      k.Style(MAT_PLASTIC, 0x1d1e20);
      k.Solid(Vec3(x0, 0.85f, zc - 0.35f), Vec3(x1, 0.9f, zc + 0.35f));
      k.Style(MAT_METAL, STEEL_L);
      for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x0, 0.85f, zc + s * 0.37f - 0.02f), Vec3(x1, 0.98f, zc + s * 0.37f + 0.02f));
      for (float x = x0 + 0.3f; x < x1; x += 1.6f)
        for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x - 0.04f, 0, zc + s * 0.4f - 0.04f), Vec3(x + 0.04f, 0.6f, zc + s * 0.4f + 0.04f));
      AnimatedPart pasta;
      pasta.slideDir = Vec3(1, 0, 0);
      pasta.slideLen = 0.5f;
      pasta.speed = 0.35f;
      MeshBuilder pb(&pasta.mesh);
      pb.Mat(MAT_PLASTIC);
      Rng r(5);
      std::vector<Vec3> pat;
      for (int i = 0; i < 9; i++) pat.push_back(Vec3(r.Range(0, 0.5f), r.Range(-0.25f, 0.25f), r.Range(0, PI)));
      for (float base = x0 + 0.1f; base < x1 - 0.6f; base += 0.5f)
        for (const Vec3& q : pat) {
          pb.ColorHex(q.x > 0.25f ? 0xead07a : 0xe2c464);
          pb.Push(Translation(Vec3(base + q.x, 0.912f, zc + q.y)) * RotationY(q.z));
          pb.CylinderX(Vec3(0, 0, 0), 0.012f, 0.045f, 6);
          pb.Pop();
        }
      w.anims.push_back(std::move(pasta));
    }
    // Schneidwerk mit drehendem Messer
    {
      k.Style(MAT_METAL, SSTEEL);
      k.Solid(Vec3(11.0f, 0, zc - 1.0f), Vec3(13.2f, 1.9f, zc + 0.9f));
      k.Style(MAT_METAL, YELLOW);
      k.Visual(Vec3(11.0f, 1.9f, zc - 1.0f), Vec3(13.2f, 1.98f, zc + 0.9f));
      k.Style(MAT_METAL, 0x2a2d31);
      b.CylinderZ(Vec3(12.1f, 1.05f, zc + 0.93f), 0.08f, 0.06f, 12);
      b.Push(Translation(Vec3(12.1f, 1.05f, zc + 1.02f)));
      b.Torus(Vec3(0), 0.42f, 0.02f, 24, 5);
      b.Pop();
      for (int i = 0; i < 6; i++) {
        float a = TAU * i / 6;
        Beam(b, Vec3(12.1f, 1.05f, zc + 1.02f), Vec3(12.1f + cosf(a) * 0.42f, 1.05f + sinf(a) * 0.42f, zc + 1.02f), 0.008f);
      }
      AnimatedPart blade;
      blade.pivot = Vec3(12.1f, 1.05f, zc + 0.97f);
      blade.axis = 2;
      blade.speed = 7.0f;
      {
        MeshBuilder bb(&blade.mesh);
        bb.Mat(MAT_METAL);
        bb.ColorHex(0xe4e8ec);
        bb.CylinderZ(blade.pivot, 0.34f, 0.008f, 28);
        bb.ColorHex(0x8a9096);
        for (int i = 0; i < 4; i++) {
          float a = TAU * i / 4;
          Vec3 d(cosf(a), sinf(a), 0);
          bb.Box(blade.pivot + d * 0.3f - Vec3(0.03f, 0.03f, 0.008f), blade.pivot + d * 0.3f + Vec3(0.03f, 0.03f, 0.008f));
        }
      }
      w.anims.push_back(std::move(blade));
      k.Sign("SCHNEIDWERK", Vec3(12.1f, 1.6f, zc + 0.91f), 0, 0.022f, 0x1a1a1a, 0, false);
    }
    // Vortrockner-Tunnel (läuft durch die Wand in den Trockenraum)
    {
      const float x0 = 13.2f, x1 = 24.5f, z0 = zc - 0.95f, z1 = zc + 0.95f;
      k.Style(MAT_PLASTIC, 0xd8dcd8);
      k.Solid(Vec3(x0, 0, z0), Vec3(x1, 2.4f, z1));
      k.Style(MAT_PLASTIC, 0xc0c4c0);
      for (float x = x0 + 1.0f; x < x1; x += 1.2f) {
        if (fabsf(x - IX) < 0.5f) continue;
        k.Visual(Vec3(x - 0.03f, 0.05f, z1), Vec3(x + 0.03f, 2.35f, z1 + 0.02f));
        k.Visual(Vec3(x - 0.03f, 0.05f, z0 - 0.02f), Vec3(x + 0.03f, 2.35f, z0));
      }
      k.Style(MAT_PLASTIC, 0xc8281e);
      for (float zz : {z0 - 0.04f, z1})
        SplitRect(x0, x1, 1.95f, 2.05f, {{IX - IWH - 0.05f, IX + IWH + 0.05f, 0, 10}}, [&](float a, float c, float ya, float yb) {
          k.Visual(Vec3(a, ya, zz), Vec3(c, yb, zz + 0.04f));
        });
      k.Style(MAT_METAL, SSTEEL);
      for (float x : {15.5f, 18.0f, 23.0f}) Pipe(b, Vec3(x, 2.4f, zc), Vec3(x, 3.5f, zc), 0.15f);
      k.Sign("VORTROCKNER", Vec3(16.6f, 1.3f, z1 + 0.025f), 0, 0.04f, 0x1a1a1a, 0, false);
    }
  }

  // ------------------------------------------------------------------------------------------
  // Linie B: Spaghetti-Presse, Stangen mit hängenden Spaghetti laufen in die Trockenkammer
  void LineB() {
    MeshBuilder& b = k.ob;
    const float zc = -3.0f;
    // Presse
    k.Style(MAT_METAL, MGREEN);
    k.Solid(Vec3(-16.5f, 0, zc - 1.5f), Vec3(-13.2f, 1.2f, zc + 1.5f));
    k.Solid(Vec3(-16.1f, 1.2f, zc - 0.8f), Vec3(-13.7f, 3.4f, zc + 0.8f));
    k.Style(MAT_METAL, SSTEEL);
    b.Cylinder(Vec3(-14.9f, 3.4f, zc), 0.55f, 1.0f, 20);
    b.Frustum(Vec3(-14.9f, 4.4f, zc), 0.55f, 0.9f, 0.7f, 20);
    b.CylinderX(Vec3(-13.5f, 3.0f, zc), 0.25f, 0.4f, 16);
    k.Clip(Vec3(-16.1f, 3.4f, zc - 0.9f), Vec3(-13.7f, 5.1f, zc + 0.9f), COL_SOLID);
    k.Style(MAT_SCREEN, 0x4ad08a);
    k.Visual(Vec3(-15.6f, 1.8f, zc + 0.8f), Vec3(-14.4f, 2.5f, zc + 0.815f));
    k.Sign("SPAGHETTI", Vec3(-14.9f, 0.7f, zc + 1.51f), 0, 0.035f, 0xf2f2ea, 0, true);
    // Unterbau der Stangenbahn (massiv) und Sperre darüber, damit sich niemand in den Spaghetti versteckt
    const float x0 = -13.2f, x1 = 10.0f;
    k.Style(MAT_METAL, 0x5a6a64);
    k.Solid(Vec3(x0, 0, zc - 0.8f), Vec3(x1, 1.2f, zc + 0.8f));
    k.Style(MAT_METAL, 0x4a5852);
    for (float x = x0 + 1.5f; x < x1; x += 3.0f)
      for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x - 0.6f, 0.15f, zc + s * 0.8f - (s < 0 ? 0.015f : 0)), Vec3(x + 0.6f, 1.05f, zc + s * 0.8f + (s > 0 ? 0.015f : 0)));
    k.Clip(Vec3(x0, 1.2f, zc - 0.8f), Vec3(x1, 3.7f, zc + 0.8f), COL_PLAYER);
    // Portale mit Laufschienen
    k.Style(MAT_METAL, STEEL);
    for (float x = x0 + 0.5f; x < x1; x += 3.0f)
      for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x - 0.05f, 1.2f, zc + s * 0.62f - 0.05f), Vec3(x + 0.05f, 3.45f, zc + s * 0.62f + 0.05f));
    for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x0, 3.3f, zc + s * 0.45f - 0.03f), Vec3(x1, 3.36f, zc + s * 0.45f + 0.03f));
    for (float x = x0 + 0.5f; x < x1; x += 3.0f) k.Visual(Vec3(x - 0.05f, 3.36f, zc - 0.67f), Vec3(x + 0.05f, 3.45f, zc + 0.67f));
    // Laufende Stangen mit hängenden Spaghetti
    AnimatedPart line;
    line.slideDir = Vec3(1, 0, 0);
    line.slideLen = 0.5f;
    line.speed = 0.12f;
    {
      MeshBuilder lb(&line.mesh);
      Rng r(9);
      for (float x = x0 + 0.2f; x < x1 - 0.6f; x += 0.5f) {
        lb.Mat(MAT_METAL);
        lb.ColorHex(0xc8ccd0);
        lb.CylinderZ(Vec3(x, 3.22f, zc), 0.012f, 0.95f, 6);
        lb.Mat(MAT_PLASTIC);
        for (int i = 0; i < 14; i++) {
          float z = zc - 0.4f + i * 0.0615f, len = 1.0f + r.Range(-0.08f, 0.08f);
          lb.ColorHex(i % 3 ? 0xead07a : 0xe2c464);
          for (float side : {-1.0f, 1.0f})
            lb.Box(Vec3(x + side * 0.014f - 0.004f, 3.21f - len, z - 0.018f), Vec3(x + side * 0.014f + 0.004f, 3.21f, z + 0.018f));
        }
      }
    }
    w.anims.push_back(std::move(line));
    // Trockenkammer B
    k.Style(MAT_PLASTIC, 0xd8dcd8);
    k.Solid(Vec3(10.0f, 0, zc - 1.6f), Vec3(14.0f, 3.6f, zc + 1.6f));
    k.Style(MAT_METAL, 0x9aa0a6);
    k.Visual(Vec3(11.0f, 0.1f, zc + 1.6f), Vec3(13.0f, 2.6f, zc + 1.62f));
    k.Style(MAT_METAL, 0x2a2d31);
    k.Visual(Vec3(12.8f, 1.2f, zc + 1.62f), Vec3(12.86f, 1.6f, zc + 1.68f));
    k.Style(MAT_SCREEN, 0xffa040);
    k.Visual(Vec3(13.2f, 2.0f, zc + 1.6f), Vec3(13.8f, 2.4f, zc + 1.615f));
    k.Sign("TROCKENKAMMER", Vec3(12.0f, 3.0f, zc + 1.61f), 0, 0.03f, 0x1a1a1a, 0, false);
  }

  // ------------------------------------------------------------------------------------------
  // Laufstege auf 4 m (Nord, West, Ost) mit Stahltreppen
  void Catwalk(Vec3 mn, Vec3 mx) {
    k.Style(MAT_METAL, 0x4a4e54);
    k.Solid(Vec3(mn.x, CAT - 0.2f, mn.z), Vec3(mx.x, CAT, mx.z));
    k.Style(MAT_METAL, 0x3a3e44);
    for (float x = mn.x + 0.3f; x < mx.x - 0.1f; x += 0.6f)
      if (mx.x - mn.x > 3) k.Visual(Vec3(x, CAT - 0.2f - 0.01f, mn.z + 0.05f), Vec3(x + 0.04f, CAT - 0.2f, mx.z - 0.05f));
  }

  void Catwalks() {
    const float n1 = BZ0 + WT;
    // Nord
    Catwalk(Vec3(-IX + IWH, 0, n1), Vec3(IX - IWH, 0, -19.9f));
    // West und Ost
    Catwalk(Vec3(-IX + IWH, 0, -19.9f), Vec3(-18.2f, 0, -9.0f));
    Catwalk(Vec3(18.2f, 0, -19.9f), Vec3(IX - IWH, 0, 0.4f));
    // Geländer
    k.Railing(Vec3(-18.2f, CAT, -19.9f), Vec3(18.2f, CAT, -19.9f), 1.05f, false);
    k.Railing(Vec3(-18.2f, CAT, -19.9f), Vec3(-18.2f, CAT, -9.0f), 1.05f, false);
    k.Railing(Vec3(18.2f, CAT, -19.9f), Vec3(18.2f, CAT, 0.4f), 1.05f, false);
    // Stützen
    k.Style(MAT_METAL, YELLOW);
    for (float x = -16.0f; x <= 16.1f; x += 4.0f) k.Solid(Vec3(x - 0.06f, 0, -20.02f), Vec3(x + 0.06f, CAT - 0.2f, -19.9f));
    for (float z : {-15.0f, -11.0f}) k.Solid(Vec3(-18.32f, 0, z - 0.06f), Vec3(-18.2f, CAT - 0.2f, z + 0.06f));
    for (float z : {-9.5f, -5.0f, -1.0f}) k.Solid(Vec3(18.2f, 0, z - 0.06f), Vec3(18.32f, CAT - 0.2f, z + 0.06f));
    // Treppen (Stahl) mit Wangen
    k.Style(MAT_METAL, 0x4a4e54);
    k.Stairs(Vec3(-18.95f, 0, -3.4f), 3, 1.3f, 0.2f, 20, 0.28f, 0);
    k.Stairs(Vec3(18.95f, 0, 6.0f), 3, 1.3f, 0.2f, 20, 0.28f, 0);
    MeshBuilder& b = k.ob;
    k.Style(MAT_METAL, YELLOW);
    for (float sx : {-1.0f, 1.0f}) {
      Beam(b, Vec3(-18.95f + sx * 0.69f, 0.2f, -3.4f), Vec3(-18.95f + sx * 0.69f, CAT + 0.05f, -9.0f), 0.025f);
      Beam(b, Vec3(18.95f + sx * 0.69f, 0.2f, 6.0f), Vec3(18.95f + sx * 0.69f, CAT + 0.05f, 0.4f), 0.025f);
      Beam(b, Vec3(-18.95f + sx * 0.69f, 1.1f, -3.4f), Vec3(-18.95f + sx * 0.69f, CAT + 1.0f, -9.0f), 0.02f);
      Beam(b, Vec3(18.95f + sx * 0.69f, 1.1f, 6.0f), Vec3(18.95f + sx * 0.69f, CAT + 1.0f, 0.4f), 0.02f);
    }
    k.Sign("AUSGANG BUERO", Vec3(IX - IWH - 0.02f, CAT + 2.6f, -0.8f), -PI * 0.5f, 0.022f, 0x2fa04a, 0.6f, true);
  }

  // ------------------------------------------------------------------------------------------
  void HallProps() {
    // Leitstand
    k.Style(MAT_METAL, 0x3a4450);
    k.Solid(Vec3(-1.6f, 0, 7.2f), Vec3(1.6f, 0.8f, 8.2f));
    k.Style(MAT_SCREEN, 0x5ab0e8);
    for (float x : {-1.0f, 0.0f, 1.0f}) k.Visual(Vec3(x - 0.38f, 0.82f, 7.95f), Vec3(x + 0.38f, 1.28f, 7.99f));
    k.Style(MAT_PLASTIC, 0x1d1e20);
    for (float x : {-1.0f, 0.0f, 1.0f}) k.Visual(Vec3(x - 0.4f, 0.8f, 7.99f), Vec3(x + 0.4f, 1.3f, 8.05f));
    k.Prop("buerostuhl", Vec3(0, 0, 6.6f), PI);
    k.Prop("klemmbrett", Vec3(-1.2f, 0.8f, 7.5f), 0.3f);
    k.Prop("telefon", Vec3(1.3f, 0.8f, 7.5f), 0);
    k.Style(MAT_SCREEN, 0x3a8ad0);
    k.Visual(Vec3(-2.5f, 3.0f, HZ1 - IWH - 0.024f), Vec3(2.5f, 4.6f, HZ1 - IWH - 0.012f));
    k.Sign("LINIE A  LINIE B", Vec3(0, 4.85f, HZ1 - IWH - 0.02f), PI, 0.03f, 0xf2f2ea, 0, true);
    // Material an den Linien
    k.Prop("mehlpalette", Vec3(-18.4f, 0, -11.0f), PI * 0.5f);
    k.Prop("mehlpalette", Vec3(-16.6f, 0, -7.4f), PI * 0.5f);
    k.Prop("mehlsack", Vec3(-16.9f, 0, -10.6f), 0.4f);
    k.Prop("teigwanne", Vec3(-8.5f, 0, -11.2f), 0.2f);
    k.Prop("teigwanne", Vec3(-7.0f, 0, -11.4f), -0.1f);
    k.Prop("teigwanne", Vec3(-11.5f, 0, -7.6f), 1.4f);
    k.Prop("schaltschrank", Vec3(-9.5f, 0, -21.4f), 0);
    k.Prop("schaltschrank", Vec3(-8.6f, 0, -21.4f), 0);
    k.Prop("schaltschrank", Vec3(9.0f, 0, -21.4f), 0);
    k.Prop("ibc", Vec3(-1.0f, 0, -18.4f), 0);
    k.Prop("ibc", Vec3(0.4f, 0, -18.4f), 0);
    k.Prop("fass_blau", Vec3(3.0f, 0, -18.6f), 0);
    k.Prop("fass_blau", Vec3(3.7f, 0, -18.5f), 0);
    k.Prop("fass_blau", Vec3(3.35f, 0, -19.2f), 0);
    k.Prop("werkzeugkiste", Vec3(-3.0f, 0, -11.6f), 0.5f);
    k.Prop("warnschild", Vec3(2.0f, 0, -11.2f), 0.2f);
    k.Prop("eimer", Vec3(-1.4f, 0, -11.3f), 0);
    k.Prop("nudelkarton", Vec3(14.6f, 0, -11.4f), 0.1f);
    k.Prop("nudelkarton", Vec3(14.6f, 0.4f, -11.4f), 0.15f);
    k.Prop("nudelkarton", Vec3(15.3f, 0, -11.5f), -0.1f);
    k.Prop("kartonpalette", Vec3(16.2f, 0, -7.4f), 0);
    k.Prop("palette", Vec3(14.2f, 0, -7.4f), 0);
    k.Prop("palette", Vec3(14.2f, 0.144f, -7.4f), 0.05f);
    k.Prop("palette", Vec3(14.2f, 0.288f, -7.4f), -0.03f);
    k.Prop("hubwagen", Vec3(9.0f, 0, -8.0f), PI * 0.5f);
    k.Prop("sackkarre", Vec3(-16.4f, 0, -8.6f), 1.2f);
    k.Prop("ventilator", Vec3(7.8f, 0, 0.4f), PI);
    k.Prop("kabeltrommel", Vec3(-9.0f, 0, 0.6f), 0.3f);
    k.Prop("trockenwagen", Vec3(13.6f, 0, 3.6f), 0);
    k.Prop("trockenwagen", Vec3(13.6f, 0, 5.0f), 0);
    k.Prop("nudelpackung", Vec3(-0.6f, 0.8f, 7.5f), 0.4f);
    k.Prop("schutzhelm", Vec3(1.0f, 0.8f, 7.45f), 0.6f);
    k.Prop("huetchen", Vec3(4.6f, 0, 4.0f), 0);
    k.Prop("huetchen", Vec3(5.3f, 0, 4.0f), 0);
    k.Prop("muelltonne", Vec3(-19.2f, 0, 8.8f), PI * 0.5f);
    k.Prop("papierkorb", Vec3(2.2f, 0, 7.8f), 0);
    k.Prop("kartonpalette", Vec3(-6.0f, 0, 6.8f), 0.05f);
    k.Prop("mehlpalette", Vec3(-8.6f, 0, 6.6f), -0.04f);
    k.Prop("tomatendose", Vec3(-8.4f, 0.784f, 6.5f), 0);
    k.Prop("palette", Vec3(10.0f, 0, 7.0f), 0.1f);
    k.Prop("parmesan", Vec3(9.8f, 0.144f, 7.0f), 0);
  }

  // ------------------------------------------------------------------------------------------
  // Trockenraum: Gestelle mit hängenden Spaghetti, Trockenwagen, Wandlüfter, warmes Licht
  void DryingRack(float x0, float x1, float zc) {
    MeshBuilder& b = k.ob;
    k.Style(MAT_METAL, SSTEEL);
    for (float x = x0; x <= x1 + 0.01f; x += (x1 - x0) / 4)
      for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x - 0.03f, 0, zc + s * 0.3f - 0.03f), Vec3(x + 0.03f, 3.0f, zc + s * 0.3f + 0.03f));
    for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x0, 2.97f, zc + s * 0.3f - 0.025f), Vec3(x1, 3.03f, zc + s * 0.3f + 0.025f));
    Rng r((u32)(zc * 100 + 7000));
    for (int lv = 0; lv < 4; lv++) {
      float y = 0.95f + lv * 0.62f;
      for (float pz : {-0.13f, 0.13f}) {
        b.Mat(MAT_WOOD);
        b.ColorHex(0xd8c090);
        b.CylinderX(Vec3((x0 + x1) * 0.5f, y, zc + pz), 0.012f, x1 - x0, 6);
        b.Mat(MAT_PLASTIC);
        for (float x = x0 + 0.08f; x < x1 - 0.06f; x += 0.1f) {
          float len = 0.42f + r.Range(-0.05f, 0.05f);
          b.ColorHex(r.Chance(0.3f) ? 0xe2c464 : 0xead07a);
          for (float side : {-1.0f, 1.0f})
            b.Box(Vec3(x - 0.022f, y - len, zc + pz + side * 0.008f - 0.005f), Vec3(x + 0.022f, y - 0.012f, zc + pz + side * 0.008f + 0.005f));
        }
      }
    }
    // massiv, damit sich niemand zwischen die Spaghetti stellt
    k.Clip(Vec3(x0 - 0.05f, 0, zc - 0.35f), Vec3(x1 + 0.05f, 3.05f, zc + 0.35f), COL_SOLID);
  }

  void DryingRoom() {
    for (float zc : {-19.6f, -16.4f, -10.0f, -6.8f}) DryingRack(25.5f, 34.5f, zc);
    for (float zc : {-19.6f, -16.4f}) DryingRack(21.2f, 24.2f, zc);
    k.Prop("trockenwagen", Vec3(25.4f, 0, -13.4f), PI * 0.5f);
    k.Prop("trockenwagen", Vec3(28.4f, 0, -13.2f), PI * 0.5f);
    k.Prop("trockenwagen", Vec3(31.2f, 0, -13.4f), PI * 0.5f);
    k.Prop("trockenwagen", Vec3(34.0f, 0, -13.2f), PI * 0.5f);
    k.Prop("trockenwagen", Vec3(22.0f, 0, -8.6f), 0);
    k.Prop("trockenwagen", Vec3(22.4f, 0, -3.6f), 0);
    k.Prop("teigwanne", Vec3(34.6f, 0, -3.4f), 0.3f);
    k.Prop("eimer", Vec3(33.6f, 0, -3.0f), 0);
    k.Prop("klemmbrett", Vec3(35.2f, 0, -4.5f), 0.4f);
    k.Prop("warnschild", Vec3(24.0f, 0, -3.4f), 0.5f);
    // Wandlüfter an der Ostwand (drehen sich)
    MeshBuilder& b = k.ob;
    for (float z : {-15.0f, -8.4f}) {
      Vec3 c(BX1 - WT - 0.35f, 5.6f, z);
      k.Style(MAT_METAL, 0x6a7078);
      b.Push(Translation(c) * RotationZ(PI * 0.5f));
      b.Frustum(Vec3(0, -0.3f, 0), 1.0f, 1.0f, 0.3f, 24, false);
      b.Torus(Vec3(0, -0.3f, 0), 1.0f, 0.04f, 24, 5);
      b.Pop();
      k.Visual(c + Vec3(0.28f, -1.05f, -1.05f), c + Vec3(0.34f, 1.05f, 1.05f));
      AnimatedPart fan;
      fan.pivot = c;
      fan.axis = 0;
      fan.speed = 4.5f;
      fan.phase = z;
      MeshBuilder fb(&fan.mesh);
      fb.Mat(MAT_METAL);
      fb.ColorHex(0x9aa0a6);
      fb.CylinderX(c, 0.14f, 0.2f, 12);
      for (int i = 0; i < 5; i++) {
        float a = TAU * i / 5;
        fb.Push(Translation(c) * RotationX(a) * RotationY(0.35f));
        fb.Box(Vec3(-0.01f, 0.1f, -0.16f), Vec3(0.01f, 0.92f, 0.16f));
        fb.Pop();
      }
      w.anims.push_back(std::move(fan));
    }
    // Heizregister und warmes Licht
    k.Style(MAT_METAL, 0x8a6a4a);
    for (float x : {24.0f, 30.0f}) k.Visual(Vec3(x, 3.4f, BZ0 + WT + 0.02f), Vec3(x + 2.5f, 4.4f, BZ0 + WT + 0.2f));
    k.Style(MAT_PLASTIC, 0xff9a40);
    k.Glow(0.8f);
    for (float x : {24.0f, 30.0f}) k.Visual(Vec3(x + 0.1f, 3.5f, BZ0 + WT + 0.2f), Vec3(x + 2.4f, 4.3f, BZ0 + WT + 0.21f));
    k.Glow(0);
    for (float x : {23.0f, 29.0f, 34.0f})
      for (float z : {-18.0f, -12.0f, -5.0f}) k.CeilingLight(Vec3(x, WALL_H, z), 9.0f, Vec3(2.6f, 1.75f, 1.0f), 0.4f);
    k.Sign("TROCKENRAUM", Vec3(28.0f, 6.0f, -2.0f - IWH - 0.02f), PI, 0.05f, 0xf2f2ea, 0, true);
  }

  // ------------------------------------------------------------------------------------------
  // Mehllager: Hochregale mit Mehlpaletten, Gabelstapler, Rolltor
  void Rack(float xc, float z0, float z1) {
    int bays = std::max(1, (int)lroundf((z1 - z0) / 2.8f));
    float bay = (z1 - z0) / bays;
    for (int i = 0; i <= bays; i++) {
      float z = z0 + i * bay;
      k.Style(MAT_METAL, 0x2a5aa8);
      for (float s : {-1.0f, 1.0f}) k.Solid(Vec3(xc + s * 0.5f - 0.05f, 0, z - 0.05f), Vec3(xc + s * 0.5f + 0.05f, 6.4f, z + 0.05f));
      for (float y = 0.6f; y < 6.2f; y += 1.2f) Beam(k.ob, Vec3(xc - 0.45f, y, z), Vec3(xc + 0.45f, y + 0.9f, z), 0.015f);
    }
    for (float y : {1.9f, 3.7f, 5.5f}) {
      k.Style(MAT_METAL, 0xe86a1a);
      for (float s : {-1.0f, 1.0f}) k.Solid(Vec3(xc + s * 0.5f - 0.04f, y - 0.12f, z0), Vec3(xc + s * 0.5f + 0.04f, y, z1));
    }
    for (int i = 0; i < bays; i++) {
      float zm = z0 + (i + 0.5f) * bay;
      for (float lv : {0.0f, 1.9f, 3.7f, 5.5f})
        for (float off : {-0.65f, 0.65f}) {
          if (rng.Chance(0.18f)) continue;
          const char* id = lv > 0 && rng.Chance(0.25f) ? "kartonpalette" : "mehlpalette";
          if (lv == 5.5f && !strcmp(id, "kartonpalette")) id = "mehlpalette";
          k.Prop(id, Vec3(xc, lv, zm + off), 0);
        }
    }
  }

  void FlourStore() {
    for (float xc : {-31.0f, -26.5f}) {
      Rack(xc, -20.8f, -12.6f);
      Rack(xc, -10.4f, -2.0f);
    }
    k.Prop("gabelstapler", Vec3(-22.6f, 0, -7.5f), PI);
    k.Prop("hubwagen", Vec3(-34.0f, 0, -6.0f), 0.3f);
    k.Prop("sackkarre", Vec3(-34.6f, 0, -16.2f), PI * 0.5f);
    k.Prop("waage", Vec3(-34.4f, 0, -2.0f), PI * 0.5f);
    k.Prop("mehlpalette", Vec3(-34.0f, 0, -19.6f), 0.02f);
    k.Prop("mehlpalette", Vec3(-34.0f, 0, -1.0f + 0.0f), -0.03f);
    k.Prop("mehlsack", Vec3(-33.2f, 0, -8.0f), 0.6f);
    k.Prop("mehlsack", Vec3(-33.3f, 0.16f, -7.9f), 0.2f);
    k.Prop("mehlsack", Vec3(-32.6f, 0, -7.4f), -0.3f);
    k.Prop("ibc", Vec3(-22.6f, 0, -20.6f), 0);
    k.Prop("palette", Vec3(-22.5f, 0, -3.0f), 0);
    k.Prop("palette", Vec3(-22.5f, 0.144f, -3.0f), 0.04f);
    k.Prop("klemmbrett", Vec3(-34.4f, 0.08f, -2.0f), 0.2f);
    // Rohre von den Silos durch die Westwand
    MeshBuilder& b = k.ob;
    k.Style(MAT_METAL, SSTEEL);
    for (float z : {-18.0f, -6.0f}) {
      Pipe(b, Vec3(BX0 + WT, 7.6f, z), Vec3(-20.5f, 7.6f, z), 0.14f);
    }
    Pipe(b, Vec3(-20.5f, 7.6f, -18.0f), Vec3(-20.5f, 7.6f, -6.0f), 0.14f);
    for (float z = -20.0f; z <= -2.0f; z += 6.0f)
      for (float x : {-33.0f, -28.75f, -23.5f}) k.CeilingLight(Vec3(x, WALL_H, z), 9.0f, Vec3(2.2f, 2.15f, 2.0f), 0.45f);
    k.Sign("MEHLLAGER", Vec3(-28.0f, 6.6f, 4.0f - IWH - 0.02f), PI, 0.05f, 0xf2f2ea, 0, true);
  }

  // ------------------------------------------------------------------------------------------
  void Workshop() {
    k.Prop("werkbank", Vec3(-33.6f, 0, 4.7f), 0);
    k.Prop("werkbank", Vec3(-24.0f, 0, 4.7f), 0);
    k.Prop("werkbank", Vec3(-35.0f, 0, 11.0f), PI * 0.5f);
    k.Prop("werkzeugkiste", Vec3(-33.9f, 0.9f, 4.7f), 0.2f);
    k.Prop("werkzeugkiste", Vec3(-35.0f, 0.9f, 11.6f), PI * 0.5f);
    k.Prop("schutzhelm", Vec3(-23.6f, 0.9f, 4.6f), 1.2f);
    k.Prop("klemmbrett", Vec3(-24.5f, 0.9f, 4.8f), 0.1f);
    k.Prop("schaltschrank", Vec3(-20.6f, 0, 11.0f), -PI * 0.5f);
    k.Prop("schaltschrank", Vec3(-20.6f, 0, 12.0f), -PI * 0.5f);
    k.Prop("kabeltrommel", Vec3(-27.0f, 0, 9.0f), 0.4f);
    k.Prop("kabeltrommel", Vec3(-28.4f, 0, 19.8f), 1.3f);
    k.Prop("regal", Vec3(-33.0f, 0, 21.3f), PI);
    k.Prop("regal", Vec3(-30.4f, 0, 21.3f), PI);
    k.Prop("ibc", Vec3(-24.0f, 0, 20.6f), 0);
    k.Prop("fass_blau", Vec3(-22.0f, 0, 20.8f), 0);
    k.Prop("ventilator", Vec3(-30.0f, 0, 14.0f), 2.4f);
    k.Prop("eimer", Vec3(-26.0f, 0, 14.6f), 0);
    k.Prop("feuerloescher", Vec3(-35.4f, 0, 15.6f), PI * 0.5f);
    k.Prop("spind", Vec3(-35.3f, 0, 7.2f), PI * 0.5f);
    k.Prop("spind", Vec3(-35.3f, 0, 7.8f), PI * 0.5f);
    // Ausgebaute Pressschnecke auf Böcken
    MeshBuilder& b = k.ob;
    k.Style(MAT_METAL, 0x5a6470);
    for (float x : {-29.5f, -25.5f}) k.Solid(Vec3(x - 0.3f, 0, 16.4f), Vec3(x + 0.3f, 0.8f, 17.0f));
    k.Style(MAT_METAL, SSTEEL);
    b.CylinderX(Vec3(-27.5f, 1.0f, 16.7f), 0.12f, 5.0f, 12);
    for (float x = -29.8f; x < -25.2f; x += 0.25f) b.CylinderX(Vec3(x, 1.0f, 16.7f), 0.24f, 0.03f, 16);
    k.Clip(Vec3(-30.0f, 0.8f, 16.4f), Vec3(-25.0f, 1.25f, 17.0f), COL_SOLID);
    for (float z : {8.0f, 15.0f})
      for (float x : {-32.0f, -24.0f}) k.CeilingLight(Vec3(x, WALL_H, z), 9.0f, Vec3(2.3f, 2.2f, 2.0f), 0.45f);
    k.Sign("WERKSTATT", Vec3(-28.0f, 3.4f, 4.0f + IWH + 0.02f), 0, 0.04f, 0xf2f2ea, 0, true);
  }

  // ------------------------------------------------------------------------------------------
  // Verpackung: Waage, Kartonband, Kartonierer, Wickler, Kartonpaletten
  void Packaging() {
    MeshBuilder& b = k.ob;
    const float zc = 13.0f;
    // Mehrkopfwaage
    k.Style(MAT_METAL, SSTEEL);
    k.Solid(Vec3(-18.6f, 0, zc - 1.2f), Vec3(-16.2f, 2.2f, zc + 1.2f));
    b.Frustum(Vec3(-17.4f, 2.2f, zc), 1.0f, 0.4f, 0.9f, 16);
    b.Cylinder(Vec3(-17.4f, 3.1f, zc), 0.4f, 0.5f, 16);
    k.Clip(Vec3(-18.6f, 2.2f, zc - 1.2f), Vec3(-16.2f, 3.6f, zc + 1.2f), COL_SOLID);
    k.Style(MAT_SCREEN, 0x4ad08a);
    k.Visual(Vec3(-17.9f, 1.2f, zc + 1.2f), Vec3(-16.9f, 1.7f, zc + 1.215f));
    // Kartonband
    const float x0 = -16.2f, x1 = -3.6f;
    k.Style(MAT_METAL, STEEL_L);
    k.Solid(Vec3(x0, 0.55f, zc - 0.45f), Vec3(x1, 0.8f, zc + 0.45f));
    k.Style(MAT_METAL, 0x9aa0a6);
    for (float x = x0 + 0.15f; x < x1; x += 0.15f) b.CylinderZ(Vec3(x, 0.83f, zc), 0.03f, 0.8f, 8);  // Rollen
    k.Clip(Vec3(x0, 0.8f, zc - 0.4f), Vec3(x1, 0.86f, zc + 0.4f), COL_SOLID);
    k.Style(MAT_METAL, STEEL_L);
    for (float x = x0 + 0.4f; x < x1; x += 1.6f)
      for (float s : {-1.0f, 1.0f}) k.Visual(Vec3(x - 0.04f, 0, zc + s * 0.4f - 0.04f), Vec3(x + 0.04f, 0.55f, zc + s * 0.4f + 0.04f));
    AnimatedPart boxes;
    boxes.slideDir = Vec3(1, 0, 0);
    boxes.slideLen = 1.4f;
    boxes.speed = 0.3f;
    {
      MeshBuilder bb(&boxes.mesh);
      for (float x = x0 + 0.4f; x < x1 - 1.5f; x += 1.4f) {
        bb.Mat(MAT_PAPER);
        bb.ColorHex(0xc49a62);
        bb.Box(Vec3(x - 0.25f, 0.86f, zc - 0.18f), Vec3(x + 0.25f, 1.2f, zc + 0.18f));
        bb.ColorHex(0xd8b884);
        bb.Box(Vec3(x - 0.25f, 1.2f, zc - 0.03f), Vec3(x + 0.25f, 1.203f, zc + 0.03f));
        bb.Mat(MAT_PLASTIC);
        bb.ColorHex(0xb02a20);
        bb.Push(Translation(Vec3(x - PixelTextWidth("PASTA", 0.016f) * 0.5f, 0.98f, zc + 0.18f)));
        BuildPixelText(bb, "PASTA", 0.016f, 0.002f);
        bb.Pop();
      }
    }
    w.anims.push_back(std::move(boxes));
    // Kartonierer mit Signalsäule
    k.Style(MAT_METAL, 0xd8dcd8);
    k.Solid(Vec3(-3.6f, 0, zc - 1.4f), Vec3(-0.4f, 2.2f, zc + 1.4f));
    k.Style(MAT_METAL, 0x2a2d31);
    k.Visual(Vec3(-3.4f, 0.9f, zc + 1.4f), Vec3(-0.6f, 2.0f, zc + 1.42f));
    k.Glass(Vec3(-3.3f, 1.0f, zc + 1.42f), Vec3(-0.7f, 1.9f, zc + 1.44f), 0x9ab0c0, 0.35f, false);
    for (int i = 0; i < 3; i++) {
      const u32 cols[3] = {0x30d050, 0xffc020, 0xff3020};
      k.Style(MAT_PLASTIC, cols[i]);
      k.Glow(i == 0 ? 1.0f : 0.15f);
      b.Cylinder(Vec3(-0.8f, 2.2f + i * 0.12f, zc - 1.1f), 0.06f, 0.11f, 12);
    }
    k.Glow(0);
    k.Sign("KARTONIERER", Vec3(-2.0f, 0.55f, zc + 1.43f), 0, 0.03f, 0x1a1a1a, 0, false);
    // Stretchwickler
    k.Style(MAT_METAL, 0x3a4450);
    k.Solid(Vec3(6.8f, 0, 12.2f), Vec3(9.2f, 0.12f, 14.6f));
    k.Style(MAT_METAL, YELLOW);
    k.Solid(Vec3(9.6f, 0, 13.1f), Vec3(10.0f, 2.8f, 13.7f));
    k.Style(MAT_PLASTIC, 0xe8eef2);
    b.Cylinder(Vec3(9.4f, 1.0f, 13.4f), 0.12f, 0.5f, 14);
    k.Prop("kartonpalette", Vec3(8.0f, 0.12f, 13.4f), 0.2f);
    // Versandbereich
    for (float x : {-17.0f, -14.8f, -5.8f, -3.4f, 6.0f, 8.4f, 14.0f, 16.4f})
      for (float z : {17.2f, 19.4f}) {
        if (rng.Chance(0.2f) || (x > 15 && z > 19)) continue;
        k.Prop("kartonpalette", Vec3(x, 0, z), PI * 0.5f + rng.Range(-0.05f, 0.05f));
      }
    for (int i = 0; i < 7; i++) k.Prop("palette", Vec3(18.6f, i * 0.144f, 11.2f), rng.Range(-0.04f, 0.04f));
    for (int i = 0; i < 5; i++) k.Prop("palette", Vec3(18.6f, i * 0.144f, 12.6f), rng.Range(-0.04f, 0.04f));
    k.Prop("gabelstapler", Vec3(12.6f, 0, 15.4f), -PI * 0.5f);
    k.Prop("hubwagen", Vec3(-7.2f, 0, 17.0f), 0.2f);
    k.Prop("nudelkarton", Vec3(-1.2f, 0, 17.4f), 0.2f);
    k.Prop("nudelkarton", Vec3(-1.0f, 0.4f, 17.5f), -0.1f);
    k.Prop("nudelkarton", Vec3(-0.6f, 0, 18.0f), 0.5f);
    k.Prop("farfalle", Vec3(-1.0f, 0.8f, 17.5f), 0.2f);
    k.Prop("warnschild", Vec3(-6.2f, 0, 20.4f), 0.0f);
    // Versandbüro-Ecke
    k.Prop("schreibtisch", Vec3(16.6f, 0, 20.9f), PI);
    k.Prop("buerostuhl", Vec3(16.6f, 0, 20.0f), 0);
    k.Prop("monitor", Vec3(16.4f, 0.76f, 21.2f), PI);
    k.Prop("klemmbrett", Vec3(17.2f, 0.76f, 20.8f), 0.4f);
    k.Prop("nudelpackung", Vec3(15.8f, 0.76f, 20.9f), 0.2f);
    k.Prop("aktenschrank", Vec3(19.3f, 0, 20.4f), -PI * 0.5f);
    for (float x : {-15.0f, -5.0f, 5.0f, 15.0f})
      for (float z : {13.0f, 18.5f}) k.CeilingLight(Vec3(x, WALL_H, z), 10.0f, Vec3(2.3f, 2.2f, 2.0f), 0.5f);
    k.Sign("VERSAND", Vec3(-10.0f, 5.0f, BZ1 - WT - 0.02f), PI, 0.06f, 0xf2f2ea, 0, true);
  }

  // ------------------------------------------------------------------------------------------
  // Lkw an Rampe 1: offener Anhänger (Ladefläche 1,2 m), Stufen vom Tor hinauf
  void Truck() {
    MeshBuilder& b = k.ob;
    const float tx0 = -11.3f, tx1 = -8.7f, tz0 = 22.6f, tz1 = 36.0f, bed = 1.2f, top = 4.0f;
    k.Style(MAT_METAL, 0x6a7078);
    k.Stairs(Vec3(-10.0f, 0, 21.1f), 2, 2.0f, 0.2f, 6, 0.25f, 0);
    // Ladefläche, Wände, Dach
    k.Style(MAT_WOOD, 0x6b5a48);
    k.Solid(Vec3(tx0, bed - 0.2f, tz0), Vec3(tx1, bed, tz1));
    k.Style(MAT_PLASTIC, 0xeef0f2);
    k.Solid(Vec3(tx0, bed, tz0), Vec3(tx0 + 0.08f, top, tz1));
    k.Solid(Vec3(tx1 - 0.08f, bed, tz0), Vec3(tx1, top, tz1));
    k.Solid(Vec3(tx0 + 0.08f, bed, tz1 - 0.08f), Vec3(tx1 - 0.08f, top, tz1));
    k.Solid(Vec3(tx0, top, tz0), Vec3(tx1, top + 0.08f, tz1));
    // Aufschrift und Streifen außen
    k.Style(MAT_PLASTIC, 0xc8281e);
    k.Visual(Vec3(tx1, bed + 0.2f, tz0 + 0.3f), Vec3(tx1 + 0.012f, bed + 0.5f, tz1 - 0.3f));
    k.Visual(Vec3(tx0 - 0.012f, bed + 0.2f, tz0 + 0.3f), Vec3(tx0, bed + 0.5f, tz1 - 0.3f));
    k.Sign("NUDELN", Vec3(tx1 + 0.02f, 2.7f, 29.3f), PI * 0.5f, 0.14f, 0xc8281e, 0, false);
    k.Sign("NUDELN", Vec3(tx0 - 0.02f, 2.7f, 29.3f), -PI * 0.5f, 0.14f, 0xc8281e, 0, false);
    // Hecktüren aufgeklappt an den Seiten
    k.Style(MAT_PLASTIC, 0xdfe2e4);
    k.Visual(Vec3(tx0 - 0.07f, bed + 0.05f, tz0), Vec3(tx0 - 0.03f, top - 0.05f, tz0 + 1.25f));
    k.Visual(Vec3(tx1 + 0.03f, bed + 0.05f, tz0), Vec3(tx1 + 0.07f, top - 0.05f, tz0 + 1.25f));
    // Fahrgestell und Räder
    k.Style(MAT_METAL, 0x24262a);
    k.Visual(Vec3(-10.6f, 0.75f, tz0 + 0.3f), Vec3(-9.4f, bed - 0.2f, tz1));
    k.Style(MAT_PLASTIC, 0x151516);
    for (float z : {24.6f, 26.0f, 27.4f})
      for (float x : {tx0 + 0.2f, tx1 - 0.2f}) {
        b.CylinderX(Vec3(x, 0.5f, z), 0.5f, 0.38f, 18);
        k.Clip(Vec3(x - 0.2f, 0, z - 0.45f), Vec3(x + 0.2f, 0.95f, z + 0.45f), COL_SOLID);
      }
    k.Clip(Vec3(-10.6f, 0.75f, tz0 + 0.3f), Vec3(-9.4f, bed - 0.2f, tz1), COL_SOLID);
    // Zugmaschine
    const float cz0 = 37.6f, cz1 = 40.6f;
    k.Style(MAT_METAL, 0x24262a);
    k.Solid(Vec3(-10.6f, 0.5f, 34.0f), Vec3(-9.4f, 1.1f, cz1));
    k.Style(MAT_METAL, 0xc8281e);
    k.Solid(Vec3(tx0 + 0.05f, 1.1f, cz0), Vec3(tx1 - 0.05f, 3.6f, cz1));
    k.Style(MAT_METAL, 0x2a2c30);
    k.Visual(Vec3(tx0 + 0.15f, 1.1f, cz1), Vec3(tx1 - 0.15f, 1.6f, cz1 + 0.12f));  // Kühlergrill
    k.Glass(Vec3(tx0 + 0.25f, 2.3f, cz1), Vec3(tx1 - 0.25f, 3.3f, cz1 + 0.03f), 0x7a8a96, 0.6f, false);
    for (float x : {tx0 + 0.05f, tx1 - 0.05f}) k.Glass(Vec3(x - 0.02f, 2.3f, cz0 + 1.5f), Vec3(x + 0.02f, 3.2f, cz1 - 0.3f), 0x7a8a96, 0.6f, false);
    k.Style(MAT_PLASTIC, 0xfff4d0);
    k.Glow(0.6f);
    for (float x : {tx0 + 0.4f, tx1 - 0.4f}) k.Visual(Vec3(x - 0.18f, 1.25f, cz1), Vec3(x + 0.18f, 1.45f, cz1 + 0.02f));
    k.Glow(0);
    k.Style(MAT_PLASTIC, 0x151516);
    for (float z : {35.2f, 36.6f, 39.6f})
      for (float x : {tx0 + 0.2f, tx1 - 0.2f}) {
        b.CylinderX(Vec3(x, 0.5f, z), 0.5f, 0.38f, 18);
        k.Clip(Vec3(x - 0.2f, 0, z - 0.45f), Vec3(x + 0.2f, 0.95f, z + 0.45f), COL_SOLID);
      }
    // Ladung
    k.Prop("kartonpalette", Vec3(-10.0f, bed, 33.8f), PI * 0.5f);
    k.Prop("kartonpalette", Vec3(-10.0f, bed, 32.2f), PI * 0.5f);
    k.Prop("kartonpalette", Vec3(-10.0f, bed, 30.6f), PI * 0.5f);
    k.Prop("nudelkarton", Vec3(-10.6f, bed, 28.4f), 0.1f);
    k.Prop("nudelkarton", Vec3(-10.6f, bed + 0.4f, 28.4f), -0.05f);
    k.Prop("nudelkarton", Vec3(-9.4f, bed, 27.8f), 1.6f);
    k.Prop("palette", Vec3(-10.0f, bed, 25.4f), PI * 0.5f);
    k.Light(Vec3(-10.0f, 3.6f, 30.0f), 7, Vec3(1.6f, 1.55f, 1.4f));
    // Rampe 2: Puffer, Ladebrücke, Paletten draußen
    k.Style(MAT_PLASTIC, 0x1a1a1a);
    for (float x : {-12.6f, -7.4f, -0.6f, 4.6f}) k.Solid(Vec3(x - 0.2f, 0.6f, BZ1), Vec3(x + 0.2f, 1.1f, BZ1 + 0.15f));
    k.Prop("palette", Vec3(2.0f, 0, 24.6f), 0.1f);
    k.Prop("palette", Vec3(2.0f, 0.144f, 24.6f), -0.06f);
    k.Prop("kartonpalette", Vec3(4.6f, 0, 25.4f), 0.04f);
    k.Prop("hubwagen", Vec3(0.6f, 0, 25.8f), 2.8f);
  }

  // ------------------------------------------------------------------------------------------
  // Bürotrakt: EG Kantine und Umkleide, Treppe, OG Meisterbüro, Labor, Archiv
  void OfficeWing() {
    const float x0 = IX + IWH, x1 = BX1 - WT;
    // Geschossdecke mit Treppenloch (x 27..32.6, z 20.3..)
    const float sx0 = 27.0f, sx1 = 32.6f, sz0 = 20.3f, s0 = BZ1 - WT;
    k.Slab(x0, -2.0f + IWH, 28.0f, HZ1, OG, OG - OGC, MAT_CARPET, 0x4a5a6e, 0xe2e0d8);
    k.Slab(28.0f, -2.0f + IWH, x1, HZ1, OG, OG - OGC, MAT_TILE, 0xdfe4e6, 0xe2e0d8);
    k.Slab(x0, HZ1, x1, sz0, OG, OG - OGC, MAT_PARQUET, 0x9a7048, 0xdedad0);
    k.Slab(x0, sz0, sx0, s0, OG, OG - OGC, MAT_PARQUET, 0x9a7048, 0xdedad0);
    k.Slab(sx1, sz0, x1, s0, OG, OG - OGC, MAT_PARQUET, 0x9a7048, 0xdedad0);
    k.Style(MAT_WOOD, 0x6a4a30);
    k.Stairs(Vec3(sx0, 0, (sz0 + s0) * 0.5f), 0, s0 - sz0 - 0.05f, 0.2f, 20, 0.28f, 0);
    k.Railing(Vec3(sx0, OG, sz0), Vec3(sx1 - 0.6f, OG, sz0), 1.0f, false);
    k.Railing(Vec3(sx0, OG, sz0), Vec3(sx0, OG, s0 - 0.08f), 1.0f, false);
    // EG Kantine
    for (float x : {23.0f, 27.0f, 31.0f})
      for (float z : {1.0f, 4.6f}) {
        k.Prop("kantinentisch", Vec3(x, 0, z), 0);
        k.Prop("kantinenstuhl", Vec3(x - 0.35f, 0, z + 0.65f), PI);
        k.Prop("kantinenstuhl", Vec3(x + 0.35f, 0, z + 0.65f), PI);
        k.Prop("kantinenstuhl", Vec3(x - 0.35f, 0, z - 0.65f), 0);
        k.Prop("kantinenstuhl", Vec3(x + 0.35f, 0, z - 0.65f), 0);
        if (rng.Chance(0.7f)) k.Prop("nudelteller", Vec3(x - 0.3f, 0.76f, z + 0.2f), rng.Range(-PI, PI));
        if (rng.Chance(0.5f)) k.Prop("nudelteller", Vec3(x + 0.3f, 0.76f, z - 0.2f), rng.Range(-PI, PI));
        if (rng.Chance(0.4f)) k.Prop("kaffeebecher", Vec3(x, 0.76f, z), 0);
      }
    // Ausgabetheke an der Ostwand
    k.Style(MAT_METAL, SSTEEL);
    k.Solid(Vec3(34.2f, 0, 0.5f), Vec3(x1 - 0.02f, 0.95f, 7.5f));
    k.Prop("kochtopf", Vec3(34.9f, 0.95f, 2.0f), 0);
    k.Prop("kochtopf", Vec3(34.9f, 0.95f, 3.2f), 0.4f);
    k.Prop("sieb", Vec3(34.9f, 0.95f, 4.2f), 0.2f);
    k.Prop("parmesan", Vec3(34.9f, 0.95f, 5.2f), 0);
    k.Prop("tomatendose", Vec3(34.7f, 0.95f, 6.0f), 0);
    k.Prop("tomatendose", Vec3(34.9f, 0.95f, 6.2f), 0);
    k.Prop("olivenoel", Vec3(35.0f, 0.95f, 6.8f), -PI * 0.5f);
    k.Prop("kaffeemaschine", Vec3(34.9f, 0.95f, 1.0f), -PI * 0.5f);
    k.Prop("automat", Vec3(21.0f, 0, -1.3f), 0);
    k.Prop("wasserspender", Vec3(22.4f, 0, -1.4f), 0);
    k.Prop("mikrowelle", Vec3(34.9f, 0.95f, 7.1f), -PI * 0.5f);
    k.Prop("muelltonne", Vec3(33.4f, 0, 9.3f), PI);
    for (float x : {23.0f, 27.0f, 31.0f})
      for (float z : {1.0f, 5.5f}) k.CeilingLight(Vec3(x, OGC, z), 8.0f, Vec3(2.2f, 2.1f, 1.9f), 0.4f);
    k.Sign("KANTINE", Vec3(IX + IWH + 0.02f, 2.6f, 8.3f), PI * 0.5f, 0.03f, 0xf2f2ea, 0, true);
    // EG Umkleide
    for (float x = 27.4f; x < 34.6f; x += 0.62f) k.Prop("spind", Vec3(x, 0, HZ1 + IWH + 0.27f), 0);
    k.Prop("sitzbank", Vec3(30.0f, 0, 12.0f), 0);
    k.Prop("stechuhr", Vec3(34.9f, 0, 12.6f), -PI * 0.5f);
    k.Prop("schutzhelm", Vec3(29.4f, 0.47f, 12.0f), 0.3f);
    k.Prop("feuerloescher", Vec3(21.0f, 0, 19.0f), PI * 0.5f);
    k.Prop("pflanze", Vec3(34.9f, 0, 17.0f), 0);
    for (float x : {23.5f, 30.0f})
      for (float z : {13.0f, 18.0f}) k.CeilingLight(Vec3(x, OGC, z), 8.0f, Vec3(2.2f, 2.15f, 2.0f), 0.4f);
    // Eingangsvordach außen
    k.Style(MAT_METAL, 0x3a3e44);
    k.Solid(Vec3(BX1, 2.9f, 13.4f), Vec3(BX1 + 1.6f, 3.05f, 16.4f));
    for (float z : {13.5f, 16.3f}) k.Solid(Vec3(BX1 + 1.45f, 0, z - 0.05f), Vec3(BX1 + 1.55f, 2.9f, z + 0.05f));
    k.Sign("NUDELFABRIK", Vec3(BX1 + 0.02f, 3.5f, 14.9f), PI * 0.5f, 0.05f, 0xf2f2ea, 0.2f, true);
    // OG Meisterbüro
    k.Prop("schreibtisch", Vec3(23.0f, OG, 2.5f), PI * 0.5f);
    k.Prop("buerostuhl", Vec3(23.9f, OG, 2.5f), -PI * 0.5f);
    k.Prop("monitor", Vec3(22.8f, OG + 0.76f, 2.3f), PI * 0.5f);
    k.Prop("pc", Vec3(22.6f, OG, 3.2f), PI * 0.5f);
    k.Prop("telefon", Vec3(22.9f, OG + 0.76f, 3.0f), PI * 0.5f);
    k.Prop("klemmbrett", Vec3(23.2f, OG + 0.76f, 1.9f), 0.3f);
    k.Prop("aktenschrank", Vec3(27.4f, OG, 0.0f), -PI * 0.5f);
    k.Prop("aktenschrank", Vec3(27.4f, OG, 1.0f), -PI * 0.5f);
    k.Prop("kaffeebecher", Vec3(23.3f, OG + 0.76f, 2.8f), 0);
    k.Prop("pflanze", Vec3(27.3f, OG, 9.3f), 0);
    k.Prop("sessel", Vec3(25.6f, OG, 8.6f), PI);
    // OG Labor
    k.Prop("werkbank", Vec3(32.0f, OG, -1.2f), 0);
    k.Prop("werkbank", Vec3(35.0f, OG, 3.0f), -PI * 0.5f);
    k.Prop("mikroskop", Vec3(31.6f, OG + 0.9f, -1.2f), 0);
    k.Prop("waage", Vec3(30.0f, OG, 8.6f), 0);
    k.Prop("nudelpackung", Vec3(32.6f, OG + 0.9f, -1.1f), 0.2f);
    k.Prop("farfalle", Vec3(35.0f, OG + 0.9f, 2.4f), 0.4f);
    k.Prop("teigwanne", Vec3(33.4f, OG, 6.0f), 0.3f);
    k.Prop("klemmbrett", Vec3(35.0f, OG + 0.9f, 3.6f), 0.1f);
    k.Prop("schutzhelm", Vec3(34.9f, OG + 0.9f, 4.0f), 0.8f);
    // OG Archiv / Besprechung
    k.Prop("kantinentisch", Vec3(30.0f, OG, 15.0f), 0);
    k.Prop("kantinentisch", Vec3(31.2f, OG, 15.0f), 0);
    for (float x : {29.6f, 30.6f, 31.6f}) {
      k.Prop("kantinenstuhl", Vec3(x, OG, 15.7f), PI);
      k.Prop("kantinenstuhl", Vec3(x, OG, 14.3f), 0);
    }
    for (float x = 21.0f; x < 26.0f; x += 1.0f) k.Prop("aktenschrank", Vec3(x, OG, HZ1 + IWH + 0.25f), 0);
    k.Prop("regal", Vec3(35.2f, OG, 12.5f), -PI * 0.5f);
    k.Prop("regal", Vec3(35.2f, OG, 14.0f), -PI * 0.5f);
    k.Prop("kopierer", Vec3(21.0f, OG, 18.5f), PI * 0.5f);
    k.Prop("nudelkarton", Vec3(34.6f, OG, 19.0f), 0.2f);
    for (float x : {23.5f, 31.5f})
      for (float z : {2.0f, 7.5f, 14.0f, 18.5f}) k.CeilingLight(Vec3(x, WALL_H, z), 8.0f, Vec3(2.2f, 2.15f, 2.0f), 0.4f);
    k.Sign("MEISTER", Vec3(25.0f, OG + 2.6f, HZ1 + IWH + 0.02f), 0, 0.025f, 0xf2f2ea, 0, true);
    k.Sign("LABOR", Vec3(28.0f + IWH + 0.02f, OG + 2.6f, 2.0f), PI * 0.5f, 0.025f, 0xf2f2ea, 0, true);
  }

  // ------------------------------------------------------------------------------------------
  void Outside() {
    MeshBuilder& b = k.ob;
    // Dachschriftzug über der Verpackung
    k.Style(MAT_METAL, 0x3a3e44);
    const float ry = WALL_H + ROOF_T;
    for (float x = -17.0f; x <= 17.1f; x += 4.25f) {
      k.Solid(Vec3(x - 0.06f, ry, 19.4f), Vec3(x + 0.06f, ry + 3.4f, 19.5f));
      Beam(b, Vec3(x, ry, 17.6f), Vec3(x, ry + 3.2f, 19.4f), 0.04f);
    }
    k.Visual(Vec3(-17.5f, ry + 0.4f, 19.5f), Vec3(17.5f, ry + 0.48f, 19.56f));
    k.Visual(Vec3(-17.5f, ry + 3.0f, 19.5f), Vec3(17.5f, ry + 3.08f, 19.56f));
    k.Sign("NUDELFABRIK", Vec3(0, ry + 1.75f, 19.6f), 0, 0.27f, 0xd8281e, 0.25f, false);
    // Schornstein im Hinterhof
    k.Style(MAT_BRICK, BRICK_D);
    k.Solid(Vec3(-30.0f, 0, -33.0f), Vec3(-26.0f, 2.2f, -29.0f));
    b.Mat(MAT_BRICK);
    b.ColorHex(BRICK);
    b.Frustum(Vec3(-28.0f, 2.2f, -31.0f), 1.5f, 1.0f, 28.0f, 20);
    b.ColorHex(0x2a2220);
    b.Cylinder(Vec3(-28.0f, 30.2f, -31.0f), 1.08f, 0.8f, 20);
    b.ColorHex(BRICK_D);
    for (float y : {12.0f, 22.0f}) {
      float r = 1.5f - 0.5f * (y - 2.2f) / 28.0f;
      b.Cylinder(Vec3(-28.0f, y, -31.0f), r + 0.06f, 0.4f, 20);
    }
    // Hinterhof: Container, Gastank, Paletten
    for (float x : {-8.0f, -4.5f}) {
      k.Style(MAT_METAL, 0x2f6a3a);
      k.Solid(Vec3(x, 0.15f, -26.0f), Vec3(x + 2.8f, 1.6f, -24.2f));
      k.Style(MAT_METAL, 0x24542e);
      k.Visual(Vec3(x - 0.05f, 1.6f, -26.05f), Vec3(x + 2.85f, 1.68f, -24.15f));
      k.Style(MAT_METAL, 0x1a1a1a);
      k.Solid(Vec3(x + 0.2f, 0, -25.9f), Vec3(x + 2.6f, 0.15f, -24.3f));
    }
    k.Style(MAT_METAL, 0xe8eaec);
    b.CylinderX(Vec3(14.0f, 1.6f, -30.0f), 1.2f, 6.0f, 24);
    b.Sphere(Vec3(11.0f, 1.6f, -30.0f), 1.2f, 18, 10);
    b.Sphere(Vec3(17.0f, 1.6f, -30.0f), 1.2f, 18, 10);
    k.Style(MAT_METAL, 0x5a5f66);
    for (float x : {12.0f, 16.0f}) k.Solid(Vec3(x - 0.2f, 0, -31.0f), Vec3(x + 0.2f, 0.6f, -29.0f));
    k.Clip(Vec3(9.8f, 0.6f, -31.2f), Vec3(18.2f, 2.8f, -28.8f), COL_SOLID);
    k.Sign("STICKSTOFF", Vec3(14.0f, 1.6f, -28.78f), 0, 0.05f, 0x1a5ab0, 0, false);
    k.Prop("palette", Vec3(-14.0f, 0, -24.0f), 0.1f);
    k.Prop("palette", Vec3(-14.0f, 0.144f, -24.0f), -0.05f);
    k.Prop("palette", Vec3(-14.0f, 0.288f, -24.0f), 0.08f);
    k.Prop("palette", Vec3(-12.4f, 0, -24.2f), 0.02f);
    k.Prop("muelltonne", Vec3(2.4f, 0, -23.6f), 0);
    k.Prop("muelltonne", Vec3(3.3f, 0, -23.6f), 0.1f);
    k.Prop("fass_blau", Vec3(24.0f, 0, -24.0f), 0);
    k.Prop("fass_blau", Vec3(24.7f, 0, -24.2f), 0);
    k.Prop("ibc", Vec3(22.0f, 0, -24.4f), 0.04f);
    k.Prop("fahrrad", Vec3(-20.0f, 0, -23.0f), 0.3f);
    // Werkshof: Laternen, Poller, Paletten
    for (float x : {-30.0f, -15.0f, 15.0f, 30.0f}) k.Prop("laterne", Vec3(x, 0, 30.0f), 0);
    for (float z : {-10.0f, 10.0f}) k.Prop("laterne", Vec3(48.0f, 0, z), 0);
    for (float x : {-40.0f, 40.0f}) k.Prop("laterne", Vec3(x, 0, -30.0f), 0);
    k.Prop("laterne", Vec3(-48.0f, 0, 12.0f), 0);
    for (float x : {-14.0f, -6.0f, 6.0f}) k.Prop("poller", Vec3(x, 0, 23.2f), 0);
    k.Prop("kartonpalette", Vec3(16.0f, 0, 26.0f), 0.1f);
    k.Prop("palette", Vec3(18.0f, 0, 26.2f), 0.05f);
    k.Prop("huetchen", Vec3(-4.0f, 0, 30.0f), 0);
    k.Prop("huetchen", Vec3(-3.0f, 0, 31.5f), 0);
    k.Prop("muelltonne", Vec3(22.0f, 0, 23.0f), PI);
    // Parkplatz
    for (float z = -16.5f; z <= 16.6f; z += 3.0f) {
      if (rng.Chance(0.35f)) continue;
      k.Prop(rng.Chance(0.5f) ? "auto_grau" : "auto_blau", Vec3(43.0f, 0, z), rng.Chance(0.5f) ? 0 : PI);
    }
    for (float z = -16.5f; z <= 16.6f; z += 3.0f) {
      if (rng.Chance(0.4f)) continue;
      k.Prop(rng.Chance(0.5f) ? "auto_grau" : "auto_blau", Vec3(53.0f, 0, z), rng.Chance(0.5f) ? 0 : PI);
    }
    k.Prop("fahrrad", Vec3(37.0f, 0, 18.0f), PI * 0.5f);
    k.Prop("fahrrad", Vec3(37.0f, 0, 18.8f), PI * 0.5f);
    // Pförtnerhaus am Tor
    {
      const float gx0 = 50.0f, gx1 = 55.0f, gz0 = 48.0f, gz1 = 53.0f, gh = 3.2f;
      k.Style(MAT_BRICK, BRICK);
      k.WallX(gx0, gx1, gz0, gz0 + 0.25f, 0, gh, {});
      k.WallX(gx0, gx1, gz1 - 0.25f, gz1, 0, gh, {{51.0f, 53.0f, 1.0f, 2.4f, true}});
      k.WallZ(gz0 + 0.25f, gz1 - 0.25f, gx0, gx0 + 0.25f, 0, gh, {{49.4f, 50.4f, 0, 2.3f}, {51.0f, 52.4f, 1.0f, 2.4f, true}});
      k.WallZ(gz0 + 0.25f, gz1 - 0.25f, gx1 - 0.25f, gx1, 0, gh, {});
      k.Glass(Vec3(51.0f, 1.0f, gz1 - 0.135f), Vec3(53.0f, 2.4f, gz1 - 0.115f), 0xb4c4cc, 0.3f, true);
      k.Glass(Vec3(gx0 + 0.115f, 1.0f, 51.0f), Vec3(gx0 + 0.135f, 2.4f, 52.4f), 0xb4c4cc, 0.3f, true);
      k.Slab(gx0 - 0.3f, gz0 - 0.3f, gx1 + 0.3f, gz1 + 0.3f, gh + 0.25f, 0.25f, MAT_CONCRETE, 0x5a5c5e, 0xe2e0d8);
      k.FloorFinish(gx0 + 0.25f, gz0 + 0.25f, gx1 - 0.25f, gz1 - 0.25f, 0.012f, MAT_TILE, 0xb8b4ac);
      DoorFrame(false, gx0, gx0 + 0.25f, 49.4f, 50.4f, 2.3f);
      k.Prop("schreibtisch", Vec3(52.5f, 0, 52.2f), PI);
      k.Prop("buerostuhl", Vec3(52.5f, 0, 51.3f), 0);
      k.Prop("monitor", Vec3(52.2f, 0.76f, 52.5f), PI);
      k.Prop("telefon", Vec3(53.1f, 0.76f, 52.4f), PI);
      k.Prop("kaffeebecher", Vec3(51.9f, 0.76f, 52.3f), 0);
      k.CeilingLight(Vec3(52.5f, gh, 50.5f), 6.0f, Vec3(2.2f, 2.1f, 1.9f), 0.3f);
      k.Sign("PFORTE", Vec3(52.5f, 2.8f, gz1 + 0.02f), 0, 0.04f, 0xf2f2ea, 0, true);
    }
    // Schranke und geschlossenes Schiebetor
    k.Style(MAT_METAL, 0xe8e8e2);
    k.Solid(Vec3(48.6f, 0, 54.6f), Vec3(49.0f, 1.0f, 55.0f));
    k.Style(MAT_PLASTIC, 0xd8281e);
    for (int i = 0; i < 8; i++) {
      k.Style(MAT_PLASTIC, i % 2 ? 0xf2f2ea : 0xd8281e);
      k.Visual(Vec3(40.0f + i * 1.1f, 0.92f, 54.75f), Vec3(41.1f + i * 1.1f, 1.02f, 54.85f));
    }
    k.Clip(Vec3(40.0f, 0, 54.6f), Vec3(48.6f, 1.0f, 55.0f), COL_SOLID);
  }

  // ------------------------------------------------------------------------------------------
  // Mehlsilos im Westen: zwischen den Beinen begehbar
  void Silos() {
    MeshBuilder& b = k.ob;
    const float sx = -47.0f;
    for (int i = 0; i < 3; i++) {
      float z = -14.0f + i * 8.0f;
      Vec3 c(sx, 0, z);
      k.Style(MAT_METAL, 0x6a7078);
      for (float a : {-1.0f, 1.0f})
        for (float d : {-1.0f, 1.0f}) k.Solid(c + Vec3(a * 1.4f - 0.12f, 0, d * 1.4f - 0.12f), c + Vec3(a * 1.4f + 0.12f, 3.4f, d * 1.4f + 0.12f));
      k.Style(MAT_METAL, 0xd0d4d8);
      b.Frustum(c + Vec3(0, 3.2f, 0), 0.35f, 2.0f, 1.8f, 24);
      k.Style(MAT_CORRUGATED, 0xd8dce0);
      b.Cylinder(c + Vec3(0, 5.0f, 0), 2.0f, 10.0f, 28);
      k.Style(MAT_METAL, 0xb8bcc2);
      b.Frustum(c + Vec3(0, 15.0f, 0), 2.05f, 0.4f, 1.0f, 28);
      for (float y : {5.0f, 10.0f, 15.0f}) {
        b.Push(Translation(c + Vec3(0, y, 0)));
        b.Torus(Vec3(0), 2.02f, 0.05f, 28, 5);
        b.Pop();
      }
      // Leiter mit Rückenschutz
      k.Style(MAT_METAL, YELLOW);
      for (float s : {-0.25f, 0.25f}) k.Visual(c + Vec3(2.1f, 3.4f, s - 0.03f), c + Vec3(2.16f, 16.0f, s + 0.03f));
      for (float y = 3.6f; y < 16.0f; y += 0.3f) k.Visual(c + Vec3(2.1f, y, -0.25f), c + Vec3(2.16f, y + 0.03f, 0.25f));
      for (float y = 6.0f; y < 16.0f; y += 1.2f) {
        b.Push(Translation(c + Vec3(2.45f, y, 0)));
        b.Torus(Vec3(0), 0.38f, 0.02f, 14, 4);
        b.Pop();
      }
      k.Clip(c + Vec3(-2.0f, 3.4f, -2.0f), c + Vec3(2.2f, 16.0f, 2.0f), COL_SOLID);
      k.Sign(i == 0 ? "MEHL 1" : i == 1 ? "MEHL 2" : "MEHL 3", c + Vec3(0, 11.0f, 2.01f), 0, 0.09f, 0x1d3a6a, 0, false);
      // Rohr zum Gebäude
      k.Style(MAT_METAL, SSTEEL);
      Pipe(b, c + Vec3(0, 16.0f, 0), c + Vec3(0, 17.0f, 0), 0.14f);
      Pipe(b, c + Vec3(0, 17.0f, 0), Vec3(BX0 - 0.5f, 10.5f, i == 1 ? -6.0f : -18.0f + i * 4.0f), 0.14f);
    }
    k.Style(MAT_METAL, SSTEEL);
    Pipe(b, Vec3(BX0 - 0.5f, 10.5f, -18.0f), Vec3(BX0 - 0.5f, 7.6f, -18.0f), 0.14f);
    Pipe(b, Vec3(BX0 - 0.5f, 10.5f, -6.0f), Vec3(BX0 - 0.5f, 7.6f, -6.0f), 0.14f);
    Pipe(b, Vec3(BX0 - 0.5f, 10.5f, -10.0f), Vec3(BX0 - 0.5f, 10.5f, -6.0f), 0.14f);
    Pipe(b, Vec3(BX0 - 0.5f, 7.6f, -18.0f), Vec3(BX0, 7.6f, -18.0f), 0.14f);
    Pipe(b, Vec3(BX0 - 0.5f, 7.6f, -6.0f), Vec3(BX0, 7.6f, -6.0f), 0.14f);
    // Laufsteg oben zwischen den Silos (nur Optik)
    k.Style(MAT_METAL, 0x4a4e54);
    k.Visual(Vec3(sx - 0.5f, 16.0f, -14.0f), Vec3(sx + 0.5f, 16.1f, 2.0f));
    k.Prop("hubwagen", Vec3(-42.0f, 0, -2.0f), 1.0f);
    k.Prop("mehlpalette", Vec3(-40.0f, 0, -12.0f), 0.1f);
    k.Prop("palette", Vec3(-40.4f, 0, 6.0f), 0.2f);
    k.Prop("palette", Vec3(-40.4f, 0.144f, 6.0f), 0.1f);
    k.Prop("fass_blau", Vec3(-52.0f, 0, 8.0f), 0);
    k.Prop("kabeltrommel", Vec3(-53.0f, 0, -18.0f), 0.6f);
  }

  // ------------------------------------------------------------------------------------------
  // Stabgitterzaun ringsum mit unsichtbarer Grenze
  void FenceLine(Vec3 a, Vec3 c) {
    Vec3 d = c - a;
    float len = Length(d);
    Vec3 dir = d / len;
    int n = std::max(1, (int)(len / 2.5f));
    k.Style(MAT_METAL, 0x2e4a3a);
    for (int i = 0; i <= n; i++) {
      Vec3 p = Lerp(a, c, (float)i / n);
      k.Visual(p + Vec3(-0.04f, 0, -0.04f), p + Vec3(0.04f, 2.1f, 0.04f));
    }
    for (float y : {0.25f, 1.95f}) {
      Vec3 lo = Min(a, c), hi = Max(a, c);
      k.Visual(Vec3(lo.x - 0.015f, y, lo.z - 0.015f) + Vec3(0, 0, 0), Vec3(hi.x + 0.015f, y + 0.05f, hi.z + 0.015f));
    }
    int m = (int)(len / 0.22f);
    for (int i = 0; i < m; i++) {
      float t = (i + 0.5f) / m;
      if (fabsf(t - roundf(t * n) / n) * len < 0.07f) continue;
      Vec3 p = a + dir * (t * len);
      k.Visual(p + Vec3(-0.008f, 0.1f, -0.008f), p + Vec3(0.008f, 2.05f, 0.008f));
    }
    Vec3 lo = Min(a, c), hi = Max(a, c);
    k.Clip(Vec3(lo.x - 0.2f, 0, lo.z - 0.2f), Vec3(hi.x + 0.2f, 40, hi.z + 0.2f));
  }

  void Fence() {
    FenceLine(Vec3(-60, 0, -40), Vec3(60, 0, -40));
    FenceLine(Vec3(-60, 0, -40), Vec3(-60, 0, 58));
    FenceLine(Vec3(60, 0, -40), Vec3(60, 0, 58));
    FenceLine(Vec3(-60, 0, 58), Vec3(40, 0, 58));
    FenceLine(Vec3(55.5f, 0, 58), Vec3(60, 0, 58));
    // Toröffnung zwischen 40 und 55,5: Schiebetor (geschlossen)
    k.Style(MAT_METAL, 0x2e4a3a);
    k.Solid(Vec3(40.0f, 0, 57.9f), Vec3(55.5f, 0.12f, 58.1f));
    for (float x = 40.2f; x < 55.4f; x += 0.3f) k.Visual(Vec3(x - 0.012f, 0.12f, 57.95f), Vec3(x + 0.012f, 1.9f, 58.05f));
    k.Visual(Vec3(40.0f, 1.9f, 57.94f), Vec3(55.5f, 1.98f, 58.06f));
    k.Clip(Vec3(40.0f, 0, 57.8f), Vec3(55.5f, 40, 58.2f));
  }

  // ------------------------------------------------------------------------------------------
  void VisualTree(Vec3 p, float s) {
    MeshBuilder& b = k.ob;
    b.Mat(MAT_WOOD);
    b.ColorHex(0x5a4330);
    b.Frustum(p, 0.3f * s, 0.18f * s, 3.4f * s, 8);
    b.Mat(MAT_LEAVES);
    b.ColorHex(0x45702a);
    b.Ellipsoid(p + Vec3(0, 4.8f * s, 0), Vec3(2.6f, 2.2f, 2.6f) * s, 12, 7);
  }

  void Backdrop() {
    MeshBuilder& b = k.ob;
    // Fernboden ringsum (nicht unter dem Gelände)
    const u32 groundColor = 0x6f8450;
    k.FloorFinish(-400, -400, 400, -40.2f, -0.02f, MAT_GRASS, groundColor);
    k.FloorFinish(-400, 58.2f, 400, 400, -0.02f, MAT_GRASS, groundColor);
    k.FloorFinish(-400, -40.2f, -60.2f, 58.2f, -0.02f, MAT_GRASS, groundColor);
    k.FloorFinish(60.2f, -40.2f, 400, 58.2f, -0.02f, MAT_GRASS, groundColor);
    k.FloorFinish(-60.2f, -40.2f, 60.2f, -40.0f, -0.01f, MAT_CONCRETE, 0x8a8a84);
    k.FloorFinish(-60.2f, 58.0f, 60.2f, 58.2f, -0.01f, MAT_CONCRETE, 0x8a8a84);
    k.FloorFinish(-60.2f, -40.0f, -60.0f, 58.0f, -0.01f, MAT_CONCRETE, 0x8a8a84);
    k.FloorFinish(60.0f, -40.0f, 60.2f, 58.0f, -0.01f, MAT_CONCRETE, 0x8a8a84);
    // Baumreihen hinter dem Zaun
    Rng r(77);
    for (float x = -56; x <= 56; x += 7) {
      VisualTree(Vec3(x + r.Range(-1, 1), 0, -45 - r.Range(0, 3)), r.Range(0.9f, 1.3f));
      VisualTree(Vec3(x + r.Range(-1, 1), 0, 63 + r.Range(0, 3)), r.Range(0.9f, 1.3f));
    }
    for (float z = -36; z <= 54; z += 7) {
      VisualTree(Vec3(-65 - r.Range(0, 3), 0, z + r.Range(-1, 1)), r.Range(0.9f, 1.3f));
      VisualTree(Vec3(65 + r.Range(0, 3), 0, z + r.Range(-1, 1)), r.Range(0.9f, 1.3f));
    }
    // Hügel und Nachbarhallen
    k.Style(MAT_GRASS, 0x5f7a44);
    b.Ellipsoid(Vec3(-180, 0, -150), Vec3(140, 32, 90), 24, 8, 0.0f);
    b.Ellipsoid(Vec3(160, 0, -190), Vec3(160, 40, 100), 24, 8, 0.0f);
    b.Ellipsoid(Vec3(40, 0, 240), Vec3(200, 28, 80), 24, 8, 0.0f);
    for (int i = 0; i < 6; i++) {
      float x = -140 + i * 55.0f, z = i % 2 ? 110.0f : 125.0f, wdt = 30 + (i % 3) * 8.0f;
      k.Style(i % 2 ? MAT_BRICK : MAT_CORRUGATED, i % 2 ? 0x8a4a36 : 0xa8acb0);
      k.Visual(Vec3(x, 0, z), Vec3(x + wdt, 10 + (i % 3) * 3.0f, z + 22));
      k.Style(MAT_CORRUGATED, 0x6a6e74);
      k.Visual(Vec3(x - 0.5f, 10 + (i % 3) * 3.0f, z - 0.5f), Vec3(x + wdt + 0.5f, 10.6f + (i % 3) * 3.0f, z + 22.5f));
    }
    for (int i = 0; i < 5; i++) {
      float z = -110 + i * 50.0f;
      k.Style(MAT_BRICK, 0x7a4232);
      k.Visual(Vec3(120, 0, z), Vec3(150, 14 + (i % 2) * 6.0f, z + 26));
    }
    // Wasserturm
    k.Style(MAT_METAL, 0x8a9096);
    for (float a = 0; a < TAU - 0.1f; a += TAU / 4) Beam(b, Vec3(-120 + cosf(a) * 4, 0, 60 + sinf(a) * 4), Vec3(-120 + cosf(a) * 2.5f, 22, 60 + sinf(a) * 2.5f), 0.3f);
    k.Style(MAT_METAL, 0xc8ccd0);
    b.Cylinder(Vec3(-120, 22, 60), 5.0f, 6.0f, 24);
    b.Frustum(Vec3(-120, 28, 60), 5.2f, 0.5f, 2.5f, 24);
    k.Sign("PASTA", Vec3(-120, 25, 65.05f), 0, 0.35f, 0xc8281e, 0, false);
  }

  // ------------------------------------------------------------------------------------------
  void Zones() {
    const float indoorSky = 0.2f;
    k.Zone(Vec3(-75, -5, -55), Vec3(75, 60, 75), 1.0f, "Werksgelände", -1);
    k.Zone(Vec3(-60, -1, BZ1), Vec3(60, 30, 58), 1.0f, "Werkshof", 0);
    k.Zone(Vec3(-60, -1, BZ0), Vec3(BX0, 30, BZ1), 1.0f, "Siloplatz", 0);
    k.Zone(Vec3(BX1, -1, BZ0), Vec3(60, 30, BZ1), 1.0f, "Parkplatz", 0);
    k.Zone(Vec3(-60, -1, -40), Vec3(60, 30, BZ0), 1.0f, "Hinterhof", 0);
    k.Zone(Vec3(50.0f, -1, 48.0f), Vec3(55.0f, 3.2f, 53.0f), 0.2f, "Pförtnerhaus", 2);
    k.Zone(Vec3(-11.3f, 1.0f, 22.6f), Vec3(-8.7f, 4.0f, 36.0f), 0.3f, "Lkw-Anhänger", 2);
    k.Zone(Vec3(-IX, -1, BZ0), Vec3(IX, SHED_HI + 0.2f, HZ1), 0.35f, "Produktionshalle", 1);
    k.Zone(Vec3(-16.0f, 1.0f, -16.5f), Vec3(-9.0f, 3.2f, -11.5f), 0.35f, "Mischerbühne", 2);
    k.Zone(Vec3(-IX, CAT - 0.3f, BZ0), Vec3(IX, CAT + 3.0f, -19.9f), 0.35f, "Laufsteg", 2);
    k.Zone(Vec3(-IX, CAT - 0.3f, -19.9f), Vec3(-18.2f, CAT + 3.0f, -9.0f), 0.35f, "Laufsteg", 2);
    k.Zone(Vec3(18.2f, CAT - 0.3f, -19.9f), Vec3(IX, CAT + 3.0f, 0.4f), 0.35f, "Laufsteg", 2);
    k.Zone(Vec3(IX, -1, BZ0), Vec3(BX1, WALL_H, -2.0f), 0.15f, "Trockenraum", 1);
    k.Zone(Vec3(BX0, -1, BZ0), Vec3(-IX, WALL_H, 4.0f), indoorSky, "Mehllager", 1);
    k.Zone(Vec3(BX0, -1, 4.0f), Vec3(-IX, WALL_H, BZ1), indoorSky, "Werkstatt", 1);
    k.Zone(Vec3(-IX, -1, HZ1), Vec3(IX, WALL_H, BZ1), 0.25f, "Verpackung", 1);
    k.Zone(Vec3(IX, -1, -2.0f), Vec3(BX1, OGC, HZ1), indoorSky, "Kantine", 1);
    k.Zone(Vec3(IX, -1, HZ1), Vec3(BX1, OGC, BZ1), 0.15f, "Umkleide", 1);
    k.Zone(Vec3(IX, OGC, -2.0f), Vec3(28.0f, WALL_H, HZ1), indoorSky, "Meisterbüro", 1);
    k.Zone(Vec3(28.0f, OGC, -2.0f), Vec3(BX1, WALL_H, HZ1), indoorSky, "Labor", 1);
    k.Zone(Vec3(IX, OGC, HZ1), Vec3(BX1, WALL_H, BZ1), 0.15f, "Archiv", 1);
  }

  void Spawns() {
    for (float x : {44.0f, 46.5f, 49.0f, 51.5f})
      for (float z : {38.0f, 40.5f, 43.0f, 45.5f}) w.hunterSpawns.push_back({Vec3(x, 0, z), PI * 0.25f});
    for (float x : {-16.0f, -8.0f, 0.0f, 8.0f, 16.0f}) {
      w.propSpawns.push_back({Vec3(x < -15 ? -14.0f : x, 0, -8.6f), 0});
      w.propSpawns.push_back({Vec3(x, 0, -0.4f), PI});
      w.propSpawns.push_back({Vec3(x, 0, 5.4f), PI});
    }
    w.propSpawns.push_back({Vec3(-6.0f, 0, 9.0f), PI});
    w.cameraPath = {Vec3(-6, 3.5f, 52),      Vec3(0, 7, 18),          Vec3(-17, 5.2f, -19.2f), Vec3(-4, 1.5f, -12),
                    Vec3(-15, 3.4f, -9.8f),  Vec3(-12.5f, 2.2f, -14), Vec3(-12, 2.2f, 1.2f),   Vec3(0, 2.4f, -3),
                    Vec3(22.5f, 2.0f, -3.5f), Vec3(31, 1.4f, -13.4f), Vec3(-22.5f, 2.5f, -11.5f), Vec3(-29, 3.2f, -16),
                    Vec3(-38, 2.2f, 10),     Vec3(-47, 9, -6)};
  }
};

}  // namespace

void BuildNudelfabrik(World& w) {
  props::BuildCatalog();
  Builder b(w);
  b.Build();
}
