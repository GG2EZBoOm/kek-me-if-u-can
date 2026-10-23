#include "game/Sounds.h"
#include "engine/Audio.h"
#include "engine/Math.h"

namespace {

const float SR = (float)audio::SAMPLE_RATE;
using Buf = std::vector<float>;
Rng g_rng(2024);
int g_ids[SND_COUNT];

float Noise() { return g_rng.Float() * 2.0f - 1.0f; }
float Fract(float x) { return x - floorf(x); }
float Saw(float ph) { return 2.0f * Fract(ph) - 1.0f; }
float Sq(float ph, float duty = 0.5f) { return Fract(ph) < duty ? 1.0f : -1.0f; }
float Tri(float ph) { return 4.0f * fabsf(Fract(ph) - 0.5f) - 1.0f; }
float SinC(float ph) { return sinf(ph * TAU); }
float Env(float t, float a, float d) { return t < a ? t / a : expf(-(t - a) * d); }
float NoteHz(int midi) { return 440.0f * powf(2.0f, (midi - 69) / 12.0f); }

struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void LP(float fc, float q = 0.707f) {
    float w = TAU * Clamp(fc, 20.0f, SR * 0.45f) / SR, c = cosf(w), al = sinf(w) / (2 * q), a0 = 1 + al;
    b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
  }
  void HP(float fc, float q = 0.707f) {
    float w = TAU * Clamp(fc, 20.0f, SR * 0.45f) / SR, c = cosf(w), al = sinf(w) / (2 * q), a0 = 1 + al;
    b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
  }
  void BP(float fc, float q) {
    float w = TAU * Clamp(fc, 20.0f, SR * 0.45f) / SR, c = cosf(w), al = sinf(w) / (2 * q), a0 = 1 + al;
    b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
  }
  float Run(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

Buf Make(float sec) { return Buf((size_t)(sec * SR), 0.0f); }
float T(size_t i) { return (float)i / SR; }

void Normalize(Buf& b, float peak = 0.9f) {
  float m = 0;
  for (float s : b) m = std::max(m, fabsf(s));
  if (m > 1e-6f)
    for (float& s : b) s *= peak / m;
}

void Fade(Buf& b, float in, float out) {
  size_t n = b.size();
  size_t fi = (size_t)(in * SR), fo = (size_t)(out * SR);
  for (size_t i = 0; i < fi && i < n; i++) b[i] *= (float)i / fi;
  for (size_t i = 0; i < fo && i < n; i++) b[n - 1 - i] *= (float)i / fo;
}

void Mix(Buf& dst, const Buf& src, float at, float gain = 1.0f) {
  size_t o = (size_t)(at * SR);
  for (size_t i = 0; i < src.size() && o + i < dst.size(); i++) dst[o + i] += src[i] * gain;
}

void Echo(Buf& b, float delay, float gain, int count) {
  Buf src = b;
  for (int k = 1; k <= count; k++) Mix(b, src, delay * k, powf(gain, (float)k));
}

void SoftClip(Buf& b, float drive) {
  for (float& s : b) s = tanhf(s * drive);
}

// Kurzer gefilterter Rauschimpuls
Buf NoiseHit(float len, float lp, float decay, float hp = 30.0f) {
  Buf b = Make(len);
  Biquad f, h;
  f.LP(lp);
  h.HP(hp);
  for (size_t i = 0; i < b.size(); i++) b[i] = h.Run(f.Run(Noise())) * expf(-T(i) * decay);
  return b;
}

// Tonhöhengleitender Sinus (für Wummern)
Buf Thump(float len, float f0, float f1, float decay) {
  Buf b = Make(len);
  float ph = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    float f = f1 + (f0 - f1) * expf(-t * 18.0f);
    ph += f / SR;
    b[i] = SinC(ph) * expf(-t * decay);
  }
  return b;
}

Buf Bell(float len, float f, float decay, float bright = 1.0f) {
  Buf b = Make(len);
  const float ratios[] = {1.0f, 2.76f, 5.4f, 8.93f};
  const float amps[] = {1.0f, 0.5f, 0.25f, 0.12f};
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i), s = 0;
    for (int k = 0; k < 4; k++) s += sinf(TAU * f * ratios[k] * t) * amps[k] * powf(bright, (float)k) * expf(-t * decay * (1 + k * 0.7f));
    b[i] = s * std::min(1.0f, t * 400.0f);
  }
  return b;
}

