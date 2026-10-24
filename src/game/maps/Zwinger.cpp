// Eigene, für Prop Hunt verkleinerte Interpretation des Dresdner Zwingers.
// Architekturvorbild: Schlösserland Sachsen, Highlights der Architektur (siehe README).
#include "game/MapKit.h"
#include "game/Maps.h"
#include "game/Shapes.h"

namespace {
constexpr u32 STONE = 0xcab994, LIGHT = 0xe0d0ad, DARK = 0x9c8866, COPPER = 0x588578, GOLD = 0xe1b84c;
constexpr float DECK = 4.3f;

class Zwinger {
  World& w;
  MapKit k;
  void Stone(u32 color = STONE) { k.Style(MAT_SANDSTONE, color); }
  void Terrace(float x0, float z0, float x1, float z1, float top, float thick = .3f) {
    Stone(LIGHT); k.Solid({x0, top - thick, z0}, {x1, top, z1});
  }
  void Column(Vec3 p, float h = 3.1f, float r = .23f) {
    Stone(LIGHT); k.ob.BoxC(p + Vec3(0, .12f, 0), {r * 1.5f, .12f, r * 1.5f});
    k.ob.Frustum(p + Vec3(0, .24f, 0), r, r * .78f, h - .5f, 12);
    k.ob.BoxC(p + Vec3(0, h - .14f, 0), {r * 1.45f, .14f, r * 1.45f});
  }
  // Echter offener Bogen: die Öffnung bleibt bis zur Bogenwurzel frei.
  void Arch(Vec3 p, float yaw, float width, float spring = 2.5f) {
    float radius = width * .5f;
    Stone(LIGHT); auto& b = k.ob;
    b.Push(Translation(p) * RotationY(yaw));
    for (int i = 0; i < 16; i++) {
      float a = PI * i / 16, c = PI * (i + 1) / 16;
      Vec2 a0(cosf(a) * radius, spring + sinf(a) * radius);
      Vec2 a1(cosf(c) * radius, spring + sinf(c) * radius);
      Vec2 b0(cosf(a) * (radius + .19f), spring + sinf(a) * (radius + .19f));
      Vec2 b1(cosf(c) * (radius + .19f), spring + sinf(c) * (radius + .19f));
      ExtrudeConvex(b, {a0, b0, b1, a1}, -.18f, .18f);
    }
    b.BoxC({0, spring + radius + .05f, .07f}, {.14f, .23f, .21f}); b.Pop();
    for (float side : {-1.0f, 1.0f}) {
      Vec3 foot = p + RotateY({side * (radius + .21f), 0, 0}, yaw);
      Column(foot, spring + .03f, .19f);
      k.ClipR(foot + Vec3(0, spring * .5f, 0), {.27f, spring * .5f, .27f}, yaw, COL_SOLID);
    }
  }
  void Balustrade(Vec3 a, Vec3 b) {
    Vec3 delta = b - a; float len = Length(delta); if (len < .1f) return;
    float yaw = atan2f(delta.x, delta.z); Vec3 c = (a + b) * .5f;
    Stone();
    k.SolidR(c + Vec3(0, .10f, 0), {.16f, .10f, len * .5f}, yaw);
    k.SolidR(c + Vec3(0, .94f, 0), {.20f, .08f, len * .5f}, yaw);
    int n = std::max(1, (int)(len / .62f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = a + delta * ((float)i / n);
      k.ob.Frustum(p + Vec3(0, .20f, 0), .10f, .065f, .22f, 8);
      k.ob.Ellipsoid(p + Vec3(0, .53f, 0), {.115f, .18f, .115f}, 8, 6);
      k.ob.Frustum(p + Vec3(0, .67f, 0), .065f, .09f, .20f, 8);
    }
    k.ClipR(c + Vec3(0, .48f, 0), {.14f, .48f, len * .5f}, yaw, COL_SOLID);
  }
  void Dome(Vec3 p, float rx, float rz, float height) {
    auto& b = k.ob; k.Style(MAT_METAL, COPPER);
    // Lathe-Profil aus überlappungsfreien Ringen: die barocke Haube bleibt klar lesbar.
    const float radii[] = {1, .88f, .74f, .88f, .80f, .48f, .18f, .03f};
    b.Push(Translation(p) * Scaling({rx, height, rz}));
    for (int i = 0; i < 7; i++) b.Frustum({0, i / 7.0f, 0}, radii[i], radii[i + 1], 1.0f / 7, 32, false);
    b.Pop();
    k.Style(MAT_GOLD, GOLD); b.Cylinder(p + Vec3(0, height, 0), .045f, .65f, 10);
    b.Sphere(p + Vec3(0, height + .55f, 0), .14f, 12, 8);
  }
  void Pavilion(float x, float z, const char* label, bool bell = false) {
    // Vier breite Zugänge und ein umlaufender Terrassenrand vermeiden Sackgassen.
    Stone();
    for (float side : {-1.0f, 1.0f}) {
      float zz = z + side * 4;
      k.WallX(x - 4, x + 4, zz - .16f, zz + .16f, 0, 4, {{x - 1.65f, x + 1.65f, 0, 3.6f}});
      Arch({x, 0, zz + side * .19f}, side > 0 ? 0 : PI, 3.3f, 2.05f);
      float xx = x + side * 4;
      Stone(); k.WallZ(z - 3.84f, z + 3.84f, xx - .16f, xx + .16f, 0, 4, {{z - 1.65f, z + 1.65f, 0, 3.6f}});
    }
    Terrace(x - 4.25f, z - 4.5f, x + 4.5f, z + 4.5f, DECK);
    Stone(); k.Solid({x - 2.35f, DECK, z - 2.35f}, {x + 2.35f, 7.3f, z + 2.35f});
    // Obere Fenster, Pilaster, Gesimse und kupferne Mansardenhaube.
    for (float side : {-1.0f, 1.0f}) {
      k.Style(MAT_GLASS, 0x476776);
      for (float dx : {-1.35f, 0.0f, 1.35f}) {
        k.Visual({x + dx - .35f, 5.0f, z + side * 2.37f - .015f}, {x + dx + .35f, 6.65f, z + side * 2.37f + .015f});
        Column({x + dx + .48f, 4.4f, z + side * 2.43f}, 2.65f, .11f); k.Style(MAT_GLASS, 0x476776);
      }
    }
    for (float side : {-1.0f, 1.0f}) for (float dz : {-1.35f, 0.0f, 1.35f}) {
      k.Style(MAT_GLASS, 0x476776);
      k.Visual({x + side * 2.37f - .015f, 5, z + dz - .35f}, {x + side * 2.37f + .015f, 6.65f, z + dz + .35f});
      Column({x + side * 2.43f, 4.4f, z + dz + .48f}, 2.65f, .11f);
    }
    for (float dx : {-3.2f, 3.2f}) {
      k.Prop("zwinger_urne", {x + dx, DECK, z + (z < 0 ? 3.25f : -3.25f)}, 0);
      Column({x + dx, 0, z + (z < 0 ? 4.3f : -4.3f)}, 3.9f, .25f);
    }
    Stone(LIGHT); k.Visual({x - 2.6f, 7.28f, z - 2.6f}, {x + 2.6f, 7.55f, z + 2.6f});
    Dome({x, 7.55f, z}, 2.7f, 2.7f, 2.9f);
    if (bell) {
      k.Style(MAT_GOLD, GOLD); k.ob.CylinderZ({x, 6.3f, z - 2.48f}, .73f, .12f, 32);
      k.Style(MAT_PLAIN, 0xf4e6bf); k.ob.CylinderZ({x, 6.3f, z - 2.56f}, .64f, .045f, 32);
      k.Style(MAT_METAL, 0x2e3030); k.Visual({x - .025f, 6.28f, z - 2.59f}, {x + .025f, 6.80f, z - 2.58f});
      k.Visual({x, 6.28f, z - 2.59f}, {x + .34f, 6.33f, z - 2.58f});
      for (int i = 0; i < 7; i++) {
        k.Style(MAT_PLASTIC, 0xe5d5bc); k.ob.Frustum({x - 1.2f + .4f * i, 4.65f + .13f * abs(i - 3), z - 2.55f}, .14f, .08f, .25f, 12);
      }
    }
    k.Sign(label, {x, 3.87f, z + (z < 0 ? 4.8f : -4.8f)}, z < 0 ? 0 : PI, .055f, 0x5f4c34, 0, false);
    k.Light({x, 3.4f, z}, 7, {1.3f, 1.1f, .8f});
  }
  void Arcades() {
    for (float side : {-1.0f, 1.0f}) {
      // Zwei Bogengalerien je Seite; die Öffnung in der Mitte gehört dem Pavillon.
      for (int i = 0; i < 16; i++) {
        if (i == 7 || i == 8) continue;
        float a = PI * i / 16, b = PI * (i + 1) / 16, m = (a + b) * .5f;
        Vec3 p0(27 * cosf(a), 0, side * (22 + 17 * sinf(a)));
        Vec3 p1(27 * cosf(b), 0, side * (22 + 17 * sinf(b)));
        Vec3 p = (p0 + p1) * .5f;
        Vec3 inward = Normalize(Vec3(-17 * cosf(m), 0, -side * 27 * sinf(m)));
        float yaw = atan2f(inward.x, inward.z), len = Distance(p0, p1);
        Stone(); k.SolidR(p - inward * 1.9f + Vec3(0, 2, 0), {len * .5f + .02f, 2, .16f}, yaw);
        Stone(LIGHT); k.SolidR(p + Vec3(0, DECK - .15f, 0), {len * .5f + .07f, .15f, 2.1f}, yaw);
        Arch(p + inward * 1.8f, yaw, len - .65f, 2.25f);
        Balustrade(p0 - inward * 2 + Vec3(0, DECK, 0), p1 - inward * 2 + Vec3(0, DECK, 0));
        if (i % 3 == 1) k.Prop("zwinger_urne", p + Vec3(0, DECK, 0), 0);
        if (i % 4 == 2) k.Prop("zwinger_vase", p - inward * .9f, yaw);
      }
      Pavilion(0, side * 39, side < 0 ? "WALLPAVILLON" : "GLOCKENSPIEL", side > 0);
      for (float sx : {-1.0f, 1.0f}) Terrace(sx < 0 ? -6.2f : 4.2f, side * 39 - 1.8f,
                                                        sx < 0 ? -4.2f : 6.2f, side * 39 + 1.8f, DECK);
      Pavilion(-27, side * 22, side < 0 ? "MATH. PHYSIK. SALON" : "PORZELLANSAMMLUNG");
      Pavilion(27, side * 22, side < 0 ? "FRANZOESISCHER PAVILLON" : "DEUTSCHER PAVILLON");
    }
    // Gerade Langgalerie mit Kronentor. Hofseitig offene Arkaden, außen Sandstein.
    for (int side : {-1, 1}) {
      for (int i = 0; i < 4; i++) {
        float z = side * (6.8f + i * 3.8f);
        Stone(); k.Solid({-30.1f, 0, z - 1.9f}, {-29.8f, 4, z + 1.9f});
        Arch({-26, 0, z}, PI * .5f, 3.05f, 2.3f);
      }
      float z0 = side < 0 ? -18.1f : 4.3f, z1 = side < 0 ? -4.3f : 18.1f;
      Terrace(-30.1f, z0, -25.9f, z1, DECK);
      Balustrade({-30.0f, DECK, z0}, {-30.0f, DECK, z1});
      // Hofseitige Treppen: zwei unabhängige Aufgänge zur umlaufenden Galerie.
      Stone(LIGHT); k.Stairs({-19.9f, 0, side * 13.0f}, 1, 2.4f, DECK / 22, 22, .30f, 0);
    }
  }
  void CrownGate() {
    // Turm quer zur Langgalerie, zentraler Durchgang zum Graben.
    Stone();
    k.WallZ(-4.3f, 4.3f, -32.0f, -31.7f, 0, 5, {{-2, 2, 0, 4.5f}});
    k.WallZ(-4.3f, 4.3f, -24.3f, -24.0f, 0, 5, {{-2, 2, 0, 4.5f}});
    for (float z : {-4.15f, 4.15f}) {
      k.WallX(-31.7f, -24.3f, z - .15f, z + .15f, 0, 4, {{-29.4f, -26.6f, 0, 3.5f}});
    }
    Arch({-23.8f, 0, 0}, PI * .5f, 4, 2.4f); Arch({-32.2f, 0, 0}, -PI * .5f, 4, 2.4f);
    Terrace(-32.3f, -4.4f, -23.7f, 4.4f, 5.2f, .25f);
    // Zwei kleine Stufen verbinden die niedrigere Galerie mit der Torplattform.
    Stone(LIGHT);
    for (float side : {-1.0f, 1.0f}) k.Stairs({-28, DECK, side * 6.1f}, side > 0 ? 3 : 2, 2.5f, .18f, 5, .35f, DECK);
    k.Solid({-30.0f, 5.2f, -2}, {-26, 8.1f, 2});
    for (float x : {-30.2f, -25.8f}) for (float z : {-1.8f, 1.8f}) Column({x, 5.2f, z}, 3.2f, .24f);
    for (float x : {-30.04f, -25.96f}) {
      k.Style(MAT_GLASS, 0x466977); k.Visual({x - .02f, 5.75f, -.55f}, {x + .02f, 7.4f, .55f});
      Stone(LIGHT); k.ob.Ellipsoid({x, 7.78f, 0}, {.18f, .25f, .52f}, 16, 8);
    }
    Dome({-28, 8.35f, 0}, 2.95f, 2.95f, 3.2f);
    auto& b = k.ob; k.Style(MAT_GOLD, GOLD);
    b.Torus({-28, 11.65f, 0}, 1.05f, .12f, 32, 8);
    for (int i = 0; i < 8; i++) {
      float a = i * TAU / 8;
      b.Frustum({-28 + 1.03f * cosf(a), 11.65f, 1.03f * sinf(a)}, .09f, .055f, .60f, 10);
      b.Sphere({-28 + 1.03f * cosf(a), 12.29f, 1.03f * sinf(a)}, .12f, 10, 6);
    }
    b.Sphere({-28, 12.6f, 0}, .21f); b.BoxC({-28, 13.0f, 0}, {.04f, .27f, .04f}); b.BoxC({-28, 13.08f, 0}, {.21f, .04f, .04f});
    k.Sign("KRONENTOR", {-23.48f, 4.75f, 0}, PI * .5f, .08f, 0x695235, 0, false);
    k.Style(MAT_WOOD, 0x8a6b44); k.Solid({-43, -.15f, -2.4f}, {-32, .03f, 2.4f});
    Balustrade({-43, .03f, -2.5f}, {-32.2f, .03f, -2.5f}); Balustrade({-43, .03f, 2.5f}, {-32.2f, .03f, 2.5f});
    k.FloorFinish(-40, -24, -33, -2.7f, .06f, MAT_WATER, 0x416d6f);
    k.FloorFinish(-40, 2.7f, -33, 24, .06f, MAT_WATER, 0x416d6f);
  }
  void SemperGallery() {
    Stone();
    std::vector<Hole> entries = {{-3, 3, 0, 4}, {-17, -14, 0, 3}, {14, 17, 0, 3}};
    k.WallZ(-18, 18, 26.8f, 27.1f, 0, 7.6f, entries);
    k.WallZ(-18, 18, 33, 33.3f, 0, 7.6f, {{-3, 3, 0, 4}});
    k.Slab(26.7f, -18, 33.4f, 18, 7.9f, .3f, MAT_METAL, 0x617775);
    for (float z : {-15.5f, 0.0f, 15.5f}) Arch({26.58f, 0, z}, -PI * .5f, z == 0 ? 5.7f : 2.7f, z == 0 ? 2.2f : 2.0f);
    for (float z : {-12.0f, -8.0f, -4.5f, 4.5f, 8.0f, 12.0f}) {
      for (float y : {1.0f, 4.9f}) {
        k.Style(MAT_GLASS, 0x3b5963); k.Visual({26.74f, y, z - .72f}, {26.77f, y + 1.8f, z + .72f});
        Stone(LIGHT); k.Visual({26.56f, y - .12f, z - .9f}, {26.79f, y, z + .9f});
      }
      Column({26.5f, 0, z + 1.4f}, 7.4f, .22f);
    }
    Stone(LIGHT); k.Visual({26.45f, 4.0f, -18}, {26.79f, 4.23f, 18});
    k.Sign("SEMPERGALERIE", {26.38f, 6.7f, 0}, -PI * .5f, .11f, 0xe4c78e, 0, false);
    for (float z : {-11.0f, 9.0f}) {
      k.Prop("zwinger_kunstkiste", {31.8f, 0, z}, -PI * .5f);
      k.Prop("zwinger_kunstkiste", {31.7f, 0, z + 2}, 0);
      k.Prop("zwinger_statue", {28.3f, 0, z + 1}, -PI * .5f);
      k.Light({30, 5.5f, z}, 9, {2.1f, 1.8f, 1.3f});
    }
    k.Zone({27.2f, 0, -18}, {32.9f, 7.5f, 18}, .35f, "Sempergalerie", 2);
  }
  void Fountain(float x, float z) {
    k.FloorFinish(x - 7, z - 6, x + 7, z + 6, .018f, MAT_GRASS, 0x5c773f);
    Stone(DARK);
    for (float side : {-1.0f, 1.0f}) {
      k.Solid({x - 3.7f, 0, z + side * 2.6f - .13f}, {x + 3.7f, .28f, z + side * 2.6f + .13f});
      k.Solid({x + side * 3.57f - .13f, 0, z - 2.47f}, {x + side * 3.57f + .13f, .28f, z + 2.47f});
    }
    k.FloorFinish(x - 3.42f, z - 2.46f, x + 3.42f, z + 2.46f, .09f, MAT_WATER, 0x579896);
    Stone(); k.Solid({x - .3f, 0, z - .3f}, {x + .3f, .45f, z + .3f});
    k.Style(MAT_WATER, 0xbad7cf); k.ob.Cylinder({x, .45f, z}, .055f, 2.0f, 10);
    for (int j = 0; j < 6; j++) {
      float a = j * TAU / 6;
      for (int i = 0; i < 18; i++) {
        float t = i / 17.0f; Vec3 p(x + cosf(a) * 1.8f * t, .13f + 2.1f * (1 - t * t), z + sinf(a) * 1.8f * t);
        k.ob.Sphere(p, .035f + .015f * t, 6, 4);
      }
    }
    for (float side : {-1.0f, 1.0f}) {
      k.Prop("zwinger_orange", {x + side * 5.6f, 0, z - 4.7f}, 0);
      k.Prop("zwinger_orange", {x + side * 5.6f, 0, z + 4.7f}, PI);
    }
  }
  void NymphBath() {
    // Intimer Grottenhof hinter dem Französischen Pavillon, mit zwei Zugängen.
    Stone(DARK);
    k.WallZ(-44, -26.3f, 35.5f, 35.9f, 0, 5.8f, {{-31, -28, 0, 3}});
    k.WallZ(-44, -30, 19.5f, 19.9f, 0, 5.8f, {{-35, -32, 0, 3}});
    k.WallX(19.5f, 35.9f, -44.4f, -44, 0, 5.8f);
    Stone(); k.Solid({24.5f, 0, -43.9f}, {30.5f, 1.0f, -42.5f});
    k.Solid({25.2f, 1.0f, -43.9f}, {29.8f, 2.0f, -43});
    k.Solid({26.0f, 2.0f, -43.9f}, {29, 3.4f, -43.4f});
    k.Style(MAT_WATER, 0x80b5ac);
    k.Visual({26.3f, 1.95f, -43.02f}, {28.7f, 3.35f, -42.98f});
    k.Visual({25.5f, .97f, -42.48f}, {29.5f, 2.0f, -42.44f});
    k.FloorFinish(24, -42.4f, 31, -39.8f, .08f, MAT_WATER, 0x537e75);
    for (float x : {21.4f, 33.6f}) for (float z : {-40.0f, -35.8f}) {
      k.Prop("zwinger_statue", {x, 0, z}, x < 25 ? PI * .5f : -PI * .5f);
      k.Prop("zwinger_urne", {x, 0, z + 1.8f}, 0);
    }
    k.Sign("NYMPHENBAD", {27.5f, 4.7f, -43.8f}, 0, .10f, 0xd6c19a, 0, false);
    k.Zone({20, 0, -44}, {35.4f, 7, -28}, .75f, "Nymphenbad", 3);
  }
  void Furnish() {
    for (float z : {-22.0f, 22.0f}) {
      for (float dx : {-1.0f, 1.0f}) {
        Stone(LIGHT); k.Solid({-27 + dx * 2.8f - .36f, 0, z - 3.16f}, {-27 + dx * 2.8f + .36f, .68f, z - 2.44f});
        k.Prop("zwinger_vase", {-27 + dx * 2.8f, .68f, z - 2.8f}, 0);
        k.Prop("zwinger_fernrohr", {-27 + dx * 2.7f, 0, z + 2.7f}, 0);
        k.Prop("zwinger_kunstkiste", {27 + dx * 2.6f, 0, z + 2.7f}, 0);
      }
    }
    for (float x : {-21.0f, 21.0f}) for (float z : {-7.0f, 7.0f}) {
      k.Prop("parkbank", {x, 0, z}, x < 0 ? PI * .5f : -PI * .5f);
      k.Prop("papierkorb", {x, 0, z + 1.9f}, 0);
      k.Prop("zwinger_urne", {x, 0, z - 2.0f}, 0);
    }
    for (float x : {-10.0f, 10.0f}) for (float z : {-28.0f, 28.0f}) {
      k.Prop("zwinger_orange", {x, 0, z}, 0);
      k.Prop("zwinger_statue", {x + (x < 0 ? -2.3f : 2.3f), 0, z}, z < 0 ? 0 : PI);
    }
    k.Sign("BITTE DIE KUNST NICHT JAGEN", {30.8f, 2.3f, -7.5f}, -PI * .5f, .055f, 0xb7a17a);
    k.Sign("VORSICHT: BEWEGLICHE AUSSTELLUNG", {-29.6f, 2.7f, 9}, PI * .5f, .045f, 0xb7a17a);
  }
  void Boundary() {
    // Unsichtbarer Spielrand: auch hohe Sprünge verlassen die Karte nicht.
    // Kein Mesh, kein Schatten und keine unsichtbare Deckung gegen Schüsse.
    k.Clip({-46, -3, -52}, {-45.5f, 40, 52}, COL_PLAYER);
    k.Clip({41.5f, -3, -52}, {42, 40, 52}, COL_PLAYER);
    k.Clip({-45.5f, -3, -52}, {41.5f, 40, -51.5f}, COL_PLAYER);
    k.Clip({-45.5f, -3, 51.5f}, {41.5f, 40, 52}, COL_PLAYER);
    // Landschaft nur hinter dem spielbaren Boden; keine neue Mauer am Horizont.
    k.FloorFinish(-145, -145, 145, 145, -.14f, MAT_GRASS, 0x5b7544);
    k.FloorFinish(-57, -85, -47, 85, -.12f, MAT_TILE, 0x9c998a);
    k.FloorFinish(43, -85, 53, 85, -.12f, MAT_TILE, 0x9c998a);
    // Abgesetzte Stadthäuser mit Lücken und zurückhaltender Silhouette.
    for (int i = 0; i < 8; i++) {
      float z = -49 + i * 14.0f, h = 7.2f + (i % 3) * 1.1f;
      for (float side : {-1.0f, 1.0f}) {
        float x = side < 0 ? -69.0f - (i % 2)*6 : 65.0f + (i % 2)*6;
        k.Style(MAT_PLASTER, i % 2 ? 0xbeb6a4 : 0xa8a398); k.Visual({x - 1.8f, 0, z - 4.7f}, {x + 1.8f, h, z + 4.7f});
        k.Style(MAT_METAL, 0x596468);
        k.ob.Push(Translation({x, h, z}) * Scaling({1.95f, 1, 4.9f})); k.ob.Frustum({}, 1, .60f, 1.7f, 4); k.ob.Pop();
        k.Style(MAT_GLASS, 0x52666c);
        for (float y : {1.4f, 4.4f}) for (float dz : {-3.0f, 0.0f, 3.0f}) {
          float xx = x - side * 1.83f;
          k.Visual({xx - .015f, y, z + dz - .55f}, {xx + .015f, y + 1.4f, z + dz + .55f});
        }
      }
    }
    for (int i = 0; i < 12; i++) for (float side : {-1.0f, 1.0f}) {
      float x = side < 0 ? -60.0f : 57.0f, z = -67 + i * 12.0f;
      k.Style(MAT_WOOD, 0x68533d); k.ob.Cylinder({x, -.14f, z}, .22f, 3.1f, 8);
      k.Style(MAT_LEAVES, i % 2 ? 0x536d35 : 0x668144);
      k.ob.Ellipsoid({x, 4.3f, z}, {2.2f, 2.5f, 2.0f}, 12, 8);
    }
  }
 public:
  explicit Zwinger(World& world) : w(world), k(world) {}
  void Build() {
    w.env.sunDir = Normalize(Vec3(-.5f, .75f, .3f)); w.env.sunColor = {3.1f, 2.7f, 2.15f};
    w.env.clouds = .25f; w.env.fogDensity = .0018f; w.env.indoorAmbient = {.40f, .37f, .32f};
    w.env.exposure = .78f;
    k.Clip({-46, -2, -52}, {42, 0, 52}, COL_SOLID);
    k.FloorFinish(-46, -52, 42, 52, 0, MAT_TILE, 0xb8ae95);
    Arcades(); CrownGate(); SemperGallery(); NymphBath();
    for (float x : {-11.0f, 11.0f}) for (float z : {-10.0f, 10.0f}) Fountain(x, z);
    Furnish(); Boundary();
    k.Zone({-46, -1, -52}, {42, 20, 52}, 1, "Dresdner Zwinger");
    k.Zone({-24, 0, -22}, {26.5f, 12, 22}, 1, "Zwingerhof", 1);
    k.Zone({-32.2f, 0, -4.3f}, {-23.8f, 5.1f, 4.3f}, .5f, "Kronentor", 2);
    k.Zone({-31, 0, -26}, {-23, 4, -18}, .45f, "Mathematisch-Physikalischer Salon", 2);
    k.Zone({-31, 0, 18}, {-23, 4, 26}, .45f, "Porzellansammlung", 2);
    k.Zone({-30, DECK, -18}, {-26, 8, 18}, 1, "Galerieterrasse", 3);
    k.Zone({-4, 0, -43}, {4, 4, -35}, .5f, "Wallpavillon", 2);
    k.Zone({-4, 0, 35}, {4, 4, 43}, .5f, "Glockenspielpavillon", 2);
    for (float x : {-4.5f, -1.5f, 1.5f, 4.5f}) for (float z : {45.0f, 46.5f, 48.0f, 49.5f})
      w.hunterSpawns.push_back({{x, 0, z}, 0});
    for (float x : {-1.5f, 1.5f}) for (float z : {-18.0f, -13.0f, -8.0f, -3.0f, 3.0f, 8.0f, 13.0f, 18.0f})
      w.propSpawns.push_back({{x, 0, z}, PI});
    w.cameraPath = {{16, 13, 28}, {-10, 4, -8}, {4, 2.1f, 1}, {-28, 7, 0},
                    {7, 2.1f, 23}, {0, 5, 39}, {7, 2.1f, -24}, {0, 5, -39},
                    {30, 3, -29}, {27, 2.2f, -42}, {-26.8f, 2, 23}, {-29.3f, .8f, 19.2f},
                    {-28, 6.1f, -12}, {-6, 2, 10}, {22, 2.4f, -5}, {29, 4.5f, 0}};
  }
};
}  // namespace

void BuildZwinger(World& w) { props::BuildCatalog(); Zwinger(w).Build(); }
