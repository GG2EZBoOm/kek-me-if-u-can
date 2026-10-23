#include "game/MapKit.h"
#include "game/Shapes.h"

void MapKit::Solid(Vec3 mn, Vec3 mx, u32 faces, u8 flags) {
  ob.Box(mn, mx, faces);
  w.AddCollider(OBox((mn + mx) * 0.5f, (mx - mn) * 0.5f), flags, ob.CurMat());
}

void MapKit::SolidR(Vec3 c, Vec3 h, float yaw, u32 faces, u8 flags) {
  ob.BoxR(c, h, yaw, faces);
  w.AddCollider(OBox(c, h, yaw), flags, ob.CurMat());
}

void MapKit::Clip(Vec3 mn, Vec3 mx, u8 flags) {
  w.AddCollider(OBox((mn + mx) * 0.5f, (mx - mn) * 0.5f), flags, MAT_PLAIN);
}

void MapKit::ClipR(Vec3 c, Vec3 h, float yaw, u8 flags) { w.AddCollider(OBox(c, h, yaw), flags, MAT_PLAIN); }

void MapKit::Glass(Vec3 mn, Vec3 mx, u32 tint, float alpha, bool collide) {
  gb.Mat(MAT_GLASS);
  gb.ColorHex(tint, alpha);
  gb.Box(mn, mx);
  if (collide) w.AddCollider(OBox((mn + mx) * 0.5f, (mx - mn) * 0.5f), COL_SOLID | COL_GLASS, MAT_GLASS);
}

void MapKit::Slab(float x0, float z0, float x1, float z1, float yTop, float thick, Material floorMat, u32 floorCol,
                  u32 ceilCol) {
  Vec3 mn(x0, yTop - thick, z0), mx(x1, yTop, z1);
  Material m = ob.CurMat();
  ob.Mat(floorMat);
  ob.ColorHex(floorCol);
  ob.Box(mn, mx, FACE_PY);
  ob.Mat(MAT_PLASTER);
  ob.ColorHex(ceilCol);
  ob.Box(mn, mx, FACE_NY | FACE_PX | FACE_NX | FACE_PZ | FACE_NZ);
  ob.Mat(m);
  w.AddCollider(OBox((mn + mx) * 0.5f, (mx - mn) * 0.5f), COL_SOLID, floorMat);
}

void MapKit::FloorFinish(float x0, float z0, float x1, float z1, float y, Material m, u32 col) {
  Material old = ob.CurMat();
  ob.Mat(m);
  ob.ColorHex(col);
  ob.Box(Vec3(x0, y - 0.01f, z0), Vec3(x1, y, z1), FACE_PY);
  ob.Mat(old);
}

// Zerlegt eine Wand mit beliebig vielen (auch übereinanderliegenden) Öffnungen in feste Quader.
// emit(a0, a1, ya, yb) erzeugt ein Wandstück zwischen a0..a1 entlang der Wand und ya..yb in der Höhe.
static void SplitWall(float w0, float w1, float y0, float y1, const std::vector<Hole>& holes,
                      const std::function<void(float, float, float, float)>& emit) {
  std::vector<float> cuts = {w0, w1};
  for (const Hole& h : holes) {
    cuts.push_back(Clamp(h.a0, w0, w1));
    cuts.push_back(Clamp(h.a1, w0, w1));
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end(), [](float a, float b) { return fabsf(a - b) < 0.0005f; }), cuts.end());
  // Segmente mit identischem Lochmuster zusammenfassen
  float runStart = w0;
  std::vector<std::pair<float, float>> runPattern;
  bool first = true;
  auto flush = [&](float end) {
    if (end - runStart < 0.0005f) return;
    float cur = y0;
    for (auto& p : runPattern) {
      if (p.first > cur + 0.001f) emit(runStart, end, cur, p.first);
      cur = std::max(cur, p.second);
    }
    if (cur < y1 - 0.001f) emit(runStart, end, cur, y1);
  };
  for (size_t i = 0; i + 1 < cuts.size(); i++) {
    float s0 = cuts[i], s1 = cuts[i + 1];
    std::vector<std::pair<float, float>> pattern;
    for (const Hole& h : holes)
      if (h.a0 <= s0 + 0.0005f && h.a1 >= s1 - 0.0005f) pattern.push_back({std::max(h.y0, y0), std::min(h.y1, y1)});
    std::sort(pattern.begin(), pattern.end());
    if (first) { runPattern = pattern; first = false; }
    else if (pattern != runPattern) {
      flush(s0);
      runStart = s0;
      runPattern = pattern;
    }
  }
  flush(w1);
}

