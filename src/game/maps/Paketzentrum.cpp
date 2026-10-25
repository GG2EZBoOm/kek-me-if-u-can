// KEK-Paketzentrum: Sortierhalle, Brücke, Retouren, Leitstelle und begehbare Wechselbrücken.
#include "game/MapKit.h"
#include "game/Maps.h"
#include "game/Shapes.h"

namespace {
constexpr u32 YELLOW = 0xffcf20, RED = 0xc91b10, STEEL = 0x56616b, WHITE = 0xe7e8e4;

class ParcelCenter {
 public:
  World& w;
  MapKit k;
  explicit ParcelCenter(World& world) : w(world), k(world) {}

  void Logo(Vec3 center, float scale, float yaw = 0) {
    auto& b = k.ob;
    b.Push(Translation(center) * RotationY(yaw) * Scaling(Vec3(scale)));
    b.Mat(MAT_PLASTIC); b.ColorHex(YELLOW);
    b.Box({-3.3f, -.9f, -.06f}, {3.3f, .9f, 0});
    b.Mat(MAT_PLAIN); b.ColorHex(RED);
    b.Push(Translation({0, 0, .012f})); BuildKekLogo(b); b.Pop();
    b.Pop();
  }
  void Text(const char* text, Vec3 pos, float pixel = .075f, float yaw = 0) {
    k.ParcelSign(text, pos, yaw, pixel);
  }
  void Package(Vec3 pos, int variant, float yaw = 0) {
    const char* names[] = {"paket_s", "paket_m", "paket_fragil", "versandrolle", "postsack", "sortierkiste"};
    k.Prop(names[variant % 6], pos, yaw);
  }
  void Rail(Vec3 a, Vec3 b) { k.Railing(a, b, 1.05f, false); }

  void Shell() {
    w.env.sunDir = Normalize(Vec3(-.35f, .7f, .6f));
    w.env.sunColor = {3.2f, 2.85f, 2.35f};
    w.env.fogDensity = .0025f;
    w.env.indoorAmbient = {.32f, .34f, .38f};
    w.env.clouds = .3f;
    w.env.exposure = .79f;
    k.Clip({-59, -2, -53}, {51, 0, 45}, COL_SOLID);
    k.FloorFinish(-59, -53, 51, 45, -.025f, MAT_ASPHALT, 0x454950);
    k.FloorFinish(-26, -22, 26, 14, 0, MAT_CONCRETE, 0x9a9e9f);
    k.FloorFinish(-26, 14, -10, 27, 0, MAT_TILE, 0xbfc4be);
    k.FloorFinish(14, 14, 26, 27, 0, MAT_PARQUET, 0xb79a6a);

    k.Style(MAT_CORRUGATED, WHITE);
    k.WallX(-26.2f, 26.2f, -22.2f, -22, 0, 8.2f, {{-3, 3, 0, 3.8f}});
    k.WallX(-26.2f, 26.2f, 14, 14.2f, 0, 8.2f, {{-22, -18, 0, 3}, {-3, 3, 0, 4.2f}, {18, 22, 0, 3}});
    k.WallZ(-22, 14, -26.2f, -26, 0, 8.2f, {{-9, -5, 0, 3.4f}});
    k.WallZ(-22, 14, 26, 26.2f, 0, 8.2f, {{-17, -12, 0, 4.3f}, {-8, -3, 0, 4.3f}, {3, 8, 0, 4.3f}});
    k.Slab(-26.3f, -22.3f, 26.3f, 14.3f, 8.5f, .28f, MAT_METAL, 0x78818a);
    k.Style(MAT_PLASTIC, YELLOW);
    k.Visual({-26.25f, 5.1f, 14.21f}, {26.25f, 8.15f, 14.3f});
    Logo({0, 6.62f, 14.32f}, 1.65f);
    Text("PAKETZENTRUM", {0, 4.7f, 14.44f}, .075f);
    Text("KEIN PAKET ZURUECKLASSEN", {0, 2.85f, -22.25f}, .07f, PI);
    for (float x : {-18.0f, -9.0f, 0.0f, 9.0f, 18.0f}) {
      k.Style(MAT_METAL, STEEL);
      k.Visual({x - .07f, 7.65f, -21.8f}, {x + .07f, 8.1f, 13.8f});
      for (float z : {-16.0f, -7.0f, 3.0f, 11.0f}) k.CeilingLight({x, 7.6f, z}, 10, {2.9f, 2.8f, 2.5f}, .55f);
    }
    // Öffentliches Retourenbüro und Leitstelle mit zwei Zugängen je Flügel.
    k.Style(MAT_PLASTER, 0xe5e0d6);
    k.WallX(-26.2f, -10, 27, 27.2f, 0, 4.2f,
            {{-22, -18, 0, 2.8f}, {-25, -23, 1.1f, 2.7f, true}, {-16.5f, -11, 1.1f, 2.7f, true}});
    k.Glass({-25, 1.1f, 27.08f}, {-23, 2.7f, 27.12f}, 0x7399aa, .32f);
    k.Glass({-16.5f, 1.1f, 27.08f}, {-11, 2.7f, 27.12f}, 0x7399aa, .32f);
    k.WallZ(14.2f, 27, -26.2f, -26, 0, 4.2f);
    k.WallZ(14.2f, 27, -10.2f, -10, 0, 4.2f, {{18, 21, 0, 2.8f}});
    k.WallX(14, 26.2f, 27, 27.2f, 0, 4.2f,
            {{18, 22, 0, 2.8f}, {15, 17, 1.1f, 2.7f, true}, {23, 25, 1.1f, 2.7f, true}});
    for (float x : {15.0f, 23.0f}) k.Glass({x, 1.1f, 27.08f}, {x + 2, 2.7f, 27.12f}, 0x7399aa, .32f);
    k.WallZ(14.2f, 27, 26, 26.2f, 0, 4.2f);
    k.WallZ(14.2f, 27, 14, 14.2f, 0, 4.2f, {{18, 22, 1.1f, 3.2f, true}});
    k.Glass({14.06f, 1.1f, 18}, {14.12f, 3.2f, 22}, 0x99c8dd, .22f);
    k.Slab(-26.3f, 14.3f, -9.9f, 27.3f, 4.5f, .28f, MAT_CONCRETE, 0x83898d);
    k.Slab(13.9f, 14.3f, 26.3f, 27.3f, 4.5f, .28f, MAT_CONCRETE, 0x83898d);
    Text("RETOUREN & ABHOLUNG", {-18, 3.5f, 27.31f}, .067f);
    Text("LEITSTELLE", {20, 3.5f, 27.31f}, .067f);
  }

