#include "game/Maps.h"
#include "game/MapDetails.h"

const std::vector<MapInfo>& AllMaps() {
  static const std::vector<MapInfo> maps = {
      {"bundestag", "Bundestag",
       "Reichstagsgebäude in Berlin: Plenarsaal, Glaskuppel mit Spiralrampen, Fraktionssäle, Kantine, Dachterrasse "
       "und der Platz der Republik mit Besucherforum und bepflanzten Versteckinseln.",
       BuildBundestag},
      {"oktoberfest", "Oktoberfest",
       "Festzelt auf der Theresienwiese: Bierbänke, Musikpodium, Empore mit Boxen, Ausschank und Hendlküche – draußen "
       "Biergarten, zweistöckige Almhütte mit Balkon, Wirtsbudenstraße, Fundbüro, Riesenrad, Kettenflieger und Autoscooter.",
       BuildOktoberfest},
      {"nudelfabrik", "Nudelfabrik",
       "Alte Backsteinfabrik: Produktionshalle mit Rührwerken, Nudelpresse und Spaghetti-Straße, Laufstege, Trockenraum, "
       "Mehllager, Verpackung und Lkw-Rampe – draußen Versandlager mit Galerie, Kesselhaus mit zwei Aufgängen, Leergutcontainer und Pausenhof.",
       BuildNudelfabrik},
      {"paketzentrum", "KEK-Paketzentrum",
       "Gelb-rote Sortierhalle mit laufenden Paketbändern, begehbarer Sortierbrücke, Paketregalen, Retourenschalter, "
       "Leitstelle und drei begehbaren Lkw-Wechselbrücken. Pausendeck und Dachterrasse verbindet eine Hofbrücke; dazu Expresshalle, Containerhof und Paketstation.",
       BuildPaketzentrum},
      {"zwinger", "Dresdner Zwinger",
       "Barocker Innenhof mit vier Brunnen, Blumengärten, goldenem Kronentor und Bogengalerien mit vier Terrassenaufgängen. "
       "Zweistöckige Sammlung, Orangerie-Café und Restaurierungshof ergänzen Porzellan, Orangenkübel und Nymphenbad.",
       BuildZwinger},
  };
  return maps;
}

int FindMap(const std::string& id) {
  const auto& m = AllMaps();
  for (int i = 0; i < (int)m.size(); i++)
    if (id == m[i].id) return i;
  return -1;
}

bool BuildMap(int index, World& w) {
  const auto& m = AllMaps();
  if (index < 0 || index >= (int)m.size()) return false;
  w.Clear();
  w.id = m[index].id;
  w.name = m[index].name;
  w.description = m[index].description;
  m[index].build(w);
  RefineMap(w);
  w.Finalize();
  return true;
}