void MapKit::RegisterDoors(bool alongX, float fixed0, float fixed1, const std::vector<Hole>& holes) {
  for (const Hole& h : holes) {
    if (h.window || h.y1 - h.y0 < 1.9f || h.a1 - h.a0 < 0.9f) continue;  // Fenster, keine Tür
    DoorInfo d;
    d.alongX = alongX;
    d.width = h.a1 - h.a0;
    d.depth = fixed1 - fixed0;
    float mid = (h.a0 + h.a1) * 0.5f, f = (fixed0 + fixed1) * 0.5f;
    d.center = alongX ? Vec3(mid, h.y0, f) : Vec3(f, h.y0, mid);
    w.doors.push_back(d);
  }
}

bool MapKit::BlocksDoor(int type, Vec3 pos, float yaw) const {
  const PropDef& d = props::Get(type);
  if (!d.solid) return false;
  OBox pb(pos + RotateY(d.colCenter, yaw), d.colHalf, yaw);
  for (const DoorInfo& dr : w.doors) {
    // Freiraum: volle Türbreite, 1.2 m vor und hinter der Wand, Körperhöhe
    float hw = dr.width * 0.5f - 0.05f, hd = dr.depth * 0.5f + 1.2f;
    OBox clear(dr.center + Vec3(0, 1.05f, 0), dr.alongX ? Vec3(hw, 0.9f, hd) : Vec3(hd, 0.9f, hw));
    if (OverlapOBox(pb, clear, nullptr)) return true;
  }
  return false;
}

void MapKit::WallX(float x0, float x1, float z0, float z1, float y0, float y1, const std::vector<Hole>& holes) {
  RegisterDoors(true, z0, z1, holes);
  SplitWall(x0, x1, y0, y1, holes, [&](float a0, float a1, float ya, float yb) {
    Solid(Vec3(a0, ya, z0), Vec3(a1, yb, z1));
  });
}

void MapKit::WallZ(float z0, float z1, float x0, float x1, float y0, float y1, const std::vector<Hole>& holes) {
  RegisterDoors(false, x0, x1, holes);
  SplitWall(z0, z1, y0, y1, holes, [&](float a0, float a1, float ya, float yb) {
    Solid(Vec3(x0, ya, a0), Vec3(x1, yb, a1));
  });
}

void MapKit::Stairs(Vec3 start, int dir, float width, float rise, int steps, float run, float baseY) {
  Vec3 fwd = dir == 0 ? Vec3(1, 0, 0) : dir == 1 ? Vec3(-1, 0, 0) : dir == 2 ? Vec3(0, 0, 1) : Vec3(0, 0, -1);
  Vec3 side(fabsf(fwd.z), 0, fabsf(fwd.x));
  for (int i = 0; i < steps; i++) {
    Vec3 a = start + fwd * (run * i) - side * (width * 0.5f);
    Vec3 b = start + fwd * (run * (i + 1)) + side * (width * 0.5f);
    Vec3 mn(std::min(a.x, b.x), baseY, std::min(a.z, b.z));
    Vec3 mx(std::max(a.x, b.x), start.y + rise * (i + 1), std::max(a.z, b.z));
    Solid(mn, mx, FACE_ALL & ~FACE_NY);
  }
}

