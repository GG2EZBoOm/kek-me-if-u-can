#pragma once
// Katalog aller Gegenstände (Props): Geometrie, Trefferzonen, Kollision, Lebenspunkte.
#include "engine/Mesh.h"

struct AABB {
  Vec3 mn, mx;
};

struct PropDef {
  std::string id;
  std::string name;          // Anzeigename
  bool disguisable = true;   // Props-Spieler können sich darin verwandeln
  bool solid = true;         // blockiert Bewegung
  bool partCollision = false; // Kollision je Einzelteil (z. B. Torbogen zum Durchgehen)
  MeshData mesh;             // lokal, Ursprung am Boden, Vorderseite +Z
  std::vector<AABB> parts;   // genaue Trefferzonen
  Vec3 bmin, bmax;           // Gesamtbox lokal
  Vec3 colCenter, colHalf;   // Kollisionsbox lokal
  float health = 100;
  float volume = 0;
  float ramDamage = 0;      // nur schwere, massive Tarnungen; 0 = kein Kollisionsangriff
  bool vehicle = false;     // schwerere Bewegung und begrenzte Drehgeschwindigkeit
  float lookYawOffset = PI; // Modellfront auf Blickrichtung ausrichten (Standard: +Z)
  bool light = false;        // Lampe
  Vec3 lightOffset;
  Vec3 lightColor;
  float lightRadius = 0;
};

namespace props {
void BuildCatalog();
int Count();
const PropDef& Get(int id);
int Find(const char* id);
}  // namespace props
