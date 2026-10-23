#include "game/GameView.h"
#include "game/Movement.h"
#include "game/Shapes.h"
#include "game/Sim.h"

namespace {

// Maße des Jäger-Skeletts (Meter)
const float kThigh = 0.43f;     // Hüfte -> Knie
const float kUpperArm = 0.30f;  // Schulter -> Ellbogen
const float kForeArm = 0.27f;   // Ellbogen -> Handmitte

// Basis für ein Glied: lokal -Y zeigt von "from" nach "to", lokal +Z in Richtung "pole"
Mat4 BoneBasis(Vec3 from, Vec3 to, Vec3 pole) {
  Vec3 y = Normalize(from - to);
  Vec3 z = pole - y * Dot(pole, y);
  z = Length(z) < 1e-4f ? Vec3(0, 0, 1) : Normalize(z);
  Vec3 x = Cross(y, z);
  Mat4 m = Mat4::Identity();
  m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
  m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
  m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
  m.m[12] = from.x; m.m[13] = from.y; m.m[14] = from.z;
  return m;
}

// Runder, leicht verjüngter Abschnitt zwischen zwei Punkten
void Limb(MeshBuilder& b, Vec3 a, Vec3 c, float ra, float rc, int segs = 12) {
  float len = Length(c - a);
  if (len < 1e-4f) return;
  b.Push(BoneBasis(a, a * 2.0f - c, Vec3(0, 0, 1)));  // Ursprung in a, +Y zeigt nach c
  b.Frustum(Vec3(0, 0, 0), ra, rc, len, segs);
  b.Pop();
}

// Elliptischer Kegelstumpf (Querschnitt in z gestaucht) – Rumpf, Gürtel, Weste
void OvalFrustum(MeshBuilder& b, Vec3 base, float r0, float r1, float h, float zScale, int segs = 16) {
  b.Push(Translation(base) * Scaling(Vec3(1, 1, zScale)));
  b.Frustum(Vec3(0, 0, 0), r0, r1, h, segs);
  b.Pop();
}

// Mehrere elliptische Querschnitte mit weichen Normalen: Stoff folgt dem Körper statt einer Röhre.
struct BodyRing { float y, x, z, offsetZ = 0; };
void TailoredForm(MeshBuilder& b, std::initializer_list<BodyRing> profile, int segs = 20) {
  std::vector<BodyRing> rings(profile);
  const Mat4 inv = Inverse(b.Xf());
  auto point = [&](int j, float a) {
    const auto& q = rings[j];
    return Vec3(cosf(a) * q.x, q.y, sinf(a) * q.z + q.offsetZ);
  };
  auto normal = [&](int j, float a) {
    Vec3 vertical = point(std::min(j + 1, (int)rings.size() - 1), a) - point(std::max(j - 1, 0), a);
    Vec3 tangent(-sinf(a) * rings[j].x, 0, cosf(a) * rings[j].z);
    Vec3 n = Normalize(Cross(vertical, tangent));
    return Normalize(Vec3(inv.m[0] * n.x + inv.m[1] * n.y + inv.m[2] * n.z,
                          inv.m[4] * n.x + inv.m[5] * n.y + inv.m[6] * n.z,
                          inv.m[8] * n.x + inv.m[9] * n.y + inv.m[10] * n.z));
  };
  for (int j = 0; j + 1 < (int)rings.size(); j++) {
    for (int i = 0; i < segs; i++) {
      float a = TAU * i / segs, c = TAU * (i + 1) / segs;
      size_t first = b.Target()->verts.size();
      b.Quad(point(j, a), point(j + 1, a), point(j + 1, c), point(j, c));
      Vec3 ns[] = {normal(j, a), normal(j + 1, a), normal(j + 1, c), normal(j, c)};
      for (int k = 0; k < 4; k++) b.Target()->verts[first + k].normal = ns[k];
    }
  }
  for (int j : {0, (int)rings.size() - 1}) {
    Vec3 center(0, rings[j].y, rings[j].offsetZ);
    for (int i = 0; i < segs; i++) {
      Vec3 a = point(j, TAU * i / segs), c = point(j, TAU * (i + 1) / segs);
      if (j == 0) b.Tri(center, a, c);
      else b.Tri(center, c, a);
    }
  }
}

// Abgeschrägte Kanten für Schutzausrüstung und Taschen.
void GearPanel(MeshBuilder& b, Vec3 mn, Vec3 mx, float cut) {
  cut = std::min(cut, std::min(mx.x - mn.x, mx.y - mn.y) * 0.3f);
  std::vector<Vec2> p = {{mn.x + cut, mn.y}, {mx.x - cut, mn.y}, {mx.x, mn.y + cut},
                         {mx.x, mx.y - cut}, {mx.x - cut, mx.y}, {mn.x + cut, mx.y},
                         {mn.x, mx.y - cut}, {mn.x, mn.y + cut}};
  ExtrudeConvex(b, p, mn.z, mx.z);
}

void BuildMeshes(Mesh& torso, Mesh& head, Mesh& thigh, Mesh& shin, Mesh& upperArm, Mesh& foreArm, Mesh& grenade) {
  const u32 uniform = 0x39495d, uniformD = 0x283647, vest = 0x343d3e, vestL = 0x505c5b;
  const u32 black = 0x191d22, skin = 0xc99573, hiVis = 0xe8c454;
  MeshData d;
  MeshBuilder b(&d);

  // --- Cargo-Hose: kräftiger Oberschenkel, Stofffalten und separate Knieschale ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  TailoredForm(b, {{-kThigh, .076f, .079f}, {-.34f, .088f, .09f, .006f},
                  {-.22f, .108f, .106f, .012f}, {-.08f, .114f, .109f}, {.035f, .09f, .09f}});
  b.ColorHex(uniformD);
  for (float side : {-1.0f, 1.0f}) {
    b.Push(Translation({side * .093f, -.205f, .005f}) * RotationY(side * PI * .5f));
    GearPanel(b, {-.055f, -.08f, -.014f}, {.055f, .08f, .018f}, .016f);
    b.ColorHex(0x475567);
    GearPanel(b, {-.056f, .038f, .019f}, {.056f, .075f, .027f}, .009f);
    b.ColorHex(uniformD);
    b.Pop();
  }
  b.ColorHex(0x465467);
  Limb(b, {-.07f, -.33f, -.06f}, {.055f, -.31f, -.065f}, .006f, .004f, 8);
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  OvalFrustum(b, {0, -.385f, 0}, .084f, .084f, .036f, 1.0f);
  b.ColorHex(0x383d43);
  GearPanel(b, {-.069f, -.435f, -.101f}, {.069f, -.313f, -.067f}, .026f);
  b.ColorHex(0x4d535a);
  GearPanel(b, {-.045f, -.412f, -.108f}, {.045f, -.335f, -.1f}, .019f);
  thigh.Upload(d);
  d.Clear();

  // --- Wade mit Falten am Stiefelschaft, geformtem Fuß und Profilsohle ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  TailoredForm(b, {{-.34f, .067f, .066f}, {-.285f, .075f, .075f, .008f},
                  {-.18f, .086f, .094f, .017f}, {-.07f, .081f, .083f, .009f}, {.025f, .073f, .075f},
                  {.05f, .045f, .046f}, {.06f, .012f, .012f}});
  b.ColorHex(uniformD);
  OvalFrustum(b, {0, -.3f, .006f}, .076f, .072f, .022f, 1.0f);
  b.Mat(MAT_LEATHER); b.ColorHex(0x24272a);
  TailoredForm(b, {{-.443f, .076f, .137f, -.05f}, {-.419f, .079f, .133f, -.05f},
                  {-.379f, .073f, .115f, -.04f}, {-.345f, .068f, .076f, -.009f},
                  {-.285f, .067f, .069f}, {-.27f, .065f, .067f}});
  b.ColorHex(0x34373a);
  b.Ellipsoid({0, -.405f, -.12f}, {.072f, .031f, .069f}, 16, 7);
  b.ColorHex(0x171a1d);
  TailoredForm(b, {{-.47f, .075f, .137f, -.051f}, {-.46f, .082f, .144f, -.051f},
                  {-.439f, .082f, .144f, -.051f}, {-.434f, .075f, .137f, -.051f}});
  b.ColorHex(0x54565a);
  for (int i = 0; i < 4; i++) {
    float y = -.325f - i * .018f, z = -.07f - i * .012f;
    Limb(b, {-.028f, y, z}, {.028f, y - .01f, z - .008f}, .003f, .003f, 6);
    Limb(b, {.028f, y, z}, {-.028f, y - .01f, z - .008f}, .003f, .003f, 6);
  }
  shin.Upload(d);
  d.Clear();

  // --- Rumpf: Taille, Brust und Schulterlinie mit eng sitzendem Plattenträger ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  TailoredForm(b, {{-.035f, .151f, .113f, .008f}, {.06f, .176f, .125f, .008f},
                  {.16f, .164f, .114f}, {.28f, .193f, .135f}, {.41f, .23f, .141f},
                  {.48f, .235f, .129f}, {.525f, .17f, .105f}, {.55f, .075f, .065f}});
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  OvalFrustum(b, {0, .115f, 0}, .181f, .18f, .046f, .73f);
  b.Mat(MAT_METAL); b.ColorHex(0x757d80);
  GearPanel(b, {-.028f, .12f, -.139f}, {.028f, .153f, -.131f}, .007f);
  b.Mat(MAT_FABRIC); b.ColorHex(vest);
  OvalFrustum(b, {0, .185f, 0}, .207f, .226f, .17f, .68f);
  std::vector<Vec2> plate = {{-.148f, .185f}, {.148f, .185f}, {.179f, .225f}, {.179f, .392f},
                             {.125f, .482f}, {-.125f, .482f}, {-.179f, .392f}, {-.179f, .225f}};
  ExtrudeConvex(b, plate, -.174f, -.12f);
  ExtrudeConvex(b, plate, .113f, .169f);
  b.ColorHex(vestL);
  for (float side : {-1.0f, 1.0f}) {
    b.Push(Translation({side * .123f, 0, 0}));
    GearPanel(b, {-.033f, .44f, -.153f}, {.033f, .53f, .14f}, .015f);
    b.Mat(MAT_PLASTIC); b.ColorHex(black);
    GearPanel(b, {-.023f, .448f, -.166f}, {.023f, .48f, -.152f}, .005f);
    b.Pop();
    b.Mat(MAT_FABRIC); b.ColorHex(vestL);
  }
  // Nähte und MOLLE-Gurte geben der Weste eine lesbare Struktur.
  b.ColorHex(0x65706b);
  for (float y : {.227f, .269f, .31f}) {
    for (int i = 0; i < 6; i++) {
      float x = -.146f + i * .05f;
      b.Box({x, y, .17f}, {x + .04f, y + .014f, .177f});
    }
  }
  b.ColorHex(vestL);
  for (int i = 0; i < 3; i++) {
    float x = -.137f + i * .092f;
    GearPanel(b, {x, .202f, -.228f}, {x + .081f, .335f, -.171f}, .014f);
    b.ColorHex(0x65706b);
    GearPanel(b, {x + .003f, .306f, -.234f}, {x + .078f, .345f, -.224f}, .008f);
    b.ColorHex(vestL);
  }
  // Funkgerät, Kabel und seitliche Utility-Taschen.
  b.Mat(MAT_PLASTIC); b.ColorHex(black);
  GearPanel(b, {-.164f, .348f, -.208f}, {-.087f, .44f, -.172f}, .008f);
  b.Cylinder({-.098f, .438f, -.185f}, .005f, .105f, 8);
  b.ColorHex(0x738377);
  b.Box({-.151f, .388f, -.211f}, {-.111f, .414f, -.207f});
  b.ColorHex(0x171c20);
  for (int i = 0; i < 4; i++) b.Box({-.152f, .358f + i * .006f, -.211f}, {-.105f, .36f + i * .006f, -.207f});
  Limb(b, {-.15f, .425f, -.19f}, {-.174f, .505f, -.09f}, .006f, .006f, 8);
  b.Mat(MAT_FABRIC); b.ColorHex(vest);
  GearPanel(b, {.164f, .03f, -.064f}, {.224f, .19f, .068f}, .018f);
  GearPanel(b, {-.221f, .065f, -.06f}, {-.165f, .19f, .07f}, .018f);
  // Rückenpatch, kleine reflektierende Markierungen und Brustabzeichen.
  b.ColorHex(0x20292c);
  GearPanel(b, {-.15f, .345f, .17f}, {.15f, .445f, .18f}, .012f);
  b.Mat(MAT_PLAIN); b.ColorHex(0xe8e3ce);
  b.Push(Translation({-PixelTextWidth("SUCHER", .0078f) * .5f, .365f, .181f}));
  BuildPixelText(b, "SUCHER", .0078f, .002f);
  b.Pop();
  b.ColorHex(hiVis);
  for (float side : {-1.0f, 1.0f}) {
    b.Box({side * .128f - .025f, .424f, -.177f}, {side * .128f + .025f, .437f, -.173f});
    b.Box({side * .128f - .022f, .327f, .171f}, {side * .128f + .022f, .339f, .178f});
  }
  b.Push(Translation({.079f, .383f, -.179f}) * RotationY(PI));
  GearPanel(b, {-.033f, -.024f, 0}, {.033f, .024f, .003f}, .009f);
  b.ColorHex(0x29343b);
  b.Push(Translation({-PixelTextWidth("K", .005f) * .5f, -.0175f, .004f}));
  BuildPixelText(b, "K", .005f, .001f);
  b.Pop(); b.Pop();
  // Hoher weicher Kragen schließt den Übergang zum Kopf.
  b.Mat(MAT_FABRIC); b.ColorHex(uniformD);
  OvalFrustum(b, {0, .505f, .005f}, .09f, .074f, .065f, .85f);
  b.Mat(MAT_PLAIN); b.ColorHex(skin);
  b.Cylinder({0, .54f, .006f}, .059f, .052f, 16);
  torso.Upload(d);
  d.Clear();

  // --- Kopf: Kiefer und Wangen, matte Haut, eng anliegende Schutzbrille ---
  b.Mat(MAT_PLAIN); b.ColorHex(skin);
  TailoredForm(b, {{0, .056f, .054f, .008f}, {.045f, .058f, .058f}, {.064f, .065f, .075f, -.012f},
                  {.095f, .085f, .086f, -.006f}, {.145f, .098f, .099f}, {.191f, .096f, .097f, .003f},
                  {.233f, .08f, .085f, .008f}, {.255f, .045f, .052f, .009f}}, 24);
  b.ColorHex(0xd4a180);
  for (float side : {-1.0f, 1.0f}) {
    b.Ellipsoid({side * .055f, .13f, -.079f}, {.026f, .02f, .014f}, 12, 7);
    b.Ellipsoid({side * .097f, .142f, .004f}, {.015f, .029f, .02f}, 12, 6);
  }
  b.ColorHex(0xb67e60);
  b.Ellipsoid({0, .137f, -.099f}, {.015f, .023f, .018f}, 12, 7);
  b.ColorHex(0x9c745c);
  b.Ellipsoid({0, .078f, -.058f}, {.058f, .022f, .035f}, 16, 7);
  b.ColorHex(0xb68164);
  b.Ellipsoid({0, .09f, -.079f}, {.033f, .01f, .019f}, 12, 6);
  b.ColorHex(0x704a3c);
  Limb(b, {-.025f, .102f, -.092f}, {.025f, .102f, -.092f}, .0025f, .0025f, 8);
  // Kurze Haare am Hinterkopf und Schläfen.
  b.Mat(MAT_FABRIC); b.ColorHex(0x42362e);
  b.Push(Translation({0, .17f, .017f}) * RotationX(PI * .5f));
  b.Ellipsoid({0, 0, 0}, {.101f, .094f, .097f}, 20, 9, -.08f);
  b.Pop();
  // Ballistischer Helm mit Gummirand und seitlichen Schienen.
  b.ColorHex(0x38434a);
  b.Ellipsoid({0, .195f, .01f}, {.116f, .109f, .123f}, 24, 12, -.12f);
  b.Mat(MAT_LEATHER); b.ColorHex(0x20272d);
  b.Push(Translation({0, .184f, .01f}) * Scaling({1, .65f, 1.06f}));
  b.Torus({0, 0, 0}, .115f, .009f, 28, 8);
  b.Pop();
  b.ColorHex(0x505b60);
  for (float side : {-1.0f, 1.0f}) {
    b.Push(Translation({side * .109f, .217f, .016f}) * RotationY(side * PI * .5f));
    GearPanel(b, {-.045f, -.015f, -.006f}, {.045f, .015f, .007f}, .008f);
    b.Pop();
  }
  b.Mat(MAT_PLASTIC); b.ColorHex(0x222a30);
  GearPanel(b, {-.02f, .226f, -.121f}, {.02f, .268f, -.108f}, .008f);
  // Brillenfassung und zwei flache bernsteinfarbene Gläser, statt aufgesetzter Kugeln.
  GearPanel(b, {-.098f, .149f, -.111f}, {.098f, .196f, -.09f}, .017f);
  b.ColorHex(0x9c7842);
  for (float side : {-1.0f, 1.0f}) {
    float cx = side * .048f;
    GearPanel(b, {cx - .039f, .156f, -.116f}, {cx + .039f, .19f, -.11f}, .011f);
    b.ColorHex(0xc6a46a);
    Limb(b, {cx - .026f, .183f, -.117f}, {cx + .019f, .183f, -.117f}, .002f, .002f, 6);
    b.ColorHex(0x9c7842);
  }
  // Kinnriemen und Headset mit Gehäuse, Polster und kurzem Mikrofonarm.
  b.Mat(MAT_LEATHER); b.ColorHex(0x343b3b);
  for (float side : {-1.0f, 1.0f}) {
    Limb(b, {side * .096f, .18f, .015f}, {side * .063f, .066f, -.037f}, .008f, .007f, 8);
    Limb(b, {side * .063f, .066f, -.037f}, {side * .02f, .058f, -.067f}, .007f, .007f, 8);
    b.ColorHex(0x1e2529);
    b.Ellipsoid({side * .112f, .143f, .012f}, {.024f, .044f, .036f}, 14, 8);
    b.ColorHex(0x505b58);
    b.Ellipsoid({side * .128f, .145f, .012f}, {.014f, .034f, .027f}, 14, 8);
    b.ColorHex(0x343b3b);
  }
  Limb(b, {-.13f, .12f, -.009f}, {-.09f, .092f, -.098f}, .004f, .004f, 8);
  Limb(b, {-.09f, .092f, -.098f}, {-.035f, .098f, -.115f}, .004f, .004f, 8);
  b.ColorHex(0x20272b);
  b.Ellipsoid({-.033f, .098f, -.115f}, {.014f, .008f, .009f}, 10, 6);
  head.Upload(d);
  d.Clear();

  // --- Ärmel: Deltoid, Bizeps, Ellbogenfalten und Schulterpatch ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  TailoredForm(b, {{-kUpperArm, .058f, .059f}, {-.23f, .069f, .07f}, {-.13f, .086f, .083f},
                  {-.045f, .091f, .088f}, {.028f, .075f, .073f}, {.057f, .048f, .046f},
                  {.073f, .012f, .012f}});
  b.ColorHex(uniformD);
  GearPanel(b, {-.058f, -.16f, .071f}, {.058f, -.052f, .091f}, .019f);
  b.ColorHex(0x5c696a);
  GearPanel(b, {-.043f, -.14f, .092f}, {.043f, -.068f, .098f}, .014f);
  b.Mat(MAT_PLAIN); b.ColorHex(hiVis);
  b.Push(Translation({-PixelTextWidth("K", .006f) * .5f, -.123f, .099f}));
  BuildPixelText(b, "K", .006f, .002f);
  b.Pop();
  upperArm.Upload(d);
  d.Clear();

  // --- Unterarm: Manschette, matte Handschuhe und ausgeformte Finger ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  TailoredForm(b, {{-.225f, .046f, .044f}, {-.18f, .051f, .05f}, {-.09f, .067f, .067f},
                  {-.02f, .063f, .061f}, {.018f, .058f, .056f}, {.045f, .033f, .032f},
                  {.052f, .008f, .008f}});
  b.ColorHex(uniformD);
  OvalFrustum(b, {0, -.225f, 0}, .049f, .051f, .034f, 1.0f);
  b.Mat(MAT_LEATHER); b.ColorHex(0x2d3336);
  b.Ellipsoid({0, -.023f, .052f}, {.051f, .061f, .025f}, 14, 8);
  b.ColorHex(0x272c2f);
  b.Ellipsoid({0, -kForeArm, 0}, {.042f, .048f, .034f}, 14, 8);
  b.ColorHex(0x4c5555);
  GearPanel(b, {-.031f, -.288f, .026f}, {.031f, -.252f, .037f}, .009f);
  b.ColorHex(0x30373a);
  for (int i = 0; i < 4; i++) {
    float x = -.027f + i * .018f;
    b.Ellipsoid({x, -.303f, -.009f}, {.01f, .021f, .025f}, 8, 6);
  }
  b.Ellipsoid({.035f, -.26f, -.021f}, {.015f, .031f, .017f}, 10, 6);
  foreArm.Upload(d);
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
  // runder, leicht verjüngter Ärmel mit Falte in der Mitte
  float rm = (ra + rc) * 0.5f;
  b.Frustum(Vec3(0, 0, 0), ra, rm, len * 0.5f, 14);
  b.Frustum(Vec3(0, len * 0.47f, 0), rm * 1.06f, rm * 1.06f, len * 0.06f, 14);
  b.Frustum(Vec3(0, len * 0.5f, 0), rm, rc, len * 0.5f, 14);
  b.Pop();
}

