#include "game/NavGrid.h"
#include <queue>

void NavGrid::Build(const World& w) {
  nodes_.clear();
  origin_ = Vec3(w.bmin.x, 0, w.bmin.z);
  gw_ = std::max(1, (int)ceilf((w.bmax.x - w.bmin.x) / cell_));
  gh_ = std::max(1, (int)ceilf((w.bmax.z - w.bmin.z) / cell_));
  colStart_.assign((size_t)gw_ * gh_, 0);
  colCount_.assign((size_t)gw_ * gh_, 0);
  std::vector<int> idx;
  std::vector<float> tops;
  for (int cz = 0; cz < gh_; cz++) {
    for (int cx = 0; cx < gw_; cx++) {
      int col = ColIndex(cx, cz);
      colStart_[col] = (int)nodes_.size();
      float x = origin_.x + (cx + 0.5f) * cell_, z = origin_.z + (cz + 0.5f) * cell_;
      w.Query(Vec3(x - 0.45f, -50, z - 0.45f), Vec3(x + 0.45f, 35, z + 0.45f), idx);
      tops.clear();
      for (int i : idx) {
        const Collider& c = w.cols[i];
        if (!(c.flags & COL_PLAYER)) continue;
        Vec3 l = c.box.ToLocal(Vec3(x, c.box.c.y, z));
        if (fabsf(l.x) > c.box.h.x || fabsf(l.z) > c.box.h.z) continue;
        float t = c.box.c.y + c.box.h.y;
        if (t > 30.0f) continue;
        tops.push_back(t);
      }
      std::sort(tops.begin(), tops.end());
      float lastAdded = -1e9f;
      for (float t : tops) {
        if (t - lastAdded < 0.1f) continue;
        // Fläche ist verschüttet, wenn direkt darüber ein anderer Körper liegt (z. B. Boden unter einer Stufe)
        bool buried = false;
        Vec3 probe(x, t + 0.02f, z);
        for (int i : idx) {
          const Collider& c = w.cols[i];
          if (!(c.flags & COL_PLAYER)) continue;
          Vec3 l = c.box.ToLocal(probe);
          if (fabsf(l.x) <= c.box.h.x && fabsf(l.y) <= c.box.h.y && fabsf(l.z) <= c.box.h.z) { buried = true; break; }
        }
        if (buried) continue;
        // Körper erst ab Stufenhöhe prüfen, sonst blockieren benachbarte Treppenstufen
        // Gleiche Breite wie PlayerBox; nur die begehbare Stufenhöhe aussparen.
        OBox body(Vec3(x, t + 1.125f, z), Vec3(0.32f, 0.675f, 0.32f));
        bool blocked = false;
        for (int i : idx) {
          const Collider& c = w.cols[i];
          if (!(c.flags & COL_PLAYER)) continue;
          if (OverlapOBox(body, c.box, nullptr)) { blocked = true; break; }
        }
        if (blocked) continue;
        nodes_.push_back({cx, cz, t});
        lastAdded = t;
      }
      colCount_[col] = (int)nodes_.size() - colStart_[col];
    }
  }
  // Zusammenhangskomponenten
  comp_.assign(nodes_.size(), -1);
  std::vector<int> sizes;
  std::vector<std::pair<int, float>> nb;
  for (int s = 0; s < (int)nodes_.size(); s++) {
    if (comp_[s] >= 0) continue;
    int id = (int)sizes.size();
    int count = 0;
    std::vector<int> stack = {s};
    comp_[s] = id;
    while (!stack.empty()) {
      int n = stack.back();
      stack.pop_back();
      count++;
      Neighbors(n, nb);
      for (auto& e : nb)
        if (comp_[e.first] < 0) { comp_[e.first] = id; stack.push_back(e.first); }
    }
    sizes.push_back(count);
  }
  mainComp_ = 0;
  for (int i = 1; i < (int)sizes.size(); i++)
    if (sizes[i] > sizes[mainComp_]) mainComp_ = i;
  LOG("Navigation: %d Knoten, %d Komponenten, Hauptkomponente %d", (int)nodes_.size(), (int)sizes.size(),
      sizes.empty() ? 0 : sizes[mainComp_]);
}

