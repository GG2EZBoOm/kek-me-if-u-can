// Automatische Tests ohne Fenster: Bewegung, Navigation, komplettes Bot-Match, Netzwerk.
#include "engine/Upnp.h"
#include "game/Bot.h"
#include "game/Invite.h"
#include "game/Maps.h"
#include "game/NetGame.h"
#include "game/Taunts.h"
#include "game/Updater.h"
#include <chrono>
#include <map>
#include <tuple>
#include <thread>

static double Now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool Check(bool cond, const char* what, int& fails) {
  LOG("%s %s", cond ? "[OK]  " : "[FAIL]", what);
  if (!cond) fails++;
  return cond;
}

static void WalkTest(const World& w, Vec3 start, float yaw, float seconds, MoveState& s, int propType = -1, u16 extra = 0) {
  s = MoveState();
  s.pos = start;
  s.yaw = yaw;
  MoveEnv env;
  env.world = &w;
  UserCmd cmd;
  cmd.yaw = yaw;
  cmd.buttons = BTN_FORWARD | extra;
  int ticks = (int)(seconds * TICK_RATE);
  for (int i = 0; i < ticks; i++) ApplyMovement(env, s, cmd, propType, false);
}

static bool Check(bool cond, const std::string& what, int& fails) { return Check(cond, what.c_str(), fails); }

static float PathLength(const std::vector<Vec3>& p) {
  float len = 0;
  for (size_t i = 1; i < p.size(); i++) len += Distance(p[i - 1], p[i]);
  return len;
}

static std::string ZoneName(const World& w, Vec3 p) {
  const Zone* z = w.ZoneAt(p + Vec3(0, 0.5f, 0));
  return z ? z->name : "?";
}

// Jede Tür muss von beiden Seiten ohne Umweg passierbar sein
static void CheckDoors(const World& w, const NavGrid& nav, int& fails) {
  int bad = 0;
  for (const DoorInfo& d : w.doors) {
    Vec3 n = d.alongX ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
    float off = d.depth * 0.5f + 0.8f;
    Vec3 a = d.center - n * off + Vec3(0, 0.05f, 0), b = d.center + n * off + Vec3(0, 0.05f, 0);
    std::vector<Vec3> path;
    bool ok = nav.Nearest(a, 0.7f) >= 0 && nav.Nearest(b, 0.7f) >= 0 && nav.FindPath(a, b, path, 6000);
    if (ok && PathLength(path) > off * 2.0f + 3.0f) ok = false;
    if (!ok) {
      bad++;
      LOG("    Tür blockiert bei (%.1f %.1f %.1f), Breite %.1f: %s <-> %s", d.center.x, d.center.y, d.center.z, d.width,
          ZoneName(w, a).c_str(), ZoneName(w, b).c_str());
    }
  }
  Check(bad == 0, StrFormat("%d Türen passierbar (%d blockiert)", (int)w.doors.size() - bad, bad), fails);
}

