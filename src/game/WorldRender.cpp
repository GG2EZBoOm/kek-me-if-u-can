#include "game/WorldRender.h"

void Frustum::FromMatrix(const Mat4& m) {
  auto row = [&](int r) { return Vec4(m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3)); };
  Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
  planes[0] = r3 + r0;
  planes[1] = r3 + r0 * -1.0f;
  planes[2] = r3 + r1;
  planes[3] = r3 + r1 * -1.0f;
  planes[4] = r3 + r2;
  planes[5] = r3 + r2 * -1.0f;
  for (Vec4& p : planes) {
    float l = Length(p.xyz());
    if (l > 0) p = p * (1.0f / l);
  }
}

bool Frustum::Sphere(Vec3 c, float r) const {
  for (const Vec4& p : planes)
    if (p.x * c.x + p.y * c.y + p.z * c.z + p.w < -r) return false;
  return true;
}

void WorldRender::Init(Renderer& r) {
  if (init_) return;
  r.ClearPropMeshes();
  radius_.clear();
  for (int i = 0; i < props::Count(); i++) {
    const PropDef& d = props::Get(i);
    r.AddPropMesh(d.mesh);
    radius_.push_back(Length(d.bmax - d.bmin) * 0.5f);
  }
  init_ = true;
}

void WorldRender::Load(Renderer& r, const World& w) {
  r.SetWorld(w.opaque, w.glass);
  r.ClearDecals();
  for (Anim& a : anims_) a.mesh->Destroy();
  anims_.clear();
  for (const AnimatedPart& a : w.anims) {
    Anim an;
    an.mesh = std::make_unique<Mesh>();
    an.mesh->Upload(a.mesh);
    an.center = a.pivot;
    for (const Vertex& v : a.mesh.verts) an.radius = std::max(an.radius, Distance(v.pos, a.pivot));
    if (a.orbit) an.radius += Length(a.offset);
    if (a.slideLen > 0) an.radius += Length(a.slideDir) * a.slideLen;
    anims_.push_back(std::move(an));
  }
  loaded_ = &w;
}

static u32 FlagColor(u8 pattern, int i, int j, int nx, int ny) {
  switch (pattern) {
    case FLAG_GERMANY: {
      int band = j * 3 / ny;
      return band == 0 ? 0x151515 : (band == 1 ? 0xdd0000 : 0xffce00);
    }
    case FLAG_EUROPE: return 0x0b3c99;
    case FLAG_BAVARIA: return ((i + j) & 1) ? 0xf4f4f0 : 0x2a6ebb;
    default: return (i * 4 / nx) % 2 ? 0xf4f4f0 : 0x2a6ebb;
  }
}