Vec3 NavGrid::NodePos(int n) const {
  const Node& nd = nodes_[n];
  return Vec3(origin_.x + (nd.cx + 0.5f) * cell_, nd.y, origin_.z + (nd.cz + 0.5f) * cell_);
}

int NavGrid::NodeAt(int cx, int cz, float y, float tol) const {
  if (cx < 0 || cz < 0 || cx >= gw_ || cz >= gh_) return -1;
  int col = ColIndex(cx, cz);
  int best = -1;
  float bd = tol;
  for (int i = 0; i < colCount_[col]; i++) {
    int n = colStart_[col] + i;
    float d = fabsf(nodes_[n].y - y);
    if (d <= bd) { bd = d; best = n; }
  }
  return best;
}

void NavGrid::Neighbors(int n, std::vector<std::pair<int, float>>& out) const {
  out.clear();
  const Node& a = nodes_[n];
  static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  static const int dz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  for (int d = 0; d < 8; d++) {
    int m = NodeAt(a.cx + dx[d], a.cz + dz[d], a.y, 0.55f);
    if (m < 0) continue;
    if (d >= 4) {
      if (NodeAt(a.cx + dx[d], a.cz, a.y, 0.55f) < 0 || NodeAt(a.cx, a.cz + dz[d], a.y, 0.55f) < 0) continue;
    }
    float cost = (d >= 4 ? 1.4142f : 1.0f) * cell_ + fabsf(nodes_[m].y - a.y) * 0.5f;
    out.push_back({m, cost});
  }
}

int NavGrid::Nearest(Vec3 p, float maxDist) const {
  if (nodes_.empty()) return -1;
  int cx = (int)floorf((p.x - origin_.x) / cell_), cz = (int)floorf((p.z - origin_.z) / cell_);
  int r = (int)ceilf(maxDist / cell_);
  int best = -1;
  float bs = 1e9f;
  for (int z = cz - r; z <= cz + r; z++)
    for (int x = cx - r; x <= cx + r; x++) {
      if (x < 0 || z < 0 || x >= gw_ || z >= gh_) continue;
      int col = ColIndex(x, z);
      for (int i = 0; i < colCount_[col]; i++) {
        int n = colStart_[col] + i;
        Vec3 np = NodePos(n);
        float dy = np.y - p.y;
        if (dy > 0.6f || dy < -3.0f) continue;
        float horizontal = DistanceXZ(np, p);
        if (horizontal > maxDist) continue;
        // Die richtige Etage hat Vorrang vor einer bequemeren Komponente am Boden.
        float s = horizontal + fabsf(dy) * 1.5f;
        if (fabsf(dy) > .6f) s += 10;
        if (comp_[n] != mainComp_) s += 5;
        if (s < bs) { bs = s; best = n; }
      }
    }
  return best;
}

bool NavGrid::WalkableLine(int a, int b) const {
  Vec3 pa = NodePos(a), pb = NodePos(b);
  float len = sqrtf(Sq(pb.x - pa.x) + Sq(pb.z - pa.z));
  int steps = std::max(1, (int)(len / (cell_ * 0.5f)));
  float y = pa.y;
  for (int i = 1; i <= steps; i++) {
    float t = (float)i / steps;
    float x = Lerp(pa.x, pb.x, t), z = Lerp(pa.z, pb.z, t);
    int cx = (int)floorf((x - origin_.x) / cell_), cz = (int)floorf((z - origin_.z) / cell_);
    // auch Nachbarzellen quer zur Linie müssen frei sein (Körperbreite)
    int n = NodeAt(cx, cz, y, 0.5f);
    if (n < 0) return false;
    float ny = nodes_[n].y;
    for (int ox = -1; ox <= 1; ox += 2) {
      if (NodeAt(cx + ox, cz, ny, 0.5f) < 0) return false;
      if (NodeAt(cx, cz + ox, ny, 0.5f) < 0) return false;
    }
    y = ny;
  }
  return fabsf(y - pb.y) < 0.6f;
}

