#pragma once
// Eigener Audio-Mixer (waveOut, 44.1 kHz Stereo) mit 3D-Positionierung und Verdeckung.
#include "engine/Common.h"
#include "engine/Math.h"

namespace audio {

constexpr int SAMPLE_RATE = 44100;

struct PlayParams {
  bool positional = false;
  Vec3 pos;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool loop = false;
  float minDist = 2.0f;   // volle Lautstärke bis hierhin
  float maxDist = 40.0f;  // ab hier unhörbar
  int group = 0;          // 0 = Effekte, 1 = Musik/Atmosphäre, 2 = Oberfläche
};

bool Init();
void Shutdown();
bool Available();
// Mono-Samples (-1..1) bei SAMPLE_RATE
int CreateSound(std::vector<float> samples);
// Stereo-Samples (L, R, L, R, ...) – nur nicht-positional; andere Abtastraten über PlayParams::pitch ausgleichen
int CreateSoundStereo(std::vector<float> interleaved);
float SoundLength(int sound);
int Play(int sound, const PlayParams& p);
void Stop(int voice);
void StopAll();
void SetVoiceVolume(int voice, float volume);
void SetVoicePos(int voice, Vec3 pos);
bool IsPlaying(int voice);
void SetListener(Vec3 pos, Vec3 forward, Vec3 right);
void SetGroupVolume(int group, float volume);
void SetMasterVolume(float v);
// Test, ob zwischen zwei Punkten eine Wand liegt (vom Spiel gesetzt)
void SetOcclusionTest(std::function<bool(Vec3, Vec3)> fn);
// Einmal pro Frame im Spiel-Thread: berechnet Pegel, Panning und Verdeckung
void Update(float dt);
// Offline-Prüfung: kurze Transienten dürfen vor dem ersten Positions-/Pegelupdate nicht verloren gehen.
bool TestTransientStart();

}  // namespace audio
