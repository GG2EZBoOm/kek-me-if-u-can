#include "engine/Video.h"
#include "engine/Audio.h"
#include "engine/GL.h"
#include "engine/Shader.h"
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>

namespace {

template <class T>
void SafeRelease(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

// Bildschirmfüllendes Rechteck ohne Vertexdaten; NV12 (begrenzter Bereich, BT.709) -> RGB
const char* kVS = R"(
uniform vec4 uRect;
out vec2 vUV;
void main() {
  vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
  vUV = vec2(c.x, 1.0 - c.y);
  gl_Position = vec4(mix(uRect.xy, uRect.zw, c), 0.0, 1.0);
}
)";
const char* kFS = R"(
uniform sampler2D uY;
uniform sampler2D uUV;
uniform float uAlpha;
in vec2 vUV;
out vec4 oColor;
void main() {
  float y = (texture(uY, vUV).r - 16.0 / 255.0) * (255.0 / 219.0);
  vec2 c = (texture(uUV, vUV).rg - 0.5) * (255.0 / 224.0);
  vec3 rgb = vec3(y + 1.5748 * c.y, y - 0.1873 * c.x - 0.4681 * c.y, y + 1.8556 * c.x);
  oColor = vec4(clamp(rgb, 0.0, 1.0) * uAlpha, 1.0);
}
)";

// Quelle aus dem Speicher (z. B. Ressource in der EXE)
IMFSourceReader* CreateReader(const void* data, size_t size) {
  IStream* stream = SHCreateMemStream((const BYTE*)data, (UINT)size);
  if (!stream) return nullptr;
  IMFByteStream* bs = nullptr;
  IMFSourceReader* reader = nullptr;
  if (SUCCEEDED(MFCreateMFByteStreamOnStream(stream, &bs))) {
    IMFAttributes* ba = nullptr;
    if (SUCCEEDED(bs->QueryInterface(IID_PPV_ARGS(&ba)))) {
      ba->SetString(MF_BYTESTREAM_CONTENT_TYPE, L"video/mp4");
      ba->Release();
    }
    IMFAttributes* attr = nullptr;
    MFCreateAttributes(&attr, 1);
    attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(MFCreateSourceReaderFromByteStream(bs, attr, &reader))) reader = nullptr;
    SafeRelease(attr);
  }
  SafeRelease(bs);
  stream->Release();
  return reader;
}

}  // namespace

VideoPlayer::~VideoPlayer() { Close(); }

bool VideoPlayer::Open(const void* data, size_t size) {
  Close();
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);  // schon initialisiert ist auch in Ordnung
  if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
    LOG("Video: Media Foundation nicht verfügbar");
    return false;
  }
  mfStarted_ = true;
  if (!OpenVideo(data, size)) {
    LOG("Video: Bildspur nicht lesbar");
    Close();
    return false;
  }
  if (!DecodeAudio(data, size)) LOG("Video: ohne Ton");
  if (!InitGL()) {
    Close();
    return false;
  }
  stop_ = false;
  ended_ = false;
  thread_ = std::thread([this]() { DecodeLoop(); });
  return true;
}

bool VideoPlayer::OpenVideo(const void* data, size_t size) {
  IMFSourceReader* r = CreateReader(data, size);
  if (!r) return false;
  r->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
  r->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
  IMFMediaType* t = nullptr;
  MFCreateMediaType(&t);
  t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  t->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  HRESULT hr = r->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, t);
  SafeRelease(t);
  IMFMediaType* cur = nullptr;
  if (FAILED(hr) || FAILED(r->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur))) {
    r->Release();
    return false;
  }
  UINT32 fw = 0, fh = 0;
  MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &fw, &fh);
  stride_ = (int)MFGetAttributeUINT32(cur, MF_MT_DEFAULT_STRIDE, fw);
  frameH_ = (int)fh;
  w_ = (int)fw;
  h_ = (int)fh;
  MFVideoArea area = {};
  if (SUCCEEDED(cur->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(area), nullptr))) {
    w_ = (int)area.Area.cx;
    h_ = (int)area.Area.cy;
  }
  cur->Release();
  PROPVARIANT var;
  PropVariantInit(&var);
  if (SUCCEEDED(r->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)))
    duration_ = (double)var.uhVal.QuadPart / 1e7;
  PropVariantClear(&var);
  reader_ = r;
  w_ &= ~1;
  h_ &= ~1;
  LOG("Video: %dx%d (Puffer %dx%d, Stride %d), %.1f s", w_, h_, (int)fw, frameH_, stride_, duration_);
  return w_ > 0 && h_ > 0 && stride_ >= w_;
}

