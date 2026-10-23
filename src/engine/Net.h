#pragma once
// UDP-Netzwerk (WinSock) und Binär-Serialisierung.
#include "engine/Common.h"
#include "engine/Math.h"

namespace net {

bool Init();
void Shutdown();

struct Address {
  u32 ip = 0;  // Host-Byte-Reihenfolge
  u16 port = 0;
  bool operator==(const Address& o) const { return ip == o.ip && port == o.port; }
  bool operator!=(const Address& o) const { return !(*this == o); }
  std::string ToString() const;
  static bool Parse(const std::string& text, u16 defaultPort, Address& out);
  static Address Broadcast(u16 port) { return {0xFFFFFFFFu, port}; }
  static Address Loopback(u16 port) { return {0x7F000001u, port}; }
};

class Socket {
 public:
  ~Socket() { Close(); }
  // port 0 = beliebiger freier Port
  bool Open(u16 port, bool broadcast = false);
  void Close();
  bool IsOpen() const { return sock_ != ~(uintptr_t)0; }
  bool Send(const Address& to, const void* data, int size);
  // Liefert Anzahl Bytes oder 0, wenn nichts anliegt
  int Recv(Address& from, void* buf, int maxSize);
  u16 LocalPort() const { return port_; }

 private:
  uintptr_t sock_ = ~(uintptr_t)0;
  u16 port_ = 0;
};

class Writer {
 public:
  std::vector<u8> buf;
  void U8(u8 v) { buf.push_back(v); }
  void U16(u16 v) { U8((u8)v); U8((u8)(v >> 8)); }
  void U32(u32 v) { U16((u16)v); U16((u16)(v >> 16)); }
  void I16(i16 v) { U16((u16)v); }
  void F32(float f) { u32 v; memcpy(&v, &f, 4); U32(v); }
  void V3(Vec3 v) { F32(v.x); F32(v.y); F32(v.z); }
  // Position mit 1 cm Auflösung (±327 m)
  void Pos16(Vec3 v) { I16(Q(v.x * 100)); I16(Q(v.y * 100)); I16(Q(v.z * 100)); }
  void Dir16(Vec3 v) { I16(Q(v.x * 32000)); I16(Q(v.y * 32000)); I16(Q(v.z * 32000)); }
  void Angle(float a) { U16((u16)(int)((WrapAngle(a) + PI) / TAU * 65535.0f)); }
  void Str(const std::string& s) {
    size_t n = std::min<size_t>(s.size(), 255);
    U8((u8)n);
    buf.insert(buf.end(), s.begin(), s.begin() + n);
  }
  int Size() const { return (int)buf.size(); }

 private:
  static i16 Q(float v) { return (i16)Clamp(roundf(v), -32767.0f, 32767.0f); }
};

class Reader {
 public:
  Reader(const u8* d, int n) : d_(d), n_(n) {}
  bool Ok() const { return ok_; }
  bool AtEnd() const { return pos_ >= n_; }
  u8 U8() {
    if (pos_ + 1 > n_) { ok_ = false; return 0; }
    return d_[pos_++];
  }
  u16 U16() { u16 a = U8(); u16 b = U8(); return (u16)(a | (b << 8)); }
  u32 U32() { u32 a = U16(); u32 b = U16(); return a | (b << 16); }
  i16 I16() { return (i16)U16(); }
  float F32() { u32 v = U32(); float f; memcpy(&f, &v, 4); return f; }
  Vec3 V3() { float x = F32(), y = F32(), z = F32(); return {x, y, z}; }
  Vec3 Pos16() { float x = I16() / 100.0f, y = I16() / 100.0f, z = I16() / 100.0f; return {x, y, z}; }
  Vec3 Dir16() { float x = I16() / 32000.0f, y = I16() / 32000.0f, z = I16() / 32000.0f; return {x, y, z}; }
  float Angle() { return U16() / 65535.0f * TAU - PI; }
  std::string Str() {
    int n = U8();
    if (pos_ + n > n_) { ok_ = false; return {}; }
    std::string s((const char*)d_ + pos_, n);
    pos_ += n;
    return s;
  }

 private:
  const u8* d_;
  int n_;
  int pos_ = 0;
  bool ok_ = true;
};

}  // namespace net
