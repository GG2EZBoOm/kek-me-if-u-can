#pragma once
// Taunts der Props. Dauer muss zu den gesampelten Comedy-Phrasen passen (Sounds.cpp).

struct TauntInfo {
  const char* name;
  float duration;
};

inline const TauntInfo kTaunts[] = {
    {"Kuckuck", 1.6f},          {"Quietscheente", 1.7f},  {"Pupser", 1.3f},       {"Fahrradklingel", 1.2f},
    {"Pannen-Fanfare", 2.6f},   {"Muh!", 1.8f},           {"Miau", 1.2f},         {"Clown-Hupe", 1.1f},
    {"Rutschpartie", 1.4f},     {"Glockenspiel", 2.4f},   {"Schadenfreude", 1.9f}, {"Feder-Fail", 1.0f},
    {"Ordnungsruf", 2.0f},      {"Peinlicher Applaus", 3.0f}, {"Ba-dum-tss", 2.2f}, {"Mini-Blaskapelle", 4.2f},
};
constexpr int NUM_TAUNTS = (int)(sizeof(kTaunts) / sizeof(kTaunts[0]));
constexpr int TAUNT_VARIANTS = 3;
