#include "game/PropCatalog.h"
#include "game/Shapes.h"

namespace {

// Baukasten für Props: zeichnet Geometrie und merkt sich Trefferzonen.
struct PK {
  MeshBuilder b;
  PropDef* def;
  bool parts = true;
  bool hasColOverride = false;
  Vec3 colC, colH;
  explicit PK(PropDef* d) : b(&d->mesh), def(d) {}

  void col(u32 hex) { b.ColorHex(hex); }
  void mat(Material m) { b.Mat(m); }
  void emis(float e) { b.Emissive(e); }
  void push(const Mat4& m) { b.Push(m); }
  void pop() { b.Pop(); }

  void part(Vec3 mn, Vec3 mx) {
    if (!parts) return;
    Vec3 lo(1e9f), hi(-1e9f);
    for (int i = 0; i < 8; i++) {
      Vec3 p((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
      p = TransformPoint(b.Xf(), p);
      lo = Min(lo, p);
      hi = Max(hi, p);
    }
    def->parts.push_back({lo, hi});
  }
  void box(Vec3 mn, Vec3 mx) { b.Box(mn, mx); part(mn, mx); }
  void boxc(Vec3 c, Vec3 h) { box(c - h, c + h); }
  void cyl(Vec3 base, float r, float h, int segs = 16) {
    b.Cylinder(base, r, h, segs);
    part(base - Vec3(r, 0, r), base + Vec3(r, h, r));
  }
  void fr(Vec3 base, float r0, float r1, float h, int segs = 16) {
    b.Frustum(base, r0, r1, h, segs);
    float r = std::max(r0, r1);
    part(base - Vec3(r, 0, r), base + Vec3(r, h, r));
  }
  void cylx(Vec3 c, float r, float len, int segs = 14) {
    b.CylinderX(c, r, len, segs);
    part(c - Vec3(len * 0.5f, r, r), c + Vec3(len * 0.5f, r, r));
  }
  void cylz(Vec3 c, float r, float len, int segs = 14) {
    b.CylinderZ(c, r, len, segs);
    part(c - Vec3(r, r, len * 0.5f), c + Vec3(r, r, len * 0.5f));
  }
  void ell(Vec3 c, Vec3 r, int segs = 14, int rings = 9) {
    b.Ellipsoid(c, r, segs, rings);
    part(c - r, c + r);
  }
  void sph(Vec3 c, float r, int segs = 12) { ell(c, Vec3(r), segs, segs * 2 / 3); }
  void torus(Vec3 c, float R, float r, int segs = 18, int sides = 8) {
    b.Torus(c, R, r, segs, sides);
    part(c - Vec3(R + r, r, R + r), c + Vec3(R + r, r, R + r));
  }
  void wedge(Vec3 mn, Vec3 mx, float h0, float h1) {
    b.Wedge(mn, mx, h0, h1);
    part(mn, Vec3(mx.x, mn.y + std::max(h0, h1), mx.z));
  }
  // Röhre zwischen zwei Punkten in der XY-Ebene (z fest)
  void tubeXY(Vec2 a, Vec2 c, float z, float r) {
    Vec2 d = c - a;
    float len = Length(d);
    push(Translation(Vec3((a.x + c.x) * 0.5f, (a.y + c.y) * 0.5f, z)) * RotationZ(atan2f(d.y, d.x)));
    box(Vec3(-len * 0.5f, -r, -r), Vec3(len * 0.5f, r, r));
    pop();
  }
  // Röhre in der YZ-Ebene
  void tubeYZ(Vec3 a, Vec3 c, float r) {
    Vec3 d = c - a;
    float len = Length(d);
    push(Translation((a + c) * 0.5f) * RotationX(atan2f(d.z, d.y)));
    box(Vec3(-r, -len * 0.5f, -r), Vec3(r, len * 0.5f, r));
    pop();
  }
  void collision(Vec3 c, Vec3 h) { hasColOverride = true; colC = c; colH = h; }
};

// --- Bundestag -------------------------------------------------------------------------------

void Sitz(PK& k) {
  k.mat(MAT_METAL); k.col(0x9aa0a8);
  k.cyl({0, 0, 0}, 0.26f, 0.035f, 20);
  k.cyl({0, 0.035f, 0}, 0.045f, 0.37f, 12);
  k.mat(MAT_FABRIC); k.col(0x4553a6);
  k.box({-0.27f, 0.40f, -0.22f}, {0.27f, 0.53f, 0.27f});
  k.box({-0.27f, 0.50f, -0.31f}, {0.27f, 1.14f, -0.19f});
  k.box({-0.25f, 1.10f, -0.30f}, {0.25f, 1.17f, -0.20f});
  k.mat(MAT_PLASTIC); k.col(0x2a2f3a);
  k.box({-0.33f, 0.40f, -0.26f}, {-0.27f, 0.70f, 0.20f});
  k.box({0.27f, 0.40f, -0.26f}, {0.33f, 0.70f, 0.20f});
}

void Rednerpult(PK& k) {
  k.mat(MAT_WOOD); k.col(0x4e3522);
  k.box({-0.48f, 0, -0.32f}, {0.48f, 0.06f, 0.32f});
  k.col(0x6b4a2e);
  k.box({-0.43f, 0.06f, -0.27f}, {0.43f, 1.02f, 0.27f});
  k.col(0x5a3d26);
  k.wedge({-0.49f, 1.02f, -0.33f}, {0.49f, 1.02f, 0.33f}, 0.04f, 0.17f);
  k.mat(MAT_GOLD); k.col(0xd4a640);
  k.push(Translation({0, 0.6f, 0.275f}) * Scaling(Vec3(0.13f)));
  BuildEagle(k.b);
  k.pop();
  k.mat(MAT_METAL); k.col(0x303338);
  for (int s = -1; s <= 1; s += 2) {
    k.push(Translation({s * 0.13f, 1.15f, 0.05f}) * RotationX(-0.55f));
    k.cyl({0, 0, 0}, 0.008f, 0.34f, 8);
    k.col(0x15161a);
    k.sph({0, 0.35f, 0}, 0.022f);
    k.col(0x303338);
    k.pop();
  }
}

void Ordner(PK& k, u32 color) {
  k.mat(MAT_PLASTIC); k.col(color);
  k.box({-0.038f, 0, -0.142f}, {0.038f, 0.318f, 0.142f});
  k.mat(MAT_PAPER); k.col(0xf4f1e8);
  k.box({-0.026f, 0.08f, 0.142f}, {0.026f, 0.24f, 0.146f});
  k.mat(MAT_PLASTIC); k.col(0x1a1a1a);
  k.cylz({0, 0.275f, 0.143f}, 0.012f, 0.006f, 10);
}

void Aktenstapel(PK& k) {
  const u32 cols[] = {0x2b5aa8, 0xc23b32, 0x3c8a4a, 0x2b2b2b, 0xd8b23a};
  float y = 0;
  for (int i = 0; i < 5; i++) {
    float h = 0.075f;
    float ox = (i % 2 ? 0.01f : -0.008f), oz = (i % 3 - 1) * 0.008f;
    k.mat(MAT_PLASTIC); k.col(cols[i]);
    k.box({-0.16f + ox, y, -0.142f + oz}, {0.16f + ox, y + h, 0.142f + oz});
    y += h;
  }
  k.mat(MAT_PAPER); k.col(0xf2efe6);
  k.box({-0.15f, y, -0.105f}, {0.15f, y + 0.03f, 0.105f});
}

void Laptop(PK& k) {
  k.mat(MAT_METAL); k.col(0x3a3d42);
  k.box({-0.17f, 0, -0.12f}, {0.17f, 0.018f, 0.12f});
  k.col(0x24262a);
  k.box({-0.15f, 0.018f, -0.06f}, {0.15f, 0.02f, 0.08f});
  k.push(Translation({0, 0.018f, -0.12f}) * RotationX(-0.28f));
  k.col(0x3a3d42);
  k.box({-0.17f, 0, -0.012f}, {0.17f, 0.23f, 0});
  k.mat(MAT_SCREEN); k.col(0x5b8ed1);
  k.box({-0.155f, 0.015f, 0}, {0.155f, 0.215f, 0.002f});
  k.pop();
}

void Wasserglas(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xd3e4ec);
  k.fr({0, 0, 0}, 0.031f, 0.037f, 0.11f, 16);
  k.col(0x9cc4dc);
  k.b.Cylinder({0, 0.106f, 0}, 0.034f, 0.006f, 16);
}

void Wasserflasche(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xbfd9e6);
  k.cyl({0, 0, 0}, 0.035f, 0.19f, 16);
  k.fr({0, 0.19f, 0}, 0.035f, 0.014f, 0.06f, 16);
  k.col(0x2a62b8);
  k.cyl({0, 0.25f, 0}, 0.015f, 0.022f, 12);
  k.mat(MAT_PAPER); k.col(0x2f6fd0);
  k.b.Cylinder({0, 0.06f, 0}, 0.0365f, 0.07f, 16, false);
}

void Kaffeebecher(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf3f3f1);
  k.cyl({0, 0, 0}, 0.04f, 0.095f, 18);
  k.push(Translation({0.045f, 0.05f, 0}) * RotationX(PI * 0.5f));
  k.torus({0, 0, 0}, 0.024f, 0.007f, 14, 6);
  k.pop();
  k.col(0x3b2416);
  k.b.Cylinder({0, 0.088f, 0}, 0.036f, 0.0085f, 16);
}

void Mikrofon(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1b1c20);
  k.cyl({0, 0, 0}, 0.055f, 0.022f, 16);
  k.mat(MAT_METAL); k.col(0x55595f);
  k.push(Translation({0, 0.022f, 0}) * RotationX(-0.6f));
  k.cyl({0, 0, 0}, 0.006f, 0.34f, 8);
  k.col(0x15161a);
  k.sph({0, 0.35f, 0}, 0.02f);
  k.mat(MAT_PLASTIC); k.col(0xd02020); k.emis(0.8f);
  k.b.Cylinder({0, 0.325f, 0}, 0.0075f, 0.01f, 8);
  k.emis(0);
  k.pop();
}

void Namensschild(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf4f4f4);
  k.push(RotationX(0.32f));
  k.box({-0.12f, 0, -0.004f}, {0.12f, 0.075f, 0.004f});
  k.col(0x1d2a4a);
  k.b.Box({-0.1f, 0.025f, 0.004f}, {0.1f, 0.045f, 0.006f});
  k.pop();
  k.col(0xf4f4f4);
  k.push(RotationX(-0.32f));
  k.box({-0.12f, 0, -0.004f}, {0.12f, 0.075f, 0.004f});
  k.pop();
}

void Ficus(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xeeeeec);
  k.fr({0, 0, 0}, 0.2f, 0.25f, 0.45f, 18);
  k.mat(MAT_CONCRETE); k.col(0x3a2c20);
  k.b.Cylinder({0, 0.44f, 0}, 0.235f, 0.015f, 18);
  k.mat(MAT_WOOD); k.col(0x5e4630);
  k.cyl({0, 0.45f, 0}, 0.025f, 0.8f, 8);
  k.mat(MAT_LEAVES); k.col(0x3f7a2c);
  k.ell({0, 1.28f, 0}, {0.46f, 0.42f, 0.46f});
  k.ell({0.14f, 1.62f, 0.08f}, {0.3f, 0.3f, 0.3f});
  k.ell({-0.2f, 1.05f, -0.1f}, {0.3f, 0.26f, 0.3f});
}

void PflanzeKlein(PK& k) {
  k.mat(MAT_CONCRETE); k.col(0xb5643c);
  k.fr({0, 0, 0}, 0.085f, 0.11f, 0.14f, 14);
  k.mat(MAT_LEAVES); k.col(0x4b8a34);
  k.ell({0, 0.24f, 0}, {0.16f, 0.13f, 0.16f});
  k.mat(MAT_PLASTIC); k.col(0xd8344a);
  k.sph({0.07f, 0.32f, 0.05f}, 0.035f, 8);
  k.sph({-0.06f, 0.33f, -0.04f}, 0.035f, 8);
  k.sph({0.0f, 0.36f, 0.07f}, 0.03f, 8);
}

void Kordelstaender(PK& k) {
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.cyl({0, 0, 0}, 0.17f, 0.03f, 20);
  k.cyl({0, 0.03f, 0}, 0.024f, 0.93f, 10);
  k.sph({0, 0.99f, 0}, 0.045f);
  k.mat(MAT_FABRIC); k.col(0x8e1b22);
  k.push(Translation({0.0f, 0.9f, 0.0f}) * RotationZ(-0.3f));
  k.box({0.02f, -0.02f, -0.02f}, {0.32f, 0.02f, 0.02f});
  k.pop();
}

void Metalldetektor(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xd6d8da);
  k.box({-0.52f, 0, -0.3f}, {-0.38f, 2.15f, 0.3f});
  k.box({0.38f, 0, -0.3f}, {0.52f, 2.15f, 0.3f});
  k.box({-0.52f, 2.05f, -0.3f}, {0.52f, 2.28f, 0.3f});
  k.col(0x3be36b); k.emis(1.0f);
  k.b.Box({-0.385f, 1.0f, -0.05f}, {-0.37f, 1.9f, 0.05f});
  k.b.Box({0.37f, 1.0f, -0.05f}, {0.385f, 1.9f, 0.05f});
  k.emis(0);
  k.col(0x30343a);
  k.b.Box({-0.2f, 2.12f, 0.3f}, {0.2f, 2.24f, 0.31f});
}

void Roentgen(PK& k) {
  k.mat(MAT_METAL); k.col(0x9ea3a8);
  k.box({-0.45f, 0, -1.2f}, {0.45f, 0.72f, 1.2f});
  k.mat(MAT_PLASTIC); k.col(0xc8cbd0);
  k.box({-0.52f, 0.72f, -0.62f}, {0.52f, 1.55f, 0.62f});
  k.col(0x1b1b1d);
  k.b.Box({-0.4f, 0.72f, -1.2f}, {0.4f, 0.75f, 1.2f});
  k.col(0x2c2f34);
  k.b.Box({-0.42f, 0.75f, 0.62f}, {0.42f, 1.3f, 0.63f});
  k.b.Box({-0.42f, 0.75f, -0.63f}, {0.42f, 1.3f, -0.62f});
  k.col(0x232528);
  k.box({0.55f, 0.9f, 0.2f}, {0.6f, 1.25f, 0.62f});
  k.mat(MAT_SCREEN); k.col(0x7fb0ff);
  k.b.Box({0.6f, 0.94f, 0.24f}, {0.605f, 1.21f, 0.58f});
}

void Feuerloescher(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xc81e1e);
  k.cyl({0, 0, 0}, 0.08f, 0.46f, 18);
  k.b.Ellipsoid({0, 0.46f, 0}, {0.08f, 0.05f, 0.08f}, 18, 6, 0.0f);
  k.mat(MAT_METAL); k.col(0x2a2a2a);
  k.box({-0.02f, 0.5f, -0.03f}, {0.02f, 0.56f, 0.03f});
  k.box({-0.02f, 0.55f, -0.09f}, {0.02f, 0.57f, 0.04f});
  k.col(0x111111);
  k.push(Translation({0.0f, 0.53f, 0.04f}) * RotationX(1.2f));
  k.cyl({0, 0, 0}, 0.012f, 0.2f, 8);
  k.pop();
  k.mat(MAT_PAPER); k.col(0xf2f2f2);
  k.b.Box({-0.05f, 0.18f, 0.079f}, {0.05f, 0.32f, 0.082f});
}

void Papierkorb(PK& k) {
  k.mat(MAT_METAL); k.col(0x50555c);
  k.fr({0, 0, 0}, 0.12f, 0.15f, 0.33f, 18);
  k.col(0x202224);
  k.b.Cylinder({0, 0.322f, 0}, 0.142f, 0.01f, 18);
  k.mat(MAT_PAPER); k.col(0xeeeeee);
  k.b.Ellipsoid({0.03f, 0.33f, 0.02f}, {0.05f, 0.03f, 0.04f}, 8, 5);
}

void Abfalleimer(PK& k) {
  k.mat(MAT_METAL); k.col(0xa9adb3);
  k.cyl({0, 0, 0}, 0.2f, 0.78f, 20);
  k.col(0x7c8187);
  k.b.Ellipsoid({0, 0.8f, 0}, {0.21f, 0.12f, 0.21f}, 20, 6, 0.0f);
  k.col(0x222326);
  k.b.Box({-0.12f, 0.6f, 0.17f}, {0.12f, 0.74f, 0.205f});
  k.part({-0.21f, 0.78f, -0.21f}, {0.21f, 0.92f, 0.21f});
}

void Buerostuhl(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1f2125);
  for (int i = 0; i < 5; i++) {
    k.push(RotationY(i * TAU / 5));
    k.box({-0.025f, 0.05f, 0}, {0.025f, 0.085f, 0.3f});
    k.sph({0, 0.03f, 0.28f}, 0.03f, 8);
    k.pop();
  }
  k.mat(MAT_METAL); k.col(0x8d9299);
  k.cyl({0, 0.085f, 0}, 0.028f, 0.36f, 10);
  k.mat(MAT_FABRIC); k.col(0x2b2d31);
  k.box({-0.25f, 0.44f, -0.24f}, {0.25f, 0.53f, 0.25f});
  k.box({-0.23f, 0.56f, -0.31f}, {0.23f, 1.1f, -0.24f});
  k.mat(MAT_PLASTIC); k.col(0x1f2125);
  k.box({-0.04f, 0.5f, -0.3f}, {0.04f, 0.58f, -0.24f});
  k.box({-0.29f, 0.62f, -0.15f}, {-0.25f, 0.66f, 0.15f});
  k.box({0.25f, 0.62f, -0.15f}, {0.29f, 0.66f, 0.15f});
  k.box({-0.28f, 0.5f, -0.02f}, {-0.26f, 0.62f, 0.02f});
  k.box({0.26f, 0.5f, -0.02f}, {0.28f, 0.62f, 0.02f});
}

void Schreibtisch(PK& k) {
  k.mat(MAT_WOOD); k.col(0xc8a27a);
  k.box({-0.8f, 0.72f, -0.4f}, {0.8f, 0.76f, 0.4f});
  k.mat(MAT_METAL); k.col(0x62666d);
  k.box({-0.78f, 0, -0.38f}, {-0.74f, 0.72f, 0.38f});
  k.box({0.74f, 0, -0.38f}, {0.78f, 0.72f, 0.38f});
  k.box({-0.74f, 0.3f, -0.38f}, {0.74f, 0.7f, -0.36f});
  k.mat(MAT_WOOD); k.col(0xb8926a);
  k.box({0.3f, 0.32f, -0.36f}, {0.72f, 0.72f, 0.3f});
  k.mat(MAT_METAL); k.col(0x3a3d42);
  for (int i = 0; i < 3; i++) k.b.Box({0.45f, 0.4f + i * 0.12f, 0.3f}, {0.57f, 0.415f + i * 0.12f, 0.315f});
}

void Aktenschrank(PK& k) {
  k.mat(MAT_METAL); k.col(0xc9ccd1);
  k.box({-0.4f, 0, -0.22f}, {0.4f, 1.9f, 0.22f});
  k.col(0x55595f);
  k.b.Box({-0.004f, 0.05f, 0.22f}, {0.004f, 1.85f, 0.224f});
  k.b.Box({-0.06f, 0.95f, 0.22f}, {-0.025f, 1.1f, 0.24f});
  k.b.Box({0.025f, 0.95f, 0.22f}, {0.06f, 1.1f, 0.24f});
  k.b.Box({-0.38f, 0.02f, 0.22f}, {0.38f, 0.06f, 0.225f});
}

