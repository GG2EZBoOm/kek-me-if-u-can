#include "game/Sounds.h"
#include "engine/Audio.h"
#include "engine/Math.h"
#include "game/Taunts.h"
#include "game/RecordedSounds.h"
#include <array>
#include <filesystem>
#include <fstream>

namespace {

const float SR = (float)audio::SAMPLE_RATE;
using Buf = std::vector<float>;
Rng g_rng(2024);
std::vector<int> g_ids[SND_COUNT];
int g_tauntVariant = 0;

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

// --- Taunts: kurze, komponierte Gags mit drei eigenständigen Darbietungen ----------------------

float CharacterPitch() { return 1.0f + (g_tauntVariant - 1) * 0.045f; }

Buf WoodenTap(float f, float len = .13f) {
  Buf b = Make(len);
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    b[i] = (SinC(f * t) + .42f * SinC(f * 1.73f * t) + .19f * SinC(f * 3.12f * t)) * expf(-t * 48);
  }
  Mix(b, NoiseHit(.018f, 3600, 170, 500), 0, .7f);
  Fade(b, .001f, .015f);
  return b;
}

// Weiche, obertonreiche Anregung statt ungefilterter Rechteckwellen. Kleine Atem-
// und Tonhöhenschwankungen machen Tiere und Lacher weniger mechanisch.
Buf ComicVoice(float len, const std::function<float(float)>& pitch,
               const std::function<Vec3(float)>& vowels, float breath = .07f) {
  Buf b = Make(len);
  Biquad f1, f2, f3, air;
  air.BP(2300, .7f);
  float phase = 0, drift = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i), u = t / len;
    if (i % 64 == 0) {
      Vec3 fm = vowels(u);
      f1.BP(fm.x, 3.0f); f2.BP(fm.y, 4.2f); f3.BP(fm.z, 5.0f);
    }
    drift += (Noise() - drift) * .0008f;
    phase += std::max(35.0f, pitch(u) * CharacterPitch() * (1 + drift * .04f)) / SR;
    float excitation = 0;
    for (int h = 1; h <= 14; h++) excitation += SinC(phase * h) * expf(-h * .11f) / h;
    float aspiration = air.Run(Noise());
    float voice = f1.Run(excitation) + .85f * f2.Run(excitation) + .35f * f3.Run(excitation);
    float env = Smoothstep(0.0f, .035f, t) * Smoothstep(0.0f, .07f, len - t);
    b[i] = (voice * 2.8f + aspiration * breath) * env;
  }
  return b;
}

Buf SlideWhistle(float len, float from, float to, float curve = 1) {
  Buf b = Make(len);
  float phase = 0;
  Biquad air; air.BP(3100, 1);
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i), u = t / len;
    phase += (from + (to - from) * powf(u, curve)) * CharacterPitch() * (1 + .004f * SinC(t * 7)) / SR;
    b[i] = (SinC(phase) + .1f * SinC(phase * 2) + air.Run(Noise()) * .075f) *
           Smoothstep(0.0f, .025f, t) * Smoothstep(0.0f, .06f, len - t);
  }
  return b;
}

Buf Kuckuck() {
  Buf b = Make(1.6f);
  for (int k = 0; k < 2; k++) {
    float at = k * (.72f + .018f * g_tauntVariant);
    Mix(b, WoodenTap(760), at, .35f);
    Mix(b, SlideWhistle(.23f, 940, 875), at + .035f, .65f);
    Mix(b, SlideWhistle(.28f, 740, 680), at + .29f, .75f);
    Mix(b, WoodenTap(510), at + .59f, .2f);
  }
  Echo(b, .062f, .13f, 2);
  return b;
}

Buf Ente() {
  Buf b = Make(1.7f);
  const float times[3][4] = {{.01f,.31f,.66f,1.12f},{.01f,.24f,.55f,1.02f},{.01f,.38f,.69f,1.09f}};
  for (int k = 0; k < 4; k++) {
    float len = k == 3 ? .43f : .22f + .02f * k;
    Buf q = ComicVoice(len, [=](float u) { return 540 + 570 * sinf(u * PI) - 180 * u + k * 35; },
        [](float u) { return Vec3(900 + 600 * sinf(u * PI), 2300 - 700 * u, 3700); }, .16f);
    Mix(b, q, times[g_tauntVariant][k]);
    Mix(b, NoiseHit(.07f, 1600, 35, 400), times[g_tauntVariant][k] + .01f, .18f);
  }
  return b;
}

Buf Pupser() {
  Buf b = Make(1.3f);
  Biquad body, rasp; body.LP(680, 1.1f); rasp.BP(850, 1.2f);
  float phase = 0, wobble = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    wobble += (Noise() - wobble) * .004f;
    float f = 90 - 28 * t + 18 * SinC(t * 3.7f) + wobble * 95 + g_tauntVariant * 6;
    phase += f / SR;
    float flap = powf(.5f + .5f * SinC(phase), 3);
    float pulse = .45f + .55f * powf(.5f + .5f * SinC(t * (11 + g_tauntVariant) + .18f * SinC(t * 2)), 2);
    float env = Smoothstep(0.0f, .018f, t) * (1 - Smoothstep(.84f, 1.06f, t));
    b[i] = (body.Run(flap - .3f) * 2 + rasp.Run(Noise()) * .22f) * pulse * env;
  }
  Mix(b, SlideWhistle(.23f, 410, 1400 + 110 * g_tauntVariant, 2), 1.02f, .28f);
  return b;
}