// Arme mit Handschuhen: rechte Hand am Griff, linke Hand vorne (z = linke Handposition)
void BuildArms(MeshBuilder& b, Vec3 rightHand, Vec3 leftHand, bool leftVisible, bool rightVisible = true) {
  const u32 glove = 0x272c2f, sleeve = 0x39495d, cuff = 0x283647;
  if (rightVisible) {
  b.Mat(MAT_LEATHER); b.ColorHex(glove);
  b.Push(Translation(rightHand) * RotationX(-0.25f));
  b.Ellipsoid({0.002f, -0.052f, 0.006f}, {0.036f, 0.052f, 0.04f}, 12, 7);  // Handrücken/Ballen
  for (int i = 0; i < 4; i++)
    b.Ellipsoid({-.002f, -.025f - i * .022f, -.04f}, {.034f, .012f, .022f}, 12, 6);  // Finger um den Griff
  Limb(b, {0.03f, -0.035f, 0.0f}, {0.036f, 0.01f, -0.022f}, 0.013f, 0.011f, 8);  // Daumen
  b.ColorHex(0x4c5555);
  GearPanel(b, {-.024f, -.074f, .036f}, {.027f, -.032f, .049f}, .01f);
  b.Pop();
  b.Mat(MAT_FABRIC); b.ColorHex(cuff);
  Vec3 rw = rightHand + Vec3(0.008f, -0.1f, 0.05f);
  Tube(b, rw, rw + Vec3(0.02f, -0.03f, 0.05f), 0.036f, 0.038f);
  b.ColorHex(sleeve);
  Tube(b, rw + Vec3(0.015f, -0.02f, 0.04f), rw + Vec3(0.13f, -0.24f, 0.38f), 0.038f, 0.046f);
  }
  if (!leftVisible) return;
  b.Mat(MAT_LEATHER); b.ColorHex(glove);
  b.Ellipsoid(leftHand + Vec3(-0.003f, -0.027f, 0.0f), {0.04f, 0.022f, 0.05f}, 12, 6);  // Handfläche unter dem Vorderschaft
  for (int i = 0; i < 4; i++)
    Limb(b, leftHand + Vec3(-0.04f, -0.02f, -0.035f + i * 0.022f), leftHand + Vec3(-0.042f, 0.026f, -0.033f + i * 0.022f), 0.0105f, 0.0095f, 8);
  Limb(b, leftHand + Vec3(0.03f, -0.025f, 0.02f), leftHand + Vec3(0.04f, 0.016f, -0.01f), 0.012f, 0.0105f, 8);  // Daumen
  b.Mat(MAT_FABRIC); b.ColorHex(cuff);
  Vec3 lw = leftHand + Vec3(-0.01f, -0.04f, 0.05f);
  Tube(b, lw, lw + Vec3(-0.03f, -0.03f, 0.05f), 0.035f, 0.037f);
  b.ColorHex(sleeve);
  Tube(b, lw + Vec3(-0.025f, -0.02f, 0.04f), lw + Vec3(-0.2f, -0.24f, 0.3f), 0.037f, 0.045f);
}