  void Conveyor(float x) {
    k.Style(MAT_METAL, STEEL); k.Solid({x - 1, 0, -17}, {x + 1, .75f, 7});
    k.Style(MAT_LEATHER, 0x252c33); k.Visual({x - .82f, .76f, -17}, {x + .82f, .88f, 7});
    for (float side : {-1.0f, 1.0f}) {
      k.Style(MAT_METAL, YELLOW); k.Solid({x + side * .93f - .045f, .75f, -17}, {x + side * .93f + .045f, 1.65f, 7});
    }
    // Die geschlossene Förderanlage ist kein Versteck; Pakete bewegen sich innerhalb des Schutzgehäuses.
    k.Clip({x - .9f, .75f, -17}, {x + .9f, 1.65f, 7});
    for (int i = 0; i < 7; i++) {
      AnimatedPart a; a.slideDir = {0, 0, 1}; a.slideLen = 21; a.speed = .85f; a.phase = i * 3.0f;
      MeshBuilder b(&a.mesh); b.Mat(MAT_PAPER); b.ColorHex(i % 2 ? 0xbd975c : 0xd2ac71);
      b.Box({x - .31f, .9f, -16.3f}, {x + .31f, 1.4f, -15.7f});
      b.ColorHex(0xf1e5c6); b.Box({x - .035f, 1.411f, -16.3f}, {x + .035f, 1.42f, -15.7f});
      w.anims.push_back(std::move(a));
    }
    // Scanportal über dem Band, mit rotem Scannerstreifen.
    k.Style(MAT_METAL, RED);
    k.Solid({x - 1.3f, 0, 1.7f}, {x - 1.1f, 3, 2.3f});
    k.Solid({x + 1.1f, 0, 1.7f}, {x + 1.3f, 3, 2.3f});
    k.Solid({x - 1.1f, 2.7f, 1.7f}, {x + 1.1f, 3, 2.3f});
    k.Style(MAT_PLASTIC, 0xff3320); k.Glow(1.2f);
    k.Visual({x - .75f, 2.61f, 1.96f}, {x + .75f, 2.64f, 2.04f}); k.Glow(0);
    Text(x < 0 ? "SORTIERUNG A" : "SORTIERUNG B", {x, 2.95f, 2.32f}, .06f);
  }

