#include "game/RoundReplay.h"
#include "game/Sim.h"

namespace {
constexpr int stride=6, middle=30, frames=61; // 10 samples/s, three seconds on each side
OBox Body(const ReplayActor& a) {
  MoveState m; m.pos=a.pos; m.colYaw=a.colYaw; m.crouching=a.crouch;
  return PlayerBox(m,a.type);
}
// Minimum separation of the two horizontal footprints. If they do not overlap,
// the nearest points must include a vertex of one of the rectangles.
float BodyGap(OBox a,OBox b) {
  float vertical=std::max(0.f,fabsf(a.c.y-b.c.y)-a.h.y-b.h.y);
  a.c.y=b.c.y=0; a.h.y=b.h.y=1;
  if (OverlapOBox(a,b,nullptr)) return vertical;
  float shortest=1e9f;
  auto corners=[&](const OBox& from,const OBox& to) {
    for (int x : {-1,1}) for (int z : {-1,1}) {
      Vec3 v=to.ToLocal(from.c+from.DirToWorld({x*from.h.x,0,z*from.h.z}));
      float dx=std::max(0.f,fabsf(v.x)-to.h.x), dz=std::max(0.f,fabsf(v.z)-to.h.z);
      shortest=std::min(shortest,dx*dx+dz*dz);
    }
  };
  corners(a,b); corners(b,a);
  return sqrtf(shortest+vertical*vertical);
}
}

ReplayActor BlendReplayActor(const ReplayActor& a,const ReplayActor& b,float t) {
  if (a.team!=b.team || a.type!=b.type || Distance(a.pos,b.pos)>3) return t<.5f ? a : b;
  ReplayActor p=b;
  p.pos=Lerp(a.pos,b.pos,t); p.yaw=LerpAngle(a.yaw,b.yaw,t);
  p.pitch=Lerp(a.pitch,b.pitch,t); p.colYaw=LerpAngle(a.colYaw,b.colYaw,t);
  return p;
}

void RoundReplay::Reset() {
  recent_.clear(); best_.clear(); distance_=4; hunter_=prop_=-1;
}

void RoundReplay::Record(const Sim& sim) {
  if (sim.tick%stride) return;
  recent_.emplace_back();
  ReplayScene& now=recent_.back();
  for (int i=0;i<MAX_PLAYERS;++i) {
    const Player& p=sim.players[i];
    if (!p.active || !p.alive) continue;
    now.actors[i]={p.team,p.ms.pos,p.ms.yaw,p.ms.pitch,p.ms.colYaw,p.propType,p.weapon,p.ms.crouching,p.shotsFired};
  }
  for (const Decoy& d : sim.decoys) if (d.active) now.decoys.push_back({d.pos,d.yaw,d.propType});
  if (recent_.size()>frames) recent_.pop_front();
  if (recent_.size()<frames) return;
  const ReplayScene& moment=recent_[middle];
  for (int h=0;h<MAX_PLAYERS;++h) {
    const ReplayActor& hunter=moment.actors[h];
    if (hunter.team!=TEAM_HUNTERS || recent_.front().actors[h].shots!=now.actors[h].shots) continue;
    for (int p=0;p<MAX_PLAYERS;++p) {
      const ReplayActor& prop=moment.actors[p];
      if (prop.team!=TEAM_PROPS || prop.type<0 || Distance(hunter.pos,prop.pos)>7.f) continue;
      float gap=BodyGap(Body(hunter),Body(prop));
      if (gap>=distance_) continue;
      // Both participants survive the pass, and the hunter actually goes past the prop.
      if (Distance(recent_.front().actors[h].pos,now.actors[h].pos)<1.25f ||
          Distance(recent_.front().actors[h].pos,recent_.front().actors[p].pos)<Distance(hunter.pos,prop.pos)+.5f ||
          Distance(now.actors[h].pos,now.actors[p].pos)<Distance(hunter.pos,prop.pos)+.5f) continue;
      if (!sim.world->LineOfSight(hunter.pos+Vec3(0,hunter.crouch ? 1.f : 1.62f,0),Body(prop).c)) continue;
      bool whole=std::all_of(recent_.begin(),recent_.end(),[&](const ReplayScene& frame) {
        return frame.actors[h].team==TEAM_HUNTERS && frame.actors[p].team==TEAM_PROPS;
      });
      if (!whole) continue;
      distance_=gap; hunter_=h; prop_=p;
      best_.assign(recent_.begin(),recent_.end());
    }
  }
}

ReplayView RoundReplay::At(float elapsed) const {
  ReplayView out;
  if (best_.empty() || elapsed<0 || elapsed>=REPLAY_SECONDS) return out;
  float index=elapsed*TICK_RATE/stride;
  int lo=std::min((int)index,frames-2);
  out.hunter=hunter_; out.prop=prop_; out.distance=distance_; out.elapsed=elapsed;
  for (int i=0;i<MAX_PLAYERS;++i)
    out.scene.actors[i]=BlendReplayActor(best_[lo].actors[i],best_[lo+1].actors[i],index-lo);
  out.scene.decoys=best_[lo].decoys;
  return out;
}