Buf Click(float freq = 3000.0f, float len = 0.01f) {
  Buf b = Make(len);
  Biquad f;
  f.BP(freq, 2.0f);
  for (size_t i = 0; i < b.size(); i++) b[i] = f.Run(Noise()) * expf(-T(i) * 500.0f) * 3.0f;
  return b;
}

// Formant-Stimme: f0(t), Formanten (F1,F2,F3) über die Zeit, Amplitude
Buf Voice(float len, const std::function<float(float)>& f0, const std::function<Vec3(float)>& formants,
          const std::function<float(float)>& amp, float pulse = 0.5f, float breath = 0.05f) {
  Buf b = Make(len);
  Biquad f1, f2, f3;
  float ph = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    if (i % 64 == 0) {
      Vec3 fm = formants(t);
      f1.BP(fm.x, 6.0f);
      f2.BP(fm.y, 8.0f);
      f3.BP(fm.z, 10.0f);
    }
    ph += f0(t) / SR;
    float src = Saw(ph) * (1.0f - pulse) + Sq(ph, 0.3f) * pulse + Noise() * breath;
    float s = f1.Run(src) * 1.0f + f2.Run(src) * 0.6f + f3.Run(src) * 0.3f;
    b[i] = s * amp(t);
  }
  return b;
}

// Blechbläser-Akkord/Ton mit sich öffnendem Filter
Buf Brass(float len, std::vector<float> freqs, float attack = 0.04f, float release = 0.15f) {
  Buf b = Make(len);
  Biquad f;
  std::vector<float> ph(freqs.size(), 0.0f);
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    float env = std::min(1.0f, t / attack) * std::min(1.0f, (len - t) / release);
    if (i % 32 == 0) f.LP(400.0f + 3200.0f * env * (0.7f + 0.3f * sinf(t * 30.0f)), 1.2f);
    float s = 0;
    for (size_t k = 0; k < freqs.size(); k++) {
      ph[k] += freqs[k] * (1.0f + 0.003f * sinf(t * 5.5f * TAU)) / SR;
      s += Saw(ph[k]);
    }
    b[i] = f.Run(s / freqs.size()) * env;
  }
  return b;
}

// --- Effekte ---------------------------------------------------------------------------------

Buf Gunshot(bool shotgun) {
  float len = shotgun ? 0.9f : 0.35f;
  Buf b = Make(len);
  Mix(b, NoiseHit(len, shotgun ? 2400.0f : 4200.0f, shotgun ? 9.0f : 30.0f), 0, 1.0f);
  Mix(b, Thump(len, shotgun ? 160.0f : 220.0f, shotgun ? 38.0f : 55.0f, shotgun ? 9.0f : 22.0f), 0, shotgun ? 1.3f : 0.9f);
  Mix(b, Click(5000.0f, 0.004f), 0, 1.5f);
  SoftClip(b, 2.2f);
  Echo(b, shotgun ? 0.07f : 0.05f, 0.28f, 3);
  Normalize(b, 0.95f);
  Fade(b, 0, 0.05f);
  return b;
}

Buf Reload(bool shotgun) {
  Buf b = Make(shotgun ? 2.3f : 1.8f);
  if (!shotgun) {
    Mix(b, Click(1800, 0.03f), 0.12f, 0.8f);
    Mix(b, NoiseHit(0.09f, 3000, 30, 900), 0.55f, 0.4f);
    Mix(b, Click(1400, 0.04f), 1.15f, 1.0f);
    Mix(b, Click(2600, 0.03f), 1.55f, 0.9f);
    Mix(b, NoiseHit(0.06f, 4000, 40, 1500), 1.6f, 0.4f);
  } else {
    for (int k = 0; k < 3; k++) Mix(b, Click(2200, 0.03f), 0.3f + k * 0.45f, 0.8f);
    Mix(b, NoiseHit(0.12f, 2500, 25, 700), 1.8f, 0.6f);
    Mix(b, Click(1500, 0.04f), 1.95f, 1.0f);
    Mix(b, NoiseHit(0.1f, 2500, 25, 700), 2.05f, 0.6f);
  }
  Normalize(b, 0.6f);
  return b;
}

Buf Step(int variant) {
  Buf b = Make(0.12f);
  Mix(b, NoiseHit(0.12f, 700.0f + variant * 120.0f, 45.0f, 60.0f), 0, 1.0f);
  Mix(b, Thump(0.12f, 110.0f, 60.0f + variant * 6.0f, 40.0f), 0, 0.5f);
  Mix(b, NoiseHit(0.03f, 5000, 120, 2000), 0.01f, 0.15f);
  Normalize(b, 0.5f);
  return b;
}