// Z-Fighting: Dreiecke in (fast) derselben Ebene, gleich ausgerichtet, überlappend, aber verschieden aussehend
// (Farbe oder Material) flackern, weil der Tiefenpuffer sie nicht sicher trennen kann – aus der Entfernung schon
// bei wenigen Millimetern Abstand.
static float Cross2(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

// Schnittfläche zweier gegen den Uhrzeigersinn orientierter Dreiecke (Sutherland-Hodgman), dazu ihr Schwerpunkt
static float OverlapArea(const Vec2 a[3], const Vec2 b[3], Vec2* centroid) {
  std::vector<Vec2> poly(a, a + 3);
  for (int e = 0; e < 3 && !poly.empty(); e++) {
    Vec2 p0 = b[e], p1 = b[(e + 1) % 3];
    std::vector<Vec2> out;
    for (size_t i = 0; i < poly.size(); i++) {
      Vec2 p = poly[i], q = poly[(i + 1) % poly.size()];
      float sp = Cross2(p1 - p0, p - p0), sq = Cross2(p1 - p0, q - p0);
      if (sp >= 0) out.push_back(p);
      if ((sp >= 0) != (sq >= 0)) out.push_back(p + (q - p) * (sp / (sp - sq)));
    }
    poly.swap(out);
  }
  float area = 0;
  Vec2 c;
  for (size_t i = 0; i < poly.size(); i++) {
    float cr = Cross2(poly[i], poly[(i + 1) % poly.size()]);
    area += cr;
    c += (poly[i] + poly[(i + 1) % poly.size()]) * cr;
  }
  if (centroid && fabsf(area) > 1e-9f) *centroid = c / (3.0f * area);
  return fabsf(area) * 0.5f;
}

static void CheckZFighting(const World& w, int& fails) {
  const int kTolMm = 5;  // so nah beieinander flackern Flächen aus der Entfernung
  const MeshData& m = w.opaque;
  struct Tri {
    Vec2 p[3];
    float mnU, mxU, mnV, mxV;
    u32 look;  // Farbe + Material
    Vec3 u, v, n;
    float d;
    int group = 0;
    Vec3 ps[3];
  };
  struct Key {
    int nx, ny, nz, d;
    bool operator<(const Key& o) const { return std::tie(nx, ny, nz, d) < std::tie(o.nx, o.ny, o.nz, o.d); }
  };
  std::map<Key, std::vector<Tri>> groups;
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
    const Vertex& a = m.verts[m.indices[i]];
    const Vertex& b = m.verts[m.indices[i + 1]];
    const Vertex& c = m.verts[m.indices[i + 2]];
    Vec3 cr = Cross(b.pos - a.pos, c.pos - a.pos);
    if (Length(cr) < 1e-5f) continue;
    Tri t;
    t.n = Normalize(cr);
    t.d = Dot(t.n, a.pos);
    Key k{(int)lroundf(t.n.x * 200), (int)lroundf(t.n.y * 200), (int)lroundf(t.n.z * 200), (int)lroundf(t.d * 1000)};
    t.ps[0] = a.pos;
    t.ps[1] = b.pos;
    t.ps[2] = c.pos;
    t.look = (u32)a.color[0] | (u32)a.color[1] << 8 | (u32)a.color[2] << 16 | (u32)a.aux[0] << 24;
    groups[k].push_back(t);
  }
  // Unsichtbar: Stelle steckt in einer festen Wand/im Boden, oder Unterseite knapp über dem Boden (keine Kamera kommt darunter)
  // Projektion in die gemeinsame Ebene der Gruppe (gerundete Normale)
  for (auto& g : groups) {
    Vec3 nq = Normalize(Vec3((float)g.first.nx, (float)g.first.ny, (float)g.first.nz));
    Vec3 up = fabsf(nq.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 u = Normalize(Cross(up, nq)), v = Cross(nq, u);
    for (Tri& t : g.second) {
      t.u = u;
      t.v = v;
      for (int j = 0; j < 3; j++) t.p[j] = Vec2(Dot(t.ps[j], u), Dot(t.ps[j], v));
      // Projektion kann die Umlaufrichtung nicht umkehren (gleiche Halbebene), sicherheitshalber prüfen
      if (Cross2(t.p[1] - t.p[0], t.p[2] - t.p[0]) < 0) std::swap(t.p[1], t.p[2]);
      t.mnU = std::min({t.p[0].x, t.p[1].x, t.p[2].x});
      t.mxU = std::max({t.p[0].x, t.p[1].x, t.p[2].x});
      t.mnV = std::min({t.p[0].y, t.p[1].y, t.p[2].y});
      t.mxV = std::max({t.p[0].y, t.p[1].y, t.p[2].y});
    }
  }
  std::vector<int> ids;
  bool dump = getenv("KEK_ZF_DUMP") != nullptr;
  auto hidden = [&](Vec3 p, Vec3 n) {
    if (n.y < -0.5f && p.y < 0.35f) return true;
    Vec3 q = p + n * 0.03f;
    w.Query(q - Vec3(0.01f, 0.01f, 0.01f), q + Vec3(0.01f, 0.01f, 0.01f), ids);
    for (int id : ids) {
      const Collider& c = w.cols[id];
      if (!(c.flags & COL_BULLET) || (c.flags & COL_GLASS) || c.prop >= 0) continue;
      Vec3 l = c.box.ToLocal(q);
      if (fabsf(l.x) < c.box.h.x && fabsf(l.y) < c.box.h.y && fabsf(l.z) < c.box.h.z) return true;
    }
    return false;
  };
  std::vector<Vec3> sites;
  std::vector<std::string> info;
  int pairs = 0;
  // Sweep über die nach mnU sortierten Dreiecke; mixed = nur Paare aus verschiedenen Gruppen (Nachbarebenen)
  auto sweep = [&](std::vector<const Tri*> list, bool mixed) {
    std::sort(list.begin(), list.end(), [](const Tri* a, const Tri* b) { return a->mnU < b->mnU; });
    for (size_t i = 0; i < list.size(); i++)
      for (size_t j = i + 1; j < list.size() && list[j]->mnU < list[i]->mxU; j++) {
        const Tri& x = *list[i];
        const Tri& y = *list[j];
        if (mixed && x.group == y.group) continue;
        if (x.look == y.look || x.mxV <= y.mnV || y.mxV <= x.mnV) continue;
        Vec2 c2;
        if (OverlapArea(x.p, y.p, &c2) < 0.0004f) continue;  // mindestens 2 × 2 cm
        Vec3 c = x.u * c2.x + x.v * c2.y + x.n * x.d;
        if (hidden(c, x.n)) continue;
        pairs++;
        bool known = false;
        for (const Vec3& s : sites) known |= Length(s - c) < 1.5f;
        if (!known) {
          if (dump)
            LOG("      A (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f) | B (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", x.ps[0].x,
                x.ps[0].y, x.ps[0].z, x.ps[1].x, x.ps[1].y, x.ps[1].z, x.ps[2].x, x.ps[2].y, x.ps[2].z, y.ps[0].x, y.ps[0].y, y.ps[0].z,
                y.ps[1].x, y.ps[1].y, y.ps[1].z, y.ps[2].x, y.ps[2].y, y.ps[2].z);
          sites.push_back(c);
          info.push_back(StrFormat("n=(%.0f,%.0f,%.0f) %06x/m%d vs %06x/m%d, Abstand %.1f mm", x.n.x * 100, x.n.y * 100, x.n.z * 100,
                                   x.look & 0xffffff, x.look >> 24, y.look & 0xffffff, y.look >> 24, fabsf(x.d - y.d) * 1000));
        }
      }
  };
  for (auto it = groups.begin(); it != groups.end(); ++it) {
    std::vector<const Tri*> list;
    for (Tri& t : it->second) {
      t.group = 0;
      list.push_back(&t);
    }
    sweep(list, false);
    for (int dd = 1; dd <= kTolMm; dd++) {
      Key nk = it->first;
      nk.d += dd;
      auto nb = groups.find(nk);
      if (nb == groups.end()) continue;
      std::vector<const Tri*> both = list;
      for (Tri& t : nb->second) {
        t.group = dd;
        both.push_back(&t);
      }
      sweep(both, true);
    }
  }
  for (size_t i = 0; i < sites.size() && i < 400; i++)
    LOG("    Z-Fighting bei (%.2f, %.2f, %.2f) %s – %s", sites[i].x, sites[i].y, sites[i].z, info[i].c_str(),
        w.ZoneAt(sites[i]) ? w.ZoneAt(sites[i])->name.c_str() : "");
  Check(sites.empty(), StrFormat("Keine flackernden Flächen (%d Stellen, %d Dreieckspaare)", (int)sites.size(), pairs), fails);
}

// Kein Versteck im Blattwerk: An keiner begehbaren Stelle darf man in Büschen oder Hecken stecken, die nur Optik
// sind (keine Kollision) – dort wäre ein Prop für Jäger unsichtbar. Innen/außen per senkrechtem Strahl (Parität).
static void CheckFoliage(const World& w, const NavGrid& nav, int& fails) {
  const MeshData& m = w.opaque;
  struct Leaf {
    Vec3 a, b, c;
  };
  std::vector<Leaf> leaves;
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
    const Vertex& a = m.verts[m.indices[i]];
    if (a.aux[0] != MAT_LEAVES) continue;
    leaves.push_back({a.pos, m.verts[m.indices[i + 1]].pos, m.verts[m.indices[i + 2]].pos});
  }
  if (leaves.empty()) {
    Check(true, "Kein Versteck im Blattwerk (kein Blattwerk)", fails);
    return;
  }
  // Raster 1 m über XZ
  Vec3 mn(1e9f, 0, 1e9f), mx(-1e9f, 0, -1e9f);
  for (const Leaf& l : leaves)
    for (Vec3 p : {l.a, l.b, l.c}) {
      mn.x = std::min(mn.x, p.x);
      mn.z = std::min(mn.z, p.z);
      mx.x = std::max(mx.x, p.x);
      mx.z = std::max(mx.z, p.z);
    }
  int gw = (int)(mx.x - mn.x) + 1, gh = (int)(mx.z - mn.z) + 1;
  std::vector<std::vector<int>> cells((size_t)gw * gh);
  for (int i = 0; i < (int)leaves.size(); i++) {
    const Leaf& l = leaves[i];
    int x0 = (int)(std::min({l.a.x, l.b.x, l.c.x}) - mn.x), x1 = (int)(std::max({l.a.x, l.b.x, l.c.x}) - mn.x);
    int z0 = (int)(std::min({l.a.z, l.b.z, l.c.z}) - mn.z), z1 = (int)(std::max({l.a.z, l.b.z, l.c.z}) - mn.z);
    for (int z = z0; z <= z1; z++)
      for (int x = x0; x <= x1; x++) cells[(size_t)z * gw + x].push_back(i);
  }
  auto inside = [&](Vec3 p) {
    int cx = (int)(p.x - mn.x), cz = (int)(p.z - mn.z);
    if (p.x < mn.x || p.z < mn.z || cx >= gw || cz >= gh) return false;
    int crossings = 0;
    for (int i : cells[(size_t)cz * gw + cx]) {
      const Leaf& l = leaves[i];
      // Liegt (x, z) im Dreieck (XZ-Projektion)? Dann Höhe der Ebene dort mit p.y vergleichen
      float d = (l.b.z - l.c.z) * (l.a.x - l.c.x) + (l.c.x - l.b.x) * (l.a.z - l.c.z);
      if (fabsf(d) < 1e-9f) continue;
      float u = ((l.b.z - l.c.z) * (p.x - l.c.x) + (l.c.x - l.b.x) * (p.z - l.c.z)) / d;
      float v = ((l.c.z - l.a.z) * (p.x - l.c.x) + (l.a.x - l.c.x) * (p.z - l.c.z)) / d;
      if (u < 0 || v < 0 || u + v > 1) continue;
      float y = l.a.y * u + l.b.y * v + l.c.y * (1 - u - v);
      if (y > p.y) crossings++;
    }
    return crossings % 2 == 1;
  };
  std::vector<Vec3> sites;
  for (int n = 0; n < nav.NodeCount(); n++) {
    if (nav.Component(n) != nav.MainComponent()) continue;
    Vec3 p = nav.NodePos(n);
    if (!inside(p + Vec3(0, 0.25f, 0)) && !inside(p + Vec3(0, 0.9f, 0))) continue;
    bool known = false;
    for (const Vec3& s : sites) known |= Length(s - p) < 3.0f;
    if (!known) sites.push_back(p);
  }
  for (size_t i = 0; i < sites.size() && i < 20; i++)
    LOG("    Versteck im Blattwerk bei (%.1f, %.1f, %.1f) – %s", sites[i].x, sites[i].y, sites[i].z,
        w.ZoneAt(sites[i]) ? w.ZoneAt(sites[i])->name.c_str() : "");
  Check(sites.empty(), StrFormat("Kein Versteck im Blattwerk (%d Stellen)", (int)sites.size()), fails);
}

