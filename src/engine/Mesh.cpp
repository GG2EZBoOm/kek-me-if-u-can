#include "engine/Mesh.h"

void MeshData::Append(const MeshData& o) {
  u32 base = (u32)verts.size();
  verts.insert(verts.end(), o.verts.begin(), o.verts.end());
  indices.reserve(indices.size() + o.indices.size());
  for (u32 i : o.indices) indices.push_back(base + i);
}

void MeshData::Bounds(Vec3& mn, Vec3& mx) const {
  mn = Vec3(1e30f);
  mx = Vec3(-1e30f);
  for (const Vertex& v : verts) { mn = Min(mn, v.pos); mx = Max(mx, v.pos); }
  if (verts.empty()) mn = mx = Vec3();
}

static Mat4 Transpose(const Mat4& m) {
  Mat4 r;
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) r.m[c * 4 + rr] = m.m[rr * 4 + c];
  return r;
}

void MeshBuilder::Color(Vec3 c, float a) {
  color_[0] = (u8)(Saturate(c.x) * 255 + 0.5f);
  color_[1] = (u8)(Saturate(c.y) * 255 + 0.5f);
  color_[2] = (u8)(Saturate(c.z) * 255 + 0.5f);
  color_[3] = (u8)(Saturate(a) * 255 + 0.5f);
}

void MeshBuilder::Push(const Mat4& m) {
  stack_.push_back(xf_);
  xf_ = xf_ * m;
  nxf_ = Transpose(Inverse(xf_));
  identity_ = false;
}

void MeshBuilder::Pop() {
  if (stack_.empty()) return;
  xf_ = stack_.back();
  stack_.pop_back();
  nxf_ = Transpose(Inverse(xf_));
  identity_ = stack_.empty();
}

void MeshBuilder::Reset() {
  stack_.clear();
  xf_ = nxf_ = Mat4::Identity();
  identity_ = true;
}

u32 MeshBuilder::AddVertex(Vec3 p, Vec3 n) {
  Vertex v;
  v.pos = p;
  v.normal = n;
  memcpy(v.color, color_, 4);
  v.aux[0] = material_;
  v.aux[1] = sky_;
  v.aux[2] = emissive_;
  v.aux[3] = 0;
  out_->verts.push_back(v);
  return (u32)out_->verts.size() - 1;
}

void MeshBuilder::Tri(Vec3 a, Vec3 b, Vec3 c) {
  if (!identity_) { a = TransformPoint(xf_, a); b = TransformPoint(xf_, b); c = TransformPoint(xf_, c); }
  Vec3 n = Normalize(Cross(b - a, c - a));
  u32 i0 = AddVertex(a, n), i1 = AddVertex(b, n), i2 = AddVertex(c, n);
  out_->indices.insert(out_->indices.end(), {i0, i1, i2});
}

void MeshBuilder::Quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
  if (!identity_) {
    a = TransformPoint(xf_, a); b = TransformPoint(xf_, b);
    c = TransformPoint(xf_, c); d = TransformPoint(xf_, d);
  }
  Vec3 n = Normalize(Cross(c - a, d - b));
  u32 i0 = AddVertex(a, n), i1 = AddVertex(b, n), i2 = AddVertex(c, n), i3 = AddVertex(d, n);
  out_->indices.insert(out_->indices.end(), {i0, i1, i2, i0, i2, i3});
}

void MeshBuilder::QuadN(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n) {
  if (!identity_) {
    a = TransformPoint(xf_, a); b = TransformPoint(xf_, b);
    c = TransformPoint(xf_, c); d = TransformPoint(xf_, d);
    n = Normalize(TransformDir(nxf_, n));
  }
  u32 i0 = AddVertex(a, n), i1 = AddVertex(b, n), i2 = AddVertex(c, n), i3 = AddVertex(d, n);
  out_->indices.insert(out_->indices.end(), {i0, i1, i2, i0, i2, i3});
}

void MeshBuilder::Box(Vec3 mn, Vec3 mx, u32 f) {
  float x0 = mn.x, y0 = mn.y, z0 = mn.z, x1 = mx.x, y1 = mx.y, z1 = mx.z;
  if (f & FACE_PX) Quad({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1});
  if (f & FACE_NX) Quad({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0});
  if (f & FACE_PY) Quad({x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0});
  if (f & FACE_NY) Quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1});
  if (f & FACE_PZ) Quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1});
  if (f & FACE_NZ) Quad({x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0});
}

void MeshBuilder::BoxR(Vec3 center, Vec3 half, float yaw, u32 faces) {
  Push(Translation(center) * RotationY(yaw));
  Box(-half, half, faces);
  Pop();
}