void Buecherregal(PK& k) {
  k.mat(MAT_WOOD); k.col(0x7a5634);
  k.box({-0.5f, 0, -0.17f}, {-0.47f, 2.0f, 0.17f});
  k.box({0.47f, 0, -0.17f}, {0.5f, 2.0f, 0.17f});
  k.box({-0.47f, 0, -0.17f}, {0.47f, 2.0f, -0.15f});
  const float shelves[] = {0.0f, 0.4f, 0.8f, 1.2f, 1.6f, 1.97f};
  for (float y : shelves) k.box({-0.47f, y, -0.15f}, {0.47f, y + 0.03f, 0.17f});
  const u32 bookCols[] = {0x7a1f1f, 0x1f3a7a, 0x2f5a2f, 0x8a6a2a, 0x2a2a2a, 0x6a3a6a, 0xb0482a, 0xd8c8a0, 0x3a5a7a, 0x5a2a1a};
  Rng rng(42);
  k.parts = false;
  for (int s = 0; s < 5; s++) {
    float y = shelves[s] + 0.03f;
    float x = -0.46f;
    while (x < 0.44f) {
      float w = rng.Range(0.022f, 0.048f);
      if (x + w > 0.46f) break;
      if (rng.Chance(0.06f)) { x += rng.Range(0.04f, 0.1f); continue; }
      float h = rng.Range(0.22f, 0.33f);
      float d = rng.Range(0.2f, 0.27f);
      k.mat(rng.Chance(0.2f) ? MAT_LEATHER : MAT_PAPER);
      k.col(bookCols[rng.Int(0, 9)]);
      k.b.Box({x, y, 0.15f - d}, {x + w, y + h, 0.15f});
      x += w + 0.002f;
    }
  }
  k.parts = true;
  for (int s = 0; s < 5; s++) k.part({-0.46f, shelves[s] + 0.03f, -0.12f}, {0.46f, shelves[s] + 0.33f, 0.15f});
}

void Kopierer(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xe4e4e2);
  k.box({-0.32f, 0, -0.3f}, {0.32f, 0.62f, 0.3f});
  k.box({-0.34f, 0.62f, -0.3f}, {0.34f, 1.0f, 0.3f});
  k.col(0x55585e);
  k.box({-0.34f, 1.0f, -0.3f}, {0.34f, 1.05f, 0.28f});
  k.col(0x3a3d42);
  k.b.Box({-0.3f, 0.2f, 0.3f}, {0.3f, 0.205f, 0.302f});
  k.b.Box({-0.3f, 0.4f, 0.3f}, {0.3f, 0.405f, 0.302f});
  k.wedge({0.05f, 0.95f, 0.22f}, {0.33f, 0.95f, 0.36f}, 0.1f, 0.03f);
  k.mat(MAT_SCREEN); k.col(0x58c46b);
  k.b.Box({0.1f, 1.02f, 0.27f}, {0.2f, 1.03f, 0.3f});
}

void Wasserspender(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf0f0ee);
  k.box({-0.16f, 0, -0.16f}, {0.16f, 1.0f, 0.16f});
  k.col(0x6fa8dc);
  k.cyl({0, 1.0f, 0}, 0.135f, 0.38f, 18);
  k.fr({0, 1.38f, 0}, 0.135f, 0.04f, 0.08f, 18);
  k.col(0x2d6fc2);
  k.b.Box({-0.08f, 0.75f, 0.16f}, {-0.04f, 0.8f, 0.19f});
  k.col(0xc82828);
  k.b.Box({0.04f, 0.75f, 0.16f}, {0.08f, 0.8f, 0.19f});
  k.col(0x8a8d92);
  k.b.Box({-0.1f, 0.55f, 0.1f}, {0.1f, 0.57f, 0.17f});
}

void Garderobe(PK& k) {
  k.mat(MAT_WOOD); k.col(0x3d2a1c);
  k.cyl({0, 0, 0}, 0.24f, 0.03f, 18);
  k.cyl({0, 0.03f, 0}, 0.025f, 1.78f, 10);
  for (int i = 0; i < 4; i++) {
    k.push(Translation({0, 1.7f, 0}) * RotationY(i * PI * 0.5f) * RotationX(0.6f));
    k.b.Box({-0.012f, 0, 0}, {0.012f, 0.02f, 0.16f});
    k.pop();
  }
  k.sph({0, 1.82f, 0}, 0.035f);
  k.mat(MAT_FABRIC); k.col(0x2d3340);
  k.ell({0.0f, 1.25f, 0.2f}, {0.2f, 0.42f, 0.09f});
}

void Sitzbank(PK& k) {
  k.mat(MAT_WOOD); k.col(0x8a6440);
  k.box({-0.8f, 0.42f, -0.2f}, {0.8f, 0.47f, 0.2f});
  k.mat(MAT_METAL); k.col(0x30333a);
  for (int s = -1; s <= 1; s += 2) {
    k.box({s * 0.7f - 0.03f, 0, -0.18f}, {s * 0.7f + 0.03f, 0.42f, -0.14f});
    k.box({s * 0.7f - 0.03f, 0, 0.14f}, {s * 0.7f + 0.03f, 0.42f, 0.18f});
    k.box({s * 0.7f - 0.03f, 0.02f, -0.18f}, {s * 0.7f + 0.03f, 0.05f, 0.18f});
  }
}

void Sessel(PK& k) {
  k.mat(MAT_LEATHER); k.col(0x1d1d1f);
  k.box({-0.38f, 0.16f, -0.36f}, {0.38f, 0.42f, 0.38f});
  k.box({-0.38f, 0.42f, -0.38f}, {0.38f, 0.88f, -0.24f});
  k.box({-0.4f, 0.16f, -0.38f}, {-0.3f, 0.62f, 0.38f});
  k.box({0.3f, 0.16f, -0.38f}, {0.4f, 0.62f, 0.38f});
  k.mat(MAT_METAL); k.col(0xb0b4ba);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.box({x * 0.34f - 0.02f, 0, z * 0.32f - 0.02f}, {x * 0.34f + 0.02f, 0.16f, z * 0.32f + 0.02f});
}

void Sofa(PK& k) {
  k.mat(MAT_LEATHER); k.col(0x5a3b26);
  k.box({-0.95f, 0.14f, -0.42f}, {0.95f, 0.4f, 0.42f});
  k.box({-0.95f, 0.4f, -0.44f}, {0.95f, 0.86f, -0.28f});
  k.box({-1.0f, 0.14f, -0.44f}, {-0.86f, 0.62f, 0.42f});
  k.box({0.86f, 0.14f, -0.44f}, {1.0f, 0.62f, 0.42f});
  k.col(0x6a4630);
  k.b.Box({-0.85f, 0.4f, -0.28f}, {-0.01f, 0.48f, 0.4f});
  k.b.Box({0.01f, 0.4f, -0.28f}, {0.85f, 0.48f, 0.4f});
  k.mat(MAT_METAL); k.col(0x1a1a1a);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.box({x * 0.9f - 0.03f, 0, z * 0.36f - 0.03f}, {x * 0.9f + 0.03f, 0.14f, z * 0.36f + 0.03f});
}

void Couchtisch(PK& k) {
  k.mat(MAT_WOOD); k.col(0x3b2a1e);
  k.box({-0.55f, 0.37f, -0.32f}, {0.55f, 0.42f, 0.32f});
  k.mat(MAT_METAL); k.col(0xa8acb2);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.box({x * 0.5f - 0.02f, 0, z * 0.27f - 0.02f}, {x * 0.5f + 0.02f, 0.37f, z * 0.27f + 0.02f});
}

void Stehlampe(PK& k) {
  k.mat(MAT_METAL); k.col(0x222327);
  k.cyl({0, 0, 0}, 0.15f, 0.03f, 18);
  k.cyl({0, 0.03f, 0}, 0.014f, 1.45f, 8);
  k.mat(MAT_FABRIC); k.col(0xeadfc4); k.emis(0.35f);
  k.fr({0, 1.42f, 0}, 0.21f, 0.13f, 0.3f, 20);
  k.emis(0);
  k.def->light = true;
  k.def->lightOffset = {0, 1.5f, 0};
  k.def->lightColor = {2.6f, 2.0f, 1.3f};
  k.def->lightRadius = 6.5f;
}

void Schreibtischlampe(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a5a9a);
  k.cyl({0, 0, 0}, 0.07f, 0.02f, 14);
  k.push(Translation({0, 0.02f, 0}) * RotationZ(0.35f));
  k.cyl({0, 0, 0}, 0.008f, 0.3f, 8);
  k.pop();
  k.push(Translation({-0.1f, 0.3f, 0}) * RotationZ(-1.1f));
  k.cyl({0, 0, 0}, 0.008f, 0.22f, 8);
  k.pop();
  k.push(Translation({0.1f, 0.38f, 0}) * RotationZ(2.4f));
  k.fr({0, 0, 0}, 0.025f, 0.06f, 0.09f, 14);
  k.mat(MAT_PLASTIC); k.col(0xfff4d0); k.emis(1.0f);
  k.b.Cylinder({0, 0.085f, 0}, 0.055f, 0.006f, 14);
  k.emis(0);
  k.pop();
  k.def->light = true;
  k.def->lightOffset = {0.14f, 0.3f, 0};
  k.def->lightColor = {1.6f, 1.4f, 1.0f};
  k.def->lightRadius = 3.0f;
}

void Monitor(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1e1f22);
  k.box({-0.11f, 0, -0.09f}, {0.11f, 0.015f, 0.08f});
  k.box({-0.025f, 0.015f, -0.04f}, {0.025f, 0.24f, -0.015f});
  k.box({-0.28f, 0.12f, -0.02f}, {0.28f, 0.46f, 0.005f});
  k.mat(MAT_SCREEN); k.col(0x3d6fa8);
  k.b.Box({-0.265f, 0.135f, 0.005f}, {0.265f, 0.445f, 0.007f});
}

void PC(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1b1c1f);
  k.box({-0.1f, 0, -0.22f}, {0.1f, 0.43f, 0.22f});
  k.col(0x3d8bff); k.emis(1.0f);
  k.b.Box({-0.06f, 0.36f, 0.22f}, {-0.03f, 0.37f, 0.224f});
  k.emis(0);
  k.col(0x2a2c30);
  k.b.Box({-0.08f, 0.25f, 0.22f}, {0.08f, 0.33f, 0.223f});
}

void Telefon(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x2a2c30);
  k.wedge({-0.1f, 0, -0.1f}, {0.1f, 0, 0.1f}, 0.07f, 0.035f);
  k.col(0x1a1b1e);
  k.cylx({0, 0.085f, -0.04f}, 0.022f, 0.2f, 10);
  k.col(0xb8bcc2);
  k.b.Box({-0.06f, 0.045f, 0.0f}, {0.06f, 0.06f, 0.08f});
}

void Kantinentisch(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xecebe6);
  k.box({-0.6f, 0.73f, -0.4f}, {0.6f, 0.76f, 0.4f});
  k.mat(MAT_METAL); k.col(0x7d8188);
  k.cyl({0, 0.02f, 0}, 0.04f, 0.71f, 10);
  k.box({-0.45f, 0, -0.04f}, {0.45f, 0.03f, 0.04f});
  k.box({-0.04f, 0, -0.32f}, {0.04f, 0.03f, 0.32f});
}

void Kantinenstuhl(PK& k) {
  k.mat(MAT_WOOD); k.col(0xb88a5a);
  k.box({-0.21f, 0.44f, -0.21f}, {0.21f, 0.47f, 0.21f});
  k.box({-0.21f, 0.62f, -0.23f}, {0.21f, 0.84f, -0.2f});
  k.mat(MAT_METAL); k.col(0x3a3d42);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.cyl({x * 0.18f, 0, z * 0.18f}, 0.012f, 0.44f, 8);
  k.cyl({-0.18f, 0.47f, -0.2f}, 0.01f, 0.18f, 6);
  k.cyl({0.18f, 0.47f, -0.2f}, 0.01f, 0.18f, 6);
}

void Tablett(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x8a5a3a);
  k.box({-0.23f, 0, -0.17f}, {0.23f, 0.02f, 0.17f});
  k.col(0xf5f5f2);
  k.cyl({-0.05f, 0.02f, 0}, 0.11f, 0.015f, 18);
  k.col(0xc9a050);
  k.ell({-0.07f, 0.04f, 0.02f}, {0.05f, 0.02f, 0.035f}, 10, 5);
  k.col(0x5c8a3a);
  k.ell({-0.01f, 0.04f, -0.03f}, {0.04f, 0.018f, 0.03f}, 10, 5);
  k.col(0xf3f3f1);
  k.cyl({0.15f, 0.02f, 0.08f}, 0.035f, 0.08f, 14);
}

void Currywurst(PK& k) {
  k.mat(MAT_PAPER); k.col(0xf4f2ec);
  k.box({-0.1f, 0, -0.055f}, {0.1f, 0.012f, 0.055f});
  k.box({-0.1f, 0.012f, -0.055f}, {0.1f, 0.03f, -0.05f});
  k.box({-0.1f, 0.012f, 0.05f}, {0.1f, 0.03f, 0.055f});
  k.mat(MAT_PLASTIC); k.col(0x9a4a22);
  for (int i = 0; i < 5; i++) k.cylx({-0.07f + i * 0.035f, 0.028f, 0}, 0.015f, 0.026f, 10);
  k.col(0xa3301c);
  k.b.Ellipsoid({0, 0.037f, 0}, {0.095f, 0.012f, 0.035f}, 14, 5, 0.0f);
  k.col(0xd99a2a);
  for (int i = 0; i < 9; i++) k.b.BoxC({-0.075f + i * 0.019f, 0.05f, ((i * 7) % 5 - 2) * 0.007f}, {0.003f, 0.0015f, 0.003f});
  k.mat(MAT_WOOD); k.col(0xd8b57a);
  k.b.Box({0.03f, 0.04f, 0.0f}, {0.12f, 0.044f, 0.008f});
}

void Brezel(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x8a4f1d);
  k.torus({-0.042f, 0.015f, 0.01f}, 0.04f, 0.014f, 16, 7);
  k.torus({0.042f, 0.015f, 0.01f}, 0.04f, 0.014f, 16, 7);
  k.push(Translation({0, 0.015f, -0.045f}));
  k.cylx({0, 0, 0}, 0.017f, 0.15f, 10);
  k.pop();
  k.col(0xf8f8f8);
  for (int i = 0; i < 8; i++) k.b.BoxC({-0.06f + i * 0.017f, 0.031f, ((i * 3) % 4 - 1.5f) * 0.012f}, {0.003f, 0.002f, 0.003f});
}

void Mate(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x6b3d12);
  k.cyl({0, 0, 0}, 0.032f, 0.17f, 16);
  k.fr({0, 0.17f, 0}, 0.032f, 0.014f, 0.05f, 16);
  k.cyl({0, 0.22f, 0}, 0.014f, 0.035f, 12);
  k.mat(MAT_METAL); k.col(0xc8c8c8);
  k.cyl({0, 0.255f, 0}, 0.016f, 0.012f, 12);
  k.mat(MAT_PAPER); k.col(0xf0c419);
  k.b.Cylinder({0, 0.05f, 0}, 0.0325f, 0.08f, 16, false);
  k.col(0x1e4fa8);
  k.b.Cylinder({0, 0.075f, 0}, 0.033f, 0.02f, 16, false);
}

void Kaffeemaschine(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1d1e21);
  k.box({-0.15f, 0, -0.18f}, {0.15f, 0.06f, 0.18f});
  k.box({-0.15f, 0.06f, -0.18f}, {0.15f, 0.42f, -0.02f});
  k.box({-0.15f, 0.3f, -0.02f}, {0.15f, 0.42f, 0.18f});
  k.mat(MAT_METAL); k.col(0xb7bbc1);
  k.b.Box({-0.151f, 0.1f, -0.181f}, {0.151f, 0.4f, -0.17f});
  k.b.Box({-0.04f, 0.27f, 0.08f}, {0.04f, 0.3f, 0.12f});
  k.mat(MAT_PLASTIC); k.col(0xf3f3f1);
  k.cyl({0, 0.06f, 0.1f}, 0.035f, 0.08f, 14);
  k.col(0x58c46b); k.emis(1.0f);
  k.b.Box({-0.1f, 0.36f, 0.18f}, {-0.07f, 0.38f, 0.183f});
  k.emis(0);
}

void Getraenkeautomat(PK& k) {
  k.mat(MAT_METAL); k.col(0xb51f24);
  k.box({-0.5f, 0, -0.42f}, {0.5f, 1.92f, 0.42f});
  k.mat(MAT_SCREEN); k.col(0x7fa8cc);
  k.b.Box({-0.44f, 0.55f, 0.42f}, {0.18f, 1.82f, 0.425f});
  k.mat(MAT_PLASTIC);
  const u32 bc[] = {0xf0c419, 0x2f8a3a, 0xd23a2a, 0x2a5ac8, 0xf4f4f4};
  for (int row = 0; row < 5; row++)
    for (int c = 0; c < 6; c++) {
      k.col(bc[(row + c) % 5]);
      k.b.Box({-0.42f + c * 0.1f, 0.62f + row * 0.24f, 0.426f}, {-0.36f + c * 0.1f, 0.78f + row * 0.24f, 0.43f});
    }
  k.col(0x1a1b1e);
  k.b.Box({0.24f, 1.0f, 0.42f}, {0.44f, 1.6f, 0.43f});
  k.b.Box({-0.4f, 0.15f, 0.42f}, {0.1f, 0.4f, 0.43f});
  k.mat(MAT_PLASTIC); k.col(0xffffff); k.emis(0.6f);
  k.b.Box({-0.44f, 1.84f, 0.42f}, {0.44f, 1.9f, 0.43f});
  k.emis(0);
}

void Mikrowelle(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xeeeeec);
  k.box({-0.25f, 0, -0.18f}, {0.25f, 0.3f, 0.18f});
  k.col(0x161718);
  k.b.Box({-0.22f, 0.04f, 0.18f}, {0.1f, 0.26f, 0.184f});
  k.col(0x3a3d42);
  k.b.Box({0.13f, 0.04f, 0.18f}, {0.23f, 0.26f, 0.183f});
}

void Kochtopf(PK& k) {
  k.mat(MAT_METAL); k.col(0xb9bdc3);
  k.cyl({0, 0, 0}, 0.14f, 0.16f, 20);
  k.cyl({0, 0.16f, 0}, 0.146f, 0.012f, 20);
  k.col(0x1d1e21);
  k.sph({0, 0.18f, 0}, 0.02f);
  k.box({-0.2f, 0.12f, -0.02f}, {-0.14f, 0.14f, 0.02f});
  k.box({0.14f, 0.12f, -0.02f}, {0.2f, 0.14f, 0.02f});
}

void Adlerfigur(PK& k) {
  k.mat(MAT_MARBLE); k.col(0xe9e6df);
  k.box({-0.16f, 0, -0.13f}, {0.16f, 0.5f, 0.13f});
  k.box({-0.19f, 0.5f, -0.16f}, {0.19f, 0.54f, 0.16f});
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.push(Translation({0, 0.76f, 0}) * Scaling(Vec3(0.2f)));
  BuildEagle(k.b);
  k.pop();
  k.part({-0.23f, 0.54f, -0.03f}, {0.23f, 0.99f, 0.03f});
}

void Flagge(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2b2e);
  k.cyl({0, 0, 0}, 0.2f, 0.05f, 18);
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.cyl({0, 0.05f, 0}, 0.02f, 2.35f, 10);
  k.sph({0, 2.42f, 0}, 0.04f);
  k.mat(MAT_FABRIC);
  k.col(0x141414); k.box({0.02f, 2.13f, -0.006f}, {0.92f, 2.33f, 0.006f});
  k.col(0xdd0000); k.box({0.02f, 1.93f, -0.006f}, {0.92f, 2.13f, 0.006f});
  k.col(0xffce00); k.box({0.02f, 1.73f, -0.006f}, {0.92f, 1.93f, 0.006f});
}