void BuildViewModels(Mesh& gren, Mesh& hands, Mesh& flash) {
  MeshData d;
  MeshBuilder b(&d);
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
  BuildArms(b, Vec3(0, 0.05f, -0.03f), Vec3(), false);
  hands.Upload(d); d.Clear();
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
  BuildMeshes(torso_, head_, thigh_, shin_, upperArm_, foreArm_, grenade_);
  BuildViewModels(vmGrenade_, grenadeHands_, flash_);
  weapons_.Init();
  MeshData d; MeshBuilder b(&d);
  BuildArms(b, {}, {}, false); rightHand_.Upload(d); d.Clear();
  BuildArms(b, {}, {}, true, false); leftHand_.Upload(d); d.Clear();
  b.Mat(MAT_METAL); b.ColorHex(0x1d2931); b.Box({-8,-.03f,-8},{8,0,8});
  b.ColorHex(0x354650);
  for(int i=-16;i<=16;i++) { float q=i*.25f; b.Box({q,0,-4},{q+.001f,.0005f,4}); b.Box({-4,0,q},{4,.0005f,q+.001f}); }
  b.Mat(MAT_PLAIN); b.ColorHex(0x101c29); b.Box({-8,0,-3.05f},{8,5,-3});
  bench_.Upload(d); d.Clear();
  b.Mat(MAT_GOLD); b.ColorHex(0xb7a16a); b.CylinderZ({},.007f,.028f,10); casing_.Upload(d);
}