Buf Klingel() {
  Buf b = Make(1.2f);
  for (int ring = 0; ring < 2; ring++) {
    float at = ring * (.47f + .02f * g_tauntVariant);
    Mix(b, WoodenTap(1300, .06f), at, .25f);
    for (int k = 0; k < 5; k++) {
      float f = (k % 2 ? 1670.0f : 1190.0f) * CharacterPitch();
      Mix(b, Bell(.58f, f, 9, .55f), at + .01f + k * .033f, .38f - k * .035f);
    }
  }
  return b;
}

Buf TrompeteFail() {
  Buf b = Make(2.6f);
  for (int k = 0; k < 4; k++) {
    float len = k == 3 ? 1.16f : .43f, f = NoteHz(57 - k * 2);
    Buf n = ComicVoice(len, [=](float u) {
      float fall = k == 3 ? powf(2.0f, -(.38f + g_tauntVariant * .05f) * u * u) : 1;
      return f * fall * (1 + .026f * SinC(u * len * 6) * Smoothstep(.15f, .6f, u));
    }, [=](float u) { return Vec3(600 + 350 * sinf(u * PI), 1250 - (k == 3 ? 450 * u : 0), 2500); }, .035f);
    Mix(b, n, k * .46f);
  }
  Echo(b, .047f, .1f, 2);
  return b;
}

Buf Muh() {
  Buf b = Make(1.8f);
  Mix(b, NoiseHit(.16f, 1200, 12, 100), 0, .28f);
  Buf moo = ComicVoice(1.65f, [](float u) { return 125 + 38 * sinf(u * PI) - 43 * u + 3 * SinC(u * 8); },
      [](float u) { float open = sinf(u * PI); return Vec3(250 + 260 * open, 620 + 300 * open, 2100); }, .1f);
  Mix(b, moo, .08f);
  return b;
}

Buf Miau() {
  Buf b = ComicVoice(1.2f, [](float u) { return 430 + 370 * sinf(powf(u, .65f) * PI) - 120 * u + 13 * SinC(u * 9); },
      [](float u) {
        return u < .35f ? Lerp(Vec3(300,2400,3400), Vec3(850,1400,2900), u/.35f)
                        : Lerp(Vec3(850,1400,2900), Vec3(330,750,2100), (u-.35f)/.65f);
      }, .07f);
  for (size_t i = 0; i < b.size(); i++) b[i] *= .86f + .14f * SinC(T(i) * 27);
  return b;
}

Buf Hupe() {
  Buf b = Make(1.1f);
  const float at[] = {0, .37f, .84f};
  for (int k = 0; k < 3; k++) {
    float len = k == 2 ? .2f : .29f + k * .09f;
    float f = k == 1 ? 260.0f : 390.0f;
    Buf honk = ComicVoice(len, [=](float u) { return f * (1 - .18f * u * u); },
        [](float u) { return Vec3(550 + 240 * sinf(u * PI), 1200, 2800); }, .045f);
    Mix(b, honk, at[k], k == 2 ? .55f : 1);
    Mix(b, WoodenTap(500, .06f), at[k], .12f);
  }
  return b;
}

Buf Pfiff() {
  Buf b = Make(1.4f);
  Mix(b, SlideWhistle(.3f, 1050, 2100, .7f), .01f, .65f);
  Mix(b, SlideWhistle(.23f, 1200, 2300, .7f), .49f, .7f);
  Mix(b, SlideWhistle(.55f, 2300, 1150, .8f), .70f, .7f);
  Echo(b, .085f, .1f, 1);
  return b;
}

Buf Glockenspiel() {
  Buf b = Make(2.4f);
  const int notes[] = {84,88,91,96,93,88,85};
  for (int k = 0; k < 7; k++) {
    float at = k * (.19f + .006f * g_tauntVariant);
    Mix(b, Bell(1.0f, NoteHz(notes[k]) * CharacterPitch(), 5.5f, .32f), at, .55f);
    Mix(b, WoodenTap(1700, .03f), at, .08f);
  }
  Mix(b, SlideWhistle(.47f, 1050, 360, .6f), 1.65f, .2f);
  return b;
}

Buf Hahaha() {
  Buf b = Make(1.9f);
  const float at[] = {.02f,.27f,.51f,.78f,1.06f};
  for (int k = 0; k < 5; k++) {
    float base = 245 + (k % 2 ? 45 : 0) - k * 13;
    Buf laugh = ComicVoice(.21f + .012f * k, [=](float u) { return base + 65 * sinf(u * PI) - 70 * u; },
        [=](float u) { return Vec3(650 + 260 * sinf(u * PI), 1250 + k * 35, 2800); }, .22f);
    Mix(b, NoiseHit(.06f, 4200, 33, 1000), at[k], .28f);
    Mix(b, laugh, at[k] + .018f, 1 - k * .07f);
  }
  Buf snort = ComicVoice(.3f, [](float u) { return 115 + 70 * sinf(u * PI); },
      [](float) { return Vec3(390,1050,2100); }, .3f);
  Mix(b, snort, 1.5f, .55f);
  return b;
}