// Jeder benannte Bereich muss erreichbar sein
static void CheckZones(const World& w, const NavGrid& nav, int& fails) {
  int bad = 0;
  for (const Zone& z : w.zones) {
    bool ok = nav.AnyReachableIn(z.mn, z.mx);
    if (!ok) {
      bad++;
      LOG("    Bereich nicht erreichbar: %s", z.name.c_str());
    }
  }
  Check(bad == 0, StrFormat("%d Bereiche erreichbar (%d nicht)", (int)w.zones.size() - bad, bad), fails);
}

static void CheckPaths(const NavGrid& nav, const std::vector<std::tuple<Vec3, Vec3, const char*>>& tests, int& fails) {
  for (const auto& t : tests) {
    std::vector<Vec3> path;
    double p0 = Now();
    bool ok = nav.FindPath(std::get<0>(t), std::get<1>(t), path);
    LOG("  Pfad %-42s %s  %.1f m, %.1f ms", std::get<2>(t), ok ? "ok" : "FEHLT", PathLength(path), (Now() - p0) * 1000);
    Check(ok, std::get<2>(t), fails);
  }
}

static void BundestagTests(const World& w, const NavGrid& nav, int& fails) {
  MoveState s;
  WalkTest(w, Vec3(-27.5f, 0.05f, 0), -PI * 0.5f, 6.0f, s);
  Check(fabsf(s.pos.y - 5.0f) < 0.1f, "Große Treppe hinauf bis auf den Balkon (y=5)", fails);
  Check(s.pos.x > -16.3f && s.pos.x < -15.4f, "Stopp an der Ringwand (keine Durchdringung)", fails);
  WalkTest(w, Vec3(-20, 0.05f, -12.5f), PI * 0.5f, 4.0f, s);
  Check(s.pos.x > -29.0f && s.pos.x < -28.4f, "Außenwand blockiert", fails);
  WalkTest(w, Vec3(-27.5f, 0.05f, 0.6f), -PI * 0.5f, 7.0f, s, props::Find("kaffeebecher"));
  Check(s.pos.y > 4.5f, "Kaffeebecher kommt die Treppe hoch", fails);
  WalkTest(w, Vec3(0, 12.0f, -16.0f), 0, 1.5f, s);
  Check(fabsf(s.pos.y - 10.0f) < 0.1f, "Landung auf der Dachterrasse", fails);
  WalkTest(w, Vec3(-10.5f, 0.05f, 0.0f), -PI * 0.5f, 6.0f, s);
  Check(s.pos.y < -0.5f, "Stufen im Plenarsaal hinab", fails);
  // Seiteneingang durch den Metalldetektor (Torbogen)
  WalkTest(w, Vec3(-28.6f, 0.05f, 5.0f), -PI * 0.5f, 3.0f, s);
  Check(s.pos.x > -24.5f, "Durch den Metalldetektor am Seiteneingang", fails);
  CheckPaths(nav,
             {{Vec3(-70, -1.2f, 0), Vec3(-22, 0, 8), "Platz -> Eingangshalle"},
              {Vec3(-22, 0, 8), Vec3(0, 10, 16), "Eingangshalle -> Dachterrasse (Turm)"},
              {Vec3(-70, -1.2f, 0), Vec3(0, 16, 4.5f), "Platz -> Kuppel-Plattform"},
              {Vec3(-22, 0, 8), Vec3(6.4f, -1.62f, 2.0f), "Eingangshalle -> Rednerpult"},
              {Vec3(-22, 0, -8), Vec3(14.5f, 5, -13.5f), "Eingangshalle -> Sitzungssaal (1. OG)"},
              {Vec3(20, 0, -8), Vec3(22, 10, 0), "Kantine -> Dachrestaurant"}},
             fails);
}

