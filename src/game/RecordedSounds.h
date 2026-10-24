#pragma once
#include "engine/Common.h"
#include <map>

using RecordedSoundBank = std::map<std::string, std::vector<float>>;
bool DecodeRecordedSounds(const u8* data, size_t size, RecordedSoundBank& out);
bool LoadRecordedSounds(RecordedSoundBank& out);
bool LoadComedySounds(RecordedSoundBank& out);
