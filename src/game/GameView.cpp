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

void BuildMeshes(Mesh& torso, Mesh& head, Mesh& thigh, Mesh& shin, Mesh& upperArm, Mesh& foreArm, Mesh& smg3p, Mesh& shotgun3p,
                 Mesh& grenade) {
  const u32 uniform = 0x1f2a44, uniformD = 0x18213a, vest = 0x2a3142, vestL = 0x353d50, black = 0x141518, skin = 0xd9a684;
  const u32 hiVis = 0xd8e030;
  MeshData d;
  MeshBuilder b(&d);

  // --- Oberschenkel (Hüfte im Ursprung, nach unten) ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  b.Ellipsoid({0, -0.02f, 0}, {0.09f, 0.08f, 0.095f}, 12, 6);
  b.Frustum({0, -kThigh, 0}, 0.07f, 0.088f, kThigh, 12);
  b.ColorHex(uniformD);  // Beintasche außen
  b.Box({-0.095f, -0.3f, -0.05f}, {0.095f, -0.17f, 0.04f});
  b.Mat(MAT_PLASTIC); b.ColorHex(black);  // Knieschoner
  b.Ellipsoid({0, -kThigh + 0.005f, -0.05f}, {0.062f, 0.07f, 0.035f}, 10, 6);
  thigh.Upload(d);
  d.Clear();

  // --- Unterschenkel mit Stiefel (Knie im Ursprung, Sohle bei -0.47) ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  b.Ellipsoid({0, 0, 0}, {0.07f, 0.05f, 0.07f}, 10, 5);
  b.Frustum({0, -0.36f, 0}, 0.058f, 0.07f, 0.36f, 12);
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  b.Frustum({0, -0.44f, 0}, 0.066f, 0.062f, 0.13f, 12);              // Schaft
  b.Ellipsoid({0, -0.43f, -0.06f}, {0.066f, 0.045f, 0.125f}, 12, 6);  // Fuß
  b.ColorHex(0x0b0b0c);
  b.Box({-0.068f, -0.47f, -0.19f}, {0.068f, -0.44f, 0.075f});        // Sohle
  shin.Upload(d);
  d.Clear();

  // --- Rumpf (Hüfte im Ursprung, Hals bei 0.56) ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  OvalFrustum(b, {0, 0, 0}, 0.165f, 0.16f, 0.16f, 0.72f);         // Becken
  OvalFrustum(b, {0, 0.15f, 0}, 0.16f, 0.215f, 0.33f, 0.62f);     // Brustkorb
  b.Ellipsoid({0, 0.48f, 0}, {0.235f, 0.08f, 0.13f}, 16, 7);      // Schultern
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  OvalFrustum(b, {0, 0.12f, 0}, 0.172f, 0.172f, 0.055f, 0.74f);   // Gürtel
  b.Mat(MAT_GOLD); b.ColorHex(0xb0b4b8);
  b.Box({-0.028f, 0.128f, -0.135f}, {0.028f, 0.168f, -0.122f});   // Schnalle
  b.Mat(MAT_LEATHER); b.ColorHex(0x1d1e22);
  b.Box({0.15f, 0.02f, -0.06f}, {0.2f, 0.17f, 0.05f});            // Holster
  b.Box({-0.19f, 0.06f, -0.03f}, {-0.155f, 0.15f, 0.06f});        // Gürteltasche
  // Plattenträger: Bauchband, Front- und Rückenplatte, Schultergurte
  b.Mat(MAT_FABRIC); b.ColorHex(vest);
  OvalFrustum(b, {0, 0.19f, 0}, 0.2f, 0.208f, 0.17f, 0.66f);
  b.Box({-0.17f, 0.19f, -0.165f}, {0.17f, 0.47f, -0.1f});
  b.Box({-0.17f, 0.19f, 0.1f}, {0.17f, 0.47f, 0.16f});
  for (float sx : {-1.0f, 1.0f}) b.Box({sx > 0 ? 0.08f : -0.15f, 0.45f, -0.14f}, {sx > 0 ? 0.15f : -0.08f, 0.545f, 0.14f});
  // Magazintaschen vorn, Funkgerät links
  b.ColorHex(vestL);
  for (int i = 0; i < 3; i++) {
    float x0 = -0.13f + i * 0.09f;
    b.Box({x0, 0.2f, -0.205f}, {x0 + 0.075f, 0.32f, -0.16f});
    b.ColorHex(0x2f3646);
    b.Box({x0 - 0.002f, 0.29f, -0.209f}, {x0 + 0.077f, 0.325f, -0.2f});  // Klappe
    b.ColorHex(vestL);
  }
  b.Box({-0.15f, 0.34f, -0.2f}, {-0.07f, 0.44f, -0.16f});
  b.Mat(MAT_PLASTIC); b.ColorHex(black);
  b.Box({-0.14f, 0.35f, -0.215f}, {-0.08f, 0.43f, -0.2f});
  b.Cylinder({-0.09f, 0.43f, -0.19f}, 0.006f, 0.12f, 6);           // Antenne
  // Reflexstreifen rundum und Schriftzug auf dem Rücken
  b.Mat(MAT_PLASTIC); b.ColorHex(hiVis); b.Emissive(0.12f);
  OvalFrustum(b, {0, 0.255f, 0}, 0.212f, 0.212f, 0.03f, 0.67f);
  b.Box({-0.17f, 0.44f, -0.169f}, {0.17f, 0.455f, -0.164f});
  b.Push(Translation({-PixelTextWidth("SUCHER", 0.0095f) * 0.5f, 0.33f, 0.161f}));
  BuildPixelText(b, "SUCHER", 0.0095f, 0.004f);
  b.Pop();
  b.Emissive(0);
  b.Mat(MAT_PLASTIC); b.ColorHex(skin);
  b.Cylinder({0, 0.5f, 0}, 0.052f, 0.09f, 10);                    // Hals
  torso.Upload(d);
  d.Clear();

  // --- Kopf (Halsansatz im Ursprung) ---
  b.Mat(MAT_PLASTIC); b.ColorHex(skin);
  b.Cylinder({0, 0, 0.005f}, 0.05f, 0.08f, 10);
  b.Ellipsoid({0, 0.165f, 0}, {0.094f, 0.118f, 0.104f}, 16, 10);
  b.Ellipsoid({0, 0.1f, -0.03f}, {0.074f, 0.058f, 0.074f}, 12, 6);   // Kiefer
  b.Ellipsoid({0, 0.152f, -0.102f}, {0.016f, 0.027f, 0.02f}, 8, 5);  // Nase
  for (float sx : {-1.0f, 1.0f}) b.Ellipsoid({sx * 0.093f, 0.158f, 0.0f}, {0.014f, 0.03f, 0.022f}, 8, 5);  // Ohren
  b.ColorHex(0xf2f0ea);
  for (float sx : {-1.0f, 1.0f}) b.Ellipsoid({sx * 0.034f, 0.178f, -0.086f}, {0.017f, 0.011f, 0.01f}, 8, 5);
  b.ColorHex(0x26201c);
  for (float sx : {-1.0f, 1.0f}) b.Sphere({sx * 0.034f, 0.178f, -0.095f}, 0.0072f, 8, 5);
  b.ColorHex(0x3a2a20);
  for (float sx : {-1.0f, 1.0f}) b.Box({sx * 0.034f - 0.017f, 0.196f, -0.1f}, {sx * 0.034f + 0.017f, 0.204f, -0.09f});  // Brauen
  b.ColorHex(0x9a5a48);
  b.Box({-0.018f, 0.112f, -0.1f}, {0.018f, 0.117f, -0.094f});  // Mund
  // Haare an Hinterkopf und Seiten (hintere Hälfte einer etwas größeren Ellipse)
  b.Mat(MAT_FABRIC); b.ColorHex(0x2e2118);
  b.Push(Translation({0, 0.168f, 0.012f}) * RotationX(PI * 0.5f));
  b.Ellipsoid({0, 0, 0}, {0.099f, 0.104f, 0.118f}, 14, 7, -0.2f);
  b.Pop();
  // Helm mit Rand, Brillenband, Schutzbrille, Halterung
  b.Mat(MAT_PLASTIC); b.ColorHex(0x1a2235);
  b.Ellipsoid({0, 0.19f, 0.006f}, {0.124f, 0.122f, 0.134f}, 18, 9, 0.0f);
  b.Push(Translation({0, 0.188f, 0.006f}) * Scaling(Vec3(1, 1, 1.08f)));
  b.Torus({0, 0, 0}, 0.124f, 0.009f, 24, 6);
  b.Pop();
  b.ColorHex(0x101114);
  b.Push(Translation({0, 0.235f, 0.006f}) * Scaling(Vec3(1, 1, 1.08f)));
  b.Torus({0, 0, 0}, 0.118f, 0.007f, 24, 5);
  b.Pop();
  b.Box({-0.018f, 0.27f, -0.142f}, {0.018f, 0.3f, -0.118f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0xff9a30); b.Emissive(0.15f);
  for (float sx : {-1.0f, 1.0f}) b.Ellipsoid({sx * 0.042f, 0.245f, -0.124f}, {0.032f, 0.021f, 0.016f}, 10, 6);
  b.Emissive(0);
  // Headset mit Mikrofon
  b.ColorHex(0x16171a);
  for (float sx : {-1.0f, 1.0f}) b.CylinderX({sx * 0.112f, 0.155f, 0.0f}, 0.04f, 0.03f, 12);
  Limb(b, {-0.11f, 0.13f, -0.025f}, {-0.045f, 0.105f, -0.098f}, 0.005f, 0.005f, 6);
  b.Sphere({-0.042f, 0.104f, -0.1f}, 0.011f, 8, 5);
  head.Upload(d);
  d.Clear();

  // --- Oberarm (Schulter im Ursprung, Ellbogen bei -kUpperArm, +Z = Ellbogen-Außenseite) ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  b.Ellipsoid({0, -0.01f, 0}, {0.07f, 0.075f, 0.07f}, 12, 6);
  b.Frustum({0, -kUpperArm, 0}, 0.052f, 0.062f, kUpperArm, 12);
  b.ColorHex(uniformD);
  b.Box({-0.064f, -0.12f, -0.035f}, {0.064f, -0.06f, 0.035f});  // Ärmeltasche/Abzeichen
  b.Mat(MAT_PLASTIC); b.ColorHex(hiVis); b.Emissive(0.1f);
  b.Box({-0.066f, -0.105f, -0.02f}, {0.066f, -0.075f, 0.02f});
  b.Emissive(0);
  upperArm.Upload(d);
  d.Clear();

  // --- Unterarm mit Handschuh (Ellbogen im Ursprung, Handmitte bei -kForeArm) ---
  b.Mat(MAT_FABRIC); b.ColorHex(uniform);
  b.Ellipsoid({0, 0, 0}, {0.055f, 0.05f, 0.055f}, 10, 5);
  b.Frustum({0, -0.21f, 0}, 0.043f, 0.052f, 0.21f, 12);
  b.Mat(MAT_PLASTIC); b.ColorHex(black);
  b.Ellipsoid({0, -0.005f, 0.035f}, {0.048f, 0.055f, 0.03f}, 10, 5);  // Ellbogenschoner
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  b.Frustum({0, -0.235f, 0}, 0.046f, 0.046f, 0.03f, 12);              // Stulpe
  b.Ellipsoid({0, -kForeArm, 0}, {0.038f, 0.05f, 0.03f}, 10, 6);     // Hand
  b.Ellipsoid({0.03f, -kForeArm + 0.015f, -0.012f}, {0.014f, 0.032f, 0.014f}, 8, 5);  // Daumen
  foreArm.Upload(d);
  d.Clear();

  // --- MP in dritter Person (Ursprung = Drehpunkt, vorne = -Z) ---
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.Box({-0.026f, -0.03f, -0.3f}, {0.026f, 0.035f, 0.06f});
  b.ColorHex(0x1f2124);
  b.Box({-0.01f, 0.035f, -0.25f}, {0.01f, 0.047f, 0.03f});
  b.Mat(MAT_PLASTIC); b.ColorHex(0x161719);
  b.Box({-0.03f, -0.05f, -0.3f}, {-0.0f + 0.03f, 0.018f, -0.15f});
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.CylinderZ({0, 0.002f, -0.37f}, 0.01f, 0.14f, 10);
  b.CylinderZ({0, 0.002f, -0.45f}, 0.016f, 0.06f, 10);
  b.ColorHex(0x1b1c1f);
  b.Push(Translation({0, -0.03f, -0.1f}) * RotationX(0.2f));
  b.Box({-0.015f, -0.15f, -0.024f}, {0.015f, 0.0f, 0.024f});  // Magazin
  b.Pop();
  b.Push(Translation({0, -0.03f, 0.02f}) * RotationX(-0.25f));
  b.Box({-0.018f, -0.1f, -0.024f}, {0.018f, 0.0f, 0.022f});   // Griff
  b.Pop();
  b.ColorHex(0x2a2c30);
  b.Box({-0.012f, -0.02f, 0.06f}, {0.012f, 0.0f, 0.24f});     // Schulterstütze
  b.Box({-0.02f, -0.06f, 0.21f}, {0.02f, 0.02f, 0.25f});
  b.ColorHex(0x111214);
  b.Box({-0.016f, 0.047f, -0.08f}, {0.016f, 0.085f, -0.02f});  // Rotpunktvisier
  b.Mat(MAT_PLASTIC); b.ColorHex(0xff3020); b.Emissive(1.0f);
  b.Box({-0.004f, 0.062f, -0.083f}, {0.004f, 0.07f, -0.079f});
  b.Emissive(0);
  smg3p.Upload(d);
  d.Clear();

  // --- Schrotflinte in dritter Person ---
  b.Mat(MAT_METAL); b.ColorHex(0x26282b);
  b.Box({-0.028f, -0.035f, -0.2f}, {0.028f, 0.035f, 0.07f});
  b.CylinderZ({0, 0.015f, -0.44f}, 0.016f, 0.5f, 10);
  b.CylinderZ({0, -0.016f, -0.4f}, 0.014f, 0.4f, 10);
  b.Mat(MAT_WOOD); b.ColorHex(0x6b4a2e);
  b.CylinderZ({0, -0.016f, -0.38f}, 0.026f, 0.15f, 12);      // Pumpe
  b.Push(Translation({0, -0.03f, 0.05f}) * RotationX(-0.3f));
  b.Box({-0.02f, -0.1f, -0.026f}, {0.02f, 0.0f, 0.024f});
  b.Pop();
  b.Push(Translation({0, -0.02f, 0.07f}) * RotationX(-0.12f));
  b.Box({-0.022f, -0.045f, 0.0f}, {0.022f, 0.025f, 0.26f});   // Schaft
  b.Pop();
  b.Mat(MAT_LEATHER); b.ColorHex(black);
  b.Box({-0.024f, -0.08f, 0.31f}, {0.024f, 0.01f, 0.33f});
  shotgun3p.Upload(d);
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
void BuildArms(MeshBuilder& b, Vec3 rightHand, Vec3 leftHand, bool leftVisible) {
  const u32 glove = 0x1c1d20, sleeve = 0x23304d, cuff = 0x2c3a5c;
  b.Mat(MAT_LEATHER); b.ColorHex(glove);
  b.Push(Translation(rightHand) * RotationX(-0.25f));
  b.Ellipsoid({0.002f, -0.052f, 0.006f}, {0.036f, 0.052f, 0.04f}, 12, 7);  // Handrücken/Ballen
  b.Ellipsoid({-0.002f, -0.06f, -0.04f}, {0.034f, 0.05f, 0.021f}, 12, 7);  // Finger um den Griff
  Limb(b, {0.03f, -0.035f, 0.0f}, {0.036f, 0.01f, -0.022f}, 0.013f, 0.011f, 8);  // Daumen
  b.Pop();
  b.Mat(MAT_FABRIC); b.ColorHex(cuff);
  Vec3 rw = rightHand + Vec3(0.008f, -0.1f, 0.05f);
  Tube(b, rw, rw + Vec3(0.02f, -0.03f, 0.05f), 0.036f, 0.038f);
  b.ColorHex(sleeve);
  Tube(b, rw + Vec3(0.015f, -0.02f, 0.04f), rw + Vec3(0.13f, -0.24f, 0.38f), 0.038f, 0.046f);
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

void BuildViewModels(Mesh& smg, Mesh& shotgun, Mesh& gren, Mesh* hands, Mesh& flash) {
  MeshData d;
  MeshBuilder b(&d);
  // --- MP (vorne = -Z, Ursprung über dem Griff) ---
  const u32 steel = 0x16181b, poly = 0x1b1c1f, polyD = 0x121315;
  // Oberbau (Stahl, oben gerundet) und Unterbau (Kunststoff)
  b.Mat(MAT_METAL); b.ColorHex(steel);
  b.Box({-0.025f, -0.006f, -0.2f}, {0.025f, 0.034f, 0.075f});
  b.CylinderZ({0, 0.03f, -0.0625f}, 0.022f, 0.275f, 14);
  b.Mat(MAT_PLASTIC); b.ColorHex(poly);
  b.Box({-0.028f, -0.046f, -0.17f}, {0.028f, -0.004f, 0.07f});
  b.ColorHex(polyD);
  b.Box({-0.029f, -0.03f, -0.13f}, {0.029f, -0.024f, -0.02f});  // Trennfuge
  // Handschutz achteckig mit Schlitzen
  b.ColorHex(poly);
  b.CylinderZ({0, -0.006f, -0.265f}, 0.031f, 0.13f, 8);
  b.ColorHex(polyD);
  for (int i = 0; i < 4; i++)
    for (float sx : {-1.0f, 1.0f}) b.Box({sx * 0.03f - 0.002f, -0.002f, -0.31f + i * 0.026f}, {sx * 0.03f + 0.002f, 0.008f, -0.296f + i * 0.026f});
  // Lauf und Mündungsaufsatz
  b.Mat(MAT_METAL); b.ColorHex(steel);
  b.CylinderZ({0, 0.004f, -0.36f}, 0.0105f, 0.08f, 12);
  b.CylinderZ({0, 0.004f, -0.42f}, 0.016f, 0.07f, 12);
  b.ColorHex(0x060708);
  for (int i = 0; i < 3; i++) b.Box({-0.0165f, 0.0f, -0.445f + i * 0.018f}, {0.0165f, 0.008f, -0.437f + i * 0.018f});
  // Schiene mit Zähnen
  b.ColorHex(0x1f2124);
  b.Box({-0.011f, 0.046f, -0.2f}, {0.011f, 0.055f, 0.06f});
  for (int i = 0; i < 9; i++) b.Box({-0.0115f, 0.055f, -0.19f + i * 0.028f}, {0.0115f, 0.059f, -0.178f + i * 0.028f});
  // Rotpunktvisier: Gehäuse, Tubus, Linse, Punkt
  b.ColorHex(0x111214);
  b.Box({-0.016f, 0.055f, -0.05f}, {0.016f, 0.066f, 0.012f});
  b.CylinderZ({0, 0.08f, -0.02f}, 0.018f, 0.064f, 14);
  b.Mat(MAT_PLASTIC); b.ColorHex(0x24384a);
  b.CylinderZ({0, 0.08f, 0.0125f}, 0.0145f, 0.002f, 14);
  b.ColorHex(0xff3a2a); b.Emissive(1.0f);
  b.Box({-0.0025f, 0.0775f, 0.0135f}, {0.0025f, 0.0825f, 0.0145f});
  b.Emissive(0);
  // Magazin (leicht gebogen) und Griff mit Fingermulden
  b.Mat(MAT_PLASTIC); b.ColorHex(polyD);
  b.Push(Translation({0, -0.04f, -0.1f}) * RotationX(0.18f));
  b.Box({-0.016f, -0.09f, -0.024f}, {0.016f, 0.0f, 0.024f});
  b.Pop();
  b.Push(Translation({0, -0.125f, -0.112f}) * RotationX(0.32f));
  b.Box({-0.0165f, -0.07f, -0.025f}, {0.0165f, 0.004f, 0.025f});
  b.Pop();
  b.ColorHex(poly);
  b.Push(Translation({0, -0.034f, 0.035f}) * RotationX(-0.25f));
  b.Box({-0.019f, -0.11f, -0.022f}, {0.019f, 0.0f, 0.024f});
  b.ColorHex(polyD);
  for (int i = 0; i < 3; i++) b.Box({-0.0195f, -0.03f - i * 0.026f, -0.0235f}, {0.0195f, -0.022f - i * 0.026f, -0.02f});
  b.Pop();
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.Box({-0.004f, -0.062f, -0.048f}, {0.004f, -0.056f, 0.02f});  // Abzugsbügel
  b.Box({0.025f, 0.012f, -0.03f}, {0.034f, 0.02f, 0.0f});        // Spannhebel
  // Schulterstütze ganz eingeschoben: Gummikappe direkt hinter dem Gehäuse (ragt sonst ins Bild)
  b.Mat(MAT_LEATHER); b.ColorHex(0x0d0d0e);
  b.Box({-0.018f, -0.05f, 0.075f}, {0.018f, 0.022f, 0.088f});
  smg.Upload(d);
  d.Clear();
  // --- Schrotflinte (brünierter Stahl, Walnuss) ---
  b.Mat(MAT_METAL); b.ColorHex(steel);
  b.Box({-0.03f, -0.04f, -0.17f}, {0.03f, 0.04f, 0.06f});
  b.CylinderZ({0, 0.03f, -0.055f}, 0.03f, 0.23f, 14);   // gerundeter Rücken
  b.CylinderZ({0, 0.022f, -0.48f}, 0.016f, 0.62f, 14);
  b.CylinderZ({0, -0.016f, -0.43f}, 0.013f, 0.5f, 12);
  b.CylinderZ({0, 0.003f, -0.77f}, 0.02f, 0.02f, 12);    // Laufschelle
  b.Mat(MAT_GOLD); b.ColorHex(0xc9a24a);
  b.Sphere({0, 0.042f, -0.78f}, 0.005f, 6, 4);
  b.Mat(MAT_WOOD); b.ColorHex(0x7a4a26);
  b.CylinderZ({0, -0.02f, -0.445f}, 0.031f, 0.17f, 14);  // Pumpe
  b.ColorHex(0x4e2e16);
  for (int i = 0; i < 6; i++) b.CylinderZ({0, -0.02f, -0.515f + i * 0.028f}, 0.0322f, 0.008f, 14);
  b.ColorHex(0x7a4a26);
  b.Push(Translation({0, -0.034f, 0.03f}) * RotationX(-0.25f));
  b.Box({-0.021f, -0.11f, -0.025f}, {0.021f, 0.0f, 0.03f});
  b.Pop();
  b.Push(Translation({0, -0.01f, 0.05f}) * RotationX(0.14f));
  b.Box({-0.023f, -0.06f, 0.0f}, {0.023f, 0.035f, 0.26f});
  b.Mat(MAT_LEATHER); b.ColorHex(0x0d0d0e);
  b.Box({-0.024f, -0.062f, 0.26f}, {0.024f, 0.037f, 0.28f});
  b.Pop();
  b.Mat(MAT_METAL); b.ColorHex(0x2a2c30);
  b.Box({-0.004f, -0.062f, -0.05f}, {0.004f, -0.056f, 0.02f});
  shotgun.Upload(d);
  d.Clear();
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
  // Hände je Waffe
  BuildArms(b, Vec3(0, -0.034f, 0.035f), Vec3(0, -0.05f, -0.23f), true);
  hands[0].Upload(d);
  d.Clear();
  BuildArms(b, Vec3(0, -0.034f, 0.03f), Vec3(0, -0.04f, -0.45f), true);
  hands[1].Upload(d);
  d.Clear();
  BuildArms(b, Vec3(0, 0.05f, -0.03f), Vec3(), false);
  hands[2].Upload(d);
  d.Clear();
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
  BuildMeshes(torso_, head_, thigh_, shin_, upperArm_, foreArm_, smg3p_, shotgun3p_, grenade_);
  BuildViewModels(vmSmg_, vmShotgun_, vmGrenade_, vmHands_, flash_);
}

void GameView::LoadWorld(Renderer& r, const World& w) {
  wr_.Load(r, w);
  fx_.clear();
  lights_.clear();
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
    case EV_SHOT:
      if (e.a == localId) {
        recoil_ = 1.0f;
        flashTimer_ = 0.05f;
        Shake(e.b == WPN_SHOTGUN ? 0.35f : 0.08f);
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
        Burst(muzzle, 4, 2.0f, Vec4(6, 4, 1.5f, 1), Vec4(2, 1, 0.2f, 0), 0.06f, 0.05f, true, 0);
      }
      AddLight(e.pos + e.dir * 0.8f, 5.0f, Vec3(4.0f, 2.8f, 1.4f), 0.06f);
      break;
    case EV_IMPACT: {
      bool metal = e.b == MAT_METAL || e.b == MAT_MIRROR || e.b == MAT_GOLD;
      if (e.c) Burst(e.pos, 7, 2.5f, V4(0x8a5a33, 1), V4(0x5a3a23, 0), 0.025f, 0.6f, false, 9.0f);
      else Burst(e.pos, 6, 1.5f, V4(0xb8b0a0, 0.8f), V4(0x8a8478, 0), 0.05f, 0.5f, false, 3.0f, 0.2f);
      if (metal) Burst(e.pos, 6, 4.0f, Vec4(6, 4, 1.5f, 1), Vec4(3, 1, 0.2f, 0), 0.015f, 0.25f, true, 12.0f);
      if (renderer_ && e.b != MAT_WATER) renderer_->AddDecal(e.pos, e.dir, e.b == MAT_GLASS ? 0.05f : 0.035f, e.b == MAT_GLASS ? Vec4(0.85f, 0.9f, 0.95f, 0.5f) : Vec4(0.04f, 0.04f, 0.04f, 0.92f));
      break;
    }
    case EV_HIT:
      Burst(e.pos, 10, 3.0f, V4(0xa87a4a, 1), V4(0x6b4a2e, 0), 0.03f, 0.7f, false, 9.0f);
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
  Mat4 gunM = Translation({0.12f, 0.44f, -0.14f}) * RotationX(pitch - lean);
  Vec3 gripR(0, -0.075f, 0.012f), gripL(0, -0.04f, -0.24f);
  const Mesh* gun = &smg3p_;
  if (p.weapon == WPN_SHOTGUN) {
    gun = &shotgun3p_;
    gripR = Vec3(0, -0.07f, 0.06f);
    gripL = Vec3(0, -0.03f, -0.34f);
  } else if (p.weapon == WPN_GRENADE) {
    gun = nullptr;
    gripR = Vec3(0.04f, -0.06f, -0.1f);
    gripL = Vec3(-0.2f, -0.12f, -0.02f);
  }
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
  if (gun) {
    it.mesh = gun;
    it.model = torsoM * gunM;
    r.Draw(it);
  }
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
  if (me.weapon != lastWeapon_) {
    switchAnim_ = 1.0f;
    lastWeapon_ = me.weapon;
  }
  const Mesh* gun = me.weapon == WPN_SHOTGUN ? &vmShotgun_ : (me.weapon == WPN_GRENADE ? &vmGrenade_ : &vmSmg_);
  if (me.weapon == WPN_GRENADE && me.ammo[WPN_GRENADE] == 0) gun = nullptr;
  float speed = sqrtf(me.vel.x * me.vel.x + me.vel.z * me.vel.z);
  float bobAmt = me.onGround ? Saturate(speed / 5.0f) : 0.2f;
  Vec3 base = Lerp(Vec3(0.165f, -0.165f, -0.42f), Vec3(0.0f, -0.044f, -0.3f), aimBlend_);
  Vec3 bob(sinf(bobPhase_) * 0.012f * bobAmt, -fabsf(cosf(bobPhase_)) * 0.014f * bobAmt, 0);
  bob *= 1.0f - aimBlend_ * 0.7f;
  float reload = me.reloadTimer > 0 ? Saturate(std::min(me.reloadTimer, GetWeapon(me.weapon).reload - me.reloadTimer) * 3.0f) : 0.0f;
  float sw = std::max(switchAnim_, Saturate(me.switchTimer * 3));
  Vec3 off = base + bob + Vec3(0, -0.25f * reload - 0.3f * sw, 0.06f * recoil_);
  Mat4 camM = Translation(cam_.pos) * RotationY(cam_.yaw) * RotationX(cam_.pitch);
  Mat4 m = camM * Translation(off) * RotationX(0.09f * recoil_ + 0.5f * reload) * RotationZ(-0.4f * reload) * Scaling(Vec3(0.58f));
  DrawItem it;
  it.viewModel = true;
  it.castShadow = false;
  it.sky = sky;
  if (gun) {
    it.mesh = gun;
    it.model = m;
    r.Draw(it);
  }
  it.mesh = &vmHands_[ClampI(me.weapon, 0, 2)];
  it.model = m;
  r.Draw(it);
  if (flashTimer_ > 0 && me.weapon != WPN_GRENADE) {
    it.mesh = &flash_;
    float z = me.weapon == WPN_SHOTGUN ? -0.81f : -0.47f;
    it.model = m * Translation({0, 0.02f, z}) * RotationZ(rng_.Range(0, TAU)) * Scaling(Vec3(rng_.Range(0.8f, 1.3f)));
    r.Draw(it);
  }
}

void GameView::Render(Renderer& r, const ClientState& s, int localId, const World& w, const ViewInput& in, float time,
                      int width, int height, ScreenFX& fx) {
  const ClientPlayer* me = (localId >= 0 && localId < MAX_PLAYERS) ? &s.players[localId] : nullptr;
  float dt = 1.0f / 60.0f;
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
    aimBlend_ = Damp(aimBlend_, in.aiming && me->weapon != WPN_GRENADE ? 1.0f : 0.0f, 14.0f, dt);
    cam.fovDeg = Lerp(in.fov, in.fov * 0.62f, aimBlend_);
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
  cam.pitch += recoil_ * 0.012f * (firstPerson ? 1.0f : 0.0f);

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
  lightBuf_ = w.lights;
  for (const FxLight& l : lights_) {
    float k = l.life / l.maxLife;
    lightBuf_.push_back({l.pos, l.radius, l.color * k});
  }
  if (flashTimer_ > 0 && firstPerson) lightBuf_.push_back({cam_.pos + cam_.Forward() * 0.8f, 5.0f, Vec3(3.5f, 2.5f, 1.2f)});
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