void Glocke(PK& k) {
  k.mat(MAT_WOOD); k.col(0x4a3020);
  k.box({-0.09f, 0, -0.09f}, {0.09f, 0.03f, 0.09f});
  k.mat(MAT_GOLD); k.col(0xd3ac52);
  k.fr({0, 0.03f, 0}, 0.065f, 0.035f, 0.09f, 18);
  k.sph({0, 0.12f, 0}, 0.036f);
  k.cyl({0, 0.14f, 0}, 0.009f, 0.07f, 8);
  k.mat(MAT_WOOD); k.col(0x2a1a10);
  k.sph({0, 0.22f, 0}, 0.022f);
}

void Absperrgitter(PK& k) {
  k.mat(MAT_METAL); k.col(0xb8bcc2);
  k.parts = false;
  k.b.CylinderX({0, 1.05f, 0}, 0.02f, 2.0f, 8);
  k.b.CylinderX({0, 0.18f, 0}, 0.02f, 2.0f, 8);
  for (int i = -1; i <= 1; i += 2) k.b.Cylinder({i * 1.0f, 0.05f, 0}, 0.022f, 1.02f, 8);
  for (int i = 0; i < 15; i++) k.b.Cylinder({-0.9f + i * 0.128f, 0.18f, 0}, 0.008f, 0.87f, 6);
  for (int i = -1; i <= 1; i += 2) k.b.Box({i * 1.0f - 0.04f, 0, -0.32f}, {i * 1.0f + 0.04f, 0.05f, 0.32f});
  k.parts = true;
  k.part({-1.02f, 0.05f, -0.03f}, {1.02f, 1.08f, 0.03f});
  k.part({-1.04f, 0, -0.32f}, {-0.96f, 0.05f, 0.32f});
  k.part({0.96f, 0, -0.32f}, {1.04f, 0.05f, 0.32f});
}

void Poller(PK& k) {
  k.mat(MAT_METAL); k.col(0x3a3f45);
  k.cyl({0, 0, 0}, 0.13f, 0.86f, 18);
  k.b.Ellipsoid({0, 0.86f, 0}, {0.13f, 0.06f, 0.13f}, 18, 5, 0.0f);
  k.mat(MAT_PLASTIC); k.col(0xf4f4f4); k.emis(0.15f);
  k.b.Cylinder({0, 0.68f, 0}, 0.132f, 0.06f, 18, false);
  k.emis(0);
}

void Parkbank(PK& k) {
  k.mat(MAT_WOOD); k.col(0x8a5a33);
  for (int i = 0; i < 4; i++) k.box({-0.9f, 0.42f, -0.22f + i * 0.11f}, {0.9f, 0.45f, -0.14f + i * 0.11f});
  k.push(Translation({0, 0.45f, -0.24f}) * RotationX(-0.25f));
  for (int i = 0; i < 3; i++) k.box({-0.9f, 0.08f + i * 0.12f, -0.015f}, {0.9f, 0.17f + i * 0.12f, 0.015f});
  k.pop();
  k.mat(MAT_METAL); k.col(0x1e2022);
  for (int s = -1; s <= 1; s += 2) {
    k.box({s * 0.78f - 0.025f, 0, -0.24f}, {s * 0.78f + 0.025f, 0.42f, -0.18f});
    k.box({s * 0.78f - 0.025f, 0, 0.16f}, {s * 0.78f + 0.025f, 0.42f, 0.22f});
    k.box({s * 0.78f - 0.025f, 0.38f, -0.26f}, {s * 0.78f + 0.025f, 0.42f, 0.24f});
    k.box({s * 0.78f - 0.025f, 0.42f, -0.3f}, {s * 0.78f + 0.025f, 0.85f, -0.25f});
  }
}

void Laterne(PK& k) {
  k.mat(MAT_METAL); k.col(0x2d3a33);
  k.cyl({0, 0, 0}, 0.14f, 0.4f, 14);
  k.fr({0, 0.4f, 0}, 0.09f, 0.06f, 4.1f, 12);
  k.box({-0.22f, 4.4f, -0.22f}, {0.22f, 4.48f, 0.22f});
  k.mat(MAT_PLASTIC); k.col(0xfff1c8); k.emis(1.0f);
  k.box({-0.17f, 4.48f, -0.17f}, {0.17f, 4.85f, 0.17f});
  k.emis(0);
  k.mat(MAT_METAL); k.col(0x2d3a33);
  k.fr({0, 4.85f, 0}, 0.28f, 0.05f, 0.25f, 4);
  k.collision({0, 2.4f, 0}, {0.14f, 2.4f, 0.14f});
  k.def->light = true;
  k.def->lightOffset = {0, 4.6f, 0};
  k.def->lightColor = {6.0f, 4.8f, 3.2f};
  k.def->lightRadius = 13.0f;
}

void Baum(PK& k) {
  k.mat(MAT_WOOD); k.col(0x5a4330);
  k.fr({0, 0, 0}, 0.26f, 0.17f, 3.2f, 12);
  k.push(Translation({0, 2.6f, 0}) * RotationZ(0.5f));
  k.fr({0, 0, 0}, 0.1f, 0.05f, 1.4f, 8);
  k.pop();
  k.mat(MAT_LEAVES); k.col(0x4a7a2a);
  k.ell({0, 4.6f, 0}, {2.3f, 2.0f, 2.3f}, 16, 10);
  k.ell({1.1f, 3.9f, 0.6f}, {1.5f, 1.3f, 1.5f}, 14, 8);
  k.ell({-1.0f, 4.0f, -0.6f}, {1.5f, 1.4f, 1.5f}, 14, 8);
  k.col(0x568a32);
  k.ell({0.2f, 5.7f, 0.1f}, {1.4f, 1.2f, 1.4f}, 14, 8);
  k.collision({0, 1.6f, 0}, {0.26f, 1.6f, 0.26f});
}

void Busch(PK& k) {
  k.mat(MAT_LEAVES); k.col(0x3d6b28);
  k.ell({0, 0.45f, 0}, {0.6f, 0.48f, 0.6f});
  k.col(0x467a2e);
  k.ell({0.3f, 0.38f, 0.22f}, {0.4f, 0.34f, 0.4f});
  k.ell({-0.3f, 0.34f, -0.2f}, {0.42f, 0.33f, 0.4f});
}

void Blumenkuebel(PK& k) {
  k.mat(MAT_CONCRETE); k.col(0x9a9a95);
  k.box({-0.4f, 0, -0.4f}, {0.4f, 0.58f, 0.4f});
  k.col(0x3a2c20);
  k.b.Box({-0.36f, 0.58f, -0.36f}, {0.36f, 0.6f, 0.36f});
  k.mat(MAT_LEAVES); k.col(0x3f7a2c);
  k.ell({0, 0.78f, 0}, {0.36f, 0.26f, 0.36f});
  k.mat(MAT_PLASTIC);
  const u32 fc[] = {0xd8344a, 0xf2c230, 0xf4f4f4, 0xb84ad8};
  for (int i = 0; i < 7; i++) {
    float a = i * 0.9f;
    k.col(fc[i % 4]);
    k.b.Sphere({cosf(a) * 0.22f, 0.94f + (i % 2) * 0.04f, sinf(a) * 0.22f}, 0.045f, 8, 5);
  }
}

void Huetchen(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xff6a13);
  k.box({-0.18f, 0, -0.18f}, {0.18f, 0.03f, 0.18f});
  k.fr({0, 0.03f, 0}, 0.14f, 0.025f, 0.6f, 16);
  k.col(0xf4f4f4);
  k.b.Frustum({0, 0.24f, 0}, 0.109f, 0.096f, 0.08f, 16, false);
  k.b.Frustum({0, 0.42f, 0}, 0.081f, 0.071f, 0.06f, 16, false);
}

void Muelltonne(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x4a4f55);
  k.box({-0.3f, 0.06f, -0.36f}, {0.3f, 1.0f, 0.34f});
  k.col(0xe0621d);
  k.box({-0.32f, 1.0f, -0.38f}, {0.32f, 1.06f, 0.37f});
  k.col(0x1a1a1a);
  k.cylx({0, 0.1f, -0.3f}, 0.1f, 0.66f, 14);
  k.col(0x4a4f55);
  k.box({-0.25f, 0.95f, -0.42f}, {0.25f, 1.0f, -0.36f});
}

void Fahrrad(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x18191b);
  for (int s = -1; s <= 1; s += 2) {
    k.push(Translation({s * 0.52f, 0.34f, 0}) * RotationX(PI * 0.5f));
    k.torus({0, 0, 0}, 0.31f, 0.022f, 24, 6);
    k.pop();
    k.mat(MAT_METAL); k.col(0x9ea3a8);
    k.cylz({s * 0.52f, 0.34f, 0}, 0.03f, 0.08f, 8);
    k.mat(MAT_PLASTIC); k.col(0x18191b);
  }
  k.mat(MAT_METAL); k.col(0xb3202a);
  Vec2 rear(-0.52f, 0.34f), front(0.52f, 0.34f), bb(-0.05f, 0.3f), seat(-0.2f, 0.85f), headT(0.38f, 0.86f), headB(0.42f, 0.7f);
  k.tubeXY(rear, bb, 0.04f, 0.014f);
  k.tubeXY(rear, bb, -0.04f, 0.014f);
  k.tubeXY(bb, seat, 0, 0.018f);
  k.tubeXY(seat, rear, 0.035f, 0.012f);
  k.tubeXY(seat, rear, -0.035f, 0.012f);
  k.tubeXY(seat, headT, 0, 0.018f);
  k.tubeXY(bb, headB, 0, 0.02f);
  k.tubeXY(headB, headT, 0, 0.022f);
  k.mat(MAT_METAL); k.col(0x7c8187);
  k.tubeXY(headB, front, 0.04f, 0.012f);
  k.tubeXY(headB, front, -0.04f, 0.012f);
  k.tubeXY(headT, Vec2(0.34f, 1.0f), 0, 0.014f);
  k.cylz({0.34f, 1.0f, 0}, 0.014f, 0.56f, 8);
  k.mat(MAT_LEATHER); k.col(0x1a1a1a);
  k.ell({-0.22f, 0.9f, 0}, {0.13f, 0.035f, 0.07f}, 10, 5);
  k.cyl({-0.2f, 0.85f, 0}, 0.012f, 0.04f, 6);
}

void Kiste(PK& k) {
  k.mat(MAT_WOOD); k.col(0xa87a4a);
  k.box({-0.35f, 0, -0.35f}, {0.35f, 0.7f, 0.35f});
  k.col(0x8a5f36);
  for (int f = 0; f < 4; f++) {
    k.push(RotationY(f * PI * 0.5f));
    k.b.Box({-0.35f, 0, 0.35f}, {0.35f, 0.07f, 0.37f});
    k.b.Box({-0.35f, 0.63f, 0.35f}, {0.35f, 0.7f, 0.37f});
    k.b.Box({-0.35f, 0, 0.35f}, {-0.28f, 0.7f, 0.37f});
    k.b.Box({0.28f, 0, 0.35f}, {0.35f, 0.7f, 0.37f});
    k.pop();
  }
}

void Karton(PK& k) {
  k.mat(MAT_PAPER); k.col(0xb8925d);
  k.box({-0.25f, 0, -0.2f}, {0.25f, 0.36f, 0.2f});
  k.col(0xd8c39a);
  k.b.Box({-0.03f, 0.36f, -0.2f}, {0.03f, 0.362f, 0.2f});
  k.b.Box({-0.03f, 0.2f, 0.2f}, {0.03f, 0.36f, 0.202f});
  k.col(0x2a2a2a);
  k.b.Box({0.08f, 0.15f, 0.2f}, {0.2f, 0.27f, 0.201f});
}

void Paket(PK& k) {
  k.mat(MAT_PAPER); k.col(0xc49a62);
  k.box({-0.15f, 0, -0.11f}, {0.15f, 0.18f, 0.11f});
  k.col(0xf4f4f0);
  k.b.Box({-0.08f, 0.18f, -0.06f}, {0.06f, 0.182f, 0.04f});
  k.col(0xd8c39a);
  k.b.Box({-0.15f, 0.18f, -0.02f}, {0.15f, 0.183f, 0.02f});
}

void TvKamera(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2c30);
  for (int i = 0; i < 3; i++) {
    k.push(RotationY(i * TAU / 3));
    k.tubeYZ({0, 1.2f, 0}, {0, 0, 0.46f}, 0.016f);
    k.pop();
  }
  k.cyl({0, 1.15f, 0}, 0.04f, 0.1f, 10);
  k.mat(MAT_PLASTIC); k.col(0x1c1d20);
  k.box({-0.12f, 1.25f, -0.28f}, {0.12f, 1.5f, 0.2f});
  k.cylz({0, 1.37f, 0.3f}, 0.075f, 0.2f, 16);
  k.col(0x0e0f10);
  k.b.Cylinder({0, 1.37f, 0.4f}, 0.06f, 0.005f, 16);
  k.box({0.12f, 1.42f, -0.2f}, {0.22f, 1.5f, -0.02f});
  k.col(0xd8202a); k.emis(1.0f);
  k.b.Box({-0.03f, 1.5f, 0.12f}, {0.03f, 1.52f, 0.16f});
  k.emis(0);
}

void Koffer(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x2c3e66);
  k.box({-0.22f, 0.04f, -0.13f}, {0.22f, 0.68f, 0.13f});
  k.col(0x24335a);
  for (int i = 0; i < 4; i++) k.b.Box({-0.18f + i * 0.12f, 0.06f, 0.13f}, {-0.16f + i * 0.12f, 0.66f, 0.135f});
  k.mat(MAT_METAL); k.col(0x8d9299);
  k.box({-0.12f, 0.68f, -0.11f}, {-0.1f, 0.98f, -0.09f});
  k.box({0.1f, 0.68f, -0.11f}, {0.12f, 0.98f, -0.09f});
  k.mat(MAT_PLASTIC); k.col(0x1a1a1a);
  k.box({-0.12f, 0.96f, -0.12f}, {0.12f, 1.0f, -0.08f});
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.sph({x * 0.18f, 0.025f, z * 0.1f}, 0.025f, 8);
}

void Schirmstaender(PK& k) {
  k.mat(MAT_METAL); k.col(0x55595f);
  k.cyl({0, 0, 0}, 0.12f, 0.5f, 16);
  const u32 uc[] = {0x1a1a1a, 0x8e1b22, 0x1f3a7a};
  for (int i = 0; i < 3; i++) {
    float a = i * TAU / 3;
    k.push(Translation({cosf(a) * 0.05f, 0.1f, sinf(a) * 0.05f}) * RotationZ(cosf(a) * 0.12f) * RotationX(sinf(a) * 0.12f));
    k.mat(MAT_FABRIC); k.col(uc[i]);
    k.fr({0, 0.25f, 0}, 0.025f, 0.045f, 0.55f, 8);
    k.mat(MAT_WOOD); k.col(0x4a3020);
    k.cyl({0, 0.8f, 0}, 0.01f, 0.12f, 6);
    k.pop();
  }
}

void Bueste(PK& k) {
  k.mat(MAT_MARBLE); k.col(0xe8e4dc);
  k.box({-0.2f, 0, -0.2f}, {0.2f, 1.08f, 0.2f});
  k.box({-0.24f, 1.08f, -0.24f}, {0.24f, 1.13f, 0.24f});
  k.box({-0.24f, 0, -0.24f}, {0.24f, 0.08f, 0.24f});
  k.mat(MAT_METAL); k.col(0x8c6a3f);
  k.b.Ellipsoid({0, 1.13f, 0}, {0.23f, 0.2f, 0.13f}, 16, 8, 0.0f);
  k.part({-0.23f, 1.13f, -0.13f}, {0.23f, 1.33f, 0.13f});
  k.cyl({0, 1.3f, 0}, 0.055f, 0.1f, 10);
  k.ell({0, 1.5f, 0.01f}, {0.095f, 0.125f, 0.11f}, 14, 9);
  k.ell({0, 1.47f, 0.1f}, {0.02f, 0.035f, 0.025f}, 8, 5);
}

void Vase(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf2f4f8);
  k.fr({0, 0, 0}, 0.08f, 0.13f, 0.16f, 18);
  k.fr({0, 0.16f, 0}, 0.13f, 0.06f, 0.16f, 18);
  k.fr({0, 0.32f, 0}, 0.06f, 0.085f, 0.07f, 18);
  k.col(0x2a4aa8);
  k.b.Frustum({0, 0.12f, 0}, 0.122f, 0.131f, 0.05f, 18, false);
}

void Globus(PK& k) {
  k.mat(MAT_WOOD); k.col(0x4a3020);
  k.cyl({0, 0, 0}, 0.16f, 0.03f, 16);
  k.cyl({0, 0.03f, 0}, 0.02f, 0.5f, 8);
  k.mat(MAT_METAL); k.col(0xc9a24a);
  k.push(Translation({0, 0.78f, 0}) * RotationX(PI * 0.5f) * RotationZ(0.4f));
  k.b.Torus({0, 0, 0}, 0.235f, 0.008f, 24, 5);
  k.pop();
  k.mat(MAT_PLASTIC); k.col(0x2f6db5);
  k.sph({0, 0.78f, 0}, 0.22f, 20);
  k.col(0x4f8f3a);
  k.b.Ellipsoid({0.1f, 0.85f, 0.15f}, {0.09f, 0.08f, 0.07f}, 10, 6);
  k.b.Ellipsoid({-0.14f, 0.72f, 0.1f}, {0.08f, 0.11f, 0.08f}, 10, 6);
  k.b.Ellipsoid({0.05f, 0.7f, -0.18f}, {0.1f, 0.07f, 0.06f}, 10, 6);
}

void Staffelei(PK& k) {
  k.mat(MAT_WOOD); k.col(0x9a7048);
  k.tubeYZ({-0.3f, 0, 0.12f}, {-0.05f, 1.7f, 0}, 0.018f);
  k.tubeYZ({0.3f, 0, 0.12f}, {0.05f, 1.7f, 0}, 0.018f);
  k.tubeYZ({0, 0, -0.45f}, {0, 1.6f, -0.02f}, 0.018f);
  k.box({-0.45f, 0.76f, 0.02f}, {0.45f, 0.8f, 0.1f});
  k.mat(MAT_FABRIC); k.col(0xf5f2ea);
  k.box({-0.42f, 0.8f, 0.02f}, {0.42f, 1.45f, 0.05f});
  k.mat(MAT_PLASTIC);
  k.col(0xd8344a); k.b.Box({-0.35f, 0.9f, 0.05f}, {-0.05f, 1.2f, 0.055f});
  k.col(0x2a5ac8); k.b.Box({0.0f, 1.0f, 0.05f}, {0.32f, 1.38f, 0.055f});
  k.col(0xf2c230); k.b.Box({-0.2f, 1.22f, 0.05f}, {0.1f, 1.4f, 0.056f});
}

void Auto(PK& k, u32 color, bool police) {
  k.mat(MAT_METAL); k.col(color);
  k.box({-2.25f, 0.32f, -0.88f}, {2.25f, 1.0f, 0.88f});
  k.box({-1.15f, 1.0f, -0.8f}, {1.0f, 1.55f, 0.8f});
  k.mat(MAT_MIRROR); k.col(0x223344);
  k.b.Box({-1.1f, 1.04f, -0.81f}, {0.95f, 1.5f, 0.81f});
  k.b.Box({0.95f, 1.04f, -0.75f}, {1.02f, 1.5f, 0.75f});
  k.b.Box({-1.17f, 1.04f, -0.75f}, {-1.1f, 1.5f, 0.75f});
  k.mat(MAT_PLASTIC); k.col(0x18191b);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.cylz({x * 1.45f, 0.34f, z * 0.82f}, 0.34f, 0.24f, 18);
  k.col(0xfff4d8); k.emis(0.8f);
  k.b.Box({2.25f, 0.7f, -0.75f}, {2.27f, 0.82f, -0.45f});
  k.b.Box({2.25f, 0.7f, 0.45f}, {2.27f, 0.82f, 0.75f});
  k.col(0xd01818);
  k.b.Box({-2.27f, 0.7f, -0.75f}, {-2.25f, 0.82f, -0.5f});
  k.b.Box({-2.27f, 0.7f, 0.5f}, {-2.25f, 0.82f, 0.75f});
  k.emis(0);
  if (police) {
    k.mat(MAT_METAL); k.col(0x1f4fa0);
    k.b.Box({-2.26f, 0.55f, -0.89f}, {2.26f, 0.72f, 0.89f});
    k.mat(MAT_PLASTIC); k.col(0x2a6cff); k.emis(1.0f);
    k.box({-0.45f, 1.55f, -0.5f}, {0.45f, 1.68f, 0.5f});
    k.emis(0);
  }
}