bool VideoPlayer::DecodeAudio(const void* data, size_t size) {
  IMFSourceReader* r = CreateReader(data, size);
  if (!r) return false;
  r->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
  if (FAILED(r->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE))) {
    r->Release();
    return false;
  }
  IMFMediaType* t = nullptr;
  MFCreateMediaType(&t);
  t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  t->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
  HRESULT hr = r->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, t);
  SafeRelease(t);
  IMFMediaType* cur = nullptr;
  if (FAILED(hr) || FAILED(r->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &cur))) {
    r->Release();
    return false;
  }
  UINT32 ch = MFGetAttributeUINT32(cur, MF_MT_AUDIO_NUM_CHANNELS, 2);
  soundRate_ = (float)MFGetAttributeUINT32(cur, MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
  cur->Release();
  std::vector<float> pcm;
  for (;;) {
    DWORD flags = 0;
    LONGLONG ts = 0;
    IMFSample* s = nullptr;
    if (FAILED(r->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &ts, &s))) break;
    if (s) {
      IMFMediaBuffer* buf = nullptr;
      if (SUCCEEDED(s->ConvertToContiguousBuffer(&buf))) {
        BYTE* p = nullptr;
        DWORD len = 0;
        if (SUCCEEDED(buf->Lock(&p, nullptr, &len))) {
          const float* f = (const float*)p;
          size_t n = len / sizeof(float);
          if (ch == 2) {
            pcm.insert(pcm.end(), f, f + n);
          } else {
            // Mono oder Mehrkanal: auf Stereo abbilden (erste zwei Kanäle bzw. verdoppeln)
            for (size_t i = 0; i + ch <= n; i += ch) {
              pcm.push_back(f[i]);
              pcm.push_back(ch > 1 ? f[i + 1] : f[i]);
            }
          }
          buf->Unlock();
        }
        buf->Release();
      }
      s->Release();
    }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
  }
  r->Release();
  if (pcm.empty()) return false;
  LOG("Video-Ton: %.1f s, %d Hz", pcm.size() / 2 / soundRate_, (int)soundRate_);
  sound_ = audio::CreateSoundStereo(std::move(pcm));
  return true;
}

bool VideoPlayer::InitGL() {
  Shader* sh = new Shader();
  if (!sh->Build("video", kVS, kFS)) {
    delete sh;
    return false;
  }
  shader_ = sh;
  glGenVertexArrays(1, &vao_);
  auto makeTex = [](unsigned& tex, GLint ifmt, GLenum fmt, int w, int h) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };
  makeTex(texY_, GL_R8, GL_RED, w_, h_);
  makeTex(texUV_, GL_RG8, GL_RG, w_ / 2, h_ / 2);
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

void VideoPlayer::DecodeLoop() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IMFSourceReader* r = (IMFSourceReader*)reader_;
  const size_t ySize = (size_t)w_ * h_, uvSize = (size_t)w_ * (h_ / 2);
  while (!stop_) {
    DWORD flags = 0;
    LONGLONG ts = 0;
    IMFSample* s = nullptr;
    if (FAILED(r->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &s))) break;
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
      IMFMediaType* cur = nullptr;
      if (SUCCEEDED(r->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur))) {
        UINT32 fw = 0, fh = 0;
        MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &fw, &fh);
        stride_ = (int)MFGetAttributeUINT32(cur, MF_MT_DEFAULT_STRIDE, fw);
        frameH_ = (int)fh;
        cur->Release();
      }
    }
    if (s) {
      Frame f;
      f.t = (double)ts / 1e7;
      IMFMediaBuffer* buf = nullptr;
      if (SUCCEEDED(s->ConvertToContiguousBuffer(&buf))) {
        BYTE* p = nullptr;
        LONG pitch = stride_;
        IMF2DBuffer* b2 = nullptr;
        bool locked2d = SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(&b2))) && SUCCEEDED(b2->Lock2D(&p, &pitch));
        DWORD len = 0;
        if (!locked2d && FAILED(buf->Lock(&p, nullptr, &len))) p = nullptr;
        if (p && pitch >= w_) {
          f.y.resize(ySize);
          f.uv.resize(uvSize);
          for (int row = 0; row < h_; row++) memcpy(&f.y[(size_t)row * w_], p + (size_t)row * pitch, w_);
          const BYTE* uvp = p + (size_t)pitch * frameH_;
          for (int row = 0; row < h_ / 2; row++) memcpy(&f.uv[(size_t)row * w_], uvp + (size_t)row * pitch, w_);
        }
        if (locked2d) b2->Unlock2D();
        else if (p) buf->Unlock();
        SafeRelease(b2);
        buf->Release();
      }
      s->Release();
      if (!f.y.empty()) {
        std::unique_lock<std::mutex> l(mu_);
        cv_.wait(l, [&]() { return stop_ || queue_.size() < 4; });
        if (stop_) break;
        queue_.push_back(std::move(f));
        cv_.notify_all();
      }
    }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
  }
  std::lock_guard<std::mutex> l(mu_);
  ended_ = true;
  cv_.notify_all();
}