void GameView::LoadWorld(Renderer& r, const World& w) {
  wr_.Load(r, w);
  fx_.clear();
  lights_.clear();
  casings_.clear();
  aimBlend_ = 0; cameraRecoil_ = {}; lastWeapon_ = -1;
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
    case EV_SHOT: {
      WeaponStats stats = GetWeaponStats(e.b, e.c);
      Vec3 right = RightFromYaw(YawFromDir(e.dir));
      casings_.push_back({e.pos + right * .16f + e.dir * .27f - Vec3(0,.12f,0),
                         right * rng_.Range(1.5f,2.7f) + Vec3(0,1.5f,0), 1.8f,
                         e.b == WPN_SHOTGUN ? .23f : 0, rng_.Range(0,TAU)});
      if (e.a == localId) {
        recoil_ = stats.recoil;
        flashTimer_ = stats.suppressed ? .015f : .045f;
        Shake((e.b == WPN_SHOTGUN ? .07f : .012f) * stats.recoil);
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
        Burst(muzzle, stats.suppressed ? 1 : 4, 2.0f, Vec4(6, 4, 1.5f, 1), Vec4(2, 1, 0.2f, 0), 0.06f, 0.05f, true, 0);
      }
      if (!stats.suppressed) AddLight(e.pos + e.dir * .8f, 5.0f, Vec3(4.0f, 2.8f, 1.4f), .045f);
      break;
    }
    case EV_IMPACT: {
      bool metal = e.b == MAT_METAL || e.b == MAT_MIRROR || e.b == MAT_GOLD;
      if (e.c) Burst(e.pos, 7, 2.5f, V4(0x8a5a33, 1), V4(0x5a3a23, 0), 0.025f, 0.6f, false, 9.0f);
      else Burst(e.pos, 6, 1.5f, V4(0xb8b0a0, 0.8f), V4(0x8a8478, 0), 0.05f, 0.5f, false, 3.0f, 0.2f);
      if (metal) Burst(e.pos, 6, 4.0f, Vec4(6, 4, 1.5f, 1), Vec4(3, 1, 0.2f, 0), 0.015f, 0.25f, true, 12.0f);
      if (renderer_ && e.b != MAT_WATER) renderer_->AddDecal(e.pos, e.dir, e.b == MAT_GLASS ? 0.05f : 0.035f, e.b == MAT_GLASS ? Vec4(0.85f, 0.9f, 0.95f, 0.5f) : Vec4(0.04f, 0.04f, 0.04f, 0.92f));
      break;
    }
    case EV_HIT:
      if (e.c == KILL_RAM || e.c == KILL_CRUSH) {
        Burst(e.pos, 18, 4.5f, V4(0xffd34e, 1), V4(0xff9256, 0), .055f, .55f, true, 7);
        Smoke(e.pos, 10, .4f, Vec4(.8f, .72f, .55f, .55f));
      } else Burst(e.pos, 10, 3.0f, V4(0xa87a4a, 1), V4(0x6b4a2e, 0), 0.03f, 0.7f, false, 9.0f);
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
  frameDt_ = Clamp(dt, .001f, .1f);
  for (auto& c : casings_) {
    if(c.delay > 0) { c.delay -= dt; continue; }
    c.life -= dt; c.vel.y -= 9.8f * dt; c.pos += c.vel * dt; c.spin += dt * 13;
  }
  casings_.erase(std::remove_if(casings_.begin(),casings_.end(),[](const Casing& c){return c.life<=0;}),casings_.end());
  if(casings_.size()>96) casings_.erase(casings_.begin(),casings_.begin()+casings_.size()-96);
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

// Zweigelenk-IK: Ellbogen e so, dass Schulter s -> e -> Hand h die Längen la, lb hat; pole gibt die Beugerichtung vor
static void SolveArm(Vec3 s, Vec3& h, Vec3 pole, float la, float lb, Vec3& e) {
  Vec3 d = h - s;
  float dist = Clamp(Length(d), fabsf(la - lb) + 1e-3f, la + lb - 1e-3f);
  Vec3 dir = Normalize(d);
  h = s + dir * dist;
  float cosA = Clamp((la * la + dist * dist - lb * lb) / (2 * la * dist), -1.0f, 1.0f);
  float sinA = sqrtf(std::max(0.0f, 1 - cosA * cosA));
  Vec3 pp = pole - dir * Dot(pole, dir);
  pp = Length(pp) < 1e-4f ? Vec3(0, -1, 0) : Normalize(pp);
  e = s + dir * (cosA * la) + pp * (sinA * la);
}

void GameView::DrawHunter(Renderer& r, const ClientPlayer& p, int id, float sky) {
  float speed = sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z);
  float amt = Saturate(speed / 4.0f);
  float ph = walkPhase_[id];
  bool crouch = p.crouching;
  float hipY = crouch ? 0.52f : 0.9f - 0.035f * amt * fabsf(sinf(ph));
  Mat4 base = Translation(p.pos) * RotationY(p.yaw);
  DrawItem it;
  it.sky = sky;
  // Beine: Oberschenkel schwingt, das Knie beugt sich beim Vorschwingen; geduckt in der Hocke
  for (int side = 0; side < 2; side++) {
    float phase = ph + (side ? PI : 0.0f);
    float thighA, knee;
    if (crouch) {
      thighA = 1.05f + (side ? 0.12f : -0.08f) + 0.25f * sinf(phase) * amt;
      knee = 1.95f + (side ? 0.08f : -0.04f);
    } else {
      thighA = sinf(phase) * 0.55f * amt;
      knee = amt * (0.12f + 0.85f * std::max(0.0f, cosf(phase)));
    }
    Mat4 hip = base * Translation({side ? 0.1f : -0.1f, hipY, 0}) * RotationX(thighA);
    it.mesh = &thigh_;
    it.model = hip;
    r.Draw(it);
    it.mesh = &shin_;
    it.model = hip * Translation({0, -kThigh, 0}) * RotationX(-knee);
    r.Draw(it);
  }
  float lean = crouch ? -0.38f : -0.06f * amt;
  Mat4 torsoM = base * Translation({0, hipY, 0}) * RotationX(lean) * RotationY(0.06f * sinf(ph) * amt);
  it.mesh = &torso_;
  it.model = torsoM;
  r.Draw(it);
  float pitch = Clamp(p.pitch, -1.0f, 1.0f);
  it.mesh = &head_;
  it.model = torsoM * Translation({0, 0.56f, 0}) * RotationX(Clamp(p.pitch, -0.8f, 0.8f) * 0.6f - lean * 0.8f);
  r.Draw(it);
  // Waffe vor der Brust, zielt unabhängig von der Rumpfneigung in Blickrichtung
  u16 build=p.weapon<FIREARM_COUNT?p.builds[p.weapon]:0;
  WeaponPose pose=MakeWeaponPose(p.weapon,build,p.reloadTimer,p.reloadDuration,p.fireTimer,p.ammo[p.weapon]==0);
  Mat4 gunM = Translation({0.12f, 0.44f, -0.14f}) * RotationX(pitch - lean + pose.reload*.15f) * RotationZ(-pose.reload*.35f);
  Vec3 gripR(0, -0.075f, 0.012f), gripL(0, -0.04f, -0.24f);
  bool gun = p.weapon < WPN_GRENADE;
  if (p.weapon == WPN_SHOTGUN) {
    gripR = Vec3(0, -0.07f, 0.06f);
    gripL = Vec3(0, -0.03f, -0.34f);
  } else if (p.weapon == WPN_GRENADE) {
    gun = false;
    gripR = Vec3(0.04f, -0.06f, -0.1f);
    gripL = Vec3(-0.2f, -0.12f, -0.02f);
  }
  if(p.weapon<FIREARM_COUNT) { gripR={0,-.075f,.035f}; gripL=pose.leftHand; }
  // Arme per IK an Griff und Vorderschaft, Ellbogen nach unten außen
  struct ArmDef {
    Vec3 shoulder, grip, pole;
  };
  const ArmDef arms[2] = {{Vec3(0.205f, 0.5f, 0), gripR, Vec3(0.9f, -1, 0.35f)}, {Vec3(-0.205f, 0.5f, 0), gripL, Vec3(-0.9f, -1, 0.2f)}};
  for (const ArmDef& a : arms) {
    Vec3 hand = TransformPoint(gunM, a.grip), elbow;
    SolveArm(a.shoulder, hand, a.pole, kUpperArm, kForeArm, elbow);
    Vec3 out = Normalize(elbow - (a.shoulder + hand) * 0.5f);
    it.mesh = &upperArm_;
    it.model = torsoM * BoneBasis(a.shoulder, elbow, out);
    r.Draw(it);
    it.mesh = &foreArm_;
    it.model = torsoM * BoneBasis(elbow, hand, out);
    r.Draw(it);
    if (!gun && &a == &arms[0]) {
      it.mesh = &grenade_;
      it.model = torsoM * Translation(hand + Vec3(0, 0.02f, -0.03f));
      r.Draw(it);
    }
  }
  if (gun) weapons_.Draw(r,p.weapon,build,torsoM*gunM,sky,false,pose);
}