void Stapelstuehle(PK& k) {
  for (int i = 0; i < 6; i++) {
    float y = i * 0.075f;
    k.mat(MAT_PLASTIC); k.col(0x3c5aa6);
    k.box({-0.21f, 0.44f + y, -0.21f}, {0.21f, 0.47f + y, 0.21f});
    k.box({-0.21f, 0.62f + y, -0.24f}, {0.21f, 0.84f + y, -0.21f});
  }
  k.mat(MAT_METAL); k.col(0x3a3d42);
  for (int x = -1; x <= 1; x += 2)
    for (int z = -1; z <= 1; z += 2) k.cyl({x * 0.19f, 0, z * 0.19f}, 0.012f, 0.44f, 8);
}

void Getraenkekiste(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x2f7d3a);
  k.box({-0.2f, 0, -0.15f}, {0.2f, 0.03f, 0.15f});
  k.box({-0.2f, 0.03f, -0.15f}, {-0.185f, 0.3f, 0.15f});
  k.box({0.185f, 0.03f, -0.15f}, {0.2f, 0.3f, 0.15f});
  k.box({-0.185f, 0.03f, -0.15f}, {0.185f, 0.3f, -0.135f});
  k.box({-0.185f, 0.03f, 0.135f}, {0.185f, 0.3f, 0.15f});
  k.mat(MAT_PLASTIC);
  for (int x = 0; x < 5; x++)
    for (int z = 0; z < 4; z++) {
      Vec3 c(-0.15f + x * 0.075f, 0.03f, -0.105f + z * 0.07f);
      k.col(0x5a3812);
      k.b.Cylinder(c, 0.027f, 0.22f, 10, false);
      k.b.Frustum(c + Vec3(0, 0.22f, 0), 0.027f, 0.012f, 0.05f, 10, false);
      k.mat(MAT_METAL); k.col(0xd0d0d0);
      k.b.Cylinder(c + Vec3(0, 0.27f, 0), 0.013f, 0.012f, 8);
      k.mat(MAT_PLASTIC);
    }
}

void Sonnenschirm(PK& k) {
  k.mat(MAT_CONCRETE); k.col(0x6a6a68);
  k.cyl({0, 0, 0}, 0.3f, 0.08f, 18);
  k.mat(MAT_METAL); k.col(0xd8d8d8);
  k.cyl({0, 0.08f, 0}, 0.025f, 2.25f, 8);
  k.mat(MAT_FABRIC); k.col(0xefe6d0);
  k.fr({0, 1.95f, 0}, 1.35f, 0.05f, 0.45f, 16);
  k.collision({0, 1.2f, 0}, {0.3f, 1.2f, 0.3f});
}

void Liegestuhl(PK& k) {
  k.mat(MAT_WOOD); k.col(0xb48a5a);
  for (int s = -1; s <= 1; s += 2) {
    k.tubeYZ({s * 0.28f, 0, 0.5f}, {s * 0.28f, 0.95f, -0.45f}, 0.02f);
    k.tubeYZ({s * 0.3f, 0, -0.4f}, {s * 0.3f, 0.5f, 0.05f}, 0.02f);
  }
  k.mat(MAT_FABRIC);
  for (int i = 0; i < 5; i++) {
    k.col(i % 2 ? 0xf4f4f0 : 0x2a6ab8);
    k.push(Translation({-0.25f + i * 0.1f + 0.05f, 0.47f, 0.02f}) * RotationX(0.78f));
    k.box({-0.05f, -0.01f, -0.6f}, {0.05f, 0.0f, 0.62f});
    k.pop();
  }
}

void Klimageraet(PK& k) {
  k.mat(MAT_METAL); k.col(0xb2b6bb);
  k.box({-0.65f, 0, -0.45f}, {0.65f, 0.95f, 0.45f});
  k.col(0x3a3d42);
  k.b.Ring({-0.3f, 0.951f, 0}, 0.05f, 0.3f, 20, true);
  k.b.Ring({0.33f, 0.951f, 0}, 0.05f, 0.25f, 20, true);
  for (int i = 0; i < 8; i++) k.b.Box({-0.6f, 0.1f + i * 0.1f, 0.45f}, {0.6f, 0.13f + i * 0.1f, 0.46f});
}

void Wahlurne(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x4c78b8);
  k.box({-0.22f, 0, -0.18f}, {0.22f, 0.45f, 0.18f});
  k.col(0x2b4b80);
  k.box({-0.23f, 0.45f, -0.19f}, {0.23f, 0.49f, 0.19f});
  k.col(0x0e0f12);
  k.b.Box({-0.12f, 0.49f, -0.01f}, {0.12f, 0.495f, 0.01f});
  k.mat(MAT_PAPER); k.col(0xf4f4f0);
  k.b.Box({-0.08f, 0.2f, 0.18f}, {0.08f, 0.34f, 0.183f});
  k.mat(MAT_PLASTIC); k.col(0x141414);
  k.push(Translation({0, 0.27f, 0.184f}) * Scaling(Vec3(0.05f)));
  BuildEagle(k.b);
  k.pop();
}

void Feuermelder(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xc81e1e);
  k.box({-0.07f, 0, -0.04f}, {0.07f, 0.14f, 0.04f});
  k.col(0xf4f4f4);
  k.b.Box({-0.04f, 0.03f, 0.04f}, {0.04f, 0.11f, 0.045f});
}

// --- Oktoberfest -----------------------------------------------------------------------------

void MugAt(PK& k, Vec3 o) {
  k.mat(MAT_PLASTIC); k.col(0xd6e4ea);
  k.cyl(o, 0.058f, 0.012f, 16);
  k.col(0xdc9a1c);
  k.cyl(o + Vec3(0, 0.012f, 0), 0.058f, 0.155f, 16);
  k.col(0xd6e4ea);
  k.cyl(o + Vec3(0, 0.167f, 0), 0.06f, 0.03f, 16);
  k.col(0xfaf6ea);
  k.b.Ellipsoid(o + Vec3(0, 0.19f, 0), Vec3(0.057f, 0.035f, 0.057f), 14, 6, 0.0f);
  k.mat(MAT_PLASTIC); k.col(0xd6e4ea);
  k.push(Translation(o + Vec3(0.068f, 0.1f, 0)) * RotationX(PI * 0.5f));
  k.torus({0, 0, 0}, 0.042f, 0.011f, 14, 6);
  k.pop();
}

void Masskrug(PK& k) { MugAt(k, Vec3(0, 0, 0)); }

void MasskrugTablett(PK& k) {
  k.mat(MAT_WOOD); k.col(0x8a5a33);
  k.box({-0.26f, 0, -0.17f}, {0.26f, 0.025f, 0.17f});
  for (int i = 0; i < 4; i++) MugAt(k, Vec3(-0.13f + (i % 2) * 0.26f - 0.03f, 0.025f, -0.08f + (i / 2) * 0.16f));
}

void WiesnBrezn(PK& k) {
  k.push(Scaling(Vec3(2.4f)));
  Brezel(k);
  k.pop();
}

void Lebkuchenherz(PK& k) {
  k.mat(MAT_WOOD); k.col(0x6b4a2e);
  k.box({-0.08f, 0, -0.06f}, {0.08f, 0.02f, 0.06f});
  k.box({-0.012f, 0.02f, -0.02f}, {0.012f, 0.15f, 0.0f});
  k.mat(MAT_PLASTIC); k.col(0x8a4a22);
  k.cylz({-0.075f, 0.38f, 0.0f}, 0.085f, 0.025f, 20);
  k.cylz({0.075f, 0.38f, 0.0f}, 0.085f, 0.025f, 20);
  ExtrudeConvex(k.b, {{-0.158f, 0.36f}, {0.0f, 0.13f}, {0.158f, 0.36f}}, -0.0125f, 0.0125f);
  k.part({-0.16f, 0.13f, -0.013f}, {0.16f, 0.47f, 0.013f});
  k.col(0xf6f2ea);
  k.b.Box({-0.1f, 0.3f, 0.0125f}, {0.1f, 0.36f, 0.016f});
  k.col(0xe04080);
  k.b.Box({-0.07f, 0.315f, 0.016f}, {0.07f, 0.345f, 0.018f});
  k.col(0x2a8a3a);
  k.b.Box({-0.13f, 0.4f, 0.0125f}, {-0.1f, 0.43f, 0.016f});
  k.b.Box({0.1f, 0.4f, 0.0125f}, {0.13f, 0.43f, 0.016f});
  k.mat(MAT_FABRIC); k.col(0x2a6ebb);
  k.b.Box({-0.01f, 0.43f, -0.005f}, {0.01f, 0.52f, 0.005f});
}

void Holzfass(PK& k) {
  k.mat(MAT_WOOD); k.col(0x5a3a22);
  k.box({-0.36f, 0, -0.32f}, {-0.26f, 0.16f, 0.32f});
  k.box({0.26f, 0, -0.32f}, {0.36f, 0.16f, 0.32f});
  k.col(0x9a6a3a);
  const float radii[6] = {0.29f, 0.32f, 0.34f, 0.34f, 0.32f, 0.29f};
  for (int i = 0; i < 6; i++) k.cylx({-0.375f + i * 0.15f, 0.47f, 0}, radii[i], 0.152f, 22);
  k.mat(MAT_METAL); k.col(0x2e2e30);
  for (float x : {-0.39f, -0.2f, 0.2f, 0.39f}) k.b.CylinderX({x, 0.47f, 0}, fabsf(x) > 0.3f ? 0.3f : 0.345f, 0.03f, 22);
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.cylx({0.49f, 0.4f, 0}, 0.025f, 0.08f, 10);
  k.box({0.52f, 0.36f, -0.012f}, {0.54f, 0.44f, 0.012f});
}

void Fass(PK& k) {
  k.mat(MAT_WOOD); k.col(0x9a6a3a);
  k.fr({0, 0, 0}, 0.23f, 0.27f, 0.3f, 22);
  k.fr({0, 0.3f, 0}, 0.27f, 0.23f, 0.3f, 22);
  k.col(0x7a5030);
  k.b.Cylinder({0, 0.6f, 0}, 0.225f, 0.01f, 22);
  k.mat(MAT_METAL); k.col(0x2e2e30);
  for (float y : {0.05f, 0.25f, 0.35f, 0.55f}) {
    float r = (y < 0.3f ? 0.23f + 0.04f * y / 0.3f : 0.27f - 0.04f * (y - 0.3f) / 0.3f) + 0.006f;
    k.b.Cylinder({0, y - 0.015f, 0}, r, 0.03f, 22, false);
  }
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.cylz({0, 0.15f, 0.27f}, 0.022f, 0.08f, 10);
}

void Bierbank(PK& k) {
  k.mat(MAT_WOOD); k.col(0xc0925a);
  k.box({-1.1f, 0.43f, -0.125f}, {1.1f, 0.47f, 0.125f});
  k.mat(MAT_METAL); k.col(0x55595f);
  for (int s = -1; s <= 1; s += 2) {
    k.push(Translation({s * 0.85f, 0, 0}) * RotationZ(s * 0.08f));
    k.box({-0.015f, 0.0f, -0.11f}, {0.015f, 0.43f, -0.09f});
    k.box({-0.015f, 0.0f, 0.09f}, {0.015f, 0.43f, 0.11f});
    k.b.Box({-0.015f, 0.02f, -0.11f}, {0.015f, 0.04f, 0.11f});
    k.pop();
  }
}

void Biertisch(PK& k) {
  k.mat(MAT_WOOD); k.col(0xc0925a);
  k.box({-1.1f, 0.73f, -0.25f}, {1.1f, 0.765f, 0.25f});
  k.col(0xa87a48);
  k.b.Box({-1.1f, 0.71f, -0.25f}, {1.1f, 0.73f, -0.23f});
  k.b.Box({-1.1f, 0.71f, 0.23f}, {1.1f, 0.73f, 0.25f});
  k.mat(MAT_METAL); k.col(0x55595f);
  for (int s = -1; s <= 1; s += 2) {
    k.box({s * 0.9f - 0.015f, 0.0f, -0.22f}, {s * 0.9f + 0.015f, 0.73f, -0.2f});
    k.box({s * 0.9f - 0.015f, 0.0f, 0.2f}, {s * 0.9f + 0.015f, 0.73f, 0.22f});
    k.b.Box({s * 0.9f - 0.015f, 0.02f, -0.22f}, {s * 0.9f + 0.015f, 0.04f, 0.22f});
  }
}

void Hendl(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf4f2ec);
  k.cyl({0, 0, 0}, 0.13f, 0.012f, 18);
  k.col(0xb5651d);
  k.ell({0, 0.06f, 0}, {0.085f, 0.05f, 0.065f}, 14, 8);
  k.col(0xa8581a);
  for (int s = -1; s <= 1; s += 2) {
    k.push(Translation({s * 0.06f, 0.04f, 0.05f}) * RotationX(1.1f) * RotationZ(s * 0.4f));
    k.cyl({0, 0, 0}, 0.012f, 0.07f, 8);
    k.sph({0, 0.075f, 0}, 0.022f, 8);
    k.pop();
  }
  k.mat(MAT_LEAVES); k.col(0x3f8a2c);
  k.b.Ellipsoid({-0.08f, 0.02f, -0.06f}, {0.025f, 0.012f, 0.02f}, 8, 4);
  k.mat(MAT_PLASTIC); k.col(0xf0d040);
  k.b.Ellipsoid({0.085f, 0.02f, -0.05f}, {0.02f, 0.008f, 0.012f}, 8, 4);
}

void Radi(PK& k) {
  k.mat(MAT_WOOD); k.col(0xb48a5a);
  k.box({-0.15f, 0, -0.09f}, {0.15f, 0.018f, 0.09f});
  k.mat(MAT_PLASTIC); k.col(0xf4f2ea);
  k.push(Translation({-0.02f, 0.05f, 0}) * RotationZ(PI * 0.5f));
  k.fr({0, -0.08f, 0}, 0.004f, 0.034f, 0.14f, 14);
  k.pop();
  k.col(0xe0dcd0);
  for (int i = 0; i < 5; i++) k.b.Cylinder({-0.08f + i * 0.03f, 0.05f, 0}, 0.0355f - i * 0.004f, 0.003f, 14, false);
  k.mat(MAT_LEAVES); k.col(0x4b8a34);
  k.ell({0.1f, 0.06f, 0.0f}, {0.04f, 0.025f, 0.035f}, 8, 5);
  k.ell({0.12f, 0.08f, 0.02f}, {0.03f, 0.02f, 0.025f}, 8, 5);
}

void Steckerlfisch(PK& k) {
  k.mat(MAT_WOOD); k.col(0x6b4a2e);
  k.box({-0.06f, 0, -0.06f}, {0.06f, 0.04f, 0.06f});
  k.col(0xc9a06a);
  k.cyl({0, 0.04f, 0}, 0.006f, 0.5f, 6);
  k.mat(MAT_PLASTIC); k.col(0xa86a2a);
  k.ell({0, 0.3f, 0}, {0.045f, 0.13f, 0.025f}, 12, 8);
  k.col(0x8a5520);
  ExtrudeConvex(k.b, {{-0.05f, 0.47f}, {0.0f, 0.41f}, {0.05f, 0.47f}}, -0.006f, 0.006f);
  k.part({-0.05f, 0.41f, -0.006f}, {0.05f, 0.47f, 0.006f});
  k.col(0x1a1a1a);
  k.b.Sphere({0.02f, 0.2f, 0.022f}, 0.006f, 6, 4);
}

void Trachtenhut(PK& k) {
  k.mat(MAT_FABRIC); k.col(0x2f4a2e);
  k.cyl({0, 0, 0}, 0.17f, 0.012f, 22);
  k.fr({0, 0.012f, 0}, 0.11f, 0.085f, 0.12f, 18);
  k.col(0x1d2a1c);
  k.b.Cylinder({0, 0.02f, 0}, 0.112f, 0.025f, 18, false);
  k.mat(MAT_FABRIC); k.col(0x6a5a48);
  for (int i = 0; i < 9; i++) {
    float a = -0.6f + i * 0.15f;
    k.push(Translation({0.0f, 0.04f, -0.1f}) * RotationX(-0.35f) * RotationZ(a));
    k.b.Cylinder({0, 0, 0}, 0.006f, 0.16f + (i % 3) * 0.02f, 5);
    k.pop();
  }
  k.part({-0.1f, 0.04f, -0.16f}, {0.1f, 0.22f, -0.08f});
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.b.Box({-0.015f, 0.025f, -0.115f}, {0.015f, 0.05f, -0.105f});
}

void Tuba(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2c30);
  for (int i = 0; i < 3; i++) {
    k.push(RotationY(i * TAU / 3));
    k.tubeYZ({0, 0.45f, 0}, {0, 0, 0.25f}, 0.012f);
    k.pop();
  }
  k.mat(MAT_GOLD); k.col(0xd8b24a);
  k.push(Translation({0, 0.62f, 0}) * RotationX(PI * 0.5f));
  k.torus({0, 0, 0}, 0.16f, 0.05f, 22, 10);
  k.pop();
  k.fr({0.06f, 0.72f, 0.0f}, 0.06f, 0.22f, 0.38f, 22);
  k.cyl({-0.08f, 0.62f, 0.0f}, 0.02f, 0.28f, 8);
  for (int i = 0; i < 3; i++) k.cyl({-0.12f + i * 0.035f, 0.7f, 0.06f}, 0.012f, 0.08f, 8);
  k.cyl({-0.2f, 0.85f, 0.0f}, 0.008f, 0.16f, 6);
}

void Pauke(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2c30);
  for (int s = -1; s <= 1; s += 2) {
    k.tubeYZ({s * 0.22f, 0.0f, -0.25f}, {s * 0.22f, 0.4f, 0.0f}, 0.012f);
    k.tubeYZ({s * 0.22f, 0.0f, 0.25f}, {s * 0.22f, 0.4f, 0.0f}, 0.012f);
  }
  k.mat(MAT_PLASTIC); k.col(0xb01e1e);
  k.cylz({0, 0.62f, 0}, 0.32f, 0.32f, 26);
  k.col(0xf4f0e6);
  k.b.CylinderZ({0, 0.62f, 0.165f}, 0.325f, 0.012f, 26);
  k.b.CylinderZ({0, 0.62f, -0.165f}, 0.325f, 0.012f, 26);
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  for (float z : {0.172f, -0.172f}) {
    k.push(Translation({0, 0.62f, z}) * RotationX(PI * 0.5f));
    k.b.Torus({0, 0, 0}, 0.325f, 0.012f, 26, 5);
    k.pop();
  }
}

void Notenstaender(PK& k) {
  k.mat(MAT_METAL); k.col(0x1d1e21);
  for (int i = 0; i < 3; i++) {
    k.push(RotationY(i * TAU / 3));
    k.tubeYZ({0, 0.25f, 0}, {0, 0, 0.25f}, 0.008f);
    k.pop();
  }
  k.cyl({0, 0.2f, 0}, 0.01f, 0.85f, 8);
  k.push(Translation({0, 1.05f, 0}) * RotationX(-0.35f));
  k.box({-0.24f, 0, -0.01f}, {0.24f, 0.32f, 0.005f});
  k.mat(MAT_PAPER); k.col(0xf4f0e2);
  k.b.Box({-0.2f, 0.03f, 0.005f}, {0.2f, 0.3f, 0.008f});
  k.mat(MAT_PLASTIC); k.col(0x1a1a1a);
  for (int i = 0; i < 6; i++) k.b.Box({-0.18f, 0.07f + i * 0.04f, 0.008f}, {0.18f, 0.074f + i * 0.04f, 0.009f});
  k.pop();
}

