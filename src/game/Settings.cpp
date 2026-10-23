#include "game/Settings.h"
#include "game/Weapons.h"
#include <map>

void Settings::Load(const std::string& path) {
  std::string text;
  if (!ReadFileText(path, text)) return;
  std::map<std::string, std::string> kv;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    size_t eq = line.find('=');
    if (eq == std::string::npos || line[0] == '#') continue;
    kv[line.substr(0, eq)] = line.substr(eq + 1);
  }
  auto f = [&](const char* k, float& v) { if (kv.count(k)) v = (float)atof(kv[k].c_str()); };
  auto i = [&](const char* k, int& v) { if (kv.count(k)) v = atoi(kv[k].c_str()); };
  auto b = [&](const char* k, bool& v) { if (kv.count(k)) v = kv[k] == "1"; };
  auto s = [&](const char* k, std::string& v) { if (kv.count(k) && !kv[k].empty()) v = kv[k]; };
  s("name", name);
  f("sensitivity", sensitivity);
  b("invertY", invertY);
  f("fov", fov);
  f("masterVolume", masterVolume);
  f("musicVolume", musicVolume);
  i("shadowQuality", shadowQuality);
  b("ssao", ssao);
  b("bloom", bloom);
  b("fxaa", fxaa);
  b("vsync", vsync);
  b("fullscreen", fullscreen);
  b("showFps", showFps);
  b("autoUpdate", autoUpdate);
  b("intros", intros);
  i("map", match.mapIndex);
  i("rounds", match.rounds);
  f("hideTime", match.hideTime);
  f("roundTime", match.roundTime);
  i("bots", match.bots);
  i("botSkill", match.botSkill);
  b("infection", match.infection);
  i("mode", match.mode);
  s("lastServer", lastServer);
  s("serverName", serverName);
  for (int w = 0; w < FIREARM_COUNT; w++) {
    int build = 0; i(w == WPN_PISTOL ? "buildPistol" : w == WPN_SHOTGUN ? "buildShotgun" : "buildSMG", build);
    builds[w] = ValidateWeaponBuild(w, (u16)build);
  }
  sensitivity = Clamp(sensitivity, 0.1f, 5.0f);
  fov = Clamp(fov, 60.0f, 120.0f);
  match.bots = ClampI(match.bots, 0, MAX_PLAYERS - 1);
  match.botSkill = ClampI(match.botSkill, 0, 2);
  match.rounds = ClampI(match.rounds, 1, 20);
  match.mode = ClampI(match.mode, 0, MODE_COUNT - 1);
}

void Settings::Save(const std::string& path) const {
  std::string t = "# Kek Me If You Can – Einstellungen\n";
  t += "name=" + name + "\n";
  t += StrFormat("sensitivity=%.3f\ninvertY=%d\nfov=%.1f\n", sensitivity, invertY ? 1 : 0, fov);
  t += StrFormat("masterVolume=%.2f\nmusicVolume=%.2f\n", masterVolume, musicVolume);
  t += StrFormat("shadowQuality=%d\nssao=%d\nbloom=%d\nfxaa=%d\nvsync=%d\nfullscreen=%d\nshowFps=%d\n", shadowQuality,
                 ssao ? 1 : 0, bloom ? 1 : 0, fxaa ? 1 : 0, vsync ? 1 : 0, fullscreen ? 1 : 0, showFps ? 1 : 0);
  t += StrFormat("autoUpdate=%d\nintros=%d\n", autoUpdate ? 1 : 0, intros ? 1 : 0);
  t += StrFormat("map=%d\nrounds=%d\nhideTime=%.0f\nroundTime=%.0f\nbots=%d\nbotSkill=%d\ninfection=%d\n", match.mapIndex,
                 match.rounds, match.hideTime, match.roundTime, match.bots, match.botSkill, match.infection ? 1 : 0);
  t += "lastServer=" + lastServer + "\n";
  t += StrFormat("mode=%d\n", match.mode);
  t += "serverName=" + serverName + "\n";
  t += StrFormat("buildSMG=%u\nbuildShotgun=%u\nbuildPistol=%u\n", builds[0], builds[1], builds[2]);
  WriteFileText(path, t);
}
