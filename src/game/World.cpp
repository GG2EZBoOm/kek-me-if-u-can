#include "game/World.h"

bool RayAABB(Vec3 o, Vec3 d, Vec3 mn, Vec3 mx, float maxT, float& tOut, Vec3& nOut) {
  float tmin = 0, tmax = maxT;
  int axis = -1;
  float sign = 0;
  for (int i = 0; i < 3; i++) {
    if (fabsf(d[i]) < 1e-8f) {
      if (o[i] < mn[i] || o[i] > mx[i]) return false;
      continue;
    }
    float inv = 1.0f / d[i];
    float t0 = (mn[i] - o[i]) * inv;
    float t1 = (mx[i] - o[i]) * inv;
    float s = -1;
    if (t0 > t1) { std::swap(t0, t1); s = 1; }
    if (t0 > tmin) { tmin = t0; axis = i; sign = s; }
    if (t1 < tmax) tmax = t1;
    if (tmin > tmax) return false;
  }
  if (axis < 0) {
    // Ursprung liegt in der Box
    tOut = 0;
    nOut = -Normalize(d);
    return true;
  }
  tOut = tmin;
  nOut = Vec3();
  nOut[axis] = sign;
  return true;
}

bool RayOBox(Vec3 o, Vec3 d, const OBox& b, float maxT, float& t, Vec3& n) {
  Vec3 lo = b.ToLocal(o);
  Vec3 ld = b.DirToLocal(d);
  Vec3 ln;
  if (!RayAABB(lo, ld, -b.h, b.h, maxT, t, ln)) return false;
  n = b.DirToWorld(ln);
  return true;
}

bool OverlapOBox(const OBox& a, const OBox& b, Vec3* mtv) {
  float dy = a.c.y - b.c.y;
  float oy = a.h.y + b.h.y - fabsf(dy);
  if (oy <= 0) return false;
  Vec3 axes[4] = {a.AxisX(), a.AxisZ(), b.AxisX(), b.AxisZ()};
  Vec3 ax[2] = {a.AxisX(), a.AxisZ()};
  Vec3 bx[2] = {b.AxisX(), b.AxisZ()};
  Vec3 d = a.c - b.c;
  d.y = 0;
  float best = 1e30f;
  Vec3 bestAxis;
  for (int i = 0; i < 4; i++) {
    Vec3 n = axes[i];
    float ra = a.h.x * fabsf(Dot(n, ax[0])) + a.h.z * fabsf(Dot(n, ax[1]));
    float rb = b.h.x * fabsf(Dot(n, bx[0])) + b.h.z * fabsf(Dot(n, bx[1]));
    float dist = Dot(d, n);
    float o = ra + rb - fabsf(dist);
    if (o <= 0) return false;
    if (o < best) {
      best = o;
      bestAxis = dist < 0 ? -n : n;
    }
  }
  if (mtv) {
    if (oy < best) *mtv = Vec3(0, dy < 0 ? -oy : oy, 0);
    else *mtv = bestAxis * best;
  }
  return true;
}

Mat4 AnimatedPart::Transform(float t) const {
  float a = phase + t * speed;
  Mat4 r = axis == 0 ? RotationX(a) : axis == 2 ? RotationZ(a) : RotationY(a);
  if (orbit) return Translation(TransformPoint(r, offset) - offset);
  return Translation(pivot) * r * Translation(pivot * -1.0f);
}

void World::Clear() {
  cols.clear();
  props.clear();
  lights.clear();
  zones.clear();
  hunterSpawns.clear();
  propSpawns.clear();
  doors.clear();
  anims.clear();
  flags.clear();
  cameraPath.clear();
  opaque.Clear();
  glass.Clear();
  grid_.clear();
}

int World::AddCollider(const OBox& b, u8 flags, u8 surface, int prop) {
  Collider c;
  c.box = b;
  c.flags = flags;
  c.surface = surface;
  c.prop = prop;
  cols.push_back(c);
  return (int)cols.size() - 1;
}

int World::AddProp(int type, Vec3 pos, float yaw) {
  const PropDef& d = props::Get(type);
  WorldProp p;
  p.type = type;
  p.pos = pos;
  p.yaw = yaw;
  int idx = (int)props.size();
  if (d.solid) {
    u8 surf = d.mesh.verts.empty() ? MAT_PLAIN : d.mesh.verts[0].aux[0];
    if (d.partCollision) {
      // durchlässige Objekte (Torbögen, Gitter): jedes Teil einzeln
      for (const AABB& a : d.parts) {
        Vec3 c = (a.mn + a.mx) * 0.5f, h = Max((a.mx - a.mn) * 0.5f, Vec3(0.02f));
        int ci = AddCollider(OBox(pos + RotateY(c, yaw), h, yaw), COL_SOLID | COL_PROP, surf, idx);
        if (p.collider < 0) p.collider = ci;
      }
    } else {
      OBox b(pos + RotateY(d.colCenter, yaw), d.colHalf, yaw);
      p.collider = AddCollider(b, COL_SOLID | COL_PROP, surf, idx);
    }
  }
  props.push_back(p);
  if (d.light) {
    PointLight l;
    l.pos = pos + RotateY(d.lightOffset, yaw);
    l.radius = d.lightRadius;
    l.color = d.lightColor;
    lights.push_back(l);
  }
  return idx;
}

