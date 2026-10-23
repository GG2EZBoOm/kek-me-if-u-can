#include "game/MapDetails.h"
#include "game/MapKit.h"

namespace {
constexpr u32 YELLOW = 0xffcf20, RED = 0xc91b10, STEEL = 0x354653;

void Planter(MapKit& k, Vec3 p, float length, u32 flowers) {
  k.Style(MAT_CONCRETE, 0x77796c);
  k.Solid(p + Vec3(-length * .5f, 0, -.48f), p + Vec3(length * .5f, .48f, .48f));
  k.Style(MAT_LEAVES, 0x365236);
  k.Solid(p + Vec3(-length * .5f + .09f, .48f, -.38f), p + Vec3(length * .5f - .09f, .78f, .38f));
  for (float x = -.5f * length + .25f; x < .5f * length; x += .32f) {
    k.Style(MAT_FABRIC, flowers);
    k.ob.Sphere(p + Vec3(x, .81f, .12f * sinf(x * 12)), .085f, 7, 4);
  }
}

void Canopy(MapKit& k, float x0, float z0, float x1, float z1, float h, u32 color, float base = 0) {
  k.Style(MAT_METAL, STEEL);
  for (float x : {x0 + .15f, x1 - .15f}) for (float z : {z0 + .15f, z1 - .15f})
    k.Solid({x - .09f, base, z - .09f}, {x + .09f, base + h, z + .09f});
  k.Slab(x0 - .25f, z0 - .25f, x1 + .25f, z1 + .25f, base + h + .18f, .18f, MAT_METAL, color, 0x949b9d);
  k.Style(MAT_METAL, color);
  k.Visual({x0 - .24f, base + h - .28f, z1 + .15f}, {x1 + .24f, base + h - .02f, z1 + .26f});
}

void Board(MapKit& k, Vec3 p, const char* title, const char* subtitle, u32 color) {
  k.Style(MAT_METAL, STEEL);
  k.Solid(p + Vec3(-.09f, 0, -.09f), p + Vec3(.09f, 2.3f, .09f));
  k.Sign(title, p + Vec3(0, 2.35f, .13f), 0, .065f, color);
  k.Sign(subtitle, p + Vec3(0, 1.76f, .13f), 0, .043f, 0xe6e4d5);
}

void ParcelPile(MapKit& k, Vec3 p, bool factory = false) {
  k.Prop("palette", p, 0);
  k.Prop(factory ? "nudelkarton" : "paket_m", p + Vec3(-.32f, .16f, 0), 0);
  k.Prop(factory ? "karton" : "paket_fragil", p + Vec3(.38f, .16f, .12f), PI * .5f);
  k.Prop(factory ? "nudelpackung" : "paket_s", p + Vec3(-.32f, .74f, 0), .12f);
}

void Container(MapKit& k, Vec3 p, u32 color, const char* label) {
  // Beidseitig offen, ebener Boden: kein unsichtbarer Sackgassen-Container.
  k.Style(MAT_CORRUGATED, color);
  k.Solid(p + Vec3(-1.7f, 0, -5), p + Vec3(-1.55f, 3.05f, 5));
  k.Solid(p + Vec3(1.55f, 0, -5), p + Vec3(1.7f, 3.05f, 5));
  k.Slab(p.x - 1.7f, p.z - 5, p.x + 1.7f, p.z + 5, p.y + 3.2f, .15f, MAT_METAL, color);
  k.FloorFinish(p.x - 1.55f, p.z - 5, p.x + 1.55f, p.z + 5, p.y + .02f, MAT_WOOD, 0x796b51);
  for (float x : {-1.64f, 1.64f}) {
    k.Style(MAT_METAL, STEEL);
    k.Visual(p + Vec3(x - .08f, 0, -5.08f), p + Vec3(x + .08f, 3.04f, -4.95f));
    k.Visual(p + Vec3(x - .08f, 0, 4.95f), p + Vec3(x + .08f, 3.04f, 5.08f));
  }
  k.Sign(label, p + Vec3(0, 2.79f, 5.12f), 0, .05f, 0xf2ddad);
  k.Prop("kiste", p + Vec3(-.95f, 0, 1.7f), 0);
  k.Prop("papierkorb", p + Vec3(1, 0, -2), 0);
  k.Light(p + Vec3(0, 2.7f, 0), 5, {1.1f, 1.2f, 1.3f});
}

void Bundestag(MapKit& k) {
  for (float x : {-59.0f, -49.0f}) for (float z : {-11.0f, 11.0f}) {
    Planter(k, {x, -1.2f, z}, 5, z < 0 ? 0xd9b763 : 0xa76b94);
    k.Prop("parkbank", {x, -1.2f, z + (z < 0 ? -2.1f : 2.1f)}, z < 0 ? 0 : PI);
    k.Prop("papierkorb", {x + 3.5f, -1.2f, z}, 0);
  }
  Canopy(k, -61, 23, -51, 28, 3.2f, 0x425b70, -1.2f);
  Board(k, {-55, -1.2f, 22}, "BESUCHER-INFORMATION", "PLENARSAAL / KUPPEL / KANTINE", 0xead8ad);
  k.Prop("schreibtisch", {-57.5f, -1.2f, 26.5f}, PI);
  k.Prop("buerostuhl", {-57.5f, -1.2f, 27.5f}, 0);
  k.Prop("aktenstapel", {-58, -.44f, 26.5f}, 0);
  k.Prop("kaffeebecher", {-57, -.44f, 26.5f}, 0);
  k.Prop("tvkamera", {-53, -1.2f, 25.5f}, -.4f);
  k.Prop("koffer", {-52.5f, -1.2f, 27}, 0);
  k.Prop("koffer", {-60, -1.2f, 24}, .3f);
  k.Sign("PLENUM  >", {-16.0f, 2.7f, -9}, PI * .5f, .08f, 0xd9bd78);
  k.Zone({-62, -1.2f, 21}, {-50, 3, 29}, 1, "Besucherforum", 2);
  k.w.cameraPath.insert(k.w.cameraPath.end(), {{-65, 2.4f, 29}, {-53, .5f, 23}});
}

void Oktoberfest(MapKit& k) {
  for (float x : {-26.0f, -10.0f, 9.0f, 27.0f}) {
    k.Prop("biertisch", {x, 0, 33.5f}, 0);
    k.Prop("holzfass", {x + 2.5f, 0, 35.6f}, .2f);
    k.Prop("getraenkekiste", {x - 1.5f, 0, 35.2f}, .1f);
    k.Prop("masskrug", {x, .79f, 33.5f}, .2f);
    k.Prop("brezel", {x + .65f, .79f, 33.5f}, .2f);
    Planter(k, {x + 1, 0, 31.5f}, 3.2f, 0xd69b3f);
  }
  Canopy(k, 16, 47, 23, 52, 3.7f, 0x96352f);
  k.Sign("FUNDSTUECKE & FAHRKARTEN", {19.5f, 3.2f, 52.35f}, 0, .075f, 0xffdf9b);
  k.Prop("biertisch", {19.5f, 0, 50.5f}, 0);
  k.Prop("teddy", {18.7f, .79f, 50.5f}, 0);
  k.Prop("lebkuchenherz", {20.1f, .79f, 50.5f}, 0);
  k.Prop("getraenkekiste", {17.1f, 0, 48.2f}, 0);
  k.Prop("ballons", {22, 0, 49}, 0);
  k.Prop("strohballen", {25.5f, 0, 49.5f}, .1f);
  Board(k, {3, 0, 49}, "FAHRGESCHAEFTE", "RIESENRAD  <    >  KETTENFLIEGER", 0xffc55e);
  k.Zone({15, 0, 46}, {26.5f, 5, 53}, 1, "Fundbuero & Fahrkarten", 2);
  k.w.cameraPath.insert(k.w.cameraPath.end(), {{29, 3.3f, 55}, {16, 1.7f, 49}});
}

void Factory(MapKit& k) {
  // Zweite, vollständig begehbare Halle füllt den vorher leeren Werkshof.
  k.Style(MAT_BRICK, 0x735040);
  k.WallX(6, 30, 39, 39.25f, 0, 6.5f, {{14, 19, 0, 4}});
  k.WallX(6, 30, 51, 51.25f, 0, 6.5f, {{10, 15, 0, 4}, {23, 28, 0, 4}});
  k.WallZ(39.25f, 51, 6, 6.25f, 0, 6.5f, {{43, 46, 0, 3}});
  k.WallZ(39.25f, 51, 29.75f, 30, 0, 6.5f, {{43, 46, 0, 3}});
  k.Slab(5.7f, 38.7f, 30.3f, 51.5f, 6.75f, .25f, MAT_CORRUGATED, 0x4d625e);
  k.FloorFinish(6.25f, 39.25f, 29.75f, 51, .025f, MAT_CONCRETE, 0x808578);
  k.Slab(19, 40, 29.5f, 49.5f, 2.4f, .16f, MAT_METAL, 0x506d65);
  k.Style(MAT_METAL, STEEL); k.Stairs({11.8f, 0, 47}, 0, 2.4f, .2f, 12, .6f, 0);
  k.Railing({19, 2.4f, 40}, {19, 2.4f, 45.65f}, 1, false);
  k.Railing({19, 2.4f, 48.35f}, {19, 2.4f, 49.5f}, 1, false);
  k.Railing({19, 2.4f, 49.5f}, {29.5f, 2.4f, 49.5f}, 1, false);
  k.Railing({19, 2.4f, 40}, {29.5f, 2.4f, 40}, 1, false);
  for (float x : {8.5f, 22.0f, 27.0f}) {
    ParcelPile(k, {x, 0, 41.5f}, true);
    k.CeilingLight({x, 6.4f, 45}, 9, {2.0f, 1.8f, 1.4f}, .45f);
  }
  for (float x : {22.0f, 27.0f}) {
    k.Prop("kartonpalette", {x, 2.4f, 41.8f}, 0);
    k.Prop("fass_blau", {x, 2.4f, 48.2f}, 0);
  }
  k.Prop("gabelstapler", {9, 0, 49}, PI * .5f);
  k.Sign("VERSANDLAGER 02", {18, 5.5f, 51.32f}, 0, .18f, 0xecc477);
  k.Sign("VERSANDLAGER 02", {18, 5.5f, 38.92f}, PI, .14f, 0xecc477);
  for (float x : {7.0f, 15.8f, 28.8f}) {
    k.Style(MAT_METAL, STEEL);
    k.Visual({x, 5.7f, 39.4f}, {x + .16f, 6.45f, 50.9f});
  }
  k.Sign("02-A", {12.5f, 4.3f, 51.33f}, 0, .07f, 0xe5d5af);
  k.Sign("02-B", {25.5f, 4.3f, 51.33f}, 0, .07f, 0xe5d5af);
  k.Zone({6.25f, 0, 39.25f}, {29.75f, 6.5f, 51}, .4f, "Versandlager", 2);
  k.Zone({19, 2.3f, 40}, {29.5f, 5, 49.5f}, .45f, "Lagergalerie", 3);
  Container(k, {-24, 0, 40}, 0x416d68, "LEERGUT");
  Canopy(k, -34, 25, -23, 31, 3.6f, 0x52685c);
  k.Prop("biertisch", {-28.5f, 0, 27.5f}, 0);
  k.Prop("bierbank", {-28.5f, 0, 28.5f}, 0);
  k.Prop("kaffeebecher", {-28, .79f, 27.5f}, 0);
  k.Prop("fahrrad", {-32.5f, 0, 29.5f}, PI * .5f);
  Planter(k, {-29, 0, 32.5f}, 5, 0xbcbf78);
  for (float x : {5.0f, 12.0f, 21.0f}) ParcelPile(k, {x, 0, 33.5f}, true);
  k.Prop("hubwagen", {17, 0, 34.5f}, .5f);
  for (float z = 24; z < 50; z += 2)
    k.FloorFinish(1.8f, z, 2.0f, z + 1.2f, .018f, MAT_PLASTIC, 0xdcbf5e);
  Board(k, {35, 0, 35}, "WERKSRUNDGANG", "PRODUKTION  /  LAGER 02", 0xe7c478);
  k.w.cameraPath.insert(k.w.cameraPath.end(), {{-1, 7, 57}, {19, 3, 43}, {8, 2, 44}, {26, 3.6f, 46}});
}

void Parcel(MapKit& k) {
  // Nordhalle und westlicher Servicehof: circa 49 Prozent mehr umfriedete Grundfläche.
  k.Style(MAT_CORRUGATED, 0x657986);
  k.WallX(-24, 22, -46, -45.8f, 0, 5.5f, {{-3, 3, 0, 3.5f}});
  k.WallX(-24, 22, -30, -29.8f, 0, 5.5f, {{-18, -13, 0, 3.5f}, {-3, 3, 0, 3.5f}, {12, 17, 0, 3.5f}});
  k.WallZ(-45.8f, -30, -24, -23.8f, 0, 5.5f, {{-40, -36, 0, 3.5f}});
  k.WallZ(-45.8f, -30, 21.8f, 22, 0, 5.5f, {{-40, -36, 0, 3.5f}});
  k.Slab(-24.2f, -46.2f, 22.2f, -29.6f, 5.75f, .25f, MAT_METAL, 0x52616c);
  k.FloorFinish(-23.8f, -45.8f, 21.8f, -30, .018f, MAT_CONCRETE, 0x878f8d);
  k.Sign("KEK  /  EXPRESS-UMSCHLAG", {-1, 4.9f, -29.7f}, 0, .105f, YELLOW);
  for (float x : {-15.5f, 0.0f, 14.5f}) {
    k.Sign(x < -1 ? "EXPRESS 01" : x > 1 ? "EXPRESS 03" : "EXPRESS 02", {x, 3.92f, -29.68f}, 0, .065f, YELLOW);
    k.FloorFinish(x - 2, -33, x - 1.9f, -30.5f, .04f, MAT_PLASTIC, YELLOW);
    k.FloorFinish(x + 1.9f, -33, x + 2, -30.5f, .04f, MAT_PLASTIC, YELLOW);
  }
  for (Vec2 span : {Vec2(-24, -18), Vec2(-13, -3), Vec2(3, 12), Vec2(17, 22)}) {
    k.Style(MAT_PLASTIC, RED); k.Visual({span.x, .45f, -29.79f}, {span.y, .82f, -29.74f});
  }
  for (float x : {-16.0f, -8.0f, 8.0f, 16.0f}) {
    k.Prop("rollcontainer", {x, 0, -42.5f}, 0);
    ParcelPile(k, {x, 0, -33.2f});
    k.Prop("postsack", {x + 1.6f, 0, -42.5f}, 0);
    k.CeilingLight({x, 5.4f, -38}, 8, {2.5f, 2.4f, 2.1f}, .55f);
  }
  k.Prop("gabelstapler", {-10, 0, -37.8f}, PI * .5f);
  k.Prop("hubwagen", {11, 0, -37.8f}, PI * .5f);
  // Seitenregale lassen den mittleren Querweg und alle sechs Hallenzugänge offen.
  for (float x : {-21.0f, 19.0f}) for (float z : {-42.0f, -33.3f}) {
    k.Style(MAT_METAL, STEEL);
    for (float dx : {-1.6f, 1.6f}) k.Solid({x + dx - .06f, 0, z - .56f}, {x + dx + .06f, 2.7f, z + .56f});
    for (float y : {.16f, 1.4f}) {
      k.Style(MAT_METAL, RED); k.Solid({x - 1.6f, y - .12f, z - .5f}, {x + 1.6f, y, z + .5f});
      for (float dx : {-1.0f, 0.0f, 1.0f}) k.Prop("paket_m", {x + dx, y, z}, 0);
    }
    // Keine nur für kleine Props zugänglichen Hohlräume unter den Regalböden.
    k.Clip({x - 1.6f, 0, z - .5f}, {x + 1.6f, 1.4f, z + .5f}, COL_PLAYER);
  }
  k.Zone({-23.8f, 0, -45.8f}, {21.8f, 5.5f, -30}, .4f, "Express-Umschlag", 2);
  Container(k, {-48, 0, -15}, RED, "RETOUREN");
  Container(k, {-40, 0, -9}, 0x537783, "SONDERFRACHT");
  for (Vec3 p : {Vec3(-51, 0, -25), Vec3(-36, 0, -22), Vec3(-52, 0, 2), Vec3(-35, 0, 5)}) {
    ParcelPile(k, p);
    k.Prop("rollcontainer", p + Vec3(2.1f, 0, 0), PI * .5f);
    k.Prop("postsack", p + Vec3(0, 0, 1.5f), .2f);
    k.FloorFinish(p.x - 1, p.z - 1, p.x + 3, p.z - .9f, .02f, MAT_PLASTIC, YELLOW);
  }
  k.Prop("gabelstapler", {-51, 0, 7}, -.4f);
  Board(k, {-33, 0, -11}, "SERVICEHOF", "RETOUREN / SONDERFRACHT", YELLOW);
  k.Zone({-55, 0, -27}, {-35, 8, 6}, 1, "Containerhof", 1);
  Canopy(k, -51, 11, -37, 22, 4.1f, YELLOW);
  k.Prop("werkbank", {-48, 0, 19}, 0);
  k.Prop("handscanner", {-48, .91f, 19}, 0);
  k.Prop("schaltschrank", {-39, 0, 20.5f}, PI);
  k.Prop("biertisch", {-41, 0, 14}, 0);
  k.Prop("bierbank", {-41, 0, 15}, 0);
  k.Prop("kaffeebecher", {-41, .79f, 14}, 0);
  k.Prop("rollcontainer", {-49, 0, 13}, 0);
  k.Zone({-52, 0, 10}, {-36, 6, 23}, 1, "Service & Pause", 2);
  // Eingang als erkennbarer Ort statt einer langen weißen Wand.
  Canopy(k, -7, 14.5f, 7, 21, 4.4f, RED);
  k.Sign("ANKOMMEN. ABTAUCHEN. KEK.", {0, 4.12f, 21.27f}, 0, .09f, YELLOW);
  for (float x : {-8.0f, 9.5f}) Planter(k, {x, 0, 33.5f}, 3, 0xe5b639);
  for (float x : {-24.0f, -12.0f, 16.0f, 24.0f}) {
    k.Style(MAT_METAL, RED);
    k.Visual({x - .12f, .1f, 27.23f}, {x + .12f, .9f, 27.3f});
  }
  for (Vec2 span : {Vec2(-26, -22), Vec2(-18, -10), Vec2(14, 18), Vec2(22, 26)}) {
    k.Style(MAT_PLASTIC, RED); k.Visual({span.x, .15f, 27.22f}, {span.y, .5f, 27.28f});
  }
  Canopy(k, 29, 32, 43, 38, 3.4f, YELLOW);
  for (int i = 0; i < 7; i++) {
    float x = 30 + i * 1.8f;
    k.Style(MAT_METAL, i % 2 ? RED : 0xb89d31);
    k.Solid({x, 0, 36.5f}, {x + 1.65f, 2.6f, 37.6f});
    for (float y : {.1f, .94f, 1.78f}) {
      k.Style(MAT_METAL, YELLOW); k.Visual({x + .09f, y, 36.46f}, {x + 1.56f, y + .74f, 36.49f});
      k.Style(MAT_METAL, STEEL); k.Visual({x + 1.25f, y + .32f, 36.43f}, {x + 1.43f, y + .4f, 36.45f});
    }
  }
  k.Sign("PAKETSTATION 24/7", {36, 3.03f, 31.64f}, PI, .09f, RED);
  for (float z = 22; z < 29; z += 1.2f)
    k.FloorFinish(-3, z, 3, z + .45f, .02f, MAT_PLASTIC, 0xd5d4bf);
  Board(k, {11, 0, 24}, "KEK-WEGWEISER", "EXPRESS  <   >  PAKETSTATION", YELLOW);
  k.w.cameraPath.insert(k.w.cameraPath.end(), {{-35, 8, -24}, {-2, 2.5f, -38}, {-53, 3, 5}, {-40, 1.7f, -12}});
}

void Zwinger(MapKit& k) {
  // Zwei neue Hofaufgänge verkürzen die Umwege zur Ostseite und schließen die isolierte Nordostecke an.
  k.Style(MAT_SANDSTONE, 0xe0d0ad);
  for (float side : {-1.0f, 1.0f}) {
    k.Stairs({16.6f, 0, side * 25.8f}, 0, 3.0f, 4.3f / 22, 22, .30f, 0);
  }
  for (float x : {-11.0f, 11.0f}) for (float z : {-10.0f, 10.0f}) {
    Planter(k, {x, 0, z - 5.5f}, 6, 0x995d82);
    Planter(k, {x, 0, z + 5.5f}, 6, 0xe1c181);
    k.Prop("parkbank", {x, 0, z < 0 ? -21.5f : 21.5f}, z < 0 ? 0 : PI);
  }
  k.Style(MAT_MARBLE, 0x82735c); k.ob.Ring({0, .032f, 0}, 2.3f, 2.6f, 64, true);
  k.Style(MAT_SANDSTONE, 0xd0bb92); k.ob.Ring({0, .034f, 0}, .01f, 2.29f, 64, true);
  Board(k, {6, 0, 26}, "ZWINGER / RUNDGANG", "KRONENTOR  <   >  NYMPHENBAD", 0xdfc89a);
  // Kleine, gerahmte Gemäldeszenen in der Galerie, komplett aus eigener Geometrie.
  for (float z : {-15.0f, -7.0f, 7.0f, 15.0f}) {
    k.Style(MAT_GOLD, 0x947138); k.Visual({32.85f, 1.6f, z - 1.05f}, {32.98f, 3.5f, z + 1.05f});
    k.Style(MAT_PLAIN, 0x547a85); k.Visual({32.82f, 1.74f, z - .9f}, {32.84f, 3.36f, z + .9f});
    k.Style(MAT_PLAIN, 0x344c42); k.Visual({32.80f, 1.75f, z - .9f}, {32.815f, 2.3f, z + .9f});
    k.Style(MAT_PLAIN, 0xc2ab74); k.Visual({32.78f, 2.1f, z - .25f}, {32.795f, 2.9f, z + .3f});
    k.Style(MAT_PLAIN, 0x79564e); k.Visual({32.76f, 2.87f, z - .35f}, {32.775f, 3.03f, z + .4f});
  }
  k.w.cameraPath.insert(k.w.cameraPath.end(), {{14, 5, -18}, {27, 5, -23}, {29, 2.2f, -3}, {32, 2.6f, -12}});
}
}

void RefineMap(World& world) {
  MapKit k(world);
  if (world.id == "bundestag") Bundestag(k);
  else if (world.id == "oktoberfest") Oktoberfest(k);
  else if (world.id == "nudelfabrik") Factory(k);
  else if (world.id == "paketzentrum") Parcel(k);
  else if (world.id == "zwinger") Zwinger(k);
}
