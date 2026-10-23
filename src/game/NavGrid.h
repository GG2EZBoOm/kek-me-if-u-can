#pragma once
// Mehrstöckiges Navigationsgitter für Bots (Spalten mit mehreren begehbaren Höhen) und A*-Suche.
#include "game/World.h"

class NavGrid {
 public:
  void Build(const World& w);
  bool Ready() const { return !nodes_.empty(); }
  // nächster Knoten zu einer Position (-1 wenn keiner in der Nähe)
  int Nearest(Vec3 p, float maxDist = 2.0f) const;
  Vec3 NodePos(int n) const;
  bool FindPath(Vec3 from, Vec3 to, std::vector<Vec3>& out, int maxExpand = 360000) const;
  int RandomNode(Rng& rng) const;
  // zufälliger Knoten in Reichweite eines Punktes
  int RandomNodeNear(Rng& rng, Vec3 p, float radius) const;
  int NodeCount() const { return (int)nodes_.size(); }
  int Component(int n) const { return n >= 0 ? comp_[n] : -1; }
  int MainComponent() const { return mainComp_; }
  // Gibt es einen erreichbaren Knoten innerhalb der Box?
  bool AnyReachableIn(Vec3 mn, Vec3 mx) const;
  std::string DebugColumn(Vec3 p) const;

 private:
  struct Node {
    int cx, cz;
    float y;
  };
  int ColIndex(int cx, int cz) const { return cz * gw_ + cx; }
  void Neighbors(int n, std::vector<std::pair<int, float>>& out) const;
  bool WalkableLine(int a, int b) const;
  int NodeAt(int cx, int cz, float y, float tol) const;

  float cell_ = 0.25f;
  Vec3 origin_;
  int gw_ = 0, gh_ = 0;
  std::vector<Node> nodes_;
  std::vector<int> colStart_, colCount_;
  std::vector<int> comp_;
  int mainComp_ = 0;
};