Buf Explosion() {
  Buf b = Make(2.6f);
  Biquad f;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    if (i % 32 == 0) f.LP(200.0f + 1400.0f * expf(-t * 3.0f), 0.9f);
    float crackle = (g_rng.Float() < 0.002f * expf(-t * 2.0f)) ? Noise() * 3.0f : 0.0f;
    b[i] = f.Run(Noise()) * Env(t, 0.005f, 2.2f) * 1.4f + crackle;
  }
  Mix(b, Thump(2.6f, 90.0f, 32.0f, 2.5f), 0, 1.4f);
  SoftClip(b, 1.8f);
  Echo(b, 0.11f, 0.3f, 3);
  Normalize(b, 0.95f);
  Fade(b, 0, 0.3f);
  return b;
}

Buf Whoosh(float len, float f0, float f1) {
  Buf b = Make(len);
  Biquad f;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i) / len;
    if (i % 32 == 0) f.BP(f0 + (f1 - f0) * t, 2.0f);
    b[i] = f.Run(Noise()) * sinf(t * PI) * 2.0f;
  }
  return b;
}

Buf Chime(std::vector<int> notes, float step, float len, float decay = 6.0f) {
  Buf b = Make(len);
  for (size_t k = 0; k < notes.size(); k++) Mix(b, Bell(len - step * k, NoteHz(notes[k]), decay, 0.6f), step * k, 0.5f);
  return b;
}

Buf Beep(float freq, float len, float vol = 0.6f, bool square = false) {
  Buf b = Make(len);
  float ph = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    ph += freq / SR;
    float s = square ? Sq(ph) * 0.4f + SinC(ph) * 0.6f : SinC(ph);
    b[i] = s * std::min(1.0f, t * 300.0f) * std::min(1.0f, (len - t) * 60.0f) * vol;
  }
  return b;
}

Buf Melody(const std::vector<std::pair<int, float>>& notes, const std::function<Buf(float, float)>& voice, float gap = 0.0f) {
  float total = 0;
  for (auto& n : notes) total += n.second + gap;
  Buf b = Make(total + 0.6f);
  float at = 0;
  for (auto& n : notes) {
    if (n.first > 0) Mix(b, voice(NoteHz(n.first), n.second), at, 1.0f);
    at += n.second + gap;
  }
  return b;
}

// --- Taunts ----------------------------------------------------------------------------------

Buf Kuckuck() {
  Buf b = Make(1.6f);
  auto tone = [](float f, float len) {
    Buf t = Make(len);
    float ph = 0;
    for (size_t i = 0; i < t.size(); i++) {
      float x = T(i);
      ph += f * (1.0f - 0.03f * x / len) / SR;
      t[i] = (SinC(ph) + 0.2f * SinC(ph * 2) + 0.05f * Noise()) * std::min(1.0f, x * 40.0f) * expf(-x * 5.0f);
    }
    return t;
  };
  for (int r = 0; r < 2; r++) {
    Mix(b, tone(NoteHz(76), 0.32f), r * 0.75f);
    Mix(b, tone(NoteHz(72), 0.38f), r * 0.75f + 0.3f);
  }
  Echo(b, 0.18f, 0.25f, 2);
  return b;
}

Buf Ente() {
  Buf b = Make(1.7f);
  for (int k = 0; k < 4; k++) {
    float len = 0.2f + 0.05f * (k % 2);
    Buf q = Make(len);
    Biquad f;
    f.BP(1500, 3.0f);
    float ph = 0;
    for (size_t i = 0; i < q.size(); i++) {
      float t = T(i) / len;
      float fr = 900.0f + 600.0f * sinf(t * PI) + 150.0f * k;
      ph += fr / SR;
      q[i] = f.Run(Sq(ph, 0.25f) + Noise() * 0.3f) * sinf(t * PI) * 3.0f;
    }
    Mix(b, q, k * 0.38f);
  }
  return b;
}

Buf Pupser() {
  Buf b = Make(1.3f);
  Biquad f;
  f.LP(450.0f, 2.0f);
  float ph = 0, jitter = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    jitter += (Noise() * 40.0f - jitter) * 0.01f;
    float fr = 80.0f + 30.0f * sinf(t * 11.0f) + jitter + (t > 1.0f ? (t - 1.0f) * 900.0f : 0.0f);
    ph += fr / SR;
    float flutter = 0.6f + 0.4f * sinf(t * 70.0f + sinf(t * 13.0f) * 3.0f);
    float env = std::min(1.0f, t * 20.0f) * std::min(1.0f, (1.3f - t) * 6.0f);
    b[i] = f.Run(Sq(ph, 0.35f) + Noise() * 0.25f) * flutter * env;
  }
  SoftClip(b, 2.0f);
  return b;
}