static void OktoberfestTests(const World& w, const NavGrid& nav, int& fails) {
  MoveState s;
  WalkTest(w, Vec3(26.9f, 0.05f, 5.6f), PI, 4.0f, s);
  Check(fabsf(s.pos.y - 3.0f) < 0.1f, "Treppe hinauf auf die Empore (y=3)", fails);
  WalkTest(w, Vec3(6.2f, 0.05f, 0), PI * 0.5f, 2.5f, s);
  Check(fabsf(s.pos.y - 1.0f) < 0.1f, "Stufen auf das Musikpodium", fails);
  WalkTest(w, Vec3(0, 0.05f, 24.0f), 0, 4.0f, s);
  Check(s.pos.z < 15.0f, "Durch den Haupteingang ins Zelt", fails);
  WalkTest(w, Vec3(-7.2f, 0.05f, 50.0f), PI, 3.0f, s);
  Check(fabsf(s.pos.y - 0.25f) < 0.05f, "Auf die Autoscooter-Fläche", fails);
  CheckPaths(nav,
             {{Vec3(55, 0, 34), Vec3(0, 0, 5.2f), "Jäger-Start -> Festzelt"},
              {Vec3(0, 0, 5.2f), Vec3(-15, 3.0f, -14), "Festzelt -> Empore"},
              {Vec3(0, 0, 5.2f), Vec3(26.2f, 0, 0), "Festzelt -> hinter die Schänke"},
              {Vec3(0, 0, 5.2f), Vec3(-22, 0, 14.5f + 1.0f), "Festzelt -> Boxen"},
              {Vec3(55, 0, 34), Vec3(-50, 0, 42.5f), "Straße -> in den Lebkuchenstand"},
              {Vec3(55, 0, 34), Vec3(-2, 0.25f, 64), "Straße -> Autoscooter"},
              {Vec3(55, 0, 34), Vec3(-36, 0.2f, 68), "Straße -> Riesenrad-Plattform"},
              {Vec3(0, 0, 5.2f), Vec3(10, 0, -25), "Festzelt -> Hinterhof"}},
             fails);
}

