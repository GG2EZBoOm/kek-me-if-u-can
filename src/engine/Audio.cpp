#include "engine/Audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace audio {

namespace {

constexpr int FRAMES = 512;
constexpr int NUM_BUFFERS = 4;
constexpr int MAX_VOICES = 64;

struct Voice {
  bool active = false;
  int id = 0;
  std::shared_ptr<std::vector<float>> data;
  double cursor = 0;
  float pitch = 1;
  bool loop = false;
  // Zielwerte (Spiel-Thread) und aktuelle Werte (Mixer)
  float targetL = 0, targetR = 0, curL = 0, curR = 0;
  float lpTarget = 1, lpCoef = 1, lpState = 0;
  // Parameter für den Spiel-Thread
  PlayParams params;
  float occlusion = 0, occTimer = 0;
  bool first = true;
};

std::mutex g_mutex;
std::vector<std::shared_ptr<std::vector<float>>> g_sounds;
Voice g_voices[MAX_VOICES];
int g_nextId = 1;
HWAVEOUT g_wave = nullptr;
HANDLE g_event = nullptr;
WAVEHDR g_hdr[NUM_BUFFERS];
std::vector<short> g_buf[NUM_BUFFERS];
std::thread g_thread;
std::atomic<bool> g_running{false};
bool g_available = false;
Vec3 g_lisPos, g_lisFwd(0, 0, -1), g_lisRight(1, 0, 0);
float g_groupVol[3] = {1, 1, 1};
float g_master = 0.8f;
std::function<bool(Vec3, Vec3)> g_occlusion;

void Mix(short* out) {
  float mix[FRAMES * 2] = {};
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (Voice& v : g_voices) {
      if (!v.active || !v.data) continue;
      const std::vector<float>& d = *v.data;
      size_t n = d.size();
      if (n == 0) { v.active = false; continue; }
      float dl = (v.targetL - v.curL) / FRAMES, dr = (v.targetR - v.curR) / FRAMES;
      float dlp = (v.lpTarget - v.lpCoef) / FRAMES;
      for (int i = 0; i < FRAMES; i++) {
        size_t idx = (size_t)v.cursor;
        if (idx >= n) {
          if (v.loop) { v.cursor -= (double)n; idx = (size_t)v.cursor; }
          else { v.active = false; break; }
        }
        float frac = (float)(v.cursor - (double)idx);
        float s0 = d[idx];
        float s1 = idx + 1 < n ? d[idx + 1] : (v.loop ? d[0] : 0.0f);
        float s = s0 + (s1 - s0) * frac;
        v.lpState += v.lpCoef * (s - v.lpState);
        s = v.lpState;
        v.curL += dl;
        v.curR += dr;
        v.lpCoef += dlp;
        mix[i * 2] += s * v.curL;
        mix[i * 2 + 1] += s * v.curR;
        v.cursor += v.pitch;
      }
      v.curL = v.targetL;
      v.curR = v.targetR;
      v.lpCoef = v.lpTarget;
    }
  }
  for (int i = 0; i < FRAMES * 2; i++) {
    float s = mix[i] * g_master;
    // weiche Begrenzung gegen Übersteuern
    s = s / (1.0f + fabsf(s) * 0.25f);
    s = Clamp(s, -1.0f, 1.0f);
    out[i] = (short)(s * 32000.0f);
  }
}

void ThreadMain() {
  while (g_running) {
    WaitForSingleObject(g_event, 50);
    for (int b = 0; b < NUM_BUFFERS; b++) {
      if (g_hdr[b].dwFlags & WHDR_DONE) {
        Mix(g_buf[b].data());
        g_hdr[b].dwFlags &= ~WHDR_DONE;
        waveOutWrite(g_wave, &g_hdr[b], sizeof(WAVEHDR));
      }
    }
  }
}

float LowpassCoef(float cutoff) { return 1.0f - expf(-TAU * cutoff / SAMPLE_RATE); }

}  // namespace

bool Init() {
  WAVEFORMATEX wf = {};
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = 2;
  wf.nSamplesPerSec = SAMPLE_RATE;
  wf.wBitsPerSample = 16;
  wf.nBlockAlign = 4;
  wf.nAvgBytesPerSec = SAMPLE_RATE * 4;
  g_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (waveOutOpen(&g_wave, WAVE_MAPPER, &wf, (DWORD_PTR)g_event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
    LOG("Audio: kein Ausgabegerät gefunden – Spiel läuft ohne Ton");
    g_available = false;
    return false;
  }
  for (int b = 0; b < NUM_BUFFERS; b++) {
    g_buf[b].assign(FRAMES * 2, 0);
    g_hdr[b] = WAVEHDR{};
    g_hdr[b].lpData = (LPSTR)g_buf[b].data();
    g_hdr[b].dwBufferLength = FRAMES * 4;
    waveOutPrepareHeader(g_wave, &g_hdr[b], sizeof(WAVEHDR));
    waveOutWrite(g_wave, &g_hdr[b], sizeof(WAVEHDR));
  }
  g_running = true;
  g_thread = std::thread(ThreadMain);
  SetThreadPriority(g_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
  g_available = true;
  LOG("Audio: %d Hz Stereo, %d Puffer à %d Frames", SAMPLE_RATE, NUM_BUFFERS, FRAMES);
  return true;
}

void Shutdown() {
  if (!g_available) return;
  g_running = false;
  SetEvent(g_event);
  if (g_thread.joinable()) g_thread.join();
  waveOutReset(g_wave);
  for (int b = 0; b < NUM_BUFFERS; b++) waveOutUnprepareHeader(g_wave, &g_hdr[b], sizeof(WAVEHDR));
  waveOutClose(g_wave);
  CloseHandle(g_event);
  g_available = false;
}

bool Available() { return g_available; }

int CreateSound(std::vector<float> samples) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_sounds.push_back(std::make_shared<std::vector<float>>(std::move(samples)));
  return (int)g_sounds.size() - 1;
}

float SoundLength(int sound) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (sound < 0 || sound >= (int)g_sounds.size()) return 0;
  return (float)g_sounds[sound]->size() / SAMPLE_RATE;
}

int Play(int sound, const PlayParams& p) {
  if (!g_available) return 0;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (sound < 0 || sound >= (int)g_sounds.size()) return 0;
  Voice* slot = nullptr;
  for (Voice& v : g_voices)
    if (!v.active) { slot = &v; break; }
  if (!slot) {
    // leisesten/ältesten Effekt ersetzen
    float best = 1e9f;
    for (Voice& v : g_voices) {
      if (v.loop) continue;
      float g = v.targetL + v.targetR;
      if (g < best) { best = g; slot = &v; }
    }
    if (!slot) return 0;
  }
  Voice& v = *slot;
  v = Voice();
  v.active = true;
  v.id = g_nextId++;
  v.data = g_sounds[sound];
  v.pitch = p.pitch;
  v.loop = p.loop;
  v.params = p;
  v.first = true;
  return v.id;
}

static Voice* FindVoice(int id) {
  for (Voice& v : g_voices)
    if (v.active && v.id == id) return &v;
  return nullptr;
}

void Stop(int voice) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (Voice* v = FindVoice(voice)) v->active = false;
}

void StopAll() {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (Voice& v : g_voices) v.active = false;
}

void SetVoiceVolume(int voice, float volume) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (Voice* v = FindVoice(voice)) v->params.volume = volume;
}

void SetVoicePos(int voice, Vec3 pos) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (Voice* v = FindVoice(voice)) v->params.pos = pos;
}

bool IsPlaying(int voice) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return FindVoice(voice) != nullptr;
}

void SetListener(Vec3 pos, Vec3 forward, Vec3 right) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_lisPos = pos;
  g_lisFwd = forward;
  g_lisRight = right;
}

void SetGroupVolume(int group, float volume) {
  if (group >= 0 && group < 3) g_groupVol[group] = volume;
}

void SetMasterVolume(float v) { g_master = Clamp(v, 0.0f, 1.5f); }

void SetOcclusionTest(std::function<bool(Vec3, Vec3)> fn) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_occlusion = std::move(fn);
}

void Update(float dt) {
  if (!g_available) return;
  // Verdeckung außerhalb der Sperre testen (Raycasts können dauern)
  struct Job { int id; Vec3 pos; };
  std::vector<Job> jobs;
  Vec3 lis;
  std::function<bool(Vec3, Vec3)> occ;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    lis = g_lisPos;
    occ = g_occlusion;
    for (Voice& v : g_voices) {
      if (!v.active || !v.params.positional) continue;
      v.occTimer -= dt;
      if (v.occTimer <= 0 || v.first) {
        v.occTimer = 0.12f;
        jobs.push_back({v.id, v.params.pos});
      }
    }
  }
  std::vector<std::pair<int, bool>> results;
  if (occ)
    for (const Job& j : jobs) {
      if (Distance(j.pos, lis) > 60.0f) continue;
      results.push_back({j.id, occ(lis, j.pos)});
    }
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto& r : results)
    if (Voice* v = FindVoice(r.first)) v->occlusion = r.second ? 1.0f : 0.0f;
  for (Voice& v : g_voices) {
    if (!v.active) continue;
    const PlayParams& p = v.params;
    float gain = p.volume * g_groupVol[ClampI(p.group, 0, 2)];
    float l = gain, r = gain;
    float cutoff = 20000.0f;
    if (p.positional) {
      Vec3 d = p.pos - g_lisPos;
      float dist = Length(d);
      float att = 1.0f;
      if (dist > p.minDist) att = p.minDist / (p.minDist + (dist - p.minDist) * 0.9f);
      att *= 1.0f - Smoothstep(p.maxDist * 0.7f, p.maxDist, dist);
      Vec3 dir = dist > 0.01f ? d / dist : Vec3();
      float pan = Dot(dir, g_lisRight) * Saturate(dist / 1.5f);
      float behind = Dot(dir, g_lisFwd) < -0.2f ? 1.0f : 0.0f;
      att *= 1.0f - 0.12f * behind;
      if (v.occlusion > 0.5f) {
        att *= 0.5f;
        cutoff = 1100.0f;
      } else if (behind > 0) {
        cutoff = 9000.0f;
      }
      // Luftdämpfung in der Ferne
      cutoff = std::min(cutoff, 20000.0f / (1.0f + dist * 0.04f));
      l = gain * att * sqrtf(0.5f * (1.0f - pan * 0.85f));
      r = gain * att * sqrtf(0.5f * (1.0f + pan * 0.85f));
    } else {
      l = r = gain * 0.7071f;
    }
    v.targetL = l;
    v.targetR = r;
    v.lpTarget = LowpassCoef(cutoff);
    if (v.first) {
      v.curL = l;
      v.curR = r;
      v.lpCoef = v.lpTarget;
      v.first = false;
    }
  }
}

}  // namespace audio