Buf Boing() {
  Buf b = Make(1.0f);
  float phase = 0, shimmer = 0;
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i);
    float f = (105 + 380 * expf(-t * 4) + 125 * SinC(t * (8 + g_tauntVariant)) * expf(-t * 3)) * CharacterPitch();
    phase += f / SR; shimmer += f * 2.73f / SR;
    b[i] = (sinf(TAU * phase + 1.5f * SinC(shimmer) * expf(-t * 5)) + .2f * SinC(shimmer)) * expf(-t * 4.5f);
  }
  Mix(b, WoodenTap(1100, .05f), 0, .2f);
  return b;
}

Buf Praesidentenglocke() {
  Buf b = Make(2.0f);
  const float times[] = {.02f,.22f,.46f,.72f,1.05f};
  for (int k = 0; k < 5; k++) {
    float f = (k % 2 ? 1030 : 980) * CharacterPitch();
    Mix(b, Bell(.95f, f, 6, .58f), times[k], .55f + .06f * (k % 2));
    Mix(b, WoodenTap(650, .04f), times[k], .16f);
  }
  Mix(b, WoodenTap(240, .22f), 1.58f, .65f);
  return b;
}

Buf HandClap(float tone) {
  Buf b = Make(.12f);
  Biquad f; f.BP(tone, .65f);
  for (size_t i = 0; i < b.size(); i++) {
    float t = T(i), envelope = expf(-t * 85);
    if (t > .009f) envelope += .55f * expf(-(t-.009f)*120);
    if (t > .018f) envelope += .35f * expf(-(t-.018f)*140);
    b[i] = f.Run(Noise()) * envelope;
  }
  Mix(b, Thump(.08f, 430, 220, 65), 0, .15f);
  Echo(b, .027f, .16f, 2);
  return b;
}

Buf Applaus() {
  Buf b = Make(3.0f);
  for (int person = 0; person < 7; person++) {
    float at = person * .031f;
    while (at < 2.25f) {
      Mix(b, HandClap(1000 + person * 240), at, g_rng.Range(.4f,.75f));
      at += .24f + person * .012f + g_rng.Range(-.038f,.038f);
    }
  }
  Mix(b, HandClap(1550), 2.56f, .8f);  // einer klatscht peinlich spät
  Echo(b, .063f, .18f, 2);
  return b;
}

Buf Tusch() {
  Buf b = Make(2.2f);
  Mix(b, Thump(.23f, 170, 65, 16), 0, .6f);
  Mix(b, WoodenTap(870, .1f), .16f, .6f);
  Mix(b, HandClap(1500), .34f, .8f);
  Mix(b, HandClap(1900), .49f, .7f);
  Mix(b, Brass(1.1f, {NoteHz(60),NoteHz(64),NoteHz(67)}, .035f, .5f), .65f, .5f);
  Mix(b, NoiseHit(1.45f, 8500, 4.8f, 3100), .65f, .5f);
  Mix(b, Bell(1.5f, 3400, 5, .35f), .65f, .12f);
  return b;
}

Buf KazooHymne() {
  Buf b = Make(4.2f);
  const int notes[] = {60,64,67,64,65,69,67,62,65,64,62,60};
  const float lengths[] = {.28f,.17f,.33f,.17f,.26f,.23f,.38f,.22f,.23f,.23f,.23f,.55f};
  float at = .02f;
  for (int k = 0; k < 12; k++) {
    float f = NoteHz(notes[k]), len = lengths[k];
    Buf note = ComicVoice(len, [=](float u) { return f * (1 + .014f * SinC(u * len * 6.4f)); },
        [](float u) { return Vec3(620 + 90 * sinf(u * PI), 1500, 2800); }, .16f);
    Mix(b, note, at, .7f);
    at += len + .025f;
  }
  for (int k = 0; k < 6; k++) {
    Mix(b, Thump(.3f, NoteHz(k % 2 ? 43 : 36), NoteHz(k % 2 ? 43 : 36), 10), .03f + k * .56f, .2f);
    Mix(b, WoodenTap(1050, .055f), .31f + k * .56f, .1f);
  }
  Echo(b, .051f, .11f, 2);
  return b;
}

using TauntMaker = Buf (*)();
const TauntMaker kTauntMakers[NUM_TAUNTS] = {Kuckuck, Ente, Pupser, Klingel, TrompeteFail, Muh, Miau, Hupe,
    Pfiff, Glockenspiel, Hahaha, Boing, Praesidentenglocke, Applaus, Tusch, KazooHymne};