static void BotMatch(World& w, BotManager& bots, int rounds, int& fails) {
  Sim sim;
  MatchSettings ms;
  ms.rounds = rounds;
  ms.hideTime = 25;
  ms.roundTime = 150;
  ms.botSkill = 1;
  sim.Init(&w, ms);
  Rng nr(3);
  for (int i = 0; i < 10; i++) sim.AddPlayer(BotManager::RandomName(nr, sim), true);
  int kills = 0, penalties = 0, taunts = 0, disguises = 0, shots = 0, decoys = 0, hits = 0;
  int propWins = 0, hunterWins = 0;
  double m0 = Now();
  int ticks = 0;
  int maxTicks = (int)((ms.hideTime + ms.roundTime + 10) * rounds * TICK_RATE) + 600;
  bool matchEnded = false;
  while (ticks < maxTicks && !matchEnded) {
    bots.Think(sim);
    sim.Tick();
    bots.OnEvents(sim, sim.events);
    for (const GameEvent& e : sim.events) {
      switch (e.type) {
        case EV_KILL: kills++; break;
        case EV_PENALTY: penalties++; break;
        case EV_TAUNT: taunts++; break;
        case EV_DISGUISE: disguises++; break;
        case EV_SHOT: shots++; break;
        case EV_DECOY: decoys++; break;
        case EV_HIT: hits++; break;
        case EV_ROUND_END:
          if (e.b == TEAM_PROPS) propWins++;
          else hunterWins++;
          LOG("  Runde %d: %s gewinnen (%.0f s, Props übrig: %d)", sim.round, e.b == TEAM_PROPS ? "Props" : "Jäger",
              ms.roundTime - std::max(0.0f, sim.phaseTimer), sim.CountAlive(TEAM_PROPS));
          break;
        case EV_PHASE:
          if (e.b == (u8)Phase::MatchEnd) matchEnded = true;
          if (e.b == (u8)Phase::Hunting) {
            std::string where;
            for (const Player& p : sim.players)
              if (p.active && p.alive && p.team == TEAM_PROPS)
                where += " " + props::Get(p.propType).name + "@" + ZoneName(w, p.ms.pos) + ";";
            LOG("  Verstecke (%d/%d versteckt):%s", bots.CountHiddenProps(sim), sim.CountAlive(TEAM_PROPS), where.c_str());
          }
          break;
      }
    }
    sim.events.clear();
    ticks++;
  }
  double dt = Now() - m0;
  LOG("  Match: %.0f s Spielzeit in %.2f s (%.0fx Echtzeit) | Verwandlungen %d, Taunts %d, Köder %d, Schüsse %d, Treffer %d, Strafen %d, Kills %d",
      ticks / 60.0, dt, ticks / 60.0 / dt, disguises, taunts, decoys, shots, hits, penalties, kills);
  LOG("  Siege: Props %d, Jäger %d", propWins, hunterWins);
  Check(disguises >= rounds * 3, "Bots verwandeln sich", fails);
  Check(shots > 0, "Jäger-Bots schießen", fails);
  Check(propWins + hunterWins == rounds, "Alle Runden beendet", fails);
}