Buf Klingel() {
  Buf b = Make(1.2f);
  for (int s = 0; s < 2; s++)
    for (int k = 0; k < 7; k++) Mix(b, Bell(0.8f, 1050.0f, 7.0f, 1.3f), s * 0.42f + k * 0.035f, 0.35f);
  return b;
}

Buf TrompeteFail() {
  Buf b = Make(2.6f);
  const int notes[4] = {55, 54, 53, 52};
  const float lens[4] = {0.45f, 0.45f, 0.45f, 1.15f};
  float at = 0;
  for (int k = 0; k < 4; k++) {
    float len = lens[k], f = NoteHz(notes[k]);
    bool last = k == 3;
    Buf n = Voice(len, [&](float t) { return f * (last ? 1.0f + 0.03f * sinf(t * 6.0f * TAU) * std::min(1.0f, t * 2) : 1.0f); },
                  [&](float t) {
                    float o = std::min(1.0f, t / 0.15f);
                    return Vec3(400.0f + 500.0f * o, 900.0f + 500.0f * o, 2500.0f);
                  },
                  [&](float t) { return std::min(1.0f, t * 30.0f) * std::min(1.0f, (len - t) * 12.0f); }, 0.2f, 0.02f);
    Mix(b, n, at);
    at += len;
  }
  Normalize(b, 0.85f);
  return b;
}

Buf Muh() {
  float len = 1.8f;
  Buf b = Voice(len, [](float t) { return 125.0f - 25.0f * t + 3.0f * sinf(t * 5.0f * TAU); },
                [](float t) {
                  float o = Smoothstep(0.1f, 0.5f, t);
                  return Vec3(250.0f + 80.0f * o, 600.0f + 150.0f * o, 2300.0f);
                },
                [&](float t) { return Smoothstep(0.0f, 0.25f, t) * std::min(1.0f, (len - t) * 3.0f); }, 0.1f, 0.04f);
  Normalize(b, 0.9f);
  return b;
}

Buf Miau() {
  float len = 1.2f;
  Buf b = Voice(len, [](float t) { return 480.0f + 380.0f * sinf(Clamp(t / 0.9f, 0.0f, 1.0f) * PI) - 60.0f * t; },
                [](float t) {
                  float u = t / 1.2f;
                  Vec3 i(300, 2300, 3000), a(750, 1150, 2700), o(350, 800, 2500);
                  return u < 0.45f ? Lerp(i, a, u / 0.45f) : Lerp(a, o, (u - 0.45f) / 0.55f);
                },
                [&](float t) { return Smoothstep(0.0f, 0.08f, t) * std::min(1.0f, (len - t) * 5.0f); }, 0.6f, 0.03f);
  Normalize(b, 0.85f);
  return b;
}

Buf Hupe() {
  Buf b = Make(1.1f);
  for (int k = 0; k < 2; k++) {
    float len = 0.38f;
    Buf h = Make(len);
    Biquad f;
    f.BP(700, 1.5f);
    float ph = 0;
    for (size_t i = 0; i < h.size(); i++) {
      float t = T(i);
      ph += (330.0f - 30.0f * t / len) / SR;
      h[i] = f.Run(Saw(ph) + Saw(ph * 1.26f) * 0.6f) * std::min(1.0f, t * 50.0f) * std::min(1.0f, (len - t) * 20.0f) * 2.5f;
    }
    Mix(b, h, k * 0.5f);
  }
  SoftClip(b, 1.5f);
  return b;
}

Buf Pfiff() {
  Buf b = Make(1.4f);
  auto sweep = [](float len, const std::function<float(float)>& f) {
    Buf s = Make(len);
    float ph = 0;
    for (size_t i = 0; i < s.size(); i++) {
      float t = T(i) / len;
      ph += f(t) / SR;
      s[i] = (SinC(ph) + Noise() * 0.04f) * sinf(t * PI);
    }
    return s;
  };
  Mix(b, sweep(0.35f, [](float t) { return 900.0f + 1700.0f * t; }), 0.0f);
  Mix(b, sweep(0.75f, [](float t) { return t < 0.35f ? 1000.0f + 1800.0f * (t / 0.35f) : 2800.0f - 1300.0f * ((t - 0.35f) / 0.65f); }), 0.5f);
  return b;
}