  void Sorting() {
    Conveyor(-6); Conveyor(6);
    k.Style(MAT_PLASTIC, YELLOW);
    for (float x : {-10.0f, 10.0f}) k.FloorFinish(x, -20, x + .1f, 12, .015f, MAT_PLASTIC, YELLOW);
    k.FloorFinish(-24, 10.9f, 24, 11.0f, .015f, MAT_PLASTIC, YELLOW);
    for (float x : {-17.0f, 17.0f}) for (float z : {-16.5f, -10.5f}) {
      // Regal: durchgehende Rückwand verhindert uneinsehbare Zwischenräume.
      k.Style(MAT_METAL, 0x435d78);
      k.Solid({x - 3.2f, 0, z - .52f}, {x + 3.2f, 3.1f, z - .44f});
      for (float dx : {-3.2f, 0.0f, 3.2f}) k.Solid({x + dx - .06f, 0, z - .42f}, {x + dx + .06f, 3.15f, z + .6f});
      for (float y : {.12f, 1.25f, 2.4f}) {
        k.Style(MAT_METAL, RED); k.Solid({x - 3.15f, y - .1f, z - .4f}, {x + 3.15f, y, z + .61f});
        for (int i = 0; i < 6; i++) Package({x - 2.65f + i * .98f, y + .015f, z + .05f}, i + (int)y, i % 2 ? 0 : PI);
      }
    }
    for (float x : {-15.0f, 15.0f}) for (float z : {2.0f, 6.0f}) {
      k.Prop("werkbank", {x, 0, z}, 0);
      k.Prop("handscanner", {x - .5f, .91f, z}, .2f);
      k.Prop("etikettendrucker", {x + .5f, .91f, z}, 0);
      k.Prop("klebeband", {x, .91f, z + .1f}, 0);
      Package({x - 1.8f, 0, z}, 2);
      k.Prop("rollcontainer", {x + 2.1f, 0, z}, -PI * .5f);
    }
    for (float x : {-12.0f, 12.0f}) for (float z : {-19.5f, -7.0f, 8.0f}) {
      k.Prop("palette", {x, 0, z}, 0); Package({x, .16f, z}, 1);
      Package({x + .36f, .69f, z}, 0);
    }
    k.Prop("gabelstapler", {-23, 0, -18.5f}, PI * .5f);
    k.Prop("hubwagen", {23, 0, -19}, 0);
    k.Prop("schaltschrank", {-24.9f, 0, -2}, PI * .5f);
    k.Prop("waage", {23, 0, 11}, PI);
    Text("HEUTE SCHON FALSCH ZUGESTELLT?", {0, 5.7f, -21.8f}, .1f);
    Logo({0, 3.8f, -21.8f}, .7f);

    // Zwei Treppen und eine breite Querbrücke: volle Runde statt Sackgasse.
    k.Slab(-24, -5, 24, -1, 3.4f, .18f, MAT_METAL, 0x737d84);
    k.Style(MAT_METAL, STEEL);
    for (float x : {-21.0f, 21.0f}) {
      k.Stairs({x, 0, 9}, 3, 2.4f, .17f, 20, .5f, 0);
      for (float side : {-1.0f, 1.0f}) {
        float sx = x + side * 1.3f;
        // Die Handläufe steigen mit den Stufen; keine unsichtbare Schranke am Treppenende.
        for (int i = 0; i < 5; i++) {
          float z = 8 - i * 2, y = .34f + i * .68f;
          k.Visual({sx - .035f, y, z - .035f}, {sx + .035f, y + 1, z + .035f});
        }
      }
    }
    Rail({-24, 3.4f, -5}, {24, 3.4f, -5});
    Rail({-19.65f, 3.4f, -1}, {19.65f, 3.4f, -1});
    Rail({-24, 3.4f, -1}, {-22.35f, 3.4f, -1}); Rail({22.35f, 3.4f, -1}, {24, 3.4f, -1});
    Rail({-24, 3.4f, -5}, {-24, 3.4f, -1}); Rail({24, 3.4f, -5}, {24, 3.4f, -1});
    for (float x : {-16.0f, 0.0f, 16.0f}) {
      Package({x, 3.4f, -3.8f}, 5); k.Prop("klemmbrett", {x + 1, 3.4f, -3.8f}, 0);
    }
    Text("SORTIERBRUECKE", {0, 3.02f, -.88f}, .1f);
  }

