#pragma once
// Baukasten zum Errichten handgebauter Karten.
#include "game/World.h"

struct Hole {
  float a0, a1;  // Bereich entlang der Wand (absolute Koordinate)
  float y0, y1;  // vertikaler Bereich
  bool window = false;
};

class MapKit {
 public:
  explicit MapKit(World& w) : w(w), ob(&w.opaque), gb(&w.glass) {}
  World& w;
  MeshBuilder ob;  // undurchsichtige Geometrie
  MeshBuilder gb;  // Glas

  void Style(Material m, u32 color) { ob.Mat(m); ob.ColorHex(color); }
  void Glow(float e) { ob.Emissive(e); }

  // Fester Block: sichtbar + Kollision
  void Solid(Vec3 mn, Vec3 mx, u32 faces = FACE_ALL, u8 flags = COL_SOLID);
  void SolidR(Vec3 c, Vec3 h, float yaw, u32 faces = FACE_ALL, u8 flags = COL_SOLID);
  // Nur sichtbar
  void Visual(Vec3 mn, Vec3 mx, u32 faces = FACE_ALL) { ob.Box(mn, mx, faces); }
  void VisualR(Vec3 c, Vec3 h, float yaw, u32 faces = FACE_ALL) { ob.BoxR(c, h, yaw, faces); }
  // Nur Kollision (unsichtbar)
  void Clip(Vec3 mn, Vec3 mx, u8 flags = COL_PLAYER);
  void ClipR(Vec3 c, Vec3 h, float yaw, u8 flags = COL_PLAYER);
  // Glasscheibe
  void Glass(Vec3 mn, Vec3 mx, u32 tint = 0xc4dbe8, float alpha = 0.25f, bool collide = true);
  // Boden-/Deckenplatte: Oberseite mit Bodenmaterial, Unterseite als Decke
  void Slab(float x0, float z0, float x1, float z1, float yTop, float thick, Material floorMat, u32 floorCol,
            u32 ceilCol = 0xeceae4);
  // Nur Bodenbelag (Oberfläche bei y)
  void FloorFinish(float x0, float z0, float x1, float z1, float y, Material m, u32 col);
  // Wände mit Öffnungen. WallX verläuft entlang X (Dicke in Z), WallZ entlang Z.
  void WallX(float x0, float x1, float z0, float z1, float y0, float y1, const std::vector<Hole>& holes = {});
  void WallZ(float z0, float z1, float x0, float x1, float y0, float y1, const std::vector<Hole>& holes = {});
  // Gerade Treppe. dir: 0=+X, 1=-X, 2=+Z, 3=-Z. start = Mitte der untersten Kante (Bodenhöhe = baseY).
  void Stairs(Vec3 start, int dir, float width, float rise, int steps, float run, float baseY);
  // Geländer (Glas mit Handlauf) entlang einer Linie in XZ
  void Railing(Vec3 a, Vec3 b, float height = 1.0f, bool glass = true);

  int Prop(const char* id, Vec3 pos, float yaw);
  void Light(Vec3 pos, float radius, Vec3 color);
  // Deckenleuchte (sichtbares Panel + Licht)
  void CeilingLight(Vec3 pos, float radius = 8.0f, Vec3 color = Vec3(2.3f, 2.1f, 1.8f), float size = 0.5f);
  void Zone(Vec3 mn, Vec3 mx, float sky, const char* name, int prio = 0);
  // Schild aus Pixel-Buchstaben. facing: Blickrichtung der Schrift (Normale).
  void Sign(const char* text, Vec3 center, float yaw, float pixel, u32 color, float glow = 0, bool backing = true);
  // KEK wayfinding: fitted yellow enamel panel, red accent, proportional lettering.
  void ParcelSign(const char* text, Vec3 center, float yaw, float pixel);

  // Würde ein Prop an dieser Stelle eine registrierte Tür versperren?
  bool BlocksDoor(int type, Vec3 pos, float yaw) const;

  // Platziert ein Prop relativ zu einem Bezugspunkt mit Drehung
  int PropRel(const char* id, Vec3 origin, float baseYaw, Vec3 local, float localYaw = 0);

 private:
  void RegisterDoors(bool alongX, float fixed0, float fixed1, const std::vector<Hole>& holes);
};