// Einladungscodes: Hin- und Rückweg, Tippvarianten, Fehlererkennung, Adressen in eingefügtem Text
static void InviteTests(int& fails) {
  Rng rng(4711);
  int bad = 0, withL = 0;
  for (int i = 0; i < 20000; i++) {
    u32 ip = rng.Next() | 1;
    u16 port = (u16)(1 + rng.Next() % 65535);
    std::string code = EncodeInvite(ip, port);
    withL += code.find('L') != std::string::npos;
    u32 ip2 = 0;
    u16 port2 = 0;
    if (!DecodeInvite(code, ip2, port2) || ip2 != ip || port2 != port) bad++;
  }
  Check(bad == 0, StrFormat("Einladungscodes hin und zurück (%d Fehler, %d Codes mit L)", bad, withL), fails);
  std::string code = EncodeInvite((203u << 24) | (0u << 16) | (113u << 8) | 7u, 27015);
  std::string lower = code;
  for (char& c : lower) c = (char)tolower((unsigned char)c);
  std::string spaced = code;
  for (char& c : spaced)
    if (c == '-') c = ' ';
  Check(ResolveJoinInput(code) == "203.0.113.7:27015", "Code wird zur Adresse aufgelöst", fails);
  Check(ResolveJoinInput("  " + lower + " ") == "203.0.113.7:27015", "Code in Kleinbuchstaben", fails);
  Check(ResolveJoinInput(spaced) == "203.0.113.7:27015", "Code mit Leerzeichen statt Strichen", fails);
  Check(ResolveJoinInput(InviteMessage("203.0.113.7:27015", code, "Oktoberfest")) == "203.0.113.7:27015",
        "Komplette Discord-Nachricht eingefügt", fails);
  Check(ResolveJoinInput("Komm rein: 192.168.0.42:27016 !") == "192.168.0.42:27016", "IPv4 mit Port aus Text", fails);
  Check(ResolveJoinInput(" 10.0.0.5 ") == "10.0.0.5", "Reine IPv4 bleibt", fails);
  std::string broken = code;
  broken[5] = broken[5] == '2' ? '3' : '2';
  u32 bip;
  u16 bport;
  int caught = 0;
  for (char c : std::string("23456789ABCDEFGHJKLMNPQRSTUVWXYZ")) {
    broken[5] = c;
    if (broken != code && (!DecodeInvite(broken, bip, bport) || bip != ((203u << 24) | (0u << 16) | (113u << 8) | 7u))) caught++;
  }
  Check(caught >= 31, StrFormat("Tippfehler werden erkannt oder ändern die Adresse (%d/31)", caught), fails);
  Check(upnp::IsPrivateIPv4("192.168.1.2") && upnp::IsPrivateIPv4("100.70.1.1") && !upnp::IsPrivateIPv4("203.0.113.7"),
        "Private und öffentliche IPv4 unterschieden", fails);
}