float World::SkyAt(Vec3 p) const {
  const Zone* z = ZoneAt(p);
  return z ? z->sky : 1.0f;
}

bool Zone::Contains(Vec3 p) const {
  if (p.x < mn.x || p.y < mn.y || p.z < mn.z || p.x > mx.x || p.y > mx.y || p.z > mx.z) return false;
  for (const Vec4& pl : planes)
    if (pl.x * p.x + pl.y * p.y + pl.z * p.z + pl.w > 0) return false;
  return true;
}

const Zone* World::ZoneAt(Vec3 p) const {
  const Zone* best = nullptr;
  for (const Zone& z : zones) {
    if (z.Contains(p)) {
      if (!best || z.priority >= best->priority) best = &z;
    }
  }
  return best;
}

void World::Insert(int idx) {
  Vec3 mn, mx;
  cols[idx].box.Bounds(mn, mx);
  int x0 = ClampI((int)floorf((mn.x - gmin_.x) / cell_), 0, gw_ - 1);
  int x1 = ClampI((int)floorf((mx.x - gmin_.x) / cell_), 0, gw_ - 1);
  int z0 = ClampI((int)floorf((mn.z - gmin_.z) / cell_), 0, gh_ - 1);
  int z1 = ClampI((int)floorf((mx.z - gmin_.z) / cell_), 0, gh_ - 1);
  for (int z = z0; z <= z1; z++)
    for (int x = x0; x <= x1; x++) grid_[z * gw_ + x].push_back(idx);
}

void World::Finalize() {
  bmin = Vec3(1e9f);
  bmax = Vec3(-1e9f);
  for (const Collider& c : cols) {
    Vec3 mn, mx;
    c.box.Bounds(mn, mx);
    bmin = Min(bmin, mn);
    bmax = Max(bmax, mx);
  }
  if (cols.empty()) { bmin = Vec3(-10); bmax = Vec3(10); }
  gmin_ = bmin - Vec3(1, 0, 1);
  gw_ = std::max(1, (int)ceilf((bmax.x - gmin_.x + 1) / cell_));
  gh_ = std::max(1, (int)ceilf((bmax.z - gmin_.z + 1) / cell_));
  grid_.assign((size_t)gw_ * gh_, {});
  for (int i = 0; i < (int)cols.size(); i++) Insert(i);
  stamp_.assign(cols.size(), 0);
  stampId_ = 0;

  // Himmelslicht in die Geometrie backen
  auto bake = [&](MeshData& md) {
    for (Vertex& v : md.verts) {
      float s = SkyAt(v.pos + v.normal * 0.08f);
      v.aux[1] = (u8)(v.aux[1] * s);
    }
  };
  bake(opaque);
  bake(glass);
  for (AnimatedPart& a : anims) bake(a.mesh);
  for (WorldProp& p : props) p.sky = SkyAt(p.pos + Vec3(0, 0.3f, 0));
  env.worldMin = bmin;
  env.worldMax = bmax;
  LOG("Welt '%s': %d Kollider, %d Props, %d Lichter, %d Vertices", name.c_str(), (int)cols.size(), (int)props.size(),
      (int)lights.size(), (int)opaque.verts.size());
}

void World::Query(Vec3 mn, Vec3 mx, std::vector<int>& out) const {
  out.clear();
  if (grid_.empty()) return;
  if (++stampId_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); stampId_ = 1; }
  int x0 = ClampI((int)floorf((mn.x - gmin_.x) / cell_), 0, gw_ - 1);
  int x1 = ClampI((int)floorf((mx.x - gmin_.x) / cell_), 0, gw_ - 1);
  int z0 = ClampI((int)floorf((mn.z - gmin_.z) / cell_), 0, gh_ - 1);
  int z1 = ClampI((int)floorf((mx.z - gmin_.z) / cell_), 0, gh_ - 1);
  for (int z = z0; z <= z1; z++)
    for (int x = x0; x <= x1; x++)
      for (int idx : grid_[z * gw_ + x]) {
        if (stamp_[idx] == stampId_) continue;
        stamp_[idx] = stampId_;
        Vec3 bmn, bmx;
        cols[idx].box.Bounds(bmn, bmx);
        if (bmx.x < mn.x || bmn.x > mx.x || bmx.y < mn.y || bmn.y > mx.y || bmx.z < mn.z || bmn.z > mx.z) continue;
        out.push_back(idx);
      }
}