void VideoPlayer::Upload(const Frame& f) {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBindTexture(GL_TEXTURE_2D, texY_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_, h_, GL_RED, GL_UNSIGNED_BYTE, f.y.data());
  glBindTexture(GL_TEXTURE_2D, texUV_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_ / 2, h_ / 2, GL_RG, GL_UNSIGNED_BYTE, f.uv.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  hasFrame_ = true;
}

void VideoPlayer::Play(double now, float volume) {
  start_ = now;
  started_ = true;
  if (sound_ >= 0) {
    audio::PlayParams pp;
    pp.volume = volume;
    pp.pitch = soundRate_ / audio::SAMPLE_RATE;  // z. B. 48 kHz im 44,1-kHz-Mixer
    pp.group = 2;
    voice_ = audio::Play(sound_, pp);
  }
}

void VideoPlayer::SetVolume(float volume) {
  if (voice_) audio::SetVoiceVolume(voice_, volume);
}

bool VideoPlayer::Update(double now) {
  double t = Elapsed(now);
  Frame latest;
  bool got = false, done = false;
  {
    std::lock_guard<std::mutex> l(mu_);
    // Alle fälligen Bilder abholen, nur das neueste zeigen (fällt der Decoder zurück, werden Bilder übersprungen)
    while (!queue_.empty() && queue_.front().t <= t) {
      if (got) dropped_++;
      latest = std::move(queue_.front());
      queue_.pop_front();
      got = true;
    }
    if (got) cv_.notify_all();
    done = ended_ && queue_.empty();
  }
  if (got) {
    Upload(latest);
    shown_++;
  }
  return !(done || (duration_ > 0 && t > duration_ + 0.5));
}

bool VideoPlayer::ShowFrameAt(double t) {
  for (;;) {
    std::unique_lock<std::mutex> l(mu_);
    cv_.wait(l, [&]() { return !queue_.empty() || ended_; });
    if (queue_.empty()) return hasFrame_;
    Frame f = std::move(queue_.front());
    queue_.pop_front();
    cv_.notify_all();
    l.unlock();
    if (f.t >= t || duration_ - f.t < 0.05) {
      Upload(f);
      return true;
    }
  }
}

void VideoPlayer::Draw(int screenW, int screenH, float alpha) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, screenW, screenH);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!hasFrame_ || !shader_) return;
  // Seitenverhältnis erhalten (Ränder oben/unten oder links/rechts)
  float sa = (float)screenW / screenH, va = (float)w_ / h_;
  float sx = va > sa ? 1.0f : va / sa, sy = va > sa ? sa / va : 1.0f;
  Shader* sh = (Shader*)shader_;
  sh->Use();
  sh->Set("uRect", Vec4(-sx, -sy, sx, sy));
  sh->Set("uAlpha", Saturate(alpha));
  sh->Set("uY", 0);
  sh->Set("uUV", 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texY_);
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, texUV_);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void VideoPlayer::Close() {
  if (started_) LOG("Video: %d Bilder gezeigt, %d ausgelassen", shown_, dropped_);
  shown_ = dropped_ = 0;
  stop_ = true;
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
  queue_.clear();
  if (voice_) audio::Stop(voice_);
  voice_ = 0;
  if (reader_) {
    ((IMFSourceReader*)reader_)->Release();
    reader_ = nullptr;
  }
  if (mfStarted_) MFShutdown();
  mfStarted_ = false;
  if (shader_) {
    Shader* sh = (Shader*)shader_;
    sh->Destroy();
    delete sh;
    shader_ = nullptr;
  }
  if (texY_) glDeleteTextures(1, &texY_);
  if (texUV_) glDeleteTextures(1, &texUV_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  texY_ = texUV_ = vao_ = 0;
  hasFrame_ = started_ = false;
  // Hinweis: der Klang bleibt im Mixer registriert (kleine Menge, nur zwei Intros pro Start)
  sound_ = -1;
}