void MapKit::Railing(Vec3 a, Vec3 b, float height, bool glass) {
  Vec3 d = b - a;
  d.y = 0;
  float len = Length(d);
  if (len < 0.01f) return;
  float yaw = atan2f(d.x, d.z);  // lokale Z-Achse entlang der Linie
  Vec3 c = (a + b) * 0.5f;
  Material m = ob.CurMat();
  if (glass) {
    gb.Mat(MAT_GLASS);
    gb.ColorHex(0xc4dbe8, 0.22f);
    gb.BoxR(c + Vec3(0, height * 0.5f, 0), Vec3(0.012f, height * 0.5f - 0.04f, len * 0.5f), yaw);
  } else {
    ob.Mat(MAT_METAL);
    ob.ColorHex(0x55595f);
    int n = std::max(1, (int)(len / 0.15f));
    for (int i = 0; i <= n; i++) {
      Vec3 p = a + (b - a) * ((float)i / n);
      ob.BoxC(p + Vec3(0, height * 0.5f, 0), Vec3(0.012f, height * 0.5f, 0.012f));
    }
  }
  ob.Mat(MAT_METAL);
  ob.ColorHex(0x9aa0a8);
  ob.BoxR(c + Vec3(0, height, 0), Vec3(0.03f, 0.025f, len * 0.5f + 0.02f), yaw);
  ob.Mat(m);
  w.AddCollider(OBox(c + Vec3(0, height * 0.5f, 0), Vec3(0.05f, height * 0.5f, len * 0.5f), yaw), COL_SOLID | COL_GLASS,
                MAT_GLASS);
}

int MapKit::Prop(const char* id, Vec3 pos, float yaw) {
  int t = props::Find(id);
  if (t < 0) {
    LOG("Unbekanntes Prop: %s", id);
    return -1;
  }
  if (BlocksDoor(t, pos, yaw)) {
    LOG("Prop '%s' bei (%.1f %.1f %.1f) würde eine Tür blockieren – ausgelassen", id, pos.x, pos.y, pos.z);
    return -1;
  }
  return w.AddProp(t, pos, yaw);
}

int MapKit::PropRel(const char* id, Vec3 origin, float baseYaw, Vec3 local, float localYaw) {
  return Prop(id, origin + RotateY(local, baseYaw), baseYaw + localYaw);
}

void MapKit::Light(Vec3 pos, float radius, Vec3 color) {
  PointLight l;
  l.pos = pos;
  l.radius = radius;
  l.color = color;
  w.lights.push_back(l);
}

void MapKit::CeilingLight(Vec3 pos, float radius, Vec3 color, float size) {
  Material m = ob.CurMat();
  ob.Mat(MAT_PLASTIC);
  ob.ColorHex(0xfff6e4);
  ob.Emissive(1.0f);
  ob.Box(pos + Vec3(-size, -0.05f, -size), pos + Vec3(size, 0.0f, size), FACE_ALL & ~FACE_PY);
  ob.Emissive(0);
  ob.Mat(m);
  Light(pos - Vec3(0, 0.35f, 0), radius, color);
}

void MapKit::Zone(Vec3 mn, Vec3 mx, float sky, const char* name, int prio) {
  ::Zone z;
  z.mn = mn;
  z.mx = mx;
  z.sky = sky;
  z.name = name;
  z.priority = prio;
  w.zones.push_back(z);
}

void MapKit::Sign(const char* text, Vec3 center, float yaw, float pixel, u32 color, float glow, bool backing) {
  float width = PixelTextWidth(text, pixel);
  float height = 7 * pixel;
  Material m = ob.CurMat();
  ob.Push(Translation(center) * RotationY(yaw));
  if (backing) {
    ob.Mat(MAT_PLASTIC);
    ob.ColorHex(0x1d2430);
    ob.Box(Vec3(-width * 0.5f - pixel * 2, -height * 0.5f - pixel * 2, -0.04f), Vec3(width * 0.5f + pixel * 2, height * 0.5f + pixel * 2, 0.0f));
  }
  ob.Mat(glow > 0 ? MAT_PLASTIC : MAT_GOLD);
  ob.ColorHex(color);
  ob.Emissive(glow);
  ob.Push(Translation(Vec3(-width * 0.5f, -height * 0.5f, 0)));
  BuildPixelText(ob, text, pixel, pixel * 0.6f);
  ob.Pop();
  ob.Emissive(0);
  ob.Pop();
  ob.Mat(m);
}
