#pragma once
// Wiederverwendbare Formen für Props und Karten: Bundesadler, Pixel-Schrift, extrudierte Polygone.
#include "engine/Mesh.h"

// Konvexes Polygon (gegen den Uhrzeigersinn in XY) zwischen z0 und z1 extrudieren
void ExtrudeConvex(MeshBuilder& b, const std::vector<Vec2>& pts, float z0, float z1);
// Stilisierter Bundesadler, ca. 2.2 breit, 2.2 hoch, zentriert, Front zeigt nach +Z, Dicke ~0.2
void BuildEagle(MeshBuilder& b);
// Pixel-Schrift (5x7) aus Quadern. Text in Großbuchstaben, Ursprung = linke untere Ecke, Schreibrichtung +X.
// Gibt die Breite zurück.
float BuildPixelText(MeshBuilder& b, const char* text, float pixel, float depth);
float PixelTextWidth(const char* text, float pixel);