  void Offices() {
    for (float x : {-23.5f, -13.0f}) {
      k.Style(MAT_PLASTIC, YELLOW); k.Solid({x - 1.8f, 0, 20.5f}, {x + 1.8f, .95f, 21.5f});
      k.Style(MAT_WOOD, 0x4d4440); k.Solid({x - 1.9f, .96f, 20.4f}, {x + 1.9f, 1.04f, 21.6f});
      k.Prop("handscanner", {x, 1.04f, 20.9f}, 0); k.Prop("etikettendrucker", {x + .65f, 1.04f, 20.9f}, 0);
      Package({x - .75f, 1.04f, 21}, 0);
      k.Prop("buerostuhl", {x, 0, 19.2f}, PI);
    }
    for (float z : {16.5f, 18.0f, 23.5f, 25.0f}) {
      k.Prop("rollcontainer", {-24.7f, 0, z}, PI * .5f);
      Package({-12.4f, 0, z}, 4);
    }
    Text("ANGEBLICH NICHT ZUHAUSE", {-18, 2.5f, 14.27f}, .067f);
    k.Prop("feuerloescher", {-25.6f, 0, 25.8f}, 0);
    for (float z : {17.0f, 24.0f}) {
      k.Prop("schreibtisch", {23.2f, 0, z}, PI * .5f);
      k.Prop("laptop", {23.2f, .77f, z}, PI * .5f);
      k.Prop("buerostuhl", {21.7f, 0, z}, -PI * .5f);
      k.Prop("kaffeebecher", {23.3f, .77f, z + .55f}, 0);
      k.Prop("aktenstapel", {23.2f, .77f, z - .55f}, 0);
      k.Prop("spind", {25.2f, 0, z + 1.7f}, -PI * .5f);
    }
    k.Prop("kantinentisch", {17, 0, 24.8f}, 0);
    k.Prop("kantinenstuhl", {17, 0, 23.7f}, PI);
    k.Prop("kaffeebecher", {17, .78f, 24.8f}, 0);
    k.Prop("klemmbrett", {17.6f, .78f, 24.8f}, .4f);
    Text("LEITSTELLE 404", {20, 2.7f, 14.27f}, .08f);
    for (float x : {-22.0f, -14.0f, 18.0f, 23.0f}) for (float z : {17.0f, 24.0f})
      k.CeilingLight({x, 4.16f, z}, 7, {2.6f, 2.5f, 2.1f}, .45f);
  }

  void Truck(float z, int number) {
    k.Style(MAT_METAL, STEEL); k.Solid({32.5f, .8f, z - 1.65f}, {42.5f, 1.2f, z + 1.65f});
    k.Style(MAT_CORRUGATED, YELLOW);
    for (float side : {-1.0f, 1.0f}) k.Solid({32.5f, 1.2f, z + side * 1.7f - .06f}, {42.5f, 4.1f, z + side * 1.7f + .06f});
    k.Solid({42.4f, 1.2f, z - 1.64f}, {42.5f, 4.1f, z + 1.64f});
    k.Slab(32.5f, z - 1.8f, 42.6f, z + 1.8f, 4.25f, .15f, MAT_METAL, WHITE);
    k.Style(MAT_PLASTIC, RED); k.Solid({42.9f, .65f, z - 1.5f}, {46.2f, 3.25f, z + 1.5f});
    k.Style(MAT_GLASS, 0x28516a); k.Visual({46.21f, 1.8f, z - 1.35f}, {46.24f, 2.95f, z + 1.35f});
    k.Style(MAT_LEATHER, 0x23272c);
    for (float x : {34.0f, 40.8f, 44.9f}) for (float side : {-1.0f, 1.0f})
      k.ob.CylinderZ({x, .55f, z + side * 1.64f}, .52f, .28f, 18);
    k.Style(MAT_METAL, 0x89939b); k.Stairs({29.5f, 0, z}, 0, 2.6f, .2f, 6, .5f, 0);
    Logo({37.5f, 2.6f, z + 1.78f}, .8f);
    Logo({37.5f, 2.6f, z - 1.78f}, .8f, PI);
    for (int i = 0; i < 4; i++) {
      Package({35.5f + i * 1.6f, 1.2f, z - .8f}, i);
      Package({36 + i * 1.5f, 1.2f, z + .8f}, i + 1, PI);
    }
    Text(StrFormat("TOR %02d", number).c_str(), {26.28f, 4.8f, z}, .1f, PI * .5f);
    k.Zone({32.5f, 1.15f, z - 1.65f}, {42.5f, 4.2f, z + 1.65f}, .3f, "Wechselbrücke", 2);
  }