void MeshBuilder::Frustum(Vec3 base, float r0, float r1, float h, int segs, bool caps) {
  if (segs < 3) segs = 3;
  u32 start = (u32)out_->verts.size();
  float slope = (r0 - r1) / (h != 0 ? h : 1e-4f);
  for (int i = 0; i <= segs; i++) {
    float a = TAU * i / segs;
    float c = cosf(a), s = sinf(a);
    Vec3 n = Normalize(Vec3(c, slope, s));
    Vec3 pb = base + Vec3(c * r0, 0, s * r0);
    Vec3 pt = base + Vec3(c * r1, h, s * r1);
    Vec3 nn = identity_ ? n : Normalize(TransformDir(nxf_, n));
    AddVertex(identity_ ? pb : TransformPoint(xf_, pb), nn);
    AddVertex(identity_ ? pt : TransformPoint(xf_, pt), nn);
  }
  for (int i = 0; i < segs; i++) {
    u32 b0 = start + i * 2, t0 = b0 + 1, b1 = b0 + 2, t1 = b0 + 3;
    out_->indices.insert(out_->indices.end(), {b1, b0, t0, b1, t0, t1});
  }
  if (!caps) return;
  for (int cap = 0; cap < 2; cap++) {
    float r = cap == 0 ? r0 : r1;
    if (r <= 0.0001f) continue;
    float y = cap == 0 ? 0.0f : h;
    Vec3 n = cap == 0 ? Vec3(0, -1, 0) : Vec3(0, 1, 0);
    Vec3 nn = identity_ ? n : Normalize(TransformDir(nxf_, n));
    Vec3 cc = base + Vec3(0, y, 0);
    u32 ci = AddVertex(identity_ ? cc : TransformPoint(xf_, cc), nn);
    u32 ring = (u32)out_->verts.size();
    for (int i = 0; i <= segs; i++) {
      float a = TAU * i / segs;
      Vec3 p = base + Vec3(cosf(a) * r, y, sinf(a) * r);
      AddVertex(identity_ ? p : TransformPoint(xf_, p), nn);
    }
    for (int i = 0; i < segs; i++) {
      if (cap == 0) out_->indices.insert(out_->indices.end(), {ci, ring + i, ring + i + 1});
      else out_->indices.insert(out_->indices.end(), {ci, ring + i + 1, ring + i});
    }
  }
}

void MeshBuilder::CylinderX(Vec3 center, float r, float len, int segs) {
  Push(Translation(center) * RotationZ(-PI * 0.5f));
  Frustum(Vec3(0, -len * 0.5f, 0), r, r, len, segs, true);
  Pop();
}

void MeshBuilder::CylinderZ(Vec3 center, float r, float len, int segs) {
  Push(Translation(center) * RotationX(PI * 0.5f));
  Frustum(Vec3(0, -len * 0.5f, 0), r, r, len, segs, true);
  Pop();
}

void MeshBuilder::Ellipsoid(Vec3 c, Vec3 rad, int segs, int rings, float yCut) {
  u32 start = (u32)out_->verts.size();
  float phi0 = asinf(Clamp(yCut, -1.0f, 1.0f));
  float phi1 = PI * 0.5f;
  for (int j = 0; j <= rings; j++) {
    float phi = Lerp(phi0, phi1, (float)j / rings);
    float cp = cosf(phi), sp = sinf(phi);
    for (int i = 0; i <= segs; i++) {
      float a = TAU * i / segs;
      Vec3 u(cp * cosf(a), sp, cp * sinf(a));
      Vec3 p = c + u * rad;
      Vec3 n = Normalize(Vec3(u.x / rad.x, u.y / rad.y, u.z / rad.z));
      if (!identity_) { p = TransformPoint(xf_, p); n = Normalize(TransformDir(nxf_, n)); }
      AddVertex(p, n);
    }
  }
  u32 row = segs + 1;
  for (int j = 0; j < rings; j++)
    for (int i = 0; i < segs; i++) {
      u32 b0 = start + j * row + i, b1 = b0 + 1, t0 = b0 + row, t1 = t0 + 1;
      out_->indices.insert(out_->indices.end(), {b1, b0, t0, b1, t0, t1});
    }
  if (yCut > -0.999f) {
    // Boden schließen
    float cp = cosf(phi0), sp = sinf(phi0);
    Vec3 n(0, -1, 0);
    Vec3 nn = identity_ ? n : Normalize(TransformDir(nxf_, n));
    Vec3 cc = c + Vec3(0, sp * rad.y, 0);
    u32 ci = AddVertex(identity_ ? cc : TransformPoint(xf_, cc), nn);
    u32 ring = (u32)out_->verts.size();
    for (int i = 0; i <= segs; i++) {
      float a = TAU * i / segs;
      Vec3 p = c + Vec3(cp * cosf(a) * rad.x, sp * rad.y, cp * sinf(a) * rad.z);
      AddVertex(identity_ ? p : TransformPoint(xf_, p), nn);
    }
    for (int i = 0; i < segs; i++) out_->indices.insert(out_->indices.end(), {ci, ring + i, ring + i + 1});
  }
}