void MasterTaunt(Buf& b, float duration) {
  b.resize((size_t)llround(duration * SR), 0);
  Biquad hp, lp; hp.HP(55); lp.LP(7200, .65f);
  float peak = 0;
  for (float& x : b) { x = lp.Run(hp.Run(x)); peak = std::max(peak, fabsf(x)); }
  if (peak < 1e-7f) return;
  // Preserve the natural attack and breath of recordings. The former tanh drive
  // flattened every voice into the same buzzy timbre and added sharp harmonics.
  double mean = 0;
  for (float x : b) mean += x;
  mean /= b.size();
  double power = 0; peak = 0;
  for (float& x : b) { x -= (float)mean; power += x * x; peak = std::max(peak, fabsf(x)); }
  float rms = (float)sqrt(power / b.size());
  float gain = std::min(.80f / std::max(peak,.001f), .17f / std::max(rms, .001f));
  for (float& x : b) x *= gain;
  Fade(b, .002f, .045f);
}

// A small sampler for the CC0 performances. The preparation bank is low-passed
// at 7 kHz; playback rates stay below 2x, safely below the 22.05 kHz Nyquist limit.
// Every phrase is assembled once on load, so this adds no per-frame audio work.
Buf Performance(const RecordedSoundBank& bank, const char* key, float len, float rate=1, float endRate=0) {
  const Buf& src = bank.at(key);
  Buf b = Make(len);
  double cursor = 0;
  if (endRate <= 0) endRate = rate;
  for (size_t i=0; i<b.size(); i++) {
    size_t at=(size_t)cursor;
    if (at+1>=src.size()) break;
    float frac=(float)(cursor-at);
    b[i]=src[at]*(1-frac)+src[at+1]*frac;
    float u=T(i)/len;
    cursor += rate+(endRate-rate)*u*u;
  }
  Fade(b,.004f,.065f);
  return b;
}

bool ComedyBankComplete(const RecordedSoundBank& bank) {
  for (const char* key : {"cat","bell","trumpet","trombone","mute","flute","glock","xylo","snare","snare2","cymbal"})
    if (!bank.count(key)) return false;
  for (const char* group : {"toy","cow","laugh","fart","spring"}) {
    int count = std::string(group)=="toy" ? 4 : std::string(group)=="cow" ? 2 : 3;
    for (int i=0;i<count;i++) if (!bank.count(StrFormat("%s_%d",group,i))) return false;
  }
  return true;
}