Buf Glockenspiel() {
  return Chime({84, 88, 91, 96, 91, 88, 84, 79}, 0.22f, 2.4f, 4.0f);
}

Buf Hahaha() {
  Buf b = Make(1.9f);
  for (int k = 0; k < 5; k++) {
    float len = 0.2f;
    float f0 = 230.0f - k * 12.0f;
    Buf h = Voice(len, [&](float t) { return f0 - 40.0f * t; }, [](float) { return Vec3(820, 1250, 2650); },
                  [&](float t) { return Smoothstep(0.0f, 0.03f, t) * std::min(1.0f, (len - t) * 15.0f); }, 0.3f, 0.12f);
    Buf asp = NoiseHit(0.05f, 3000, 40, 800);
    Mix(b, asp, k * 0.3f, 0.3f);
    Mix(b, h, k * 0.3f + 0.03f, 1.0f);
  }
  Normalize(b, 0.85f);
  return b;
}

Buf Boing() {
  Buf b = Make(1.0f);
  float ph = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    float f = 140.0f + 260.0f * t + 60.0f * sinf(t * 28.0f) * expf(-t * 3.0f);
    ph += f / SR;
    b[i] = (SinC(ph) + 0.3f * Tri(ph * 2)) * expf(-t * 3.0f) * std::min(1.0f, t * 200.0f);
  }
  return b;
}

Buf Praesidentenglocke() {
  Buf b = Make(2.0f);
  for (int k = 0; k < 6; k++) {
    float f = k % 2 ? 1220.0f : 1180.0f;
    Mix(b, Bell(1.2f, f, 3.5f, 1.1f), k * 0.24f, 0.4f);
  }
  return b;
}

Buf Applaus() {
  Buf b = Make(3.0f);
  for (float t = 0; t < 2.9f;) {
    float density = 8.0f + 35.0f * sinf(Clamp(t / 2.9f, 0.0f, 1.0f) * PI);
    t += -logf(std::max(1e-4f, g_rng.Float())) / density;
    Buf c = Make(0.025f);
    Biquad f;
    f.BP(g_rng.Range(900.0f, 2800.0f), 1.5f);
    for (size_t i = 0; i < c.size(); i++) c[i] = f.Run(Noise()) * expf(-T(i) * 220.0f) * 2.5f;
    Mix(b, c, t, g_rng.Range(0.3f, 1.0f));
  }
  Normalize(b, 0.85f);
  return b;
}

Buf Tusch() {
  Buf b = Make(2.2f);
  std::vector<float> cmaj = {NoteHz(60), NoteHz(64), NoteHz(67), NoteHz(72)};
  Mix(b, Brass(0.16f, cmaj), 0.0f);
  Mix(b, Brass(0.16f, cmaj), 0.2f);
  Mix(b, Brass(0.16f, cmaj), 0.4f);
  Mix(b, Brass(1.3f, {NoteHz(67), NoteHz(71), NoteHz(74), NoteHz(79)}, 0.05f, 0.4f), 0.62f);
  Normalize(b, 0.9f);
  return b;
}

Buf KazooHymne() {
  std::vector<std::pair<int, float>> tune = {{60, 0.45f}, {62, 0.2f}, {64, 0.45f}, {65, 0.3f}, {67, 0.5f},
                                             {67, 0.25f}, {69, 0.35f}, {67, 0.35f}, {65, 0.3f}, {64, 0.3f}, {62, 0.3f}, {60, 0.6f}};
  Buf b = Melody(tune, [](float f, float len) {
    return Voice(len, [&](float t) { return f * (1.0f + 0.01f * sinf(t * 7.0f * TAU)); }, [](float) { return Vec3(600, 1400, 2800); },
                 [&](float t) { return std::min(1.0f, t * 25.0f) * std::min(1.0f, (len - t) * 15.0f); }, 0.15f, 0.25f);
  });
  b.resize((size_t)(4.2f * SR));
  Normalize(b, 0.85f);
  return b;
}

// --- Oberfläche, Fanfaren, Atmosphäre --------------------------------------------------------