void Teddy(PK& k) {
  const u32 fur = 0x8a5a2e, light = 0xc89a6a;
  k.mat(MAT_FABRIC); k.col(fur);
  k.ell({0, 0.3f, 0}, {0.2f, 0.25f, 0.17f}, 16, 10);
  k.sph({0, 0.64f, 0.02f}, 0.15f, 16);
  k.sph({-0.11f, 0.77f, 0.0f}, 0.05f, 10);
  k.sph({0.11f, 0.77f, 0.0f}, 0.05f, 10);
  k.ell({-0.2f, 0.36f, 0.05f}, {0.06f, 0.13f, 0.06f}, 10, 6);
  k.ell({0.2f, 0.36f, 0.05f}, {0.06f, 0.13f, 0.06f}, 10, 6);
  k.ell({-0.11f, 0.08f, 0.12f}, {0.07f, 0.07f, 0.13f}, 10, 6);
  k.ell({0.11f, 0.08f, 0.12f}, {0.07f, 0.07f, 0.13f}, 10, 6);
  k.col(light);
  k.b.Ellipsoid({0, 0.6f, 0.14f}, {0.06f, 0.045f, 0.04f}, 10, 6);
  k.b.Ellipsoid({0, 0.3f, 0.15f}, {0.12f, 0.15f, 0.04f}, 12, 6);
  k.mat(MAT_PLASTIC); k.col(0x111111);
  k.b.Sphere({-0.05f, 0.68f, 0.14f}, 0.016f, 6, 4);
  k.b.Sphere({0.05f, 0.68f, 0.14f}, 0.016f, 6, 4);
  k.b.Sphere({0, 0.62f, 0.18f}, 0.018f, 6, 4);
  k.col(0xd01c2c);
  k.b.Ellipsoid({0, 0.5f, 0.12f}, {0.08f, 0.03f, 0.03f}, 10, 5);
}

void Zuckerwatte(PK& k) {
  k.mat(MAT_WOOD); k.col(0x6b4a2e);
  k.cyl({0, 0, 0}, 0.06f, 0.03f, 12);
  k.col(0xe8d8b8);
  k.cyl({0, 0.03f, 0}, 0.008f, 0.3f, 6);
  k.mat(MAT_FABRIC); k.col(0xff9ad5);
  k.ell({0, 0.45f, 0}, {0.13f, 0.15f, 0.13f}, 14, 9);
  k.col(0xffb8e2);
  k.b.Ellipsoid({0.04f, 0.53f, 0.03f}, {0.08f, 0.08f, 0.08f}, 10, 6);
}

void Ballons(PK& k) {
  k.mat(MAT_METAL); k.col(0x8d9299);
  k.cyl({0, 0, 0}, 0.08f, 0.05f, 12);
  const u32 cols[5] = {0xe0242c, 0x2a6ebb, 0xf2c230, 0x2fa04a, 0xd84ab8};
  for (int i = 0; i < 5; i++) {
    float a = i * TAU / 5;
    Vec3 top(cosf(a) * 0.22f, 1.45f + (i % 2) * 0.15f, sinf(a) * 0.22f);
    k.mat(MAT_PLASTIC); k.col(0xeeeeee);
    k.parts = false;
    Vec3 d = top - Vec3(0, 0.05f, 0);
    float len = Length(d);
    Vec3 y = d / len, ref(1, 0, 0);
    Vec3 x = Normalize(Cross(ref, y)), z = Cross(x, y);
    Mat4 m = Mat4::Identity();
    m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z; m.m[4] = y.x; m.m[5] = y.y; m.m[6] = y.z;
    m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z; m.m[13] = 0.05f;
    k.push(m);
    k.b.Box({-0.003f, 0, -0.003f}, {0.003f, len, 0.003f});
    k.pop();
    k.parts = true;
    k.col(cols[i]);
    k.ell(top + Vec3(0, 0.15f, 0), {0.13f, 0.16f, 0.13f}, 14, 9);
  }
  k.collision({0, 0.9f, 0}, {0.15f, 0.9f, 0.15f});
}

void Mandeln(PK& k) {
  k.mat(MAT_WOOD); k.col(0x6b4a2e);
  k.cyl({0, 0, 0}, 0.05f, 0.02f, 12);
  k.mat(MAT_PAPER); k.col(0xf4f0e6);
  k.fr({0, 0.02f, 0}, 0.012f, 0.065f, 0.2f, 14);
  k.col(0xd0303a);
  k.b.Frustum({0, 0.12f, 0}, 0.04f, 0.052f, 0.04f, 14, false);
  k.mat(MAT_PLASTIC); k.col(0x7a4a22);
  for (int i = 0; i < 9; i++) {
    float a = i * 2.4f;
    k.b.Ellipsoid({cosf(a) * 0.03f * (i % 3) * 0.6f, 0.225f + (i % 2) * 0.012f, sinf(a) * 0.03f * (i % 3) * 0.6f}, {0.018f, 0.01f, 0.012f}, 8, 4);
  }
}

void Lampion(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2c30);
  k.cyl({0, 0, 0}, 0.12f, 0.025f, 12);
  k.cyl({0, 0.025f, 0}, 0.01f, 0.86f, 6);
  k.mat(MAT_PAPER); k.emis(0.55f);
  const float pr[5] = {0.1f, 0.18f, 0.21f, 0.18f, 0.1f};
  for (int i = 0; i < 4; i++) {
    k.col(i % 2 ? 0xf4f0e6 : 0xd8302a);
    k.b.Frustum({0, 0.9f + i * 0.09f, 0}, pr[i], pr[i + 1], 0.09f, 16, i == 0 || i == 3);
  }
  k.part({-0.22f, 0.9f, -0.22f}, {0.22f, 1.26f, 0.22f});
  k.emis(0);
  k.def->light = true;
  k.def->lightOffset = {0, 1.08f, 0};
  k.def->lightColor = {2.4f, 1.3f, 0.7f};
  k.def->lightRadius = 4.5f;
}

void Strohballen(PK& k) {
  k.mat(MAT_FABRIC); k.col(0xd8b860);
  k.box({-0.55f, 0, -0.35f}, {0.55f, 0.45f, 0.35f});
  k.col(0x6b5a30);
  for (float x : {-0.3f, 0.3f}) k.b.Box({x - 0.012f, -0.002f, -0.352f}, {x + 0.012f, 0.452f, 0.352f});
}

void Gartenzwerg(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x5a3a22);
  k.box({-0.09f, 0, -0.06f}, {0.09f, 0.05f, 0.09f});
  k.col(0x2a6a2e);
  k.box({-0.08f, 0.05f, -0.05f}, {0.08f, 0.14f, 0.06f});
  k.col(0x2a5aaa);
  k.fr({0, 0.14f, 0}, 0.11f, 0.08f, 0.16f, 14);
  k.col(0x5a3a22);
  k.b.Cylinder({0, 0.18f, 0}, 0.102f, 0.025f, 14, false);
  k.col(0xf0c8a0);
  k.sph({0, 0.36f, 0.01f}, 0.07f, 12);
  k.col(0xf6f6f6);
  k.ell({0, 0.31f, 0.05f}, {0.065f, 0.07f, 0.04f}, 10, 6);
  k.col(0xd8202a);
  k.fr({0, 0.4f, 0}, 0.075f, 0.0f, 0.2f, 14);
  k.sph({0.05f, 0.22f, 0.08f}, 0.025f, 8);
}

void Holzpferd(PK& k) {
  k.mat(MAT_GOLD); k.col(0xd8b24a);
  k.cyl({0, 0, 0}, 0.25f, 0.04f, 18);
  k.cyl({0, 0.04f, 0}, 0.025f, 2.1f, 10);
  k.mat(MAT_PLASTIC); k.col(0xf4f0ea);
  k.ell({0, 0.95f, 0}, {0.13f, 0.16f, 0.42f}, 14, 9);
  k.push(Translation({0, 1.05f, 0.33f}) * RotationX(-0.6f));
  k.ell({0, 0.12f, 0}, {0.08f, 0.18f, 0.09f}, 10, 7);
  k.pop();
  k.ell({0, 1.32f, 0.5f}, {0.07f, 0.08f, 0.15f}, 10, 7);
  for (int s = -1; s <= 1; s += 2)
    for (int f = -1; f <= 1; f += 2) {
      k.push(Translation({s * 0.07f, 0.85f, f * 0.28f}) * RotationX(f * 0.5f));
      k.box({-0.025f, -0.42f, -0.025f}, {0.025f, 0.0f, 0.025f});
      k.pop();
    }
  k.col(0xd8202a);
  k.b.Box({-0.135f, 0.98f, -0.15f}, {0.135f, 1.13f, 0.15f});
  k.col(0x2a6ebb);
  k.b.Box({-0.137f, 1.02f, -0.1f}, {0.137f, 1.08f, 0.1f});
  k.col(0x5a3a22);
  k.ell({0, 1.2f, 0.42f}, {0.03f, 0.12f, 0.1f}, 8, 5);
}

void Trachtenpuppe(PK& k) {
  k.mat(MAT_METAL); k.col(0x2a2c30);
  k.cyl({0, 0, 0}, 0.2f, 0.03f, 16);
  k.cyl({0, 0.03f, 0}, 0.015f, 0.2f, 8);
  k.mat(MAT_FABRIC); k.col(0x2a6a3e);
  k.fr({0, 0.2f, 0}, 0.3f, 0.15f, 0.72f, 20);
  k.col(0x3a5aa8);
  k.b.Box({-0.12f, 0.3f, 0.2f}, {0.12f, 0.88f, 0.215f});
  k.col(0x1d1d22);
  k.box({-0.15f, 0.9f, -0.1f}, {0.15f, 1.2f, 0.1f});
  k.col(0xf6f6f6);
  k.box({-0.14f, 1.2f, -0.09f}, {0.14f, 1.32f, 0.09f});
  k.ell({-0.19f, 1.22f, 0}, {0.06f, 0.07f, 0.06f}, 10, 6);
  k.ell({0.19f, 1.22f, 0}, {0.06f, 0.07f, 0.06f}, 10, 6);
  k.mat(MAT_PLASTIC); k.col(0xf0ece4);
  k.cyl({0, 1.32f, 0}, 0.04f, 0.06f, 8);
  k.ell({0, 1.48f, 0}, {0.09f, 0.11f, 0.1f}, 12, 8);
  k.col(0xd8304a);
  k.b.Box({-0.06f, 1.0f, 0.1f}, {0.06f, 1.04f, 0.105f});
}

void Autoscooter(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1a1a1a);
  k.push(Translation({0, 0.2f, 0}) * Scaling(Vec3(0.68f, 1.0f, 1.05f)));
  k.b.Torus({0, 0, 0}, 0.88f, 0.1f, 28, 8);
  k.pop();
  k.part({-0.67f, 0.1f, -1.03f}, {0.67f, 0.3f, 1.03f});
  k.mat(MAT_PLASTIC); k.col(0xc8141c);
  k.box({-0.58f, 0.12f, -0.95f}, {0.58f, 0.48f, 0.9f});
  k.wedge({-0.58f, 0.48f, 0.2f}, {0.58f, 0.48f, 0.9f}, 0.12f, 0.0f);
  k.col(0xf2c230);
  k.b.Box({-0.585f, 0.3f, -0.95f}, {0.585f, 0.36f, 0.9f});
  k.mat(MAT_LEATHER); k.col(0x1d1d22);
  k.box({-0.4f, 0.48f, -0.7f}, {0.4f, 0.62f, -0.1f});
  k.box({-0.4f, 0.48f, -0.85f}, {0.4f, 1.0f, -0.7f});
  k.mat(MAT_PLASTIC); k.col(0x222222);
  k.push(Translation({0, 0.82f, 0.1f}) * RotationX(1.1f));
  k.torus({0, 0, 0}, 0.13f, 0.018f, 16, 6);
  k.pop();
  k.mat(MAT_METAL); k.col(0x9aa0a8);
  k.cyl({0, 0.48f, -0.9f}, 0.02f, 1.8f, 8);
  k.mat(MAT_PLASTIC); k.col(0x7fd0ff); k.emis(1.0f);
  k.b.Sphere({0, 2.3f, -0.9f}, 0.04f, 8, 5);
  k.emis(0);
  k.collision({0, 0.4f, 0}, {0.68f, 0.4f, 1.03f});
}

void Dixi(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x2a6ab8);
  k.box({-0.55f, 0, -0.55f}, {0.55f, 2.25f, 0.55f});
  k.col(0xe8eef4);
  k.box({-0.6f, 2.25f, -0.6f}, {0.6f, 2.35f, 0.6f});
  k.col(0x235aa0);
  k.b.Box({-0.42f, 0.05f, 0.55f}, {0.42f, 2.1f, 0.56f});
  k.col(0xe8eef4);
  k.b.Box({-0.1f, 1.7f, 0.56f}, {0.1f, 1.85f, 0.565f});
  k.col(0x2a8a3a);
  k.b.Box({0.28f, 1.05f, 0.56f}, {0.36f, 1.12f, 0.57f});
  k.col(0x1a1a1a);
  k.cyl({0.35f, 2.35f, -0.35f}, 0.04f, 0.3f, 8);
}

void Lautsprecher(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x18191b);
  k.box({-0.26f, 0, -0.22f}, {0.26f, 0.95f, 0.22f});
  k.col(0x2e3034);
  k.b.CylinderZ({0, 0.3f, 0.225f}, 0.17f, 0.01f, 20);
  k.b.CylinderZ({0, 0.72f, 0.225f}, 0.08f, 0.01f, 16);
  k.col(0x0e0f10);
  k.b.CylinderZ({0, 0.3f, 0.232f}, 0.06f, 0.006f, 14);
  k.col(0x3be36b); k.emis(1.0f);
  k.b.Box({0.18f, 0.88f, 0.22f}, {0.2f, 0.9f, 0.225f});
  k.emis(0);
}

void Kerze(PK& k) {
  k.mat(MAT_GOLD); k.col(0xc9a24a);
  k.cyl({0, 0, 0}, 0.05f, 0.015f, 12);
  k.cyl({0, 0.015f, 0}, 0.012f, 0.04f, 8);
  k.mat(MAT_PLASTIC); k.col(0xf4f0e2);
  k.cyl({0, 0.055f, 0}, 0.022f, 0.14f, 12);
  k.col(0xffc060); k.emis(1.0f);
  k.b.Ellipsoid({0, 0.215f, 0}, {0.008f, 0.02f, 0.008f}, 8, 5);
  k.emis(0);
  k.def->light = true;
  k.def->lightOffset = {0, 0.25f, 0};
  k.def->lightColor = {0.9f, 0.6f, 0.3f};
  k.def->lightRadius = 2.2f;
}

void SchirmBW(PK& k) {
  k.mat(MAT_CONCRETE); k.col(0x6a6a68);
  k.cyl({0, 0, 0}, 0.3f, 0.08f, 18);
  k.mat(MAT_WOOD); k.col(0xb48a5a);
  k.cyl({0, 0.08f, 0}, 0.03f, 2.3f, 8);
  k.mat(MAT_FABRIC);
  const int seg = 16;
  for (int i = 0; i < seg; i++) {
    float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
    k.col(i % 2 ? 0xf4f4f0 : 0x2a6ebb);
    Vec3 apex(0, 2.45f, 0), p0(cosf(a0) * 1.5f, 2.0f, sinf(a0) * 1.5f), p1(cosf(a1) * 1.5f, 2.0f, sinf(a1) * 1.5f);
    k.b.Tri(apex, p1, p0);
    k.b.Tri(apex, p0, p1);
  }
  k.part({-1.5f, 2.0f, -1.5f}, {1.5f, 2.45f, 1.5f});
  k.collision({0, 1.2f, 0}, {0.3f, 1.2f, 0.3f});
}

// --- Nudelfabrik ------------------------------------------------------------------------------

// Europalette 1,2 x 0,8 m, Oberkante 0,144 m (Ursprung = Bodenmitte + o)
void PaletteAt(PK& k, Vec3 o) {
  k.mat(MAT_WOOD); k.col(0xc8a070);
  for (float z : {-0.35f, 0.0f, 0.35f}) k.b.Box(o + Vec3(-0.6f, 0, z - 0.05f), o + Vec3(0.6f, 0.022f, z + 0.05f));
  k.col(0xb08a5a);
  for (float x : {-0.55f, 0.0f, 0.55f})
    for (float z : {-0.35f, 0.0f, 0.35f}) k.b.Box(o + Vec3(x - 0.05f, 0.022f, z - 0.05f), o + Vec3(x + 0.05f, 0.1f, z + 0.05f));
  k.col(0xc8a070);
  for (float z : {-0.35f, 0.0f, 0.35f}) k.b.Box(o + Vec3(-0.6f, 0.1f, z - 0.05f), o + Vec3(0.6f, 0.122f, z + 0.05f));
  k.col(0xd2ab7a);
  for (int i = 0; i < 5; i++) {
    float x = -0.55f + i * 0.275f;
    k.b.Box(o + Vec3(x - 0.05f, 0.122f, -0.4f), o + Vec3(x + 0.05f, 0.144f, 0.4f));
  }
  k.part(o + Vec3(-0.6f, 0, -0.4f), o + Vec3(0.6f, 0.144f, 0.4f));
}

// Gefüllter Mehlsack (liegend), c = Mitte der Unterseite
void SackAt(PK& k, Vec3 c, u32 col, bool text) {
  k.mat(MAT_PAPER); k.col(col);
  k.b.Box(c + Vec3(-0.28f, 0, -0.18f), c + Vec3(0.28f, 0.11f, 0.18f));
  k.b.Ellipsoid(c + Vec3(0, 0.11f, 0), {0.27f, 0.05f, 0.17f}, 14, 5);
  k.col(0xcfc3a5);
  for (float x : {-0.29f, 0.29f}) k.b.Box(c + Vec3(x - 0.012f, 0.02f, -0.185f), c + Vec3(x + 0.012f, 0.1f, 0.185f));  // Falz
  k.mat(MAT_PLASTIC); k.col(0x2a5aa8);
  k.b.Box(c + Vec3(-0.27f, 0.035f, 0.18f), c + Vec3(0.27f, 0.065f, 0.183f));
  if (text) {
    k.col(0xb03020);
    k.push(Translation(c + Vec3(-PixelTextWidth("MEHL", 0.02f) * 0.5f, 0.157f, 0.07f)) * RotationX(-PI * 0.5f));
    BuildPixelText(k.b, "MEHL", 0.02f, 0.003f);
    k.pop();
  }
}

void Mehlsack(PK& k) {
  SackAt(k, Vec3(0, 0, 0), 0xe6dcc4, true);
  k.part({-0.3f, 0, -0.19f}, {0.3f, 0.16f, 0.19f});
}

void Mehlpalette(PK& k) {
  PaletteAt(k, Vec3(0, 0, 0));
  for (int l = 0; l < 4; l++)
    for (int i = 0; i < 4; i++) {
      Vec3 c(-0.3f + (i % 2) * 0.6f, 0.144f + l * 0.16f, -0.2f + (i / 2) * 0.4f);
      SackAt(k, c + Vec3((l % 2 ? 0.01f : -0.01f), 0, 0), (i + l) % 2 ? 0xe6dcc4 : 0xe0d4b8, l == 3);
    }
  k.part({-0.6f, 0.144f, -0.4f}, {0.6f, 0.144f + 4 * 0.16f, 0.4f});
}

void Palette(PK& k) { PaletteAt(k, Vec3(0, 0, 0)); }

