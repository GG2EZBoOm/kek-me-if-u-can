#include "game/Shapes.h"

void BuildKekLogo(MeshBuilder& b) {
  Mat4 slant = Mat4::Identity(); slant.m[4] = .28f;
  b.Push(slant);
  auto polygon = [&](std::initializer_list<Vec2> points) { ExtrudeConvex(b, std::vector<Vec2>(points), 0, .025f); };
  for (float x : {-1.65f, .65f}) {
    b.Box({x, -.7f, 0}, {x + .28f, .7f, .025f});
    polygon({{x + .25f, -.13f}, {x + 1.05f, .7f}, {x + .66f, .7f}, {x + .25f, .25f}});
    polygon({{x + .25f, .16f}, {x + .25f, -.24f}, {x + .65f, -.7f}, {x + 1.06f, -.7f}});
  }
  b.Box({-.5f, -.7f, 0}, {-.22f, .7f, .025f});
  for (float y : {-.7f, -.12f, .44f}) b.Box({-.22f, y, 0}, {.5f, y + .26f, .025f});
  for (int i = 0; i < 3; i++) {
    float y = -.69f + i * .19f;
    b.Box({-3, y, 0}, {-1.85f, y + .1f, .025f});
    b.Box({1.88f, y, 0}, {3, y + .1f, .025f});
  }
  b.Pop();
}

void ExtrudeConvex(MeshBuilder& b, const std::vector<Vec2>& pts, float z0, float z1) {
  int n = (int)pts.size();
  if (n < 3) return;
  for (int i = 1; i + 1 < n; i++) {
    b.Tri(Vec3(pts[0].x, pts[0].y, z1), Vec3(pts[i].x, pts[i].y, z1), Vec3(pts[i + 1].x, pts[i + 1].y, z1));
    b.Tri(Vec3(pts[0].x, pts[0].y, z0), Vec3(pts[i + 1].x, pts[i + 1].y, z0), Vec3(pts[i].x, pts[i].y, z0));
  }
  for (int i = 0; i < n; i++) {
    Vec2 a = pts[i], c = pts[(i + 1) % n];
    b.Quad(Vec3(a.x, a.y, z0), Vec3(c.x, c.y, z0), Vec3(c.x, c.y, z1), Vec3(a.x, a.y, z1));
  }
}

void BuildEagle(MeshBuilder& b) {
  const float d = 0.09f;  // halbe Dicke
  // Körper
  b.Ellipsoid(Vec3(0, -0.02f, 0), Vec3(0.36f, 0.58f, d * 1.25f), 18, 10);
  // Brust etwas hervorgehoben
  b.Ellipsoid(Vec3(0, 0.12f, 0.03f), Vec3(0.28f, 0.34f, d * 1.1f), 16, 8);
  // Hals und Kopf (Blick nach links aus Betrachtersicht)
  b.Ellipsoid(Vec3(-0.05f, 0.56f, 0), Vec3(0.16f, 0.2f, d), 14, 8);
  b.Ellipsoid(Vec3(-0.14f, 0.74f, 0), Vec3(0.17f, 0.15f, d), 14, 8);
  ExtrudeConvex(b, {{-0.28f, 0.80f}, {-0.46f, 0.70f}, {-0.36f, 0.64f}, {-0.24f, 0.68f}}, -d * 0.7f, d * 0.7f);
  // Flügel (gespiegelt)
  for (int side = -1; side <= 1; side += 2) {
    float s = (float)side;
    auto P = [&](float x, float y) { return Vec2(x * s, y); };
    std::vector<Vec2> wing = {P(0.22f, 0.42f), P(0.62f, 0.66f), P(1.02f, 0.72f), P(1.08f, 0.52f), P(0.98f, 0.02f), P(0.3f, -0.18f)};
    if (side > 0) std::reverse(wing.begin(), wing.end());  // Punkte sind im Uhrzeigersinn definiert
    ExtrudeConvex(b, wing, -d, d);
    // Federfinger hängen nach unten
    for (int f = 0; f < 6; f++) {
      float fx = 0.38f + f * 0.125f;
      float top = 0.05f + f * 0.06f;
      float len = 0.34f + 0.06f * (f % 3);
      b.Ellipsoid(Vec3(fx * s, top - len * 0.5f, 0), Vec3(0.062f, len * 0.5f + 0.04f, d * 0.85f), 10, 6);
    }
    // Obere Flügelkante
    b.Ellipsoid(Vec3(0.66f * s, 0.66f, 0), Vec3(0.42f, 0.08f, d), 12, 6);
  }
  // Schwanzfedern
  for (int f = -2; f <= 2; f++) {
    b.Ellipsoid(Vec3(f * 0.1f, -0.68f, 0), Vec3(0.06f, 0.24f, d * 0.8f), 10, 6);
  }
  // Beine und Krallen
  for (int side = -1; side <= 1; side += 2) {
    float s = (float)side;
    b.BoxC(Vec3(0.17f * s, -0.58f, 0), Vec3(0.05f, 0.12f, d * 0.7f));
    for (int c = -1; c <= 1; c++) b.BoxC(Vec3(0.17f * s + c * 0.05f, -0.73f, 0), Vec3(0.018f, 0.06f, d * 0.6f));
  }
}

// 5x7-Glyphen, oberste Zeile zuerst, Bit 4 = linkes Pixel
struct PixGlyph { char c; u8 rows[7]; };
static const PixGlyph kPix[] = {
  {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
  {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
  {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}}, {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
  {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
  {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
  {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
  {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}}, {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
  {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
  {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}}, {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
  {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
  {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
  {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}}, {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
  {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}}, {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
  {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
  {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
  {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
  {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
  {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
  {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}}, {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
  {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}}, {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},
  {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
};

static const PixGlyph* FindPix(char c) {
  for (const PixGlyph& g : kPix) if (g.c == c) return &g;
  return nullptr;
}

float PixelTextWidth(const char* text, float pixel) {
  int n = (int)strlen(text);
  return n > 0 ? (n * 6 - 1) * pixel : 0.0f;
}

float BuildPixelText(MeshBuilder& b, const char* text, float pixel, float depth) {
  float x = 0;
  for (const char* p = text; *p; p++) {
    const PixGlyph* g = FindPix(*p);
    if (g) {
      for (int row = 0; row < 7; row++) {
        u8 bits = g->rows[row];
        int col = 0;
        while (col < 5) {
          if (bits & (0x10 >> col)) {
            int start = col;
            while (col < 5 && (bits & (0x10 >> col))) col++;
            float y0 = (6 - row) * pixel;
            b.Box(Vec3(x + start * pixel, y0, 0), Vec3(x + col * pixel, y0 + pixel, depth));
          } else {
            col++;
          }
        }
      }
    }
    x += 6 * pixel;
  }
  return x - pixel;
}