bool World::RayProp(Vec3 o, Vec3 d, int type, Vec3 pos, float yaw, float maxT, float& tOut, Vec3& nOut) {
  const PropDef& def = props::Get(type);
  Vec3 lo = RotateY(o - pos, -yaw);
  Vec3 ld = RotateY(d, -yaw);
  float t;
  Vec3 n;
  if (!RayAABB(lo, ld, def.bmin - Vec3(0.01f), def.bmax + Vec3(0.01f), maxT, t, n)) return false;
  bool hit = false;
  float best = maxT;
  Vec3 bestN;
  for (const AABB& p : def.parts) {
    if (RayAABB(lo, ld, p.mn, p.mx, best, t, n) && t < best) {
      best = t;
      bestN = n;
      hit = true;
    }
  }
  if (!hit) return false;
  tOut = best;
  nOut = RotateY(bestN, yaw);
  return true;
}

bool World::TestCollider(int idx, Vec3 o, Vec3 d, float maxT, bool exactProps, float& t, Vec3& n) const {
  const Collider& c = cols[idx];
  if (exactProps && c.prop >= 0) {
    const WorldProp& p = props[c.prop];
    return RayProp(o, d, p.type, p.pos, p.yaw, maxT, t, n);
  }
  return RayOBox(o, d, c.box, maxT, t, n);
}

RayHit World::Raycast(Vec3 o, Vec3 dir, float maxDist, u8 mask, bool exactProps) const {
  RayHit hit;
  if (grid_.empty()) return hit;
  Vec3 d = Normalize(dir);
  if (++stampId_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); stampId_ = 1; }
  float best = maxDist;
  int bestIdx = -1;
  Vec3 bestN;

  float fx = (o.x - gmin_.x) / cell_, fz = (o.z - gmin_.z) / cell_;
  int ix = (int)floorf(fx), iz = (int)floorf(fz);
  int stepX = d.x > 0 ? 1 : -1, stepZ = d.z > 0 ? 1 : -1;
  float tDeltaX = fabsf(d.x) > 1e-8f ? cell_ / fabsf(d.x) : 1e30f;
  float tDeltaZ = fabsf(d.z) > 1e-8f ? cell_ / fabsf(d.z) : 1e30f;
  float tMaxX = fabsf(d.x) > 1e-8f ? ((gmin_.x + (ix + (stepX > 0 ? 1 : 0)) * cell_) - o.x) / d.x : 1e30f;
  float tMaxZ = fabsf(d.z) > 1e-8f ? ((gmin_.z + (iz + (stepZ > 0 ? 1 : 0)) * cell_) - o.z) / d.z : 1e30f;
  float tCur = 0;
  int guard = 0;
  while (tCur <= best && guard++ < 4096) {
    if (ix >= 0 && iz >= 0 && ix < gw_ && iz < gh_) {
      for (int idx : grid_[iz * gw_ + ix]) {
        if (stamp_[idx] == stampId_) continue;
        stamp_[idx] = stampId_;
        if (!(cols[idx].flags & mask)) continue;
        float t;
        Vec3 n;
        if (TestCollider(idx, o, d, best, exactProps, t, n) && t < best) {
          best = t;
          bestIdx = idx;
          bestN = n;
        }
      }
    } else {
      // außerhalb des Gitters: abbrechen, wenn wir uns entfernen
      bool leaving = (ix < 0 && stepX < 0) || (ix >= gw_ && stepX > 0) || (iz < 0 && stepZ < 0) || (iz >= gh_ && stepZ > 0);
      if (leaving) break;
    }
    float tNext = std::min(tMaxX, tMaxZ);
    if (best <= tNext) break;
    if (tMaxX < tMaxZ) { tCur = tMaxX; tMaxX += tDeltaX; ix += stepX; }
    else { tCur = tMaxZ; tMaxZ += tDeltaZ; iz += stepZ; }
    if (tCur > maxDist) break;
  }
  if (bestIdx >= 0) {
    hit.hit = true;
    hit.t = best;
    hit.pos = o + d * best;
    hit.normal = bestN;
    hit.collider = bestIdx;
    hit.prop = cols[bestIdx].prop;
    hit.surface = cols[bestIdx].surface;
    hit.flags = cols[bestIdx].flags;
  }
  return hit;
}

bool World::LineOfSight(Vec3 a, Vec3 b) const {
  Vec3 d = b - a;
  float len = Length(d);
  if (len < 1e-4f) return true;
  RayHit h = Raycast(a, d / len, len, COL_BULLET, false);
  return !h.hit;
}