void Nudelpackung(PK& k) {
  k.mat(MAT_PAPER); k.col(0x1f4fa0);
  k.box({-0.04f, 0, -0.016f}, {0.04f, 0.27f, 0.016f});
  k.mat(MAT_PLASTIC); k.col(0xe8c35a);
  k.b.Box({-0.022f, 0.06f, 0.016f}, {0.022f, 0.205f, 0.0175f});  // Sichtfenster
  k.col(0xd8202a);
  k.b.Box({-0.04f, 0.222f, 0.016f}, {0.04f, 0.252f, 0.0172f});
  k.col(0xf4f0e6);
  k.b.Box({-0.03f, 0.02f, 0.016f}, {0.03f, 0.045f, 0.0172f});
}

void Farfalle(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf0c860);
  k.ell({0, 0.12f, 0}, {0.11f, 0.12f, 0.045f}, 14, 9);
  k.col(0x2a8a4a);
  k.b.Box({-0.1f, 0.22f, -0.01f}, {0.1f, 0.25f, 0.01f});  // Schweißnaht
  k.col(0xf6f0e2);
  k.b.Box({-0.055f, 0.08f, 0.032f}, {0.055f, 0.15f, 0.05f});  // Etikett
  k.col(0xd8202a);
  k.b.Box({-0.04f, 0.1f, 0.05f}, {0.04f, 0.13f, 0.052f});
}

void Nudelkarton(PK& k) {
  k.mat(MAT_PAPER); k.col(0xc49a62);
  k.box({-0.3f, 0, -0.2f}, {0.3f, 0.4f, 0.2f});
  k.col(0xd8b884);
  k.b.Box({-0.3f, 0.4f, -0.03f}, {0.3f, 0.403f, 0.03f});  // Klebeband
  k.mat(MAT_PLASTIC); k.col(0xb02a20);
  k.push(Translation({-PixelTextWidth("PASTA", 0.018f) * 0.5f, 0.2f, 0.2f}));
  BuildPixelText(k.b, "PASTA", 0.018f, 0.003f);
  k.pop();
  k.col(0x1a1a1a);
  for (int i = 0; i < 9; i++) k.b.Box({-0.24f + i * 0.012f, 0.06f, 0.2f}, {-0.236f + i * 0.012f - (i % 3) * 0.001f, 0.12f, 0.202f});  // Strichcode
}

void Kartonpalette(PK& k) {
  PaletteAt(k, Vec3(0, 0, 0));
  k.mat(MAT_PAPER);
  for (int l = 0; l < 3; l++)
    for (int i = 0; i < 4; i++) {
      float x0 = -0.6f + (i % 2) * 0.6f, z0 = -0.4f + (i / 2) * 0.4f, y0 = 0.144f + l * 0.4f;
      k.col((i + l) % 2 ? 0xc49a62 : 0xb88e58);
      k.b.Box({x0 + 0.005f, y0, z0 + 0.005f}, {x0 + 0.595f, y0 + 0.395f, z0 + 0.395f});
      if (i / 2 == 1) {
        k.mat(MAT_PLASTIC); k.col(0xb02a20);
        k.push(Translation({x0 + 0.3f - PixelTextWidth("PASTA", 0.012f) * 0.5f, y0 + 0.18f, 0.395f}));
        BuildPixelText(k.b, "PASTA", 0.012f, 0.002f);
        k.pop();
        k.mat(MAT_PAPER);
      }
    }
  k.mat(MAT_PLASTIC); k.col(0xeef0f2);  // Umreifungsbänder
  for (float x : {-0.32f, 0.28f}) {
    k.b.Box({x, 0.144f, 0.4f}, {x + 0.04f, 1.344f, 0.404f});
    k.b.Box({x, 0.144f, -0.404f}, {x + 0.04f, 1.344f, -0.4f});
    k.b.Box({x, 1.344f, -0.404f}, {x + 0.04f, 1.348f, 0.404f});
  }
  k.part({-0.6f, 0.144f, -0.4f}, {0.6f, 1.344f, 0.4f});
}

void Tomatendose(PK& k) {
  k.mat(MAT_METAL); k.col(0xb8bcc2);
  k.cyl({0, 0, 0}, 0.065f, 0.02f, 20);
  k.cyl({0, 0.122f, 0}, 0.065f, 0.018f, 20);
  k.mat(MAT_PAPER); k.col(0xc8281e);  // Etikett
  k.cyl({0, 0.02f, 0}, 0.065f, 0.068f, 20);
  k.col(0x2f8a3a);
  k.b.Cylinder({0, 0.088f, 0}, 0.065f, 0.014f, 20);
  k.col(0xf4e6c0);
  k.b.Cylinder({0, 0.102f, 0}, 0.065f, 0.02f, 20);
  k.mat(MAT_METAL); k.col(0x9aa0a6);
  k.b.Torus({0, 0.14f, 0}, 0.06f, 0.006f, 20, 5);
  k.b.Torus({0, 0.004f, 0}, 0.06f, 0.006f, 20, 5);
}

void Olivenoel(PK& k) {
  k.mat(MAT_METAL); k.col(0xc8b860);
  k.b.Box({-0.08f, 0, -0.055f}, {0.08f, 0.05f, 0.055f});
  k.b.Box({-0.08f, 0.22f, -0.055f}, {0.08f, 0.3f, 0.055f});
  k.mat(MAT_PAPER); k.col(0x2f6a2a);
  k.b.Box({-0.08f, 0.05f, -0.055f}, {0.08f, 0.22f, 0.055f});
  k.col(0xe8d070);
  k.b.Box({-0.05f, 0.09f, 0.055f}, {0.05f, 0.18f, 0.057f});
  k.col(0x2f6a2a);
  k.b.Ellipsoid({0, 0.135f, 0.057f}, {0.03f, 0.018f, 0.002f}, 8, 4);  // Olive
  k.mat(MAT_METAL); k.col(0x8a8e94);
  k.b.Cylinder({0.045f, 0.3f, 0}, 0.014f, 0.035f, 10);
  k.b.Box({-0.055f, 0.3f, -0.006f}, {0.0f, 0.335f, 0.006f});
  k.part({-0.08f, 0, -0.055f}, {0.08f, 0.335f, 0.055f});
}

void Parmesan(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xd2ae5c);
  k.fr({0, 0, 0}, 0.2f, 0.215f, 0.06f, 28);
  k.fr({0, 0.06f, 0}, 0.215f, 0.2f, 0.06f, 28);
  k.col(0xe6cf8a);
  k.b.Cylinder({0, 0.12f, 0}, 0.19f, 0.004f, 28);
  k.mat(MAT_PAPER); k.col(0xf4f0e6);
  k.b.Cylinder({0, 0.124f, 0}, 0.06f, 0.002f, 18);
}

void Teigwanne(PK& k) {
  const float y0 = 0.16f, y1 = 0.78f;
  k.mat(MAT_METAL); k.col(0xc0c6cc);
  k.box({-0.45f, y0, -0.3f}, {0.45f, y0 + 0.03f, 0.3f});
  k.box({-0.45f, y0 + 0.03f, -0.3f}, {-0.42f, y1, 0.3f});
  k.box({0.42f, y0 + 0.03f, -0.3f}, {0.45f, y1, 0.3f});
  k.box({-0.42f, y0 + 0.03f, -0.3f}, {0.42f, y1, -0.27f});
  k.box({-0.42f, y0 + 0.03f, 0.27f}, {0.42f, y1, 0.3f});
  k.mat(MAT_PLASTIC); k.col(0xf0dca8);  // Teig
  k.b.Box({-0.42f, y0 + 0.03f, -0.27f}, {0.42f, 0.6f, 0.27f});
  k.b.Ellipsoid({-0.08f, 0.6f, 0.02f}, {0.3f, 0.07f, 0.2f}, 14, 5);
  k.mat(MAT_METAL); k.col(0x8a9096);
  k.b.Box({0.45f, 0.64f, -0.2f}, {0.5f, 0.68f, 0.2f});  // Schiebegriff
  for (float sx : {-1.0f, 1.0f})
    for (float sz : {-1.0f, 1.0f}) {
      k.col(0x8a9096);
      k.b.Cylinder({sx * 0.36f, 0.09f, sz * 0.22f}, 0.012f, 0.07f, 8);
      k.mat(MAT_PLASTIC); k.col(0x1d1e20);
      k.b.CylinderX({sx * 0.36f, 0.05f, sz * 0.22f}, 0.05f, 0.04f, 12);
      k.mat(MAT_METAL);
    }
  k.part({-0.45f, 0, -0.3f}, {0.45f, y0, 0.3f});
}

void Trockenwagen(PK& k) {
  k.mat(MAT_METAL); k.col(0xb8bec4);
  for (float sx : {-0.65f, 0.65f})
    for (float sz : {-0.32f, 0.32f}) k.b.Cylinder({sx, 0.1f, sz}, 0.02f, 1.85f, 8);
  for (float y : {0.12f, 1.93f})
    for (float sz : {-0.32f, 0.32f}) k.b.CylinderX({0, y, sz}, 0.018f, 1.32f, 8);
  for (float y : {0.12f, 1.93f})
    for (float sx : {-0.65f, 0.65f}) k.b.CylinderZ({sx, y, 0}, 0.018f, 0.66f, 8);
  Rng r(17);
  for (int lv = 0; lv < 3; lv++) {
    float y = 0.8f + lv * 0.52f;
    for (float pz : {-0.16f, 0.16f}) {
      k.mat(MAT_WOOD); k.col(0xd8c090);
      k.b.CylinderX({0, y, pz}, 0.012f, 1.28f, 6);
      k.mat(MAT_PLASTIC);
      for (int i = 0; i < 12; i++) {
        float x = -0.56f + i * 0.102f, len = 0.36f + r.Range(-0.04f, 0.04f);
        k.col(i % 3 ? 0xead07a : 0xe2c464);
        for (float side : {-1.0f, 1.0f})
          k.b.Box({x - 0.02f, y - len, pz + side * 0.008f - 0.005f}, {x + 0.02f, y - 0.01f, pz + side * 0.008f + 0.005f});
      }
    }
  }
  k.mat(MAT_PLASTIC); k.col(0x1d1e20);
  for (float sx : {-0.65f, 0.65f})
    for (float sz : {-0.32f, 0.32f}) k.b.CylinderX({sx, 0.05f, sz}, 0.05f, 0.04f, 12);
  k.part({-0.68f, 0, -0.35f}, {0.68f, 1.95f, 0.35f});
}

void Hubwagen(PK& k) {
  k.mat(MAT_METAL); k.col(0x2c2e32);
  for (float sx : {-0.27f, 0.27f}) k.box({sx - 0.08f, 0.0f, -0.62f}, {sx + 0.08f, 0.085f, 0.5f});
  k.col(0xe0a020);
  k.box({-0.36f, 0.0f, 0.5f}, {0.36f, 0.32f, 0.72f});
  k.col(0x2c2e32);
  k.cyl({0, 0.32f, 0.61f}, 0.05f, 0.25f, 10);
  k.push(Translation({0, 0.55f, 0.62f}) * RotationX(0.35f));
  k.col(0xe0a020);
  k.box({-0.025f, 0, -0.025f}, {0.025f, 0.62f, 0.025f});
  k.col(0x1d1e20);
  k.box({-0.15f, 0.62f, -0.03f}, {0.15f, 0.68f, 0.03f});
  k.pop();
  k.mat(MAT_PLASTIC); k.col(0x1a1b1d);
  k.b.CylinderX({0, 0.08f, 0.8f}, 0.08f, 0.12f, 14);
}

void Gabelstapler(PK& k) {
  const u32 yellow = 0xe8a51c;
  k.mat(MAT_METAL); k.col(yellow);
  k.box({-0.55f, 0.2f, -0.6f}, {0.55f, 1.0f, 0.95f});
  k.col(0x2c2e32);
  k.box({-0.58f, 0.2f, 0.95f}, {0.58f, 1.15f, 1.3f});  // Gegengewicht
  k.col(yellow);
  for (float sx : {-0.5f, 0.5f}) {
    k.box({sx - 0.035f, 1.0f, -0.5f}, {sx + 0.035f, 2.15f, -0.43f});
    k.box({sx - 0.035f, 1.0f, 0.82f}, {sx + 0.035f, 2.15f, 0.89f});
  }
  k.box({-0.55f, 2.15f, -0.52f}, {0.55f, 2.2f, 0.91f});  // Schutzdach
  k.col(0x1d1e20);
  for (int i = 0; i < 6; i++) k.b.Box({-0.5f, 2.2f, -0.45f + i * 0.24f}, {0.5f, 2.21f, -0.42f + i * 0.24f});
  k.mat(MAT_LEATHER); k.col(0x1d1e20);
  k.box({-0.26f, 1.0f, 0.3f}, {0.26f, 1.12f, 0.75f});
  k.box({-0.26f, 1.12f, 0.7f}, {0.26f, 1.62f, 0.8f});
  k.mat(MAT_PLASTIC); k.col(0x1d1e20);
  k.b.Push(Translation({0, 1.42f, -0.12f}) * RotationX(-0.9f));
  k.b.Torus({0, 0, 0}, 0.16f, 0.018f, 18, 6);
  k.b.Pop();
  k.b.Box({-0.03f, 1.0f, -0.25f}, {0.03f, 1.38f, -0.18f});  // Lenksäule
  // Hubmast und Gabeln vorn (-Z)
  k.mat(MAT_METAL); k.col(0x3a3d42);
  for (float sx : {-0.32f, 0.32f}) k.box({sx - 0.05f, 0.1f, -0.78f}, {sx + 0.05f, 2.5f, -0.66f});
  k.box({-0.37f, 2.42f, -0.78f}, {0.37f, 2.5f, -0.66f});
  k.col(0x2c2e32);
  k.box({-0.45f, 0.08f, -0.86f}, {0.45f, 0.75f, -0.78f});
  for (float sx : {-0.3f, 0.3f}) k.box({sx - 0.06f, 0.08f, -1.95f}, {sx + 0.06f, 0.13f, -0.86f});
  k.mat(MAT_PLASTIC); k.col(0x151516);
  for (float sx : {-0.6f, 0.6f}) {
    k.cylx({sx, 0.3f, -0.35f}, 0.3f, 0.2f, 18);
    k.cylx({sx, 0.24f, 0.98f}, 0.24f, 0.18f, 16);
  }
  k.mat(MAT_PLASTIC); k.col(0xff8a20); k.emis(0.6f);
  k.b.Box({-0.06f, 2.2f, 0.4f}, {0.06f, 2.28f, 0.52f});  // Rundumleuchte
  k.emis(0);
}

void Spind(PK& k) {
  k.mat(MAT_METAL); k.col(0x8fa3b0);
  k.box({-0.3f, 0.0f, -0.25f}, {0.3f, 1.85f, 0.25f});
  k.col(0x6d7f8a);
  k.b.Box({-0.003f, 0.04f, 0.25f}, {0.003f, 1.8f, 0.253f});
  k.col(0x55606a);
  for (float sx : {-0.15f, 0.15f})
    for (int i = 0; i < 5; i++) k.b.Box({sx - 0.07f, 1.5f + i * 0.04f, 0.25f}, {sx + 0.07f, 1.515f + i * 0.04f, 0.252f});
  k.col(0x30353a);
  for (float sx : {-0.06f, 0.06f}) k.b.Box({sx - 0.012f, 0.95f, 0.25f}, {sx + 0.012f, 1.1f, 0.265f});
  k.mat(MAT_PAPER); k.col(0xf4f0e6);
  k.b.Box({-0.22f, 1.32f, 0.25f}, {-0.1f, 1.39f, 0.252f});
}

void Stechuhr(PK& k) {
  k.mat(MAT_METAL); k.col(0x5a5f66);
  k.box({-0.06f, 0, -0.06f}, {0.06f, 1.1f, 0.06f});
  k.box({-0.2f, 0, -0.15f}, {0.2f, 0.04f, 0.15f});
  k.mat(MAT_PLASTIC); k.col(0xd8d4c8);
  k.box({-0.17f, 1.1f, -0.1f}, {0.17f, 1.48f, 0.1f});
  k.col(0xf8f6ee);
  k.b.CylinderZ({0, 1.36f, 0.1f}, 0.085f, 0.006f, 20);
  k.col(0x1a1a1a);
  k.b.Box({-0.004f, 1.36f, 0.103f}, {0.004f, 1.43f, 0.106f});
  k.b.Box({0.0f, 1.356f, 0.103f}, {0.05f, 1.364f, 0.106f});
  k.b.Box({-0.07f, 1.15f, 0.1f}, {0.07f, 1.2f, 0.11f});  // Kartenschlitz
  k.mat(MAT_PAPER); k.col(0xe8dfc8);
  for (int i = 0; i < 6; i++) k.b.Box({-0.14f + i * 0.05f, 1.48f, -0.01f}, {-0.11f + i * 0.05f, 1.56f, 0.0f});  // Stempelkarten
}

void Schutzhelm(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf2c418);
  k.b.Ellipsoid({0, 0.012f, 0}, {0.13f, 0.15f, 0.15f}, 16, 8, 0.0f);
  k.b.Push(Scaling(Vec3(1, 1, 1.18f)));
  k.b.Cylinder({0, 0, 0.015f}, 0.152f, 0.012f, 20);
  k.b.Pop();
  k.b.Box({-0.012f, 0.12f, -0.12f}, {0.012f, 0.165f, 0.1f});  // Grat
  k.part({-0.15f, 0, -0.18f}, {0.15f, 0.165f, 0.2f});
}

void Warnschild(PK& k) {
  for (float s : {-1.0f, 1.0f}) {
    k.push(Translation({0, 0, s * 0.16f}) * RotationX(-s * 0.26f));
    k.mat(MAT_PLASTIC); k.col(0xf2c418);
    k.box({-0.15f, 0, -0.008f}, {0.15f, 0.64f, 0.008f});
    k.col(0x1a1a1a);
    float f = s * 0.0095f;
    k.b.Box({-0.15f, 0.56f, f - 0.0015f}, {0.15f, 0.64f, f + 0.0015f});
    k.b.Box({-0.016f, 0.3f, f - 0.0015f}, {0.016f, 0.47f, f + 0.0015f});
    k.b.Box({-0.016f, 0.24f, f - 0.0015f}, {0.016f, 0.272f, f + 0.0015f});
    k.pop();
  }
}

void Werkzeugkiste(PK& k) {
  k.mat(MAT_METAL); k.col(0xc0281e);
  k.box({-0.25f, 0, -0.11f}, {0.25f, 0.2f, 0.11f});
  k.col(0xa82018);
  k.b.Wedge({-0.25f, 0.2f, -0.11f}, {0.25f, 0.2f, 0.11f}, 0.03f, 0.03f);
  k.col(0x2a2c30);
  k.b.Box({-0.12f, 0.23f, -0.012f}, {-0.1f, 0.3f, 0.012f});
  k.b.Box({0.1f, 0.23f, -0.012f}, {0.12f, 0.3f, 0.012f});
  k.b.Box({-0.12f, 0.29f, -0.012f}, {0.12f, 0.31f, 0.012f});
  k.col(0xb8bcc2);
  for (float sx : {-0.18f, 0.18f}) k.b.Box({sx - 0.02f, 0.16f, 0.11f}, {sx + 0.02f, 0.19f, 0.12f});
  k.part({-0.25f, 0, -0.11f}, {0.25f, 0.31f, 0.11f});
}

void Eimer(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x2a7ad0);
  k.fr({0, 0, 0}, 0.12f, 0.15f, 0.3f, 20);
  k.col(0xf0f2f4);
  k.b.Cylinder({0, 0.26f, 0}, 0.146f, 0.002f, 20);  // Wasser
  k.mat(MAT_METAL); k.col(0x9aa0a6);
  k.b.Push(Translation({0, 0.3f, 0}) * RotationX(PI * 0.5f));
  k.b.Torus({0, 0, 0}, 0.15f, 0.005f, 20, 4);
  k.b.Pop();
}

