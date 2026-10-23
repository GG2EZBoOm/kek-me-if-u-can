#pragma once
// Vektor-/Matrixmathematik. Rechtshändig, Y nach oben, Matrizen spaltenweise (OpenGL).
#include <cmath>
#include <cstdint>

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int ClampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Saturate(float v) { return Clamp(v, 0.0f, 1.0f); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Smoothstep(float a, float b, float x) { float t = Saturate((x - a) / (b - a)); return t * t * (3 - 2 * t); }
inline float Sign(float v) { return v < 0 ? -1.0f : 1.0f; }
inline float Sq(float v) { return v * v; }
// Winkel auf (-PI, PI] bringen
inline float WrapAngle(float a) {
  a = fmodf(a + PI, TAU);
  if (a < 0) a += TAU;
  return a - PI;
}
inline float AngleDiff(float from, float to) { return WrapAngle(to - from); }
inline float LerpAngle(float a, float b, float t) { return a + AngleDiff(a, b) * t; }
// Exponentielle Annäherung, framerate-unabhängig
inline float Damp(float cur, float target, float rate, float dt) { return Lerp(target, cur, expf(-rate * dt)); }

struct Vec2 {
  float x = 0, y = 0;
  constexpr Vec2() = default;
  constexpr Vec2(float a, float b) : x(a), y(b) {}
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2 operator/(float s) const { return {x / s, y / s}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float Length(Vec2 a) { return sqrtf(Dot(a, a)); }

struct Vec3 {
  float x = 0, y = 0, z = 0;
  constexpr Vec3() = default;
  constexpr Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
  explicit constexpr Vec3(float s) : x(s), y(s), z(s) {}
  Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(Vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
  Vec3 operator/(Vec3 o) const { return {x / o.x, y / o.y, z / o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  Vec3& operator*=(Vec3 o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
  float& operator[](int i) { return (&x)[i]; }
  float operator[](int i) const { return (&x)[i]; }
};
inline Vec3 operator*(float s, Vec3 v) { return v * s; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(Vec3 a) { return sqrtf(Dot(a, a)); }
inline float LengthSq(Vec3 a) { return Dot(a, a); }
inline Vec3 Normalize(Vec3 a) { float l = Length(a); return l > 1e-8f ? a / l : Vec3(0, 0, 0); }
inline Vec3 Lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline Vec3 Min(Vec3 a, Vec3 b) { return {fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)}; }
inline Vec3 Max(Vec3 a, Vec3 b) { return {fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)}; }
inline Vec3 Abs(Vec3 a) { return {fabsf(a.x), fabsf(a.y), fabsf(a.z)}; }
inline float Distance(Vec3 a, Vec3 b) { return Length(a - b); }
inline float DistanceXZ(Vec3 a, Vec3 b) { return sqrtf(Sq(a.x - b.x) + Sq(a.z - b.z)); }
// Blickrichtung für yaw/pitch. yaw=0 schaut nach -Z, positiver yaw dreht nach links.
inline Vec3 DirFromYawPitch(float yaw, float pitch) {
  float cp = cosf(pitch);
  return {-sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp};
}
inline Vec3 RightFromYaw(float yaw) { return {cosf(yaw), 0, -sinf(yaw)}; }
inline float YawFromDir(Vec3 d) { return atan2f(-d.x, -d.z); }
// Rotation eines Punktes um Y (lokal -> Welt)
inline Vec3 RotateY(Vec3 v, float yaw) {
  float c = cosf(yaw), s = sinf(yaw);
  return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 0;
  constexpr Vec4() = default;
  constexpr Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
  constexpr Vec4(Vec3 v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
  Vec3 xyz() const { return {x, y, z}; }
  Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
  Vec4 operator+(Vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
};

struct Mat4 {
  float m[16];  // m[spalte*4 + zeile]
  static Mat4 Identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
  }
  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
      r.m[c * 4 + rr] = s;
    }
  return r;
}
inline Vec4 operator*(const Mat4& a, Vec4 v) {
  return {a.m[0] * v.x + a.m[4] * v.y + a.m[8] * v.z + a.m[12] * v.w,
          a.m[1] * v.x + a.m[5] * v.y + a.m[9] * v.z + a.m[13] * v.w,
          a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w,
          a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w};
}
inline Vec3 TransformPoint(const Mat4& a, Vec3 p) { return (a * Vec4(p, 1)).xyz(); }
inline Vec3 TransformDir(const Mat4& a, Vec3 d) { return (a * Vec4(d, 0)).xyz(); }

inline Mat4 Translation(Vec3 t) {
  Mat4 r = Mat4::Identity();
  r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
  return r;
}
inline Mat4 Scaling(Vec3 s) {
  Mat4 r = Mat4::Identity();
  r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
  return r;
}
inline Mat4 RotationY(float a) {
  Mat4 r = Mat4::Identity();
  float c = cosf(a), s = sinf(a);
  r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
  return r;
}
inline Mat4 RotationX(float a) {
  Mat4 r = Mat4::Identity();
  float c = cosf(a), s = sinf(a);
  r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
  return r;
}
inline Mat4 RotationZ(float a) {
  Mat4 r = Mat4::Identity();
  float c = cosf(a), s = sinf(a);
  r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
  return r;
}
inline Mat4 Perspective(float fovy, float aspect, float zn, float zf) {
  Mat4 r{};
  float f = 1.0f / tanf(fovy * 0.5f);
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (zf + zn) / (zn - zf);
  r.m[11] = -1;
  r.m[14] = 2 * zf * zn / (zn - zf);
  return r;
}
inline Mat4 Ortho(float l, float rr, float b, float t, float n, float f) {
  Mat4 r = Mat4::Identity();
  r.m[0] = 2 / (rr - l);
  r.m[5] = 2 / (t - b);
  r.m[10] = -2 / (f - n);
  r.m[12] = -(rr + l) / (rr - l);
  r.m[13] = -(t + b) / (t - b);
  r.m[14] = -(f + n) / (f - n);
  return r;
}
inline Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up) {
  Vec3 f = Normalize(target - eye);
  Vec3 s = Normalize(Cross(f, up));
  Vec3 u = Cross(s, f);
  Mat4 r = Mat4::Identity();
  r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
  r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
  r.m[12] = -Dot(s, eye);
  r.m[13] = -Dot(u, eye);
  r.m[14] = Dot(f, eye);
  return r;
}
inline Mat4 Inverse(const Mat4& mat) {
  const float* m = mat.m;
  Mat4 r;
  float* inv = r.m;
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (fabsf(det) < 1e-12f) return Mat4::Identity();
  det = 1.0f / det;
  for (int i = 0; i < 16; i++) inv[i] *= det;
  return r;
}

// Schneller deterministischer Zufallsgenerator (PCG32)
struct Rng {
  uint64_t state = 0x853c49e6748fea9bULL;
  uint64_t inc = 0xda3e39cb94b95bdbULL;
  Rng() = default;
  explicit Rng(uint64_t seed) { Seed(seed); }
  void Seed(uint64_t seed) {
    state = 0;
    inc = (seed << 1u) | 1u;
    Next();
    state += seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    Next();
  }
  uint32_t Next() {
    uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xs >> rot) | (xs << ((-(int)rot) & 31));
  }
  float Float() { return (Next() >> 8) * (1.0f / 16777216.0f); }
  float Range(float a, float b) { return a + (b - a) * Float(); }
  int Int(int lo, int hiInclusive) {
    if (hiInclusive <= lo) return lo;
    return lo + (int)(Next() % (uint32_t)(hiInclusive - lo + 1));
  }
  bool Chance(float p) { return Float() < p; }
};

inline uint32_t HashU32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d;
  x ^= x >> 15; x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}

// Farbe aus 0xRRGGBB
inline Vec3 Hex(uint32_t c) {
  return {((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f};
}
