#include "game/RecordedSounds.h"
#include "engine/Audio.h"
#include <windows.h>

bool DecodeRecordedSounds(const u8* data, size_t size, RecordedSoundBank& out) {
  out.clear();
  if (!data || size < 12 || size > 16 * 1024 * 1024 || memcmp(data, "KSB1", 4)) return false;
  auto u32at = [&](size_t p) { return (u32)data[p] | (u32)data[p+1] << 8 | (u32)data[p+2] << 16 | (u32)data[p+3] << 24; };
  u32 rate = u32at(4), count = u32at(8);
  if (rate != audio::SAMPLE_RATE || !count || count > 128) return false;
  size_t pos = 12;
  RecordedSoundBank decoded;
  for (u32 k = 0; k < count; k++) {
    if (size - pos < 8) return false;
    u32 nameSize = u32at(pos), frames = u32at(pos + 4); pos += 8;
    if (!nameSize || nameSize > 32 || !frames || frames > audio::SAMPLE_RATE * 5 ||
        (size_t)nameSize + (size_t)frames * 2 > size - pos) return false;
    std::string name((const char*)data + pos, nameSize); pos += nameSize;
    for (char c : name) if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_') return false;
    if (decoded.count(name)) return false;
    std::vector<float> samples(frames);
    for (u32 i = 0; i < frames; i++, pos += 2) {
      u16 value = (u16)data[pos] | (u16)data[pos + 1] << 8;
      samples[i] = (value < 32768 ? (int)value : (int)value - 65536) / 32768.0f;
    }
    decoded.emplace(std::move(name), std::move(samples));
  }
  if (pos != size) return false;
  out = std::move(decoded);
  return true;
}

static bool LoadSoundResource(int id, RecordedSoundBank& out) {
  HMODULE module = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!resource) { out.clear(); return false; }
  HGLOBAL loaded = LoadResource(module, resource);
  return DecodeRecordedSounds((const u8*)LockResource(loaded), SizeofResource(module, resource), out);
}

bool LoadRecordedSounds(RecordedSoundBank& out) { return LoadSoundResource(301,out); }
bool LoadComedySounds(RecordedSoundBank& out) { return LoadSoundResource(302,out); }