void GameView::DrawHunterPreview(Renderer& r, Vec3 pos, float yaw, float pitch, float walkPhase, float speed, bool crouch,
                                 int weapon, float sky) {
  ClientPlayer p;
  p.pos = pos;
  p.yaw = yaw;
  p.pitch = pitch;
  p.vel = Vec3(-sinf(yaw), 0, -cosf(yaw)) * speed;
  p.crouching = crouch;
  p.weapon = (u8)weapon;
  p.team = TEAM_HUNTERS;
  p.alive = true;
  walkPhase_[0] = walkPhase;
  DrawHunter(r, p, 0, sky);
}

void GameView::DrawViewModel(Renderer& r, const ClientPlayer& me, const ViewInput& in, float sky) {
  if(me.weapon != lastWeapon_) { switchAnim_=1; lastWeapon_=me.weapon; lastYaw_=in.yaw; lastPitch_=in.pitch; sway_={}; }
  int weapon=ClampI(me.weapon,0,WPN_COUNT-1); u16 build=weapon<FIREARM_COUNT?me.builds[weapon]:0;
  float speed=sqrtf(me.vel.x*me.vel.x+me.vel.z*me.vel.z);
  bool sprint=(me.lastButtons & BTN_SPRINT) && !(me.lastButtons & (BTN_AIM|BTN_FIRE)) && speed>4.5f;
  sprintBlend_=Damp(sprintBlend_,sprint?1.0f:0.0f,10,frameDt_);
  sway_.x=Damp(sway_.x,Clamp(WrapAngle(in.yaw-lastYaw_)/frameDt_,-3.0f,3.0f)*.012f,8,frameDt_);
  sway_.y=Damp(sway_.y,Clamp((in.pitch-lastPitch_)/frameDt_,-3.0f,3.0f)*.010f,8,frameDt_);
  lastYaw_=in.yaw; lastPitch_=in.pitch;
  float bobAmt=me.onGround?Saturate(speed/5):.15f;
  Vec3 base=Lerp(Vec3(weapon==WPN_PISTOL?.13f:.19f,-.18f,weapon==WPN_PISTOL?-.48f:-.44f),Vec3(0,-WeaponSightHeight(weapon,build)*.80f,weapon==WPN_PISTOL?-.40f:-.33f),aimBlend_);
  Vec3 bob(sinf(bobPhase_)*.007f*bobAmt,-fabsf(cosf(bobPhase_))*.009f*bobAmt,0);
  bob*=1-aimBlend_;  // sight axis remains aligned with the authoritative shot during ADS
  WeaponPose pose=MakeWeaponPose(weapon,build,me.reloadTimer,me.reloadDuration,me.fireTimer,me.ammo[weapon]==0);
  float sw=std::max(switchAnim_,Saturate(me.switchTimer*2.5f));
  Vec3 off=base+bob+Vec3(-sway_.x*(1-aimBlend_),sway_.y*(1-aimBlend_)-.055f*pose.reload-.32f*sw-.09f*sprintBlend_,.045f*recoil_);
  Mat4 camM=Translation(cam_.pos)*RotationY(cam_.yaw)*RotationX(cam_.pitch);
  Mat4 model=camM*Translation(off)*RotationX(.045f*recoil_*(1-aimBlend_*.85f)+.15f*pose.reload-.30f*sprintBlend_)*
             RotationZ(-.45f*pose.reload+.45f*sprintBlend_)*Scaling(Vec3(.80f));
  DrawItem it; it.viewModel=true; it.castShadow=false; it.sky=sky; it.model=model;
  if(weapon<FIREARM_COUNT) {
    weapons_.Draw(r,weapon,build,model,sky,true,pose);
    it.mesh=&rightHand_; it.model=model*Translation({0,-.025f,.035f}); r.Draw(it);
    it.mesh=&leftHand_; it.model=model*Translation(pose.leftHand); r.Draw(it);
  } else {
    if(me.ammo[WPN_GRENADE]) { it.mesh=&vmGrenade_; r.Draw(it); }
    it.mesh=&grenadeHands_; r.Draw(it);
  }
  if(flashTimer_>0 && weapon<FIREARM_COUNT) {
    it.mesh=&flash_; float scale=GetWeaponStats(weapon,build).suppressed?.12f:.55f;
    it.model=model*Translation({0,.025f,WeaponMuzzleZ(weapon,build)-.025f})*RotationZ(rng_.Range(0,TAU))*Scaling(Vec3(scale)); r.Draw(it);
  }
}