void MeshBuilder::Torus(Vec3 c, float R, float r, int segs, int sides) {
  u32 start = (u32)out_->verts.size();
  for (int j = 0; j <= sides; j++) {
    float phi = -PI + TAU * j / sides;
    float cp = cosf(phi), sp = sinf(phi);
    for (int i = 0; i <= segs; i++) {
      float a = TAU * i / segs;
      Vec3 n(cp * cosf(a), sp, cp * sinf(a));
      Vec3 p = c + Vec3((R + r * cp) * cosf(a), r * sp, (R + r * cp) * sinf(a));
      if (!identity_) { p = TransformPoint(xf_, p); n = Normalize(TransformDir(nxf_, n)); }
      AddVertex(p, n);
    }
  }
  u32 row = segs + 1;
  for (int j = 0; j < sides; j++)
    for (int i = 0; i < segs; i++) {
      u32 b0 = start + j * row + i, b1 = b0 + 1, t0 = b0 + row, t1 = t0 + 1;
      out_->indices.insert(out_->indices.end(), {b1, b0, t0, b1, t0, t1});
    }
}

void MeshBuilder::Ring(Vec3 c, float r0, float r1, int segs, bool up) {
  for (int i = 0; i < segs; i++) {
    float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
    Vec3 i0 = c + Vec3(cosf(a0) * r0, 0, sinf(a0) * r0);
    Vec3 i1 = c + Vec3(cosf(a1) * r0, 0, sinf(a1) * r0);
    Vec3 o0 = c + Vec3(cosf(a0) * r1, 0, sinf(a0) * r1);
    Vec3 o1 = c + Vec3(cosf(a1) * r1, 0, sinf(a1) * r1);
    if (up) Quad(i0, i1, o1, o0);
    else Quad(i1, i0, o0, o1);
  }
}

void MeshBuilder::Wedge(Vec3 mn, Vec3 mx, float h0, float h1) {
  float x0 = mn.x, y0 = mn.y, z0 = mn.z, x1 = mx.x, z1 = mx.z;
  float ya = y0 + h0, yb = y0 + h1;
  Quad({x0, yb, z1}, {x1, yb, z1}, {x1, ya, z0}, {x0, ya, z0});
  Quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1});
  Quad({x1, y0, z1}, {x1, y0, z0}, {x1, ya, z0}, {x1, yb, z1});
  Quad({x0, y0, z0}, {x0, y0, z1}, {x0, yb, z1}, {x0, ya, z0});
  if (h1 > 0.001f) Quad({x0, y0, z1}, {x1, y0, z1}, {x1, yb, z1}, {x0, yb, z1});
  if (h0 > 0.001f) Quad({x1, y0, z0}, {x0, y0, z0}, {x0, ya, z0}, {x1, ya, z0});
}

void Mesh::Upload(const MeshData& d, bool dynamic) {
  if (!vao_) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, aux));
  } else {
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  }
  size_t vb = d.verts.size() * sizeof(Vertex);
  size_t ib = d.indices.size() * sizeof(u32);
  if (dynamic && vb <= vboCap_ && vboCap_ > 0) glBufferSubData(GL_ARRAY_BUFFER, 0, vb, d.verts.data());
  else { glBufferData(GL_ARRAY_BUFFER, vb, d.verts.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW); vboCap_ = vb; }
  if (dynamic && ib <= iboCap_ && iboCap_ > 0) glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ib, d.indices.data());
  else { glBufferData(GL_ELEMENT_ARRAY_BUFFER, ib, d.indices.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW); iboCap_ = ib; }
  indexCount_ = (int)d.indices.size();
  glBindVertexArray(0);
}

void Mesh::Destroy() {
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ibo_);
  }
  vao_ = vbo_ = ibo_ = 0;
  indexCount_ = 0;
  vboCap_ = iboCap_ = 0;
}

void Mesh::Draw() const {
  if (!indexCount_) return;
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

void Mesh::DrawInstanced(int count) const {
  if (!indexCount_ || count <= 0) return;
  glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr, count);
}
