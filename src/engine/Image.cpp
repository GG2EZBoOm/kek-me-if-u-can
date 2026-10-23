#include "engine/Image.h"

static u32 g_crcTable[256];
static bool g_crcInit = false;

static u32 Crc(const u8* data, size_t n, u32 crc = 0xFFFFFFFFu) {
  if (!g_crcInit) {
    for (u32 i = 0; i < 256; i++) {
      u32 c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      g_crcTable[i] = c;
    }
    g_crcInit = true;
  }
  for (size_t i = 0; i < n; i++) crc = g_crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

static void PutU32(std::vector<u8>& v, u32 x) {
  v.push_back((u8)(x >> 24)); v.push_back((u8)(x >> 16)); v.push_back((u8)(x >> 8)); v.push_back((u8)x);
}

static void Chunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data) {
  PutU32(out, (u32)data.size());
  size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  u32 c = Crc(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu;
  PutU32(out, c);
}

bool WritePNG(const std::string& path, int w, int h, const u8* rgba) {
  std::vector<u8> raw;
  raw.reserve((size_t)(w * 4 + 1) * h);
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    raw.insert(raw.end(), rgba + (size_t)y * w * 4, rgba + (size_t)(y + 1) * w * 4);
  }
  std::vector<u8> z;
  z.push_back(0x78); z.push_back(0x01);
  size_t pos = 0;
  u32 a = 1, b = 0;
  for (u8 c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  while (pos < raw.size()) {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    bool last = pos + n == raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back((u8)(n & 0xFF)); z.push_back((u8)(n >> 8));
    z.push_back((u8)(~n & 0xFF)); z.push_back((u8)((~n >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  PutU32(z, (b << 16) | a);

  std::vector<u8> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<u8> ihdr;
  PutU32(ihdr, (u32)w); PutU32(ihdr, (u32)h);
  ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
  Chunk(out, "IHDR", ihdr);
  Chunk(out, "IDAT", z);
  Chunk(out, "IEND", {});
  FILE* f = _wfopen(Utf8ToWide(path).c_str(), L"wb");
  if (!f) return false;
  fwrite(out.data(), 1, out.size(), f);
  fclose(f);
  return true;
}
