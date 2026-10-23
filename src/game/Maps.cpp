#include "game/Maps.h"

const std::vector<MapInfo>& AllMaps() {
  static const std::vector<MapInfo> maps = {
      {"bundestag", "Bundestag",
       "Reichstagsgebäude in Berlin: Plenarsaal, Glaskuppel mit Spiralrampen, Fraktionssäle, Kantine, Dachterrasse "
       "und der Platz der Republik an der Spree.",
       BuildBundestag},
      {"oktoberfest", "Oktoberfest",
       "Festzelt auf der Theresienwiese: Bierbänke, Musikpodium, Empore mit Boxen, Ausschank und Hendlküche – draußen "
       "Biergarten, Wirtsbudenstraße, Riesenrad, Kettenflieger und Autoscooter.",
       BuildOktoberfest},
      {"nudelfabrik", "Nudelfabrik",
       "Alte Backsteinfabrik: Produktionshalle mit Rührwerken, Nudelpresse und Spaghetti-Straße, Laufstege, Trockenraum, "
       "Mehllager, Verpackung mit Lkw an der Rampe – draußen Mehlsilos, Werkshof und Parkplatz.",
       BuildNudelfabrik},
      {"paketzentrum", "KEK-Paketzentrum",
       "Gelb-rote Sortierhalle mit laufenden Paketbändern, begehbarer Sortierbrücke, Paketregalen, Retourenschalter, "
       "Leitstelle und drei begehbaren Lkw-Wechselbrücken. Zugestellt? Angeblich.",
       BuildPaketzentrum},
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
  w.Finalize();
  return true;
}