void GameView::RenderWorkbench(Renderer& r,int weapon,u16 build,float angle,float elevation,int slot,float time,int width,int height) {
  Camera cam; cam.pos={0,.75f,1.8f}; cam.pitch=-.22f; cam.fovDeg=42;
  Environment env; env.sunDir=Normalize(Vec3(-.4f,.8f,.5f)); env.sunColor=Vec3(2.3f,2.5f,2.7f);
  env.skyZenith=Hex(0x111b25); env.skyHorizon=Hex(0x263747); env.skyAmbient=Vec3(.55f,.65f,.75f);
  env.groundAmbient=Vec3(.26f,.32f,.38f); env.fogDensity=.008f; env.exposure=1.15f;
  r.Begin(cam,env,width,height,time); r.ShowWorld(false); cam_=r.Cam();
  DrawItem table; table.mesh=&bench_; r.Draw(table);
  // Move the inspection model into the open right side of the panel layout.
  float scale=weapon==WPN_PISTOL?2.0f:weapon==WPN_SHOTGUN?1.00f:1.25f;
  Mat4 model=Translation({.30f,weapon==WPN_PISTOL?.51f:.46f,0})*RotationY(angle)*RotationZ(elevation)*Scaling(Vec3(scale));
  weapons_.Draw(r,weapon,build,model,1,false,WeaponPose(),slot);
  r.SetLights({{{-.7f,1.4f,.7f},3,Vec3(2.8f,2.3f,1.7f)},{{1.1f,.8f,-.8f},3,Vec3(1.4f,2.2f,3.0f)}});
  ScreenFX fx; fx.vignette=.38f; r.Render(fx);
}