bool NavGrid::FindPath(Vec3 from, Vec3 to, std::vector<Vec3>& out, int maxExpand) const {
  out.clear();
  int s = Nearest(from, 2.0f), g = Nearest(to, 3.0f);
  if (s < 0 || g < 0) return false;
  if (comp_[s] != comp_[g]) return false;
  struct QE {
    float f;
    int n;
    bool operator<(const QE& o) const { return f > o.f; }
  };
  std::vector<float> gs(nodes_.size(), 1e30f);
  std::vector<int> parent(nodes_.size(), -1);
  std::vector<u8> closed(nodes_.size(), 0);
  std::priority_queue<QE> open;
  Vec3 gp = NodePos(g);
  auto h = [&](int n) {
    Vec3 p = NodePos(n);
    float dx = fabsf(p.x - gp.x), dz = fabsf(p.z - gp.z);
    return ((std::max(dx, dz) + 0.4142f * std::min(dx, dz)) + fabsf(p.y - gp.y) * 0.5f) * 1.3f;  // leicht gewichtet: schneller
  };
  gs[s] = 0;
  open.push({h(s), s});
  std::vector<std::pair<int, float>> nb;
  int expanded = 0;
  bool found = false;
  while (!open.empty() && expanded < maxExpand) {
    QE cur = open.top();
    open.pop();
    if (closed[cur.n]) continue;
    if (cur.n == g) { found = true; break; }
    if (cur.f - h(cur.n) > gs[cur.n] + 1e-3f) continue;
    closed[cur.n] = 1;
    expanded++;
    Neighbors(cur.n, nb);
    for (auto& e : nb) {
      if (closed[e.first]) continue;
      float ng = gs[cur.n] + e.second;
      if (ng < gs[e.first]) {
        gs[e.first] = ng;
        parent[e.first] = cur.n;
        open.push({ng + h(e.first), e.first});
      }
    }
  }
  if (!found) return false;
  std::vector<int> raw;
  for (int n = g; n >= 0; n = parent[n]) raw.push_back(n);
  std::reverse(raw.begin(), raw.end());
  // Glätten
  size_t i = 0;
  out.push_back(NodePos(raw[0]));
  while (i + 1 < raw.size()) {
    size_t j = std::min(raw.size() - 1, i + 24);
    while (j > i + 1 && !WalkableLine(raw[i], raw[j])) j--;
    out.push_back(NodePos(raw[j]));
    i = j;
  }
  return true;
}

std::string NavGrid::DebugColumn(Vec3 p) const {
  int cx = (int)floorf((p.x - origin_.x) / cell_), cz = (int)floorf((p.z - origin_.z) / cell_);
  if (cx < 0 || cz < 0 || cx >= gw_ || cz >= gh_) return "-";
  int col = ColIndex(cx, cz);
  std::string s;
  for (int i = 0; i < colCount_[col]; i++) {
    int n = colStart_[col] + i;
    s += StrFormat("[%.2f c%d]", nodes_[n].y, comp_[n]);
  }
  return s.empty() ? "leer" : s;
}

int NavGrid::RandomNode(Rng& rng) const {
  if (nodes_.empty()) return -1;
  for (int tries = 0; tries < 200; tries++) {
    int n = rng.Int(0, (int)nodes_.size() - 1);
    if (comp_[n] == mainComp_) return n;
  }
  return -1;
}

int NavGrid::RandomNodeNear(Rng& rng, Vec3 p, float radius) const {
  for (int tries = 0; tries < 30; tries++) {
    float a = rng.Float() * TAU, r = sqrtf(rng.Float()) * radius;
    int n = Nearest(p + Vec3(cosf(a) * r, rng.Range(-0.5f, 0.5f), sinf(a) * r), 1.5f);
    if (n >= 0 && comp_[n] == mainComp_) return n;
  }
  return -1;
}

bool NavGrid::AnyReachableIn(Vec3 mn, Vec3 mx) const {
  int x0 = std::max(0, (int)floorf((mn.x - origin_.x) / cell_)), x1 = std::min(gw_ - 1, (int)floorf((mx.x - origin_.x) / cell_));
  int z0 = std::max(0, (int)floorf((mn.z - origin_.z) / cell_)), z1 = std::min(gh_ - 1, (int)floorf((mx.z - origin_.z) / cell_));
  for (int z = z0; z <= z1; z++)
    for (int x = x0; x <= x1; x++) {
      int col = ColIndex(x, z);
      for (int i = 0; i < colCount_[col]; i++) {
        int n = colStart_[col] + i;
        if (comp_[n] == mainComp_ && nodes_[n].y >= mn.y - 0.05f && nodes_[n].y <= mx.y) return true;
      }
    }
  return false;
}