// Updater: Versionsvergleich, Release-Antwort von GitHub, SHA-256
static void UpdaterTests(int& fails) {
  using update::CompareVersions;
  Check(CompareVersions("1.0.1", "1.0.0") == 1 && CompareVersions("v1.0.0", "1.0") == 0 && CompareVersions("1.9.0", "1.10.0") == -1 &&
            CompareVersions("2", "1.99.99") == 1,
        "Versionsvergleich (v-Präfix, fehlende Stellen, zweistellige Nummern)", fails);
  const char* json = R"({"tag_name":"v1.2.3","name":"Kek \u00e4 \ud83c\udf7a","draft":false,"body":"Neu:\n- Dosen","assets":[
    {"name":"KekMeIfYouCan.exe","size":1435136,"digest":"sha256:ABCDEF01","browser_download_url":"https://github.com/GG2EZBoOm/kek-me-if-u-can/releases/download/v1.2.3/KekMeIfYouCan.exe","uploader":{"login":"x","id":1}},
    {"name":"KekMeIfYouCanCLI.exe","size":12,"digest":null,"browser_download_url":"https://github.com/GG2EZBoOm/kek-me-if-u-can/releases/download/v1.2.3/KekMeIfYouCanCLI.exe"},
    {"name":"Fremd.exe","size":5,"browser_download_url":"https://evil.example/KekMeIfYouCan.exe"}]})";
  update::Release r;
  std::string err;
  bool ok = update::ParseRelease(json, r, err);
  Check(ok && r.version == "1.2.3" && r.assets.size() == 2, "Release-Antwort gelesen, fremde Download-Adresse verworfen", fails);
  Check(ok && r.assets.size() == 2 && r.assets[0].sha256 == "ABCDEF01" && r.assets[0].size == 1435136 && r.assets[1].sha256.empty(),
        "Prüfsumme und Größe der Dateien übernommen", fails);
  Check(!update::ParseRelease("{\"tag_name\":\"v1\",", r, err) && !update::ParseRelease("{\"tag_name\":\"nightly\"}", r, err),
        "Kaputte Antwort und Tag ohne Version abgelehnt", fails);
  Check(update::Sha256Hex("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 (Testvektor \"abc\")",
        fails);
}