Buf Fanfare(bool win) {
  if (win) {
    Buf b = Make(2.6f);
    int seq[4] = {60, 64, 67, 72};
    for (int k = 0; k < 4; k++) Mix(b, Brass(0.22f, {NoteHz(seq[k]), NoteHz(seq[k] + 12) * 0.5f}), k * 0.18f, 0.8f);
    Mix(b, Brass(1.6f, {NoteHz(60), NoteHz(64), NoteHz(67), NoteHz(72)}, 0.05f, 0.6f), 0.75f, 1.0f);
    Normalize(b, 0.85f);
    return b;
  }
  Buf b = TrompeteFail();
  return b;
}

Buf Ambience(bool outdoor) {
  float len = 8.0f, xf = 1.5f;
  Buf b = Make(len + xf);
  Biquad f1, f2;
  f1.LP(outdoor ? 500.0f : 220.0f, 0.7f);
  f2.LP(outdoor ? 1800.0f : 120.0f, 0.7f);
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    float gust = outdoor ? 0.5f + 0.5f * sinf(t * 0.7f) * sinf(t * 0.23f + 1.0f) : 1.0f;
    b[i] = f1.Run(Noise()) * 0.5f * gust + f2.Run(Noise()) * 0.15f * gust;
    if (!outdoor) b[i] += (sinf(TAU * 50.0f * t) * 0.02f + sinf(TAU * 100.0f * t) * 0.012f);
  }
  if (outdoor) {
    for (int k = 0; k < 7; k++) {
      float at = g_rng.Range(0.3f, len - 0.6f);
      Buf chirp = Make(0.35f);
      float ph = 0;
      float base = g_rng.Range(2600.0f, 4200.0f);
      for (size_t i = 0; i < chirp.size(); i++) {
        float t = T(i);
        int syl = (int)(t / 0.07f);
        float u = fmodf(t, 0.07f) / 0.07f;
        ph += (base + 900.0f * sinf(u * PI) * (syl % 2 ? -1 : 1)) / SR;
        chirp[i] = SinC(ph) * sinf(u * PI) * 0.15f;
      }
      Mix(b, chirp, at);
    }
  }
  // nahtlose Schleife: Ende in den Anfang überblenden
  size_t n = (size_t)(len * SR), x = (size_t)(xf * SR);
  for (size_t i = 0; i < x; i++) {
    float a = (float)i / x;
    b[i] = b[i] * a + b[n + i] * (1.0f - a);
  }
  b.resize(n);
  Normalize(b, 0.5f);
  return b;
}

Buf MenuMusic() {
  // ruhige Akkordfläche Am – F – C – G mit Glockenmotiv
  float bar = 3.2f;
  int chords[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 55, 64}, {55, 59, 62}};
  Buf b = Make(bar * 4 + 2.0f);
  for (int c = 0; c < 4; c++) {
    Buf pad = Make(bar + 1.5f);
    Biquad f;
    f.LP(900.0f, 0.8f);
    float ph[6] = {};
    for (size_t i = 0; i < pad.size(); i++) {
      float t = T(i);
      float env = Smoothstep(0.0f, 0.8f, t) * (1.0f - Smoothstep(bar, bar + 1.5f, t));
      float s = 0;
      for (int k = 0; k < 3; k++) {
        float fr = NoteHz(chords[c][k]);
        ph[k] += fr * 1.003f / SR;
        ph[k + 3] += fr * 0.997f / SR;
        s += Saw(ph[k]) + Saw(ph[k + 3]);
      }
      pad[i] = f.Run(s / 6.0f) * env;
    }
    Mix(b, pad, c * bar, 0.6f);
    Mix(b, Bell(2.0f, NoteHz(chords[c][2] + 12), 2.5f, 0.4f), c * bar + 0.4f, 0.12f);
    Mix(b, Bell(2.0f, NoteHz(chords[c][1] + 24), 2.5f, 0.4f), c * bar + 1.6f, 0.08f);
  }
  Echo(b, 0.37f, 0.35f, 4);
  size_t n = (size_t)(bar * 4 * SR), x = (size_t)(1.5f * SR);
  for (size_t i = 0; i < x && n + i < b.size(); i++) {
    float a = (float)i / x;
    b[i] = b[i] * a + b[n + i] * (1.0f - a);
  }
  b.resize(n);
  Normalize(b, 0.5f);
  return b;
}

}  // namespace

