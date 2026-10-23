#pragma once
// Taunts der Props. Dauer muss zu den synthetisierten Klängen passen (Sounds.cpp).

struct TauntInfo {
  const char* name;
  float duration;
};

inline const TauntInfo kTaunts[] = {
    {"Kuckuck", 1.6f},          {"Quietscheente", 1.7f},  {"Pupser", 1.3f},       {"Fahrradklingel", 1.2f},
    {"Trompeten-Fail", 2.6f},   {"Muh!", 1.8f},           {"Miau", 1.2f},         {"Hupe", 1.1f},
    {"Pfiff", 1.4f},            {"Glockenspiel", 2.4f},   {"Hahaha", 1.9f},       {"Boing", 1.0f},
    {"Präsidentenglocke", 2.0f}, {"Applaus", 3.0f},       {"Tusch", 2.2f},        {"Kazoo-Hymne", 4.2f},
};
constexpr int NUM_TAUNTS = (int)(sizeof(kTaunts) / sizeof(kTaunts[0]));
constexpr int TAUNT_VARIANTS = 3;