// Stoffgitter aller Fahnen neu aufbauen: überlagerte Wellen entlang der Fahne, Durchhängen bei wenig Wind
void WorldRender::BuildFlags(const World& w, const Frustum& fr, float t) {
  flagData_.Clear();
  for (const WindFlag& f : w.flags) {
    Vec3 X = RotateY(Vec3(1, 0, 0), f.yaw), N = RotateY(Vec3(0, 0, 1), f.yaw), Y(0, 1, 0);
    Vec3 mid = f.attach + X * (f.width * 0.5f) - Y * (f.height * 0.5f);
    if (!fr.Sphere(mid, f.width * 0.7f + f.height)) continue;
    const int nx = 16;
    const int ny = f.pattern == FLAG_GERMANY ? 9 : (f.pattern == FLAG_EUROPE ? 8 : 6);
    auto pos = [&](float u, float v) {
      float a = f.wind;
      float wave = sinf(t * 5.6f - u * 7.0f + f.phase) * 0.6f + sinf(t * 9.1f - u * 12.0f + v * 3.0f + f.phase * 1.3f) * 0.28f +
                   sinf(t * 2.3f + f.phase * 0.7f) * 0.15f;
      float dz = a * f.width * 0.13f * (0.15f + u) * wave * u;
      float dx = -a * f.width * 0.035f * u * fabsf(wave);
      float dy = -f.width * 0.05f * u * u * (1.0f - 0.75f * a);
      return f.attach + X * (u * f.width + dx) + Y * (-v * f.height + dy) + N * dz;
    };
    auto normal = [&](float u, float v) {
      const float e = 0.01f;
      Vec3 du = pos(std::min(u + e, 1.0f), v) - pos(std::max(u - e, 0.0f), v);
      Vec3 dv = pos(u, std::min(v + e, 1.0f)) - pos(u, std::max(v - e, 0.0f));
      return Normalize(Cross(dv, du));
    };
    u8 sky = (u8)(Saturate(w.SkyAt(mid)) * 255);
    auto emitQuad = [&](const Vec3* p, const Vec3* n, u32 col, bool back) {
      u32 base = (u32)flagData_.verts.size();
      Vec3 c = Hex(col);
      for (int k = 0; k < 4; k++) {
        Vertex vx;
        vx.pos = p[k];
        vx.normal = back ? n[k] * -1.0f : n[k];
        vx.color[0] = (u8)(c.x * 255); vx.color[1] = (u8)(c.y * 255); vx.color[2] = (u8)(c.z * 255); vx.color[3] = 255;
        vx.aux[0] = MAT_FABRIC; vx.aux[1] = sky; vx.aux[2] = 0; vx.aux[3] = 0;
        flagData_.verts.push_back(vx);
      }
      // Vorderseite: oben links, unten links, unten rechts, oben rechts (gegen den Uhrzeigersinn)
      const u32 front[6] = {0, 1, 2, 0, 2, 3}, rev[6] = {0, 2, 1, 0, 3, 2};
      for (u32 k : back ? rev : front) flagData_.indices.push_back(base + k);
    };
    for (int j = 0; j < ny; j++)
      for (int i = 0; i < nx; i++) {
        float u0 = (float)i / nx, u1 = (float)(i + 1) / nx, v0 = (float)j / ny, v1 = (float)(j + 1) / ny;
        Vec3 p[4] = {pos(u0, v0), pos(u0, v1), pos(u1, v1), pos(u1, v0)};
        Vec3 n[4] = {normal(u0, v0), normal(u0, v1), normal(u1, v1), normal(u1, v0)};
        u32 col = FlagColor(f.pattern, i, j, nx, ny);
        emitQuad(p, n, col, false);
        emitQuad(p, n, col, true);
      }
    if (f.pattern == FLAG_EUROPE) {
      // Sternenkranz als kleine Rauten auf beiden Seiten
      for (int s = 0; s < 12; s++) {
        float a = TAU * s / 12;
        float u = 0.5f + cosf(a) * 0.33f * f.height / f.width, v = 0.5f + sinf(a) * 0.33f;
        Vec3 c = pos(u, v), n = normal(u, v);
        Vec3 tu = Normalize(pos(u + 0.01f, v) - pos(u - 0.01f, v)), tv = Normalize(pos(u, v + 0.01f) - pos(u, v - 0.01f));
        float r = f.height * 0.045f;
        for (int side = 0; side < 2; side++) {
          Vec3 o = n * (side ? -0.006f : 0.006f);
          Vec3 p[4] = {c + o - tu * r, c + o + tv * r, c + o + tu * r, c + o - tv * r};
          Vec3 nn[4] = {n, n, n, n};
          emitQuad(p, nn, 0xffcc00, side == 1);
        }
      }
    }
  }
}

bool WorldRender::PropVisible(int type, Vec3 pos, float yaw, const Frustum& fr, Vec3 camPos) const {
  const PropDef& d = props::Get(type);
  Vec3 c = pos + RotateY((d.bmin + d.bmax) * 0.5f, yaw);
  float rad = radius_[type];
  if (Distance(c, camPos) > 40.0f + rad * 60.0f) return false;  // kleine Dinge in der Ferne weglassen
  return fr.Sphere(c, rad);
}

void WorldRender::Submit(Renderer& r, const World& w, const Frustum& fr, Vec3 camPos, int highlightProp, float highlight) {
  for (int i = 0; i < (int)w.props.size(); i++) {
    const WorldProp& p = w.props[i];
    if (!PropVisible(p.type, p.pos, p.yaw, fr, camPos)) continue;
    PropInstance inst;
    inst.pos = p.pos;
    inst.yaw = p.yaw;
    inst.sky = p.sky;
    if (i == highlightProp) inst.highlight = highlight;
    r.Prop(p.type, inst);
  }
  for (size_t i = 0; i < anims_.size() && i < w.anims.size(); i++) {
    const Anim& an = anims_[i];
    if (!fr.Sphere(an.center, an.radius)) continue;
    DrawItem it;
    it.mesh = an.mesh.get();
    it.model = w.anims[i].Transform(r.Time());
    r.Draw(it);
  }
  if (!w.flags.empty()) {
    BuildFlags(w, fr, r.Time());
    if (!flagData_.Empty()) {
      flagMesh_.Upload(flagData_, true);
      DrawItem it;
      it.mesh = &flagMesh_;
      r.Draw(it);
    }
  }
}