void Kabeltrommel(PK& k) {
  k.mat(MAT_WOOD); k.col(0xb08a5a);
  for (float z : {-0.3f, 0.3f}) k.cylz({0, 0.48f, z}, 0.48f, 0.05f, 24);
  k.cylz({0, 0.48f, 0}, 0.16f, 0.56f, 16);
  k.mat(MAT_PLASTIC); k.col(0x1a1b1d);
  k.b.CylinderZ({0, 0.48f, 0}, 0.33f, 0.54f, 24);  // Kabel
  k.col(0xe06a1a);
  k.b.CylinderZ({0, 0.48f, 0.12f}, 0.335f, 0.06f, 24);
  k.mat(MAT_METAL); k.col(0x55595f);
  for (float z : {-0.33f, 0.33f}) k.b.CylinderZ({0, 0.48f, z}, 0.05f, 0.03f, 10);
}

void Standventilator(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xe8eaec);
  k.cyl({0, 0, 0}, 0.2f, 0.04f, 20);
  k.cyl({0, 0.04f, 0}, 0.018f, 1.05f, 8);
  k.col(0xd8dadc);
  k.box({-0.06f, 1.05f, -0.1f}, {0.06f, 1.2f, 0.06f});
  k.mat(MAT_METAL); k.col(0xc8ccd0);
  k.b.Push(Translation({0, 1.15f, 0.12f}) * RotationX(PI * 0.5f));
  k.b.Torus({0, 0, 0}, 0.24f, 0.008f, 28, 4);
  k.b.Torus({0, 0, 0}, 0.16f, 0.004f, 24, 4);
  k.b.Pop();
  for (int i = 0; i < 12; i++) {
    float a = TAU * i / 12;
    Vec3 d(cosf(a), sinf(a), 0);
    k.b.Box(Vec3(0, 1.15f, 0.12f) + d * 0.12f - Vec3(0.004f, 0.004f, 0.004f), Vec3(0, 1.15f, 0.12f) + d * 0.12f + Vec3(0.004f, 0.004f, 0.004f));
  }
  k.mat(MAT_PLASTIC); k.col(0x7ab8e0);
  for (int i = 0; i < 3; i++) {
    k.b.Push(Translation({0, 1.15f, 0.11f}) * RotationZ(TAU * i / 3) * RotationY(0.3f));
    k.b.Box({-0.04f, 0.02f, -0.004f}, {0.04f, 0.21f, 0.004f});
    k.b.Pop();
  }
  k.part({-0.25f, 0.9f, 0.05f}, {0.25f, 1.4f, 0.17f});
}

void Sackkarre(PK& k) {
  k.mat(MAT_METAL); k.col(0x2f6ab0);
  for (float sx : {-0.2f, 0.2f}) {
    k.push(Translation({sx, 0.12f, 0.08f}) * RotationX(-0.18f));
    k.box({-0.015f, 0, -0.015f}, {0.015f, 1.2f, 0.015f});
    k.pop();
  }
  for (float y : {0.45f, 0.85f}) k.box({-0.2f, y, 0.0f}, {0.2f, y + 0.025f, 0.03f});
  k.col(0x55595f);
  k.box({-0.22f, 0.0f, -0.25f}, {0.22f, 0.02f, 0.05f});  // Schaufel
  k.mat(MAT_PLASTIC); k.col(0x1a1b1d);
  for (float sx : {-0.27f, 0.27f}) k.cylx({sx, 0.13f, 0.12f}, 0.13f, 0.06f, 16);
  k.col(0x1d1e20);
  k.box({-0.21f, 1.3f, 0.28f}, {0.21f, 1.34f, 0.32f});  // Griff
}

void Nudelsieb(PK& k) {
  k.mat(MAT_METAL); k.col(0xc8ccd2);
  k.b.Push(RotationX(PI));
  k.b.Ellipsoid({0, -0.13f, 0}, {0.13f, 0.11f, 0.13f}, 18, 8, 0.0f);
  k.b.Pop();
  k.b.Cylinder({0, 0, 0}, 0.07f, 0.03f, 16);
  k.b.Push(Translation({0, 0.13f, 0}) * RotationX(PI * 0.5f));
  k.b.Torus({0, 0, 0}, 0.13f, 0.006f, 20, 4);
  k.b.Pop();
  for (float sx : {-1.0f, 1.0f}) k.b.Box({sx * 0.13f - 0.03f, 0.11f, -0.012f}, {sx * 0.13f + 0.03f, 0.125f, 0.012f});
  k.col(0x55595f);
  for (int i = 0; i < 16; i++) {
    float a = TAU * i / 16;
    Vec3 p(cosf(a) * 0.105f, 0.07f, sinf(a) * 0.105f);
    k.b.Box(p - Vec3(0.006f), p + Vec3(0.006f));
  }
  k.part({-0.16f, 0, -0.13f}, {0.16f, 0.135f, 0.13f});
}

void Werkbank(PK& k) {
  k.mat(MAT_METAL); k.col(0x2f6ab0);
  for (float sx : {-0.82f, 0.82f})
    for (float sz : {-0.36f, 0.36f}) k.box({sx - 0.03f, 0, sz - 0.03f}, {sx + 0.03f, 0.84f, sz + 0.03f});
  k.box({-0.85f, 0.15f, -0.39f}, {0.85f, 0.18f, 0.39f});
  k.mat(MAT_WOOD); k.col(0xc89a62);
  k.box({-0.9f, 0.84f, -0.42f}, {0.9f, 0.9f, 0.42f});
  k.mat(MAT_METAL); k.col(0x55595f);
  k.box({0.5f, 0.6f, -0.38f}, {0.8f, 0.84f, 0.36f});  // Schubladenblock
  k.col(0x3a3d42);
  for (int i = 0; i < 3; i++) k.b.Box({0.6f, 0.64f + i * 0.07f, 0.36f}, {0.7f, 0.655f + i * 0.07f, 0.372f});
  k.col(0x30353a);  // Schraubstock
  k.box({-0.75f, 0.9f, 0.2f}, {-0.55f, 1.0f, 0.4f});
  k.box({-0.7f, 1.0f, 0.22f}, {-0.6f, 1.08f, 0.38f});
  k.b.CylinderZ({-0.65f, 0.96f, 0.47f}, 0.012f, 0.16f, 8);
}

void Waage(PK& k) {
  k.mat(MAT_METAL); k.col(0xc0c6cc);
  k.box({-0.4f, 0, -0.4f}, {0.4f, 0.08f, 0.4f});
  k.col(0x55595f);
  k.box({-0.03f, 0.08f, -0.42f}, {0.03f, 1.0f, -0.36f});
  k.mat(MAT_PLASTIC); k.col(0x2a2c30);
  k.box({-0.15f, 0.95f, -0.45f}, {0.15f, 1.12f, -0.36f});
  k.mat(MAT_SCREEN); k.col(0x7aff8a);
  k.b.Box({-0.11f, 1.0f, -0.36f}, {0.11f, 1.07f, -0.358f});
}

void Mikroskop(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf0f0ee);
  k.box({-0.1f, 0, -0.14f}, {0.1f, 0.04f, 0.1f});
  k.box({-0.03f, 0.04f, -0.13f}, {0.03f, 0.3f, -0.07f});
  k.box({-0.07f, 0.13f, -0.08f}, {0.07f, 0.145f, 0.06f});  // Objekttisch
  k.mat(MAT_METAL); k.col(0x2a2c30);
  k.push(Translation({0, 0.24f, -0.06f}) * RotationX(0.5f));
  k.cyl({0, 0, 0}, 0.022f, 0.16f, 12);
  k.pop();
  k.b.Cylinder({0, 0.16f, 0.0f}, 0.012f, 0.05f, 10);
}

void Klemmbrett(PK& k) {
  k.mat(MAT_WOOD); k.col(0x8a5a33);
  k.box({-0.115f, 0, -0.16f}, {0.115f, 0.006f, 0.16f});
  k.mat(MAT_PAPER); k.col(0xf6f4ee);
  k.b.Box({-0.1f, 0.006f, -0.13f}, {0.1f, 0.008f, 0.15f});
  k.col(0x5a5f66);
  for (int i = 0; i < 6; i++) k.b.Box({-0.08f, 0.008f, -0.1f + i * 0.04f}, {0.06f - (i % 2) * 0.03f, 0.009f, -0.094f + i * 0.04f});
  k.mat(MAT_METAL); k.col(0xb8bcc2);
  k.b.Box({-0.05f, 0.006f, -0.165f}, {0.05f, 0.02f, -0.12f});
}

void Nudelteller(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf6f6f2);
  k.fr({0, 0, 0}, 0.07f, 0.13f, 0.025f, 24);
  k.col(0xeee6c8);
  k.b.Cylinder({0, 0.025f, 0}, 0.1f, 0.001f, 24);
  k.col(0xe8c870);  // Spaghetti-Nest
  for (int i = 0; i < 3; i++) k.b.Torus({0, 0.035f + i * 0.012f, 0}, 0.06f - i * 0.012f, 0.012f, 20, 6);
  k.col(0xb8281a);  // Soße
  k.b.Ellipsoid({0, 0.06f, 0}, {0.04f, 0.018f, 0.04f}, 12, 5);
  k.col(0x2f8a3a);
  k.b.Ellipsoid({0.012f, 0.078f, 0.008f}, {0.012f, 0.003f, 0.007f}, 8, 3);  // Basilikum
  k.mat(MAT_METAL); k.col(0xc8ccd2);
  k.b.Box({0.06f, 0.03f, -0.006f}, {0.17f, 0.036f, 0.006f});  // Gabel
}

void Schaltschrank(PK& k) {
  k.mat(MAT_METAL); k.col(0xd4d6d2);
  k.box({-0.4f, 0.1f, -0.2f}, {0.4f, 2.0f, 0.2f});
  k.col(0x5a5f66);
  k.box({-0.4f, 0, -0.2f}, {0.4f, 0.1f, 0.2f});
  k.col(0xb8bab6);
  k.b.Box({-0.002f, 0.15f, 0.2f}, {0.002f, 1.95f, 0.203f});
  k.col(0x30353a);
  for (float sx : {-0.06f, 0.06f}) k.b.Box({sx - 0.015f, 1.0f, 0.2f}, {sx + 0.015f, 1.12f, 0.22f});
  k.mat(MAT_PLASTIC); k.col(0xf2c418);  // Warnschild
  k.b.Box({-0.32f, 1.6f, 0.2f}, {-0.16f, 1.76f, 0.203f});
  k.col(0x1a1a1a);
  k.b.Box({-0.245f, 1.63f, 0.203f}, {-0.235f, 1.73f, 0.205f});
  k.col(0x30d050); k.emis(0.9f);
  k.b.Box({0.18f, 1.7f, 0.2f}, {0.22f, 1.74f, 0.21f});
  k.col(0xff3020);
  k.b.Box({0.26f, 1.7f, 0.2f}, {0.3f, 1.74f, 0.21f});
  k.emis(0);
}

void Kunststofffass(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x1f5fb0);
  k.cyl({0, 0, 0}, 0.29f, 0.9f, 22);
  k.col(0x1a54a0);
  for (float y : {0.3f, 0.6f}) {
    k.b.Push(Translation({0, y, 0}));
    k.b.Torus({0, 0, 0}, 0.29f, 0.012f, 22, 4);
    k.b.Pop();
  }
  k.col(0x18489a);
  k.b.Cylinder({0.15f, 0.9f, 0}, 0.04f, 0.02f, 10);
  k.b.Cylinder({-0.15f, 0.9f, 0}, 0.03f, 0.02f, 10);
}

void IbcTank(PK& k) {
  PaletteAt(k, Vec3(0, 0, 0));
  k.mat(MAT_PLASTIC); k.col(0xe8eae4);
  k.box({-0.55f, 0.15f, -0.36f}, {0.55f, 1.1f, 0.36f});
  k.col(0xd0d2cc);
  k.b.Box({-0.5f, 0.15f, 0.36f}, {0.5f, 0.75f, 0.362f});  // Füllstand
  k.mat(MAT_METAL); k.col(0x9aa0a6);
  for (float x : {-0.58f, -0.29f, 0.0f, 0.29f, 0.58f}) {
    k.b.Box({x - 0.012f, 0.15f, 0.37f}, {x + 0.012f, 1.14f, 0.39f});
    k.b.Box({x - 0.012f, 0.15f, -0.39f}, {x + 0.012f, 1.14f, -0.37f});
  }
  for (float y : {0.15f, 0.62f, 1.12f}) {
    k.b.Box({-0.6f, y, 0.37f}, {0.6f, y + 0.02f, 0.39f});
    k.b.Box({-0.6f, y, -0.39f}, {0.6f, y + 0.02f, -0.37f});
  }
  k.col(0x55595f);
  k.cyl({0, 1.1f, 0}, 0.08f, 0.05f, 14);
  k.mat(MAT_PLASTIC); k.col(0x1a1b1d);
  k.b.CylinderZ({0, 0.25f, 0.42f}, 0.04f, 0.06f, 10);
  k.part({-0.6f, 0.144f, -0.39f}, {0.6f, 1.15f, 0.39f});
}

// --- KEK-Paketzentrum -----------------------------------------------------------------------
void Versandpaket(PK& k, Vec3 size, bool fragile) {
  k.mat(MAT_PAPER); k.col(fragile ? 0xb48148 : 0xc99d61);
  k.box({-size.x / 2, 0, -size.z / 2}, {size.x / 2, size.y, size.z / 2});
  k.col(0xd6b789);
  k.box({-.035f, size.y + .009f, -size.z / 2}, {.035f, size.y + .012f, size.z / 2});
  k.col(0xf5efe2);
  k.box({size.x * .02f, size.y + .01f, -size.z * .28f}, {size.x * .36f, size.y + .014f, size.z * .08f});
  k.col(0x25252a);
  for (int i = 0; i < 7; i++) {
    float x = size.x * (.05f + i * .04f);
    k.b.Box({x, size.y + .024f, -size.z * .22f}, {x + .005f, size.y + .026f, size.z * .02f});
  }
  k.col(0xd21b12); k.mat(MAT_PLAIN);
  k.push(Translation({0, size.y * .47f, size.z * .5f + .011f}) * Scaling(Vec3(size.x * .13f)));
  BuildKekLogo(k.b); k.pop();
  if (fragile) {
    k.col(0xb91912);
    k.b.Box({-size.x * .46f, size.y * .15f, size.z * .5f + .012f}, {-size.x * .4f, size.y * .8f, size.z * .5f + .025f});
  }
}
void Versandrolle(PK& k) {
  k.mat(MAT_PAPER); k.col(0xb98d57); k.cyl({0, 0, 0}, .1f, .72f, 18);
  k.col(0xf0e8d7); k.cyl({0, 0, 0}, .103f, .04f, 18); k.cyl({0, .68f, 0}, .103f, .04f, 18);
  k.col(0xffce24); k.cyl({0, .3f, 0}, .105f, .12f, 18);
}
void Handscanner(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x242b32); k.box({-.045f, 0, -.035f}, {.045f, .14f, .04f});
  k.col(0xffcc19); k.box({-.075f, .12f, -.055f}, {.075f, .23f, .065f});
  k.col(0x143543); k.box({-.055f, .15f, .066f}, {.055f, .213f, .077f});
  k.col(0xf04425); k.box({-.04f, .155f, -.066f}, {.04f, .195f, -.055f});
}
void Klebeband(PK& k) {
  k.mat(MAT_PAPER); k.col(0xbc9561); k.torus({0, .047f, 0}, .065f, .026f, 20, 10);
  k.col(0xf1e9d6); k.b.Torus({0, .048f, 0}, .041f, .008f, 20, 8);
}
void Postsack(PK& k) {
  k.mat(MAT_FABRIC); k.col(0xd5ba7c); k.ell({0, .38f, 0}, {.3f, .38f, .25f}, 16, 10);
  k.col(0x8d734b); k.cyl({0, .68f, 0}, .105f, .1f, 12);
  k.col(0xc21c12); k.push(Translation({0, .38f, .249f}) * Scaling(Vec3(.075f))); BuildKekLogo(k.b); k.pop();
}
void Sortierkiste(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xffc91a);
  k.box({-.3f, .025f, -.21f}, {.3f, .07f, .21f});
  for (float x : {-.28f, .28f}) k.box({x - .02f, .07f, -.21f}, {x + .02f, .32f, .21f});
  for (float z : {-.19f, .19f}) k.box({-.26f, .07f, z - .02f}, {.26f, .32f, z + .02f});
  k.col(0xcc2015); k.b.Box({-.13f, .15f, .222f}, {.13f, .25f, .228f});
}
void Rollcontainer(PK& k) {
  k.mat(MAT_METAL); k.col(0x6f7980); k.box({-.45f, .14f, -.34f}, {.45f, .21f, .34f});
  for (float x : {-.41f, .41f}) for (float z : {-.29f, .29f}) {
    k.col(0x252a30); k.cylz({x, .09f, z}, .09f, .055f, 12);
    k.col(0x9ba2a5); k.box({x - .018f, .21f, z - .018f}, {x + .018f, 1.7f, z + .018f});
  }
  for (float y : {.5f, .9f, 1.3f, 1.68f}) {
    k.box({-.43f, y, -.31f}, {.43f, y + .018f, -.29f});
    for (float x : {-.42f, .42f}) k.box({x - .01f, y, -.28f}, {x + .01f, y + .018f, .31f});
  }
  k.push(Translation({0, .22f, 0})); Versandpaket(k, {.68f, .48f, .49f}, false); k.pop();
  k.push(Translation({-.08f, .71f, -.03f})); Versandpaket(k, {.46f, .35f, .36f}, true); k.pop();
}
void Etikettendrucker(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0x343c43); k.box({-.14f, 0, -.14f}, {.14f, .19f, .14f});
  k.col(0xf6c923); k.box({-.145f, .14f, -.145f}, {.145f, .22f, .15f});
  k.col(0xeceee8); k.box({-.09f, .08f, .15f}, {.09f, .089f, .32f});
  k.col(0x232a30);
  for (int i = 0; i < 9; i++) k.b.Box({-.07f + i * .016f, .099f, .18f}, {-.064f + i * .016f, .102f, .26f});
}

// --- Dresdner Zwinger: eigenständig modellierte Museums- und Gartenobjekte ---------------------
void ZwingerVase(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xf3eee2);
  k.fr({0, 0, 0}, .19f, .14f, .08f); k.fr({0, .08f, 0}, .14f, .25f, .25f);
  k.fr({0, .33f, 0}, .25f, .12f, .22f); k.cyl({0, .55f, 0}, .115f, .12f);
  k.col(0x244d9a); k.torus({0, .68f, 0}, .12f, .022f); k.torus({0, .10f, 0}, .15f, .018f);
  for (int i = 0; i < 8; i++) {
    float a = i * TAU / 8;
    k.ell({sinf(a) * .241f, .34f, cosf(a) * .241f}, {.035f, .09f, .035f}, 8, 6);
  }
  k.col(0xf3eee2); k.fr({0, .70f, 0}, .14f, .05f, .07f); k.sph({0, .80f, 0}, .045f);
}

void ZwingerUrne(PK& k) {
  k.mat(MAT_SANDSTONE); k.col(0xc9b58f);
  k.boxc({0, .09f, 0}, {.34f, .09f, .34f}); k.fr({0, .18f, 0}, .23f, .16f, .25f);
  k.fr({0, .43f, 0}, .16f, .38f, .43f); k.fr({0, .86f, 0}, .38f, .22f, .24f);
  k.torus({0, 1.1f, 0}, .24f, .045f); k.fr({0, 1.12f, 0}, .26f, .04f, .17f);
  k.sph({0, 1.34f, 0}, .075f);
  for (float x : {-.37f, .37f}) k.ell({x, .87f, 0}, {.075f, .15f, .12f});
}

