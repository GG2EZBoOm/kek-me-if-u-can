#pragma once
// Videowiedergabe (MP4: H.264 + AAC) über Windows Media Foundation – ohne externe Bibliotheken.
// Bild: Der Decoder liefert NV12 in einem eigenen Thread, ein Shader wandelt nach RGB (BT.709).
// Ton: wird beim Öffnen komplett dekodiert und über den eigenen Mixer abgespielt.
#include "engine/Common.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

class VideoPlayer {
 public:
  ~VideoPlayer();
  // data muss bis Close() gültig bleiben (z. B. eine Ressource in der EXE)
  bool Open(const void* data, size_t size);
  void Close();
  // Startet Bild und Ton; now = plat::Time()
  void Play(double now, float volume);
  void SetVolume(float volume);
  // Holt fällige Bilder und lädt sie hoch; false, sobald das Video zu Ende ist
  bool Update(double now);
  // Testmodus ohne Uhr: wartet, bis das Bild zum Zeitpunkt t (Sekunden) bereitsteht
  bool ShowFrameAt(double t);
  // Zeichnet das aktuelle Bild seitenrichtig ins Fenster (schwarze Ränder), alpha blendet nach Schwarz
  void Draw(int screenW, int screenH, float alpha);
  double Duration() const { return duration_; }
  double Elapsed(double now) const { return started_ ? now - start_ : 0.0; }

 private:
  struct Frame {
    double t = 0;
    std::vector<u8> y, uv;
  };
  bool OpenVideo(const void* data, size_t size);
  bool DecodeAudio(const void* data, size_t size);
  void DecodeLoop();
  void Upload(const Frame& f);
  bool InitGL();

  void* reader_ = nullptr;  // IMFSourceReader (Video)
  bool mfStarted_ = false;
  int w_ = 0, h_ = 0;       // sichtbare Größe
  int frameH_ = 0;          // Höhe inkl. Ausrichtung (UV-Ebene beginnt danach)
  int stride_ = 0;
  double duration_ = 0;
  double start_ = 0;
  bool started_ = false;
  bool hasFrame_ = false;
  int sound_ = -1, voice_ = 0;
  int shown_ = 0, dropped_ = 0;  // Statistik für das Protokoll
  float soundRate_ = 48000;

  std::thread thread_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<Frame> queue_;
  std::atomic<bool> stop_{false};
  bool ended_ = false;  // Decoder ist fertig (unter mu_)

  unsigned texY_ = 0, texUV_ = 0, vao_ = 0;
  void* shader_ = nullptr;
};