Buf ComedyTaunt(const RecordedSoundBank& bank, int kind, int take) {
  Buf b=Make(kTaunts[kind].duration);
  auto sample=[&](const char* key,float at,float len,float gain=1,float rate=1,float endRate=0) {
    Mix(b,Performance(bank,key,len,rate,endRate),at,gain);
  };
  auto note=[&](const char* key,int midi,int root,float at,float len,float gain=1,float bend=1) {
    float rate=powf(2.0f,(midi-root)/12.0f);
    sample(key,at,len,gain,rate,rate*bend);
  };
  switch(kind) {
    case 0: { // A wooden cuckoo-clock duet: answer, hesitation, or an impatient third call.
      const float timing[3][4]={{.02f,.30f,.77f,1.06f},{.02f,.33f,.91f,1.22f},{.01f,.24f,.65f,.95f}};
      for(int i=0;i<4;i++) {
        note("flute",i%2 ? 77 : 81,72,timing[take][i],i%2 ? .31f : .23f,.9f);
        Mix(b,WoodenTap(640,.06f),timing[take][i],.05f);
      }
      if(take==2) note("flute",76,72,1.29f,.23f,.65f);
      break;
    }
    case 1: { // Different real toy squeezes, with a delayed punchline.
      const int seq[3][3]={{0,1,3},{2,1,0},{1,3,2}};
      const float at[3][3]={{.01f,.56f,1.15f},{.01f,.72f,1.08f},{.01f,.35f,.93f}};
      for(int i=0;i<3;i++) sample(StrFormat("toy_%d",seq[take][i]).c_str(),at[take][i],i==2 ? .48f : .53f,1, i==2 ? .88f : 1);
      break;
    }
    case 2:
      sample(StrFormat("fart_%d",take).c_str(),.01f,1.06f,1);
      if(take==1) sample("fart_1",.69f,.30f,.7f,1.3f);
      Mix(b,SlideWhistle(.18f,530,980,.6f),1.07f,.08f);
      break;
    case 3: { // Ding-ding, hesitant ding, and an impatient little bell roll.
      const float at[3][4]={{.01f,.13f,.53f,.64f},{.01f,.17f,.74f,.86f},{.01f,.10f,.19f,.61f}};
      for(int i=0;i<4;i++) note("glock",i%2 ? 84 : 79,84,at[take][i],.52f,.8f);
      Mix(b,WoodenTap(950,.08f),.01f,.06f);
      break;
    }
    case 4: { // Real brass with a sinking last note; three deliberately botched endings.
      const int melodies[3][4]={{57,55,53,51},{60,57,55,52},{55,54,52,48}};
      for(int i=0;i<4;i++) note(take==1 ? "mute" : "trombone",melodies[take][i],60,
          i*.44f+.01f,i==3 ? 1.2f : .38f,.85f,i==3 ? .73f : .99f);
      break;
    }
    case 5:
      if(take==0) sample("cow_1",.02f,1.72f,1,.93f);
      else if(take==1) {sample("cow_0",.02f,.79f);sample("cow_0",.92f,.79f,.95f,.86f);}
      else {sample("cow_0",.02f,.53f,.7f,1.18f);sample("cow_1",.53f,1.20f,1,1.05f);}
      break;
    case 6:
      if(take==2) {sample("cat",.01f,.52f,.8f,1.28f);sample("cat",.57f,.58f,1,1.16f);}
      else sample("cat",.01f,1.14f,1,take==0 ? .96f : 1.10f,take==0 ? .88f : 1.02f);
      break;
    case 7: {
      const int melody[3][3]={{67,60,64},{60,67,55},{64,60,57}};
      const float timing[3][3]={{.01f,.30f,.84f},{.01f,.35f,.73f},{.01f,.22f,.62f}};
      for(int i=0;i<3;i++) note("mute",melody[take][i],60,timing[take][i],i==2 ? .23f : .29f,.9f,.87f);
      break;
    }
    case 8:
      if(take==0) Mix(b,SlideWhistle(.78f,1800,430,.7f),.01f,.65f);
      else if(take==1) {Mix(b,SlideWhistle(.32f,640,1550,.6f),.01f,.60f);Mix(b,SlideWhistle(.48f,1550,410,1.4f),.39f,.65f);}
      else {Mix(b,SlideWhistle(.40f,700,1600),.01f,.62f);Mix(b,SlideWhistle(.52f,1600,330,.7f),.47f,.65f);}
      note("xylo",48,72,take==2 ? 1.05f : .92f,.4f,.8f);
      break;
    case 9: {
      const int melodies[3][7]={{79,83,86,91,88,83,80},{84,88,91,88,86,83,81},{91,88,86,83,81,79,78}};
      for(int i=0;i<7;i++) note("glock",melodies[take][i],84,i*(take==2 ? .17f : .20f),.96f,.8f);
      note("xylo",55,72,1.65f,.6f,.7f);
      break;
    }
    case 10:
      sample(StrFormat("laugh_%d",take).c_str(),.02f,1.81f,1,take==2 ? .91f : 1);
      break;
    case 11:
      sample(StrFormat("spring_%d",take).c_str(),.01f,.94f,1,1.18f);
      Mix(b,Boing(),0,.16f);
      break;
    case 12: {
      const float timing[3][3]={{.01f,.32f,.70f},{.01f,.21f,1.04f},{.01f,.54f,.74f}};
      for(int i=0;i<3;i++) sample("bell",timing[take][i],.92f,.8f,1.0f+take*.07f);
      Mix(b,WoodenTap(220,.20f),1.66f,.35f);
      break;
    }
    case 13: { // Small, deliberately unenthusiastic audience; one person is always late.
      int people=take==0 ? 3 : take==1 ? 5 : 2;
      for(int person=0;person<people;person++)
        for(int hit=0;hit<(take==1 ? 6 : 4);hit++)
          Mix(b,HandClap(850+person*310),.02f+person*.048f+hit*(take==1 ? .27f : .40f)+g_rng.Range(0,.03f),.75f);
      Mix(b,HandClap(1350),take==2 ? 2.70f : 2.54f,.9f);
      break;
    }
    case 14: {
      const float at[3][3]={{.01f,.23f,.50f},{.01f,.15f,.57f},{.01f,.31f,.69f}};
      sample("snare",at[take][0],.24f,.8f); sample("snare2",at[take][1],.28f,1);
      sample("snare",at[take][2],.25f,.7f); sample("cymbal",at[take][2],1.45f,.7f);
      Mix(b,Thump(.18f,130,65,18),at[take][2],.15f);
      break;
    }
    case 15: { // Three original miniature brass-band gags, with an off-key last note.
      const int melodies[3][10]={{60,64,67,64,65,69,67,62,64,59},{67,64,60,64,67,69,65,62,59,60},{60,60,67,65,64,62,64,67,65,58}};
      float at=.01f;
      for(int i=0;i<10;i++) {
        float len=i==9 ? .65f : (i%3==0 ? .35f : .24f);
        note(i==9 ? "trombone" : "mute",melodies[take][i],60,at,len,.85f,i==9 ? .84f : 1);
        if(i%2==0) note("trombone",i%4 ? 55 : 48,60,at,.23f,.45f);
        if(i%2==1) sample(i%4 ? "snare2" : "snare",at,.13f,.12f);
        at+=len+.035f;
      }
      break;
    }
  }
  // A tiny room tail gives the acoustic performances body without washing out direction.
  if(kind!=2 && kind!=13) Echo(b,.043f,.065f,2);
  MasterTaunt(b,kTaunts[kind].duration);
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

static std::vector<Buf> MakeBaseSounds() {
  g_rng = Rng(2024);
  g_tauntVariant = 0;
  std::vector<Buf> s(SND_COUNT);
  s[SND_SMG] = Gunshot(false);
  s[SND_SHOTGUN] = Gunshot(true);
  {
    Buf pistol=Make(.40f);
    Mix(pistol,NoiseHit(.12f,4800,48,500),0,.65f);
    Mix(pistol,Thump(.15f,210,85,32),0,.80f);
    Mix(pistol,Click(2400,.012f),.045f,.22f);
    Normalize(pistol,.82f); Fade(pistol,.001f,.045f);
    s[SND_PISTOL]=pistol;
    Biquad low; low.LP(1500);
    for(float& sample:pistol) sample=low.Run(sample)*.50f;
    s[SND_PISTOL_SUPPRESSED]=pistol;
    Buf reload=Make(1.45f);
    Mix(reload,Click(1800,.04f),.09f,.6f);
    Mix(reload,Click(2700,.04f),.87f,.8f);
    Mix(reload,Click(3300,.025f),1.26f,.7f);
    Fade(reload,.002f,.03f); s[SND_RELOAD_PISTOL]=reload;
  }
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
    s[SND_SHELL] = e;
  }
  for (int w = 0; w < 2; w++) {
    Buf quiet = s[w ? SND_SHOTGUN : SND_SMG];
    Biquad low; low.LP(1400);
    for (float& sample : quiet) sample = low.Run(sample) * .55f;
    s[w ? SND_SHOTGUN_SUPPRESSED : SND_SMG_SUPPRESSED] = std::move(quiet);
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
  for (int k = 0; k < NUM_TAUNTS; k++) {
    g_rng = Rng(202410u + k * 397u);
    s[SND_TAUNT_FIRST + k] = kTauntMakers[k]();
    MasterTaunt(s[SND_TAUNT_FIRST + k], kTaunts[k].duration);
  }
  return s;
}

using SoundVariations = std::vector<std::vector<Buf>>;

static SoundVariations MakeSoundVariations() {
  auto base = MakeBaseSounds();
  SoundVariations sounds(SND_COUNT);
  for (int i = 0; i < SND_COUNT; i++) sounds[i].push_back(std::move(base[i]));
  for (int v = 1; v < TAUNT_VARIANTS; v++) {
    g_tauntVariant = v;
    for (int k = 0; k < NUM_TAUNTS; k++) {
      g_rng = Rng(202410u + k * 397u + v * 7919u);
      Buf b = kTauntMakers[k]();
      MasterTaunt(b, kTaunts[k].duration);
      sounds[SND_TAUNT_FIRST + k].push_back(std::move(b));
    }
  }
  g_tauntVariant = 0;
  RecordedSoundBank comedy;
  if (LoadComedySounds(comedy) && ComedyBankComplete(comedy)) {
    for (int k=0;k<NUM_TAUNTS;k++) for (int v=0;v<TAUNT_VARIANTS;v++) {
      g_rng=Rng(10813u+k*397u+v*7919u);
      g_tauntVariant=v;
      sounds[SND_TAUNT_FIRST+k][v]=ComedyTaunt(comedy,k,v);
    }
    g_tauntVariant=0;
    LOG("Audio: 26 CC0-Aufnahmen für 48 Comedy-Taunts geladen.");
  } else LOG("Audio: Comedy-Bank fehlt oder ist beschädigt; Synthese-Ersatz wird verwendet.");
  RecordedSoundBank recordings;
  bool recorded = LoadRecordedSounds(recordings);
  for (const char* key : {"smg_shot", "smg_burst", "arcade_punch", "empty", "switch_0", "switch_1"})
    recorded &= recordings.count(key) != 0;
  for (const char* group : {"load", "unload", "wall", "wood", "metal", "glass"})
    for (int v = 0; v < 3; v++) recorded &= recordings.count(StrFormat("%s_%d", group, v)) != 0;
  if (!recorded) {
    LOG("Audio: Aufnahme-Bank fehlt oder ist beschädigt; Synthese-Ersatz wird verwendet.");
    return sounds;
  }
  for (int id : {SND_SMG, SND_RELOAD_SMG, SND_RELOAD_SHOTGUN, SND_SWITCH, SND_SHELL, SND_SMG_SUPPRESSED,
                 SND_PISTOL, SND_PISTOL_SUPPRESSED, SND_RELOAD_PISTOL,
                 SND_IMPACT_WALL, SND_IMPACT_WOOD, SND_IMPACT_METAL, SND_IMPACT_GLASS}) sounds[id].clear();
  sounds[SND_EMPTY][0] = recordings.at("empty");
  for (int v = 0; v < 3; v++) {
    Buf shot = Make(.72f);
    Mix(shot, recordings.at("smg_shot"), 0, .9f);
    Mix(shot, recordings.at("smg_burst"), .001f * v, .13f + v * .045f);
    Mix(shot, recordings.at("arcade_punch"), 0, .07f);
    Normalize(shot, .86f); Fade(shot, .001f, .06f);
    Buf quiet = shot; Biquad low; low.LP(1400);
    for (float& sample : quiet) sample = low.Run(sample) * .55f;
    sounds[SND_SMG_SUPPRESSED].push_back(std::move(quiet));
    sounds[SND_SMG].push_back(std::move(shot));
    const Buf& unload = recordings.at(StrFormat("unload_%d", v));
    const Buf& load = recordings.at(StrFormat("load_%d", v));
    const Buf& latch = recordings.at(StrFormat("switch_%d", v % 2));
    // Short single-shot design, layered from the supplied recordings and a dry low end.
    Buf pistol=Make(.40f);
    Mix(pistol,recordings.at("smg_shot"),0,.72f);
    for(size_t i=0;i<pistol.size();i++) pistol[i]*=expf(-T(i)*(7.0f+v));
    Mix(pistol,Thump(.14f,220+v*8.0f,90,34),0,.36f);
    Mix(pistol,latch,.045f,.10f);
    Normalize(pistol,.82f); Fade(pistol,.001f,.045f);
    Buf muted=pistol; Biquad pistolLow; pistolLow.LP(1500);
    for(float& sample:muted) sample=pistolLow.Run(sample)*.50f;
    sounds[SND_PISTOL].push_back(std::move(pistol));
    sounds[SND_PISTOL_SUPPRESSED].push_back(std::move(muted));
    Buf pistolReload=Make(1.45f);
    Mix(pistolReload,unload,.09f,.7f); Mix(pistolReload,load,.87f,.8f); Mix(pistolReload,latch,1.26f,.7f);
    Fade(pistolReload,.002f,.03f); sounds[SND_RELOAD_PISTOL].push_back(std::move(pistolReload));
    Buf shell = Make(.22f); Mix(shell, load, 0, .7f); Mix(shell, latch, .10f, .25f);
    Fade(shell,.002f,.025f); sounds[SND_SHELL].push_back(std::move(shell));
    Buf reload = Make(1.9f);
    Mix(reload, unload, .09f, .85f);
    Mix(reload, load, 1.10f);
    Mix(reload, latch, 1.65f);
    Fade(reload, .002f, .035f);
    sounds[SND_RELOAD_SMG].push_back(std::move(reload));
    Buf shotgun = Make(2.4f);
    for (int k = 0; k < 3; k++) Mix(shotgun, load, .24f + k * .43f, .65f);
    Mix(shotgun, unload, 1.75f, .8f);
    Mix(shotgun, latch, 2.21f);
    Fade(shotgun, .002f, .035f);
    sounds[SND_RELOAD_SHOTGUN].push_back(std::move(shotgun));
    Buf change = Make(.24f);
    Mix(change, latch, 0, .8f);
    Mix(change, load, .07f, .3f);
    Fade(change, .001f, .03f);
    sounds[SND_SWITCH].push_back(std::move(change));
    const char* materials[] = {"wall", "wood", "metal", "glass"};
    for (int k = 0; k < 4; k++) sounds[SND_IMPACT_WALL + k].push_back(recordings.at(StrFormat("%s_%d", materials[k], v)));
  }
  LOG("Audio: 24 aufbereitete Aufnahmen und %d Taunt-Varianten bereit.", NUM_TAUNTS * TAUNT_VARIANTS);
  return sounds;
}

void BuildSounds() {
  if (!g_ids[0].empty()) return;
  auto sounds = MakeSoundVariations();
  for (int id = 0; id < SND_COUNT; id++)
    for (Buf& b : sounds[id]) g_ids[id].push_back(audio::CreateSound(std::move(b)));
}

int Snd(int id, u32 variant) {
  return id >= 0 && id < SND_COUNT && !g_ids[id].empty() ? g_ids[id][variant % g_ids[id].size()] : -1;
}

static bool WritePreviewWav(const std::filesystem::path& path, const Buf& samples) {
  std::ofstream out(path, std::ios::binary);
  auto word = [&](u32 value, int bytes) { for (int k = 0; k < bytes; k++) out.put((char)(value >> (8*k))); };
  out.write("RIFF", 4); word(36 + (u32)samples.size()*2, 4); out.write("WAVEfmt ", 8);
  word(16,4); word(1,2); word(1,2); word(audio::SAMPLE_RATE,4); word(audio::SAMPLE_RATE*2,4);
  word(2,2); word(16,2); out.write("data",4); word((u32)samples.size()*2,4);
  for (float s : samples) word((u16)(i16)lroundf(Clamp(s,-1,1)*32767),2);
  return out.good();
}

int RunSoundTests(const std::string& previewDir) {
  int fails = 0;
  auto check = [&](bool ok, const std::string& msg) { LOG("[%s]   %s", ok ? "OK" : "FAIL", msg.c_str()); if (!ok) fails++; };
  check(audio::TestTransientStart(), "Kurze Klanganfänge bleiben bis zum ersten Mixer-Update erhalten");
  RecordedSoundBank recordings;
  check(LoadRecordedSounds(recordings) && recordings.size() == 24, "Alle 24 eingebetteten Aufnahmen lesbar");
  RecordedSoundBank comedy;
  check(LoadComedySounds(comedy) && comedy.size()==26 && ComedyBankComplete(comedy),
        "Alle 26 CC0-Comedy- und Instrumentenaufnahmen eingebettet und vollständig");
  auto missing=comedy; missing.erase("cat");
  check(!ComedyBankComplete(missing),"Unvollständige Comedy-Bank aktiviert den sicheren Synthese-Ersatz");
  std::vector<u8> packet = {'K','S','B','1',0x44,0xac,0,0,1,0,0,0,1,0,0,0,2,0,0,0,'a',0,0x80,0xff,0x7f};
  RecordedSoundBank decoded;
  check(DecodeRecordedSounds(packet.data(),packet.size(),decoded) && decoded.at("a")[0] == -1 && decoded.at("a")[1] > .999f,
        "PCM-Bank dekodiert vorzeichenbehaftete Samples");
  bool rejects = true;
  for (size_t n = 0; n < packet.size(); n++) rejects &= !DecodeRecordedSounds(packet.data(),n,decoded) && decoded.empty();
  auto huge = packet; huge[16] = 0xff; huge[17] = 0xff; huge[18] = 0xff; huge[19] = 0xff;
  rejects &= !DecodeRecordedSounds(huge.data(),huge.size(),decoded) && decoded.empty();
  auto wrongRate = packet; wrongRate[4] = 0;
  rejects &= !DecodeRecordedSounds(wrongRate.data(),wrongRate.size(),decoded);
  auto trailing = packet; trailing.push_back(0);
  rejects &= !DecodeRecordedSounds(trailing.data(),trailing.size(),decoded);
  check(rejects, "Abgeschnittene, übergroße und falsche Aufnahme-Banken werden verworfen");
  auto sounds = MakeSoundVariations();
  bool valid = true;
  for (const auto& variants : sounds) for (const Buf& b : variants) {
    valid &= !b.empty();
    for (float s : b) valid &= std::isfinite(s);
  }
  check(valid, "Alle Klänge enthalten endliche, nichtleere Audiodaten");
  std::filesystem::path dir(Utf8ToWide(previewDir));
  if (!previewDir.empty()) std::filesystem::create_directories(dir);
  Buf montage;
  for (int k = 0; k < NUM_TAUNTS; k++) {
    const auto& versions = sounds[SND_TAUNT_FIRST+k];
    bool good = versions.size() == TAUNT_VARIANTS;
    for (size_t v = 0; v < versions.size(); v++) {
      const Buf& b = versions[v];
      good &= b.size() == (size_t)llround(kTaunts[k].duration * SR);
      double power = 0, dc = 0; float peak = 0;
      for (float s : b) { power += s*s; dc += s; peak = std::max(peak, fabsf(s)); }
      float rms = (float)sqrt(power / b.size());
      good &= rms > .04f && rms < .24f && peak <= .83f && fabs(dc / b.size()) < .002f;
      good &= fabsf(b.front()) < .0001f && fabsf(b.back()) < .0001f;
      if (v) good &= versions[v] != versions[v-1];
      if (!previewDir.empty()) good &= WritePreviewWav(dir / StrFormat("taunt_%02d_v%d.wav",k+1,(int)v+1), b);
    }
    check(good, std::string(kTaunts[k].name) + ": drei Varianten, korrekte Dauer, Pegel und weiche Ränder");
    montage.insert(montage.end(), versions[0].begin(), versions[0].end());
    montage.resize(montage.size() + (size_t)(.35f * SR), 0);
  }
  bool reloads = true;
  for (int id : {SND_RELOAD_SMG,SND_RELOAD_SHOTGUN,SND_RELOAD_PISTOL}) for (const Buf& b : sounds[id])
    reloads &= fabsf((float)b.size()/SR - (id == SND_RELOAD_PISTOL ? 1.45f : id == SND_RELOAD_SMG ? 1.9f : 2.4f)) < .001f;
  check(reloads, "Aufgenommene Nachladegeräusche passen zu den Waffen-Timern");
  bool pistol=sounds[SND_PISTOL].size()==3 && sounds[SND_PISTOL_SUPPRESSED].size()==3;
  for(int v=0;v<3 && pistol;v++) {
    double loud=0,quiet=0;
    for(float s:sounds[SND_PISTOL][v]) loud+=s*s;
    for(float s:sounds[SND_PISTOL_SUPPRESSED][v]) quiet+=s*s;
    pistol &= loud>0 && quiet>0 && quiet<loud*.5 && sounds[SND_PISTOL][v]!=sounds[SND_SMG][v];
  }
  check(pistol,"Pistole besitzt drei eigene Schussvarianten mit leiseren Schalldämpfer-Versionen");
  if(!previewDir.empty()) {
    check(WritePreviewWav(dir/"pistol.wav",sounds[SND_PISTOL][0]) &&
          WritePreviewWav(dir/"pistol_suppressed.wav",sounds[SND_PISTOL_SUPPRESSED][0]) &&
          WritePreviewWav(dir/"outdoor.wav",sounds[SND_AMB_OUTDOOR][0]),"Pistolen- und Außenatmosphäre-Hörprobe exportiert");
  }
  if (!previewDir.empty()) check(WritePreviewWav(dir / "taunts_preview.wav",montage), "Taunt-Hörprobe exportiert");
  LOG("Soundtest beendet: %d Fehler", fails);
  return fails;
}