void BuildSounds() {
  std::vector<Buf> s(SND_COUNT);
  s[SND_SMG] = Gunshot(false);
  s[SND_SHOTGUN] = Gunshot(true);
  {
    Buf e = Make(0.08f);
    Mix(e, Click(3500, 0.01f), 0);
    Mix(e, Click(2500, 0.01f), 0.035f, 0.7f);
    s[SND_EMPTY] = e;
  }
  s[SND_RELOAD_SMG] = Reload(false);
  s[SND_RELOAD_SHOTGUN] = Reload(true);
  {
    Buf e = Make(0.25f);
    Mix(e, Click(1600, 0.03f), 0);
    Mix(e, NoiseHit(0.08f, 3000, 40, 800), 0.06f, 0.4f);
    Mix(e, Click(2400, 0.03f), 0.16f);
    Normalize(e, 0.5f);
    s[SND_SWITCH] = e;
  }
  {
    Buf w = NoiseHit(0.18f, 1600, 45, 80);
    Mix(w, Click(4000, 0.006f), 0, 0.8f);
    Normalize(w, 0.6f);
    s[SND_IMPACT_WALL] = w;
    Buf wd = NoiseHit(0.2f, 900, 35, 100);
    Mix(wd, Thump(0.2f, 320, 180, 25), 0, 0.8f);
    Normalize(wd, 0.6f);
    s[SND_IMPACT_WOOD] = wd;
    Buf m = Bell(0.4f, 2300, 14, 1.4f);
    Mix(m, Click(6000, 0.005f), 0, 1.0f);
    Normalize(m, 0.5f);
    s[SND_IMPACT_METAL] = m;
    Buf g = Make(0.5f);
    for (int k = 0; k < 6; k++) Mix(g, Bell(0.3f, g_rng.Range(3000, 6500), 20, 0.5f), k * 0.02f, 0.3f);
    Mix(g, NoiseHit(0.1f, 8000, 40, 2500), 0, 0.5f);
    Normalize(g, 0.5f);
    s[SND_IMPACT_GLASS] = g;
  }
  {
    Buf h = NoiseHit(0.25f, 700, 25, 80);
    Mix(h, Thump(0.25f, 260, 120, 18), 0, 1.0f);
    Normalize(h, 0.8f);
    s[SND_HIT_PROP] = h;
    Buf hm = Beep(2700, 0.035f, 0.5f);
    Mix(hm, Beep(4100, 0.025f, 0.3f), 0.0f);
    s[SND_HITMARKER] = hm;
    Buf pen = Make(0.3f);
    float ph = 0;
    for (size_t i = 0; i < pen.size(); i++) {
      float t = T(i);
      ph += (140.0f - 60.0f * t) / SR;
      pen[i] = Saw(ph) * 0.4f * std::min(1.0f, (0.3f - t) * 20.0f);
    }
    s[SND_PENALTY] = pen;
  }
  for (int k = 0; k < 4; k++) s[SND_STEP0 + k] = Step(k);
  {
    Buf j = NoiseHit(0.15f, 900, 30, 100);
    Normalize(j, 0.3f);
    s[SND_JUMP] = j;
  }
  {
    Buf kp = Make(1.4f);
    Buf pop = Make(0.12f);
    float ph = 0;
    for (size_t i = 0; i < pop.size(); i++) {
      float t = T(i);
      ph += (1400.0f * expf(-t * 25.0f) + 200.0f) / SR;
      pop[i] = SinC(ph) * expf(-t * 30.0f);
    }
    Mix(kp, pop, 0);
    Mix(kp, NoiseHit(0.1f, 3000, 40, 300), 0, 0.6f);
    Mix(kp, Chime({84, 91, 96}, 0.08f, 1.2f, 5.0f), 0.08f, 0.8f);
    Normalize(kp, 0.85f);
    s[SND_KILL_PROP] = kp;
    Buf kh = Make(1.0f);
    ph = 0;
    for (size_t i = 0; i < kh.size(); i++) {
      float t = T(i);
      ph += (800.0f * expf(-t * 2.5f) + 120.0f) / SR;
      kh[i] = (SinC(ph) * 0.6f + Tri(ph) * 0.3f) * std::min(1.0f, (1.0f - t) * 4.0f);
    }
    Mix(kh, Thump(0.4f, 120, 50, 10), 0.75f, 0.8f);
    Normalize(kh, 0.7f);
    s[SND_KILL_HUNTER] = kh;
  }
  {
    Buf d = Whoosh(0.45f, 300, 2500);
    Mix(d, Chime({88, 91, 95, 100}, 0.05f, 0.9f, 7.0f), 0.1f, 0.6f);
    Normalize(d, 0.7f);
    s[SND_DISGUISE] = d;
    Buf dc = NoiseHit(0.3f, 1200, 15, 100);
    Mix(dc, Chime({76, 83}, 0.06f, 0.6f, 6.0f), 0.05f, 0.5f);
    Normalize(dc, 0.6f);
    s[SND_DECOY] = dc;
    Buf db = Make(0.8f);
    Mix(db, s[SND_IMPACT_GLASS], 0);
    Mix(db, Whoosh(0.3f, 2000, 400), 0, 0.5f);
    Normalize(db, 0.7f);
    s[SND_DECOY_BREAK] = db;
  }
  {
    Buf so = Make(2.5f);
    float ph = 0;
    for (size_t i = 0; i < so.size(); i++) {
      float t = T(i);
      ph += (1500.0f - 400.0f * std::min(1.0f, t * 4.0f)) / SR;
      so[i] = SinC(ph) * expf(-t * 3.0f) * std::min(1.0f, t * 200.0f);
    }
    Echo(so, 0.3f, 0.4f, 4);
    Normalize(so, 0.7f);
    s[SND_SONAR] = so;
    Buf pi = Make(0.4f);
    Mix(pi, Beep(1900, 0.08f, 0.8f, true), 0);
    Mix(pi, Beep(1900, 0.08f, 0.8f, true), 0.16f);
    s[SND_PING] = pi;
  }
  {
    Buf g = Whoosh(0.35f, 500, 1500);
    Mix(g, Click(3000, 0.01f), 0, 0.8f);
    Normalize(g, 0.5f);
    s[SND_GRENADE_THROW] = g;
  }
  s[SND_EXPLOSION] = Explosion();
  {
    Buf l = Make(0.12f);
    Mix(l, Click(2800, 0.01f), 0);
    Mix(l, Click(2000, 0.01f), 0.05f);
    Normalize(l, 0.45f);
    s[SND_LOCK] = l;
    Buf nf = Make(0.35f);
    Mix(nf, Beep(520, 0.12f, 0.5f, true), 0);
    Mix(nf, Beep(390, 0.16f, 0.5f, true), 0.15f);
    s[SND_NOFIT] = nf;
  }
  s[SND_CLICK] = Beep(1300, 0.04f, 0.35f);
  s[SND_HOVER] = Beep(2200, 0.012f, 0.1f);
  s[SND_COUNTDOWN] = Beep(880, 0.14f, 0.5f, true);
  s[SND_GO] = Beep(1320, 0.5f, 0.5f, true);
  s[SND_ROUND_START] = Brass(1.4f, {NoteHz(48), NoteHz(55), NoteHz(60), NoteHz(64)}, 0.25f, 0.5f);
  Normalize(s[SND_ROUND_START], 0.7f);
  {
    Buf h = Make(2.4f);
    for (int k = 0; k < 2; k++) {
      Buf sir = Make(1.0f);
      float ph = 0;
      for (size_t i = 0; i < sir.size(); i++) {
        float t = T(i);
        ph += (500.0f + 500.0f * sinf(Clamp(t, 0.0f, 1.0f) * PI * 0.5f)) / SR;
        sir[i] = (Sq(ph, 0.5f) * 0.3f + SinC(ph) * 0.7f) * sinf(t * PI);
      }
      Mix(h, sir, k * 1.1f, 0.6f);
    }
    s[SND_HUNT_START] = h;
  }
  s[SND_WIN] = Fanfare(true);
  s[SND_LOSE] = Fanfare(false);
  s[SND_CHAT] = Beep(1700, 0.05f, 0.25f);
  {
    Buf e = Make(1.6f);
    for (int k = 0; k < 4; k++) Mix(e, Bell(0.5f, 1600, 9, 1.0f), k * 0.3f, 0.5f);
    s[SND_ENDSPURT] = e;
  }
  s[SND_AMB_OUTDOOR] = Ambience(true);
  s[SND_AMB_INDOOR] = Ambience(false);
  s[SND_MUSIC_MENU] = MenuMusic();
  Buf (*taunts[16])() = {Kuckuck, Ente, Pupser, Klingel, TrompeteFail, Muh, Miau, Hupe,
                         Pfiff, Glockenspiel, Hahaha, Boing, Praesidentenglocke, Applaus, Tusch, KazooHymne};
  for (int k = 0; k < 16; k++) {
    s[SND_TAUNT_FIRST + k] = taunts[k]();
    Normalize(s[SND_TAUNT_FIRST + k], 0.9f);
  }
  for (int i = 0; i < SND_COUNT; i++) g_ids[i] = audio::CreateSound(std::move(s[i]));
}

int Snd(int id) { return (id >= 0 && id < SND_COUNT) ? g_ids[id] : -1; }
