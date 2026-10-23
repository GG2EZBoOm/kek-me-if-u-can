#pragma once
#include "game/World.h"

struct MapInfo {
  const char* id;
  const char* name;
  const char* description;
  void (*build)(World& w);
};

const std::vector<MapInfo>& AllMaps();
int FindMap(const std::string& id);
// Baut die Karte (deterministisch) und finalisiert sie.
bool BuildMap(int index, World& w);

void BuildBundestag(World& w);
void BuildOktoberfest(World& w);
void BuildNudelfabrik(World& w);
void BuildPaketzentrum(World& w);