void ZwingerOrange(PK& k) {
  k.mat(MAT_SANDSTONE); k.col(0x9d5b3a);
  k.fr({0, 0, 0}, .48f, .64f, .72f, 16); k.torus({0, .72f, 0}, .62f, .065f);
  k.col(0x46382b); k.cyl({0, .73f, 0}, .56f, .025f);
  k.mat(MAT_WOOD); k.col(0x705035); k.cyl({0, .75f, 0}, .075f, 1.0f, 10);
  k.mat(MAT_LEAVES);
  for (int i = 0; i < 6; i++) {
    float a = i * TAU / 6; k.col(i % 2 ? 0x446331 : 0x54783a);
    k.ell({.28f * sinf(a), 1.92f + .08f * (i % 2), .28f * cosf(a)}, {.48f, .56f, .48f}, 12, 8);
    k.mat(MAT_PLASTIC); k.col(0xf3a32c);
    k.sph({.62f * sinf(a), 1.84f + .16f * (i % 3), .62f * cosf(a)}, .085f, 10); k.mat(MAT_LEAVES);
  }
  k.collision({0, 1.25f, 0}, {.70f, 1.25f, .70f});
}

void ZwingerStatue(PK& k) {
  k.mat(MAT_SANDSTONE); k.col(0xcbb894);
  k.boxc({0, .12f, 0}, {.43f, .12f, .40f}); k.boxc({0, .36f, 0}, {.31f, .12f, .28f});
  k.fr({0, .48f, 0}, .30f, .19f, .90f, 14); k.ell({.03f, 1.49f, 0}, {.24f, .32f, .18f});
  k.ell({.06f, 1.94f, .02f}, {.16f, .21f, .17f});
  k.col(0xb3a07d); k.ell({.06f, 2.03f, -.05f}, {.18f, .16f, .15f});
  k.col(0xcbb894);
  k.push(Translation({-.29f, 1.41f, .02f}) * RotationZ(-.55f)); k.ell({}, {.085f, .32f, .09f}); k.pop();
  k.push(Translation({.30f, 1.58f, .02f}) * RotationZ(.8f)); k.ell({}, {.085f, .30f, .09f}); k.pop();
  k.ell({.47f, 1.8f, .05f}, {.18f, .06f, .19f});
  for (float x : {-.17f, -.06f, .06f, .17f}) k.boxc({x, .91f, .22f}, {.018f, .36f, .018f});
}

void ZwingerTelescope(PK& k) {
  k.mat(MAT_WOOD); k.col(0x6e4529);
  for (float a : {0.0f, TAU / 3, TAU * 2 / 3}) {
    k.push(RotationY(a) * Translation({0, .55f, .22f}) * RotationX(.35f)); k.boxc({}, {.035f, .57f, .035f}); k.pop();
  }
  k.mat(MAT_GOLD); k.col(0xbca052); k.cyl({0, 1.05f, 0}, .09f, .23f);
  k.push(Translation({0, 1.37f, 0}) * RotationX(-.38f));
  k.cylz({}, .105f, 1.1f); k.col(0x343c45); k.cylz({0, 0, .58f}, .117f, .12f);
  k.mat(MAT_GLASS); k.col(0x568ea8); k.cylz({0, 0, .648f}, .094f, .012f);
  k.mat(MAT_METAL); k.col(0x343c45); k.cylz({0, 0, -.62f}, .05f, .17f); k.pop();
}

void ZwingerCrate(PK& k) {
  k.mat(MAT_WOOD); k.col(0xa98653); k.box({-.72f, 0, -.53f}, {.72f, 1.15f, .53f});
  k.col(0x775d3f);
  for (float x : {-.59f, .59f}) for (float z : {-.55f, .55f}) k.boxc({x, .575f, z}, {.065f, .575f, .025f});
  k.mat(MAT_PAPER); k.col(0xe5d9bc); k.box({-.46f, .38f, .557f}, {.46f, .79f, .57f});
  k.mat(MAT_PLAIN); k.col(0x55412d);
  k.push(Translation({-PixelTextWidth("KUNST", .035f) * .5f, .47f, .58f})); BuildPixelText(k.b, "KUNST", .035f, .006f); k.pop();
}

void ZwingerPitcher(PK& k) {
  k.mat(MAT_PLASTIC); k.col(0xe3ded1);
  k.fr({},.14f,.20f,.08f); k.ell({0,.24f,0},{.225f,.21f,.225f},20,12);
  k.fr({0,.38f,0},.15f,.12f,.065f); k.fr({0,.445f,0},.14f,.065f,.05f);
  k.col(0x355481); k.sph({0,.51f,0},.039f);
  k.torus({0,.085f,0},.176f,.014f);
  k.push(Translation({-.24f,.265f,0})*RotationX(PI*.5f)*Scaling({.79f,1,1.18f}));
  k.torus({},.12f,.027f); k.pop();
  k.col(0xe3ded1);
  k.push(Translation({.155f,.24f,0})*RotationZ(-.68f));
  k.fr({},.078f,.049f,.255f,14); k.col(0x355481); k.torus({0,.252f,0},.049f,.009f); k.pop();
  for (int i=0; i<8; ++i) {
    float a=TAU*i/8; k.push(RotationY(a));
    k.ell({0,.245f,.217f},{.028f,.057f,.012f},8,6); k.pop();
  }
}

void ZwingerCello(PK& k) {
  k.mat(MAT_METAL); k.col(0x44433d); k.cyl({},.018f,.19f,8);
  k.mat(MAT_WOOD); k.col(0x924a27);
  k.ell({0,.44f,0},{.25f,.29f,.105f},20,12);
  k.ell({0,.68f,0},{.145f,.20f,.091f},16,10);
  k.ell({0,.84f,0},{.196f,.19f,.097f},18,10);
  k.boxc({0,1.17f,0},{.038f,.28f,.037f}); k.ell({0,1.48f,0},{.06f,.085f,.049f});
  k.col(0x302923); k.boxc({0,1.03f,.075f},{.039f,.34f,.015f});
  k.boxc({0,.38f,.106f},{.044f,.09f,.012f});
  k.col(0xd8b27c); k.boxc({0,.59f,.105f},{.070f,.026f,.017f});
  for (int i=0; i<4; ++i) {
    float x=(i-1.5f)*.014f;
    k.col(0xc9bda4); k.boxc({x,.89f,.115f},{.0025f,.49f,.004f});
    k.col(0x392e29); k.cylx({i%2 ? -.058f : .058f,1.40f+i*.035f,0},.019f,.058f,8);
  }
  for (float side : {-1.f,1.f}) {
    k.col(0x473020); k.ell({side*.097f,.60f,.091f},{.014f,.066f,.012f},8,6);
    k.mat(MAT_METAL); k.col(0x373c39); k.tubeYZ({side*.10f,.20f,-.02f},{side*.10f,.019f,-.17f},.014f);
  }
}

struct Entry {
  const char* id;
  const char* name;
  bool disguisable;
  std::function<void(PK&)> build;
};

std::vector<PropDef> g_defs;

}  // namespace

namespace props {

void BuildCatalog() {
  if (!g_defs.empty()) return;
  const Entry entries[] = {
      {"karton", "Umzugskarton", true, Karton},
      {"sitz", "Abgeordnetensitz", true, Sitz},
      {"rednerpult", "Rednerpult", true, Rednerpult},
      {"ordner_blau", "Aktenordner (blau)", true, [](PK& k) { Ordner(k, 0x2b5aa8); }},
      {"ordner_rot", "Aktenordner (rot)", true, [](PK& k) { Ordner(k, 0xc23b32); }},
      {"aktenstapel", "Aktenstapel", true, Aktenstapel},
      {"laptop", "Laptop", true, Laptop},
      {"wasserglas", "Wasserglas", true, Wasserglas},
      {"wasserflasche", "Wasserflasche", true, Wasserflasche},
      {"kaffeebecher", "Kaffeebecher", true, Kaffeebecher},
      {"mikrofon", "Tischmikrofon", true, Mikrofon},
      {"namensschild", "Namensschild", true, Namensschild},
      {"ficus", "Ficus", true, Ficus},
      {"pflanze", "Topfpflanze", true, PflanzeKlein},
      {"kordel", "Kordelständer", true, Kordelstaender},
      {"detektor", "Metalldetektor", false, Metalldetektor},
      {"roentgen", "Gepäckscanner", false, Roentgen},
      {"feuerloescher", "Feuerlöscher", true, Feuerloescher},
      {"papierkorb", "Papierkorb", true, Papierkorb},
      {"abfalleimer", "Abfalleimer", true, Abfalleimer},
      {"buerostuhl", "Bürostuhl", true, Buerostuhl},
      {"schreibtisch", "Schreibtisch", true, Schreibtisch},
      {"aktenschrank", "Aktenschrank", true, Aktenschrank},
      {"regal", "Bücherregal", true, Buecherregal},
      {"kopierer", "Kopierer", true, Kopierer},
      {"wasserspender", "Wasserspender", true, Wasserspender},
      {"garderobe", "Garderobenständer", true, Garderobe},
      {"sitzbank", "Sitzbank", true, Sitzbank},
      {"sessel", "Ledersessel", true, Sessel},
      {"sofa", "Ledersofa", true, Sofa},
      {"couchtisch", "Couchtisch", true, Couchtisch},
      {"stehlampe", "Stehlampe", true, Stehlampe},
      {"tischlampe", "Schreibtischlampe", true, Schreibtischlampe},
      {"monitor", "Monitor", true, Monitor},
      {"pc", "Computer", true, PC},
      {"telefon", "Telefon", true, Telefon},
      {"kantinentisch", "Kantinentisch", true, Kantinentisch},
      {"kantinenstuhl", "Kantinenstuhl", true, Kantinenstuhl},
      {"tablett", "Tablett mit Essen", true, Tablett},
      {"currywurst", "Currywurst", true, Currywurst},
      {"brezel", "Brezel", true, Brezel},
      {"mate", "Mate-Flasche", true, Mate},
      {"kaffeemaschine", "Kaffeemaschine", true, Kaffeemaschine},
      {"automat", "Getränkeautomat", true, Getraenkeautomat},
      {"mikrowelle", "Mikrowelle", true, Mikrowelle},
      {"kochtopf", "Kochtopf", true, Kochtopf},
      {"adlerfigur", "Adler-Statue", true, Adlerfigur},
      {"flagge", "Bundesflagge", true, Flagge},
      {"glocke", "Präsidentenglocke", true, Glocke},
      {"gitter", "Absperrgitter", true, Absperrgitter},
      {"poller", "Poller", true, Poller},
      {"parkbank", "Parkbank", true, Parkbank},
      {"laterne", "Laterne", false, Laterne},
      {"baum", "Linde", false, Baum},
      {"busch", "Busch", true, Busch},
      {"kuebel", "Blumenkübel", true, Blumenkuebel},
      {"huetchen", "Leitkegel", true, Huetchen},
      {"muelltonne", "Mülltonne", true, Muelltonne},
      {"fahrrad", "Fahrrad", true, Fahrrad},
      {"kiste", "Holzkiste", true, Kiste},
      {"paket", "Paket", true, Paket},
      {"tvkamera", "TV-Kamera", true, TvKamera},
      {"koffer", "Rollkoffer", true, Koffer},
      {"schirmstaender", "Schirmständer", true, Schirmstaender},
      {"bueste", "Büste", true, Bueste},
      {"vase", "Vase", true, Vase},
      {"globus", "Globus", true, Globus},
      {"staffelei", "Gemälde", true, Staffelei},
      {"polizeiauto", "Polizeiwagen", true, [](PK& k) { Auto(k, 0xf2f2f2, true); }},
      {"auto_grau", "Limousine", true, [](PK& k) { Auto(k, 0x3a3f47, false); }},
      {"auto_blau", "Kleinwagen", true, [](PK& k) { Auto(k, 0x2a4a8a, false); }},
      {"stapelstuehle", "Stuhlstapel", true, Stapelstuehle},
      {"getraenkekiste", "Getränkekiste", true, Getraenkekiste},
      {"sonnenschirm", "Sonnenschirm", true, Sonnenschirm},
      {"liegestuhl", "Liegestuhl", true, Liegestuhl},
      {"klima", "Klimagerät", true, Klimageraet},
      {"wahlurne", "Wahlurne", true, Wahlurne},
      {"feuermelder", "Feuermelder", true, Feuermelder},
      // Oktoberfest
      {"masskrug", "Maßkrug", true, Masskrug},
      {"masstablett", "Maßkrug-Tablett", true, MasskrugTablett},
      {"wiesnbrezn", "Wiesnbrezn", true, WiesnBrezn},
      {"lebkuchenherz", "Lebkuchenherz", true, Lebkuchenherz},
      {"holzfass", "Holzfass", true, Holzfass},
      {"fass", "Bierfass", true, Fass},
      {"bierbank", "Bierbank", true, Bierbank},
      {"biertisch", "Biertisch", true, Biertisch},
      {"hendl", "Hendl", true, Hendl},
      {"radi", "Radi", true, Radi},
      {"steckerlfisch", "Steckerlfisch", true, Steckerlfisch},
      {"trachtenhut", "Trachtenhut", true, Trachtenhut},
      {"tuba", "Tuba", true, Tuba},
      {"pauke", "Pauke", true, Pauke},
      {"notenstaender", "Notenständer", true, Notenstaender},
      {"teddy", "Teddybär", true, Teddy},
      {"zuckerwatte", "Zuckerwatte", true, Zuckerwatte},
      {"ballons", "Luftballons", true, Ballons},
      {"mandeln", "Gebrannte Mandeln", true, Mandeln},
      {"lampion", "Lampion", true, Lampion},
      {"strohballen", "Strohballen", true, Strohballen},
      {"gartenzwerg", "Gartenzwerg", true, Gartenzwerg},
      {"holzpferd", "Karussellpferd", true, Holzpferd},
      {"trachtenpuppe", "Dirndl-Puppe", true, Trachtenpuppe},
      {"autoscooter", "Autoscooter", true, Autoscooter},
      {"dixi", "Toilettenhäuschen", true, Dixi},
      {"lautsprecher", "Lautsprecher", true, Lautsprecher},
      {"kerze", "Kerze", true, Kerze},
      {"schirm_bw", "Biergartenschirm", true, SchirmBW},
      {"mehlsack", "Mehlsack", true, Mehlsack},
      {"mehlpalette", "Mehlpalette", true, Mehlpalette},
      {"palette", "Europalette", true, Palette},
      {"nudelpackung", "Spaghetti-Packung", true, Nudelpackung},
      {"farfalle", "Farfalle-Tüte", true, Farfalle},
      {"nudelkarton", "Nudelkarton", true, Nudelkarton},
      {"kartonpalette", "Kartonpalette", true, Kartonpalette},
      {"tomatendose", "Tomatendose", true, Tomatendose},
      {"olivenoel", "Olivenöl-Kanister", true, Olivenoel},
      {"parmesan", "Parmesanlaib", true, Parmesan},
      {"teigwanne", "Teigwanne", true, Teigwanne},
      {"trockenwagen", "Trockenwagen", true, Trockenwagen},
      {"hubwagen", "Hubwagen", true, Hubwagen},
      {"gabelstapler", "Gabelstapler", true, Gabelstapler},
      {"spind", "Spind", true, Spind},
      {"stechuhr", "Stechuhr", true, Stechuhr},
      {"schutzhelm", "Schutzhelm", true, Schutzhelm},
      {"warnschild", "Warnaufsteller", true, Warnschild},
      {"werkzeugkiste", "Werkzeugkiste", true, Werkzeugkiste},
      {"eimer", "Eimer", true, Eimer},
      {"kabeltrommel", "Kabeltrommel", true, Kabeltrommel},
      {"ventilator", "Standventilator", true, Standventilator},
      {"sackkarre", "Sackkarre", true, Sackkarre},
      {"sieb", "Nudelsieb", true, Nudelsieb},
      {"werkbank", "Werkbank", true, Werkbank},
      {"waage", "Plattformwaage", true, Waage},
      {"mikroskop", "Mikroskop", true, Mikroskop},
      {"klemmbrett", "Klemmbrett", true, Klemmbrett},
      {"nudelteller", "Teller Spaghetti", true, Nudelteller},
      {"schaltschrank", "Schaltschrank", true, Schaltschrank},
      {"fass_blau", "Kunststofffass", true, Kunststofffass},
      {"ibc", "IBC-Tank", true, IbcTank},
      {"paket_s", "KEK-Päckchen", true, [](PK& k) { Versandpaket(k, {.36f, .28f, .3f}, false); }},
      {"paket_m", "KEK-Paket", true, [](PK& k) { Versandpaket(k, {.66f, .52f, .48f}, false); }},
      {"paket_fragil", "Vorsicht, zerbrechlich!", true, [](PK& k) { Versandpaket(k, {.48f, .64f, .44f}, true); }},
      {"versandrolle", "Versandrolle", true, Versandrolle},
      {"handscanner", "Handscanner", true, Handscanner},
      {"klebeband", "Klebebandrolle", true, Klebeband},
      {"postsack", "Postsack", true, Postsack},
      {"sortierkiste", "Sortierkiste", true, Sortierkiste},
      {"rollcontainer", "Paket-Rollcontainer", true, Rollcontainer},
      {"etikettendrucker", "Etikettendrucker", true, Etikettendrucker},
      {"zwinger_vase", "Porzellanvase", true, ZwingerVase},
      {"zwinger_urne", "Barockurne", true, ZwingerUrne},
      {"zwinger_orange", "Orangenkübel", true, ZwingerOrange},
      {"zwinger_statue", "Sandsteinfigur", true, ZwingerStatue},
      {"zwinger_fernrohr", "Historisches Fernrohr", true, ZwingerTelescope},
      {"zwinger_kunstkiste", "Kunsttransportkiste", true, ZwingerCrate},
      {"zwinger_kanne", "Porzellankanne", true, ZwingerPitcher},
      {"zwinger_cello", "Violoncello", true, ZwingerCello},
  };
  g_defs.reserve(COUNTOF(entries));
  for (const Entry& e : entries) {
    g_defs.emplace_back();
    PropDef& d = g_defs.back();
    d.id = e.id;
    d.name = e.name;
    d.disguisable = e.disguisable;
    PK k(&d);
    e.build(k);
    d.mesh.Bounds(d.bmin, d.bmax);
    if (k.hasColOverride) {
      d.colCenter = k.colC;
      d.colHalf = k.colH;
    } else {
      d.colCenter = (d.bmin + d.bmax) * 0.5f;
      d.colHalf = Max((d.bmax - d.bmin) * 0.5f, Vec3(0.03f));
    }
    if (d.parts.empty()) d.parts.push_back({d.bmin, d.bmax});
    Vec3 sz = d.bmax - d.bmin;
    d.volume = sz.x * sz.y * sz.z;
    d.health = Clamp(20.0f + 110.0f * cbrtf(d.volume), 20.0f, 150.0f);
    d.partCollision = d.id == "detektor" || d.id == "gitter";
    // Absichtlich ausgewählte massive Formen: ein großer Schirm/Busch ist keine Abrissbirne.
    for (const char* heavy : {"sofa", "automat", "aktenschrank", "kopierer", "dixi", "mehlpalette",
                             "kartonpalette", "ibc", "schaltschrank", "rollcontainer", "werkbank",
                             "zwinger_orange", "zwinger_statue", "zwinger_kunstkiste"})
      if (d.id == heavy) d.ramDamage = 70;
    if (d.id == "autoscooter") d.ramDamage = 105;
    if(d.id=="auto_grau" || d.id=="auto_blau" || d.id=="polizeiauto" || d.id=="gabelstapler") {
      d.vehicle=true;d.ramDamage=105;
      d.lookYawOffset=d.id=="gabelstapler" ? 0 : PI*.5f;
    }
  }
  LOG("Prop-Katalog: %d Gegenstände", (int)g_defs.size());
}

int Count() { return (int)g_defs.size(); }
const PropDef& Get(int id) { return g_defs[ClampI(id, 0, (int)g_defs.size() - 1)]; }
int Find(const char* id) {
  for (int i = 0; i < (int)g_defs.size(); i++)
    if (g_defs[i].id == id) return i;
  return -1;
}

}  // namespace props