  void Yard() {
    Truck(-14.5f, 1); Truck(-5.5f, 2); Truck(5.5f, 3);
    for (float x : {-33.0f, -27.0f, -21.0f, -15.0f}) {
      k.FloorFinish(x, 32, x + .1f, 40, .012f, MAT_PLASTIC, WHITE);
      if (x < -26) k.Prop(x < -30 ? "auto_grau" : "auto_blau", {x + 2.7f, 0, 36}, 0);
    }
    for (float z : {-18.0f, 2.0f, 10.0f}) {
      k.Prop("rollcontainer", {-31, 0, z}, 0);
      k.Prop("palette", {-33, 0, z}, PI * .5f);
      Package({-33, .16f, z}, 1); Package({-33, .69f, z}, 2);
    }
    for (float x : {-18.0f, -12.0f, 12.0f, 18.0f}) {
      k.Prop("rollcontainer", {x, 0, -27}, PI);
      Package({x + 1.5f, 0, -27}, 4);
    }
    // Hoher Zaun begrenzt das Gelände auch in der Mondschicht sichtbar.
    auto fence = [&](Vec3 a, Vec3 b) {
      Vec3 delta = b - a; int count = (int)Length(delta);
      for (int i = 0; i <= count; i++) {
        Vec3 p = a + delta * ((float)i / count);
        k.Style(MAT_METAL, STEEL); k.Visual(p, p + Vec3(.035f, 7.5f, .035f));
      }
      for (float y : {.1f, 1.2f, 2.3f, 3.5f, 4.8f, 6.2f, 7.5f}) {
        Vec3 mn = Min(a, b) + Vec3(0, y, 0), mx = Max(a, b) + Vec3(.04f, y + .04f, .04f);
        k.Visual(mn, mx);
      }
      k.Clip(Min(a, b), Max(a, b) + Vec3(.08f, 7.6f, .08f), COL_SOLID);
    };
    fence({-57, 0, -51}, {-57, 0, 43}); fence({49, 0, -51}, {49, 0, 43});
    fence({-57, 0, -51}, {49, 0, -51}); fence({-57, 0, 43}, {49, 0, 43});
    Logo({0, 2, 42.8f}, .75f, PI);
    for (Vec3 p : {Vec3(-34, 0, 27), Vec3(30, 0, 29), Vec3(-33, 0, -27)}) {
      k.Style(MAT_METAL, STEEL); k.Solid(p, p + Vec3(.16f, 6, .16f));
      k.CeilingLight(p + Vec3(.08f, 5.9f, .08f), 12, {2.4f, 2.3f, 1.9f}, .45f);
    }
    // Sichtbare Industriekulisse jenseits des Zauns, ohne zusätzlichen Spielbereich.
    k.Style(MAT_CORRUGATED, 0x788593);
    k.Visual({-90, -1, -75}, {-63, 12, -40}); k.Visual({55, -1, -70}, {95, 15, 5});
  }

  void Build() {
    Shell(); Sorting(); Offices(); Yard();
    k.Zone({-58, -1, -52}, {50, 30, 44}, 1, "KEK-Betriebshof");
    k.Zone({-26, 0, -22}, {26, 8.2f, 14}, .35f, "Sortierhalle", 1);
    k.Zone({-24, 3.3f, -5}, {24, 6.5f, -1}, .4f, "Sortierbrücke", 2);
    k.Zone({-26, 0, 14.2f}, {-10, 4.2f, 27}, .3f, "Retouren & Abholung", 1);
    k.Zone({14.2f, 0, 14.2f}, {26, 4.2f, 27}, .4f, "Leitstelle", 1);
    k.Zone({26.3f, 0, -21}, {32, 6, 13}, 1, "Verladung", 1);
    k.Zone({-25, 0, -21}, {25, 3.2f, -9}, .35f, "Paketlager", 2);
    for (float x : {-4.5f, -1.5f, 1.5f, 4.5f}) for (float z : {32.0f, 34.5f, 37.0f, 39.5f})
      w.hunterSpawns.push_back({{x, 0, z}, 0});
    for (float x : {-2.0f, 0.0f, 2.0f, 10.7f}) for (float z : {-18.0f, -12.0f, 2.0f, 9.5f})
      w.propSpawns.push_back({{x, 0, z}, PI});
    w.cameraPath = {{-8, 8, 41}, {0, 4, 8}, {1, 2.4f, 10}, {-6, 1.8f, -10},
                    {-18, 5, -2}, {12, 1.8f, -14}, {-10.5f, 2, -19}, {-20, 1.6f, -16},
                    {-18, 2.1f, 25}, {-23, 1.1f, 19}, {17, 2, 25}, {23, 1.3f, 17},
                    {29, 3, 16}, {37, 2, -4}, {33.5f, 2.7f, -5.5f}, {41, 2, -5.5f}};
  }
};
}

void BuildPaketzentrum(World& w) { props::BuildCatalog(); ParcelCenter(w).Build(); }