void GameView::Render(Renderer& r, const ClientState& s, int localId, const World& w, const ViewInput& in, float time,
                      int width, int height, ScreenFX& fx) {
  const ClientPlayer* me = (localId >= 0 && localId < MAX_PLAYERS) ? &s.players[localId] : nullptr;
  float dt = frameDt_;
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
    WeaponStats stats = GetWeaponStats(me->weapon, me->weapon < FIREARM_COUNT ? me->builds[me->weapon] : 0);
    aimBlend_ = Damp(aimBlend_, me->aimProgress, 24.0f, dt);
    cam.fovDeg = Lerp(in.fov, in.fov / stats.zoom, aimBlend_);
    cameraRecoil_.x = Damp(cameraRecoil_.x, me->recoilYaw, 35, dt);
    cameraRecoil_.y = Damp(cameraRecoil_.y, me->recoilPitch, 35, dt);
    cam.yaw += cameraRecoil_.x; cam.pitch += cameraRecoil_.y;
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
  for(const auto& c : casings_) if(c.delay<=0) {
    DrawItem shell; shell.mesh=&casing_; shell.model=Translation(c.pos)*RotationX(c.spin)*RotationZ(c.spin*.7f);
    shell.sky=w.SkyAt(c.pos); r.Draw(shell);
  }
  lightBuf_ = w.lights;
  for (const FxLight& l : lights_) {
    float k = l.life / l.maxLife;
    lightBuf_.push_back({l.pos, l.radius, l.color * k});
  }
  if (flashTimer_ > .02f && firstPerson) lightBuf_.push_back({cam_.pos + cam_.Forward() * 0.8f, 5.0f, Vec3(3.5f, 2.5f, 1.2f)});
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