int RunTests(int matchRounds) {
  int fails = 0;
  InviteTests(fails);
  UpdaterTests(fails);
  props::BuildCatalog();
  for (int m = 0; m < (int)AllMaps().size(); m++) {
    World w;
    double t0 = Now();
    BuildMap(m, w);
    LOG("=== Karte %s: gebaut in %.0f ms, %d Türen, %d Bereiche ===", w.name.c_str(), (Now() - t0) * 1000, (int)w.doors.size(),
        (int)w.zones.size());
    BotManager bots;
    bots.Init(w);
    CheckDoors(w, bots.Nav(), fails);
    CheckZones(w, bots.Nav(), fails);
    CheckZFighting(w, fails);
    CheckFoliage(w, bots.Nav(), fails);
    if (w.id == "bundestag") BundestagTests(w, bots.Nav(), fails);
    if (w.id == "oktoberfest") OktoberfestTests(w, bots.Nav(), fails);
    if (matchRounds > 0) BotMatch(w, bots, matchRounds, fails);
  }
  LOG("Tests beendet: %d Fehler", fails);
  return fails;
}
// Netzwerk über Loopback: Server mit Bots + ein Client, der läuft und schießt
int RunNetTest() {
  int fails = 0;
  props::BuildCatalog();
  MatchSettings ms;
  ms.bots = 5;
  ms.hideTime = 3;
  ms.roundTime = 60;
  LocalSession host;
  host.SetDedicated(true);
  if (!Check(host.Start(ms, "", true, "Testserver"), "Server startet auf Port 27015", fails)) return fails;
  NetClient client;
  Check(client.Connect("127.0.0.1", "Tester"), "Client öffnet Verbindung", fails);
  int frame = 0;
  client.makeCmd = [&]() {
    UserCmd c;
    c.yaw = frame * 0.01f;
    c.buttons = BTN_FORWARD | ((frame / 30) % 2 ? BTN_FIRE : 0);
    return c;
  };
  double t0 = Now();
  int events = 0;
  float maxErr = 0;
  for (frame = 0; frame < 60 * 12; frame++) {
    host.Update(TICK_DT);
    client.Update(TICK_DT);
    std::vector<GameEvent> evs;
    client.TakeEvents(evs);
    events += (int)evs.size();
    if (client.Ready() && frame > 240) {
      int id = client.LocalId();
      const Player& sp = host.GetSim().players[id];
      // Vorhersage gegenüber Server (Server liegt ~1 Tick + Latenz zurück, daher grobe Schranke)
      float err = DistanceXZ(sp.ms.pos, client.LocalMove()->pos);
      maxErr = std::max(maxErr, err);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
  LOG("Netztest: %.2f s, Client bereit=%d, Slot=%d, Ereignisse=%d, max. Abweichung Vorhersage=%.2f m", Now() - t0,
      client.Ready(), client.LocalId(), events, maxErr);
  LOG("  %s | %s", host.ServerInfo().c_str(), client.ServerInfo().c_str());
  Check(client.Ready(), "Client verbunden und Snapshots empfangen", fails);
  int seen = 0;
  for (const ClientPlayer& p : client.State().players)
    if (p.active) seen++;
  LOG("  Client sieht %d Spieler (Server: %d)", seen, host.GetSim().CountActive());
  Check(seen == host.GetSim().CountActive(), "Alle Spieler im Snapshot", fails);
  Check(events > 0, "Ereignisse kommen an", fails);
  Check(maxErr < 1.5f, "Vorhersage bleibt nah am Server", fails);
  bool named = client.State().players[client.LocalId()].name == std::string("Tester");
  Check(named, "Spielername übertragen", fails);
  LOG("Netztest beendet: %d Fehler", fails);
  return fails;
}