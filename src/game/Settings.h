#pragma once
// Benutzereinstellungen (prophunt.ini neben der EXE).
#include "game/GameTypes.h"

struct Settings {
  std::string name = "Spieler";
  float sensitivity = 1.0f;
  bool invertY = false;
  float fov = 85.0f;
  float masterVolume = 0.8f;
  float musicVolume = 0.5f;
  int shadowQuality = 1;  // 0 = mittel (2048), 1 = hoch (4096)
  bool ssao = true;
  bool bloom = true;
  bool fxaa = true;
  bool vsync = true;
  bool fullscreen = false;
  bool showFps = false;
  bool autoUpdate = true;  // beim Start nach einer neuen Version auf GitHub suchen
  bool intros = true;      // Intro-Videos vor dem Hauptmenü
  MatchSettings match;
  u16 builds[FIREARM_COUNT] = {};
  std::string lastServer = "127.0.0.1";
  std::string serverName = "Kek-Server";

  void Load(const std::string& path);
  void Save(const std::string& path) const;
};
