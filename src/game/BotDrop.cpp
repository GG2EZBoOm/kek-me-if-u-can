#include "game/BotDrop.h"

namespace {
float PathLength(const std::vector<Vec3>& path) {
  float length=0;for(size_t i=1;i<path.size();i++) length+=Distance(path[i-1],path[i]);return length;
}
}

bool SimulateBotDrop(const World& world,const NavGrid& nav,const MoveState& start,
                    GameMode mode,BotDropPlan& plan) {
  MoveEnv env;env.world=&world;env.mode=mode;
  MoveState s=start;
  if(!s.onGround || BoxBlocked(env,PlayerBox(s,-1),.025f)) return false;
  bool airborne=false;
  Vec3 direction(-sinf(plan.yaw),0,-cosf(plan.yaw));
  for(int tick=0;tick<180;tick++) {
    UserCmd cmd;cmd.yaw=plan.yaw;cmd.buttons=BTN_FORWARD;
    if(tick==0 && plan.jump) cmd.buttons|=BTN_JUMP;
    ApplyMovement(env,s,cmd,-1,false);
    airborne|=!s.onGround;
    if(BoxBlocked(env,PlayerBox(s,-1),.025f)) return false;
    if(airborne && s.onGround) {
      float drop=start.pos.y-s.pos.y;
      // A normal jump can briefly touch the top of a rail before stepping off it.
      // Continue the same forward command instead of treating that as the final landing.
      if(drop<1.2f) continue;
      if(drop>20 || Dot(s.pos-start.pos,direction)<.8f) return false;
      int n=nav.Nearest(s.pos,.45f);
      if(n<0 || fabsf(nav.NodePos(n).y-s.pos.y)>.2f) return false;
      // Brake on the actual landing surface; a narrow ledge is not a safe destination.
      Vec3 touchdown=s.pos;
      for(int stop=0;stop<35;stop++) {
        UserCmd brake;brake.yaw=plan.yaw;ApplyMovement(env,s,brake,-1,false);
        if(!s.onGround || fabsf(s.pos.y-touchdown.y)>.3f) return false;
      }
      n=nav.Nearest(s.pos,.45f);
      if(n<0 || fabsf(nav.NodePos(n).y-s.pos.y)>.2f) return false;
      plan.landing=s.pos;plan.seconds=(tick+1)*TICK_DT;return true;
    }
    if(!airborne && tick>35 && DistanceXZ(s.pos,start.pos)<.4f) return false;
    if(s.pos.y<world.bmin.y-2) return false;
  }
  return false;
}

bool FindBotDrop(const World& world,const NavGrid& nav,Vec3 from,Vec3 target,
                 GameMode mode,BotDropPlan& plan) {
  int goal=nav.Below(target);
  int source=nav.Nearest(from,.6f);
  if(source<0 || goal<0) return false;
  Vec3 destination=nav.NodePos(goal);
  if(from.y-destination.y<1.2f) return false;
  std::vector<Vec3> regular;
  float best=nav.FindPath(from,destination,regular) ? PathLength(regular) : 1e9f;
  bool found=false;int simulations=0;
  Vec3 toward=Normalize(Vec3(destination.x-from.x,0,destination.z-from.z));
  if(LengthSq(toward)<.1f) toward={1,0,0};
  const float angles[]={0,.25f,-.25f,.5f,-.5f,.85f,-.85f,1.2f,-1.2f,1.57f,-1.57f,PI};
  for(float angle:angles) {
    Vec3 direction=RotateY(toward,angle);
    float lastEdge=-10;
    for(float r=.5f;r<=14;r+=.5f) {
      Vec3 edge=from+direction*r;
      int at=nav.Nearest(edge,.32f);
      if(at>=0 && fabsf(nav.NodePos(at).y-from.y)<.6f) continue;
      if(r-lastEdge<2) continue;
      lastEdge=r;
      bool lower=false;
      for(float ahead : {1.5f,3.0f,5.0f}) {
        int below=nav.Below(edge+direction*ahead);
        if(below>=0 && from.y-nav.NodePos(below).y>1.2f && nav.Component(below)==nav.Component(goal)) lower=true;
      }
      if(!lower) continue;
      for(float back : {.25f,.375f,.5f,.625f,.75f,.875f,1.0f,1.125f,1.25f,1.375f,1.5f}) {
        Vec3 launch=edge-direction*back;
        int n=nav.Nearest(launch,.32f);
        if(n<0 || fabsf(nav.NodePos(n).y-from.y)>.6f || nav.Component(n)!=nav.Component(source)) continue;
        // Sub-cell takeoff placement matters when clearing a railing near the apex.
        launch.y=nav.NodePos(n).y;
        for(bool jump : {false,true}) {
          if(++simulations>120) return found;
          BotDropPlan candidate;candidate.takeoff=launch;candidate.yaw=atan2f(-direction.x,-direction.z);candidate.jump=jump;
          MoveState initial;initial.pos=launch+Vec3(0,.01f,0);initial.onGround=true;
          if(!SimulateBotDrop(world,nav,initial,mode,candidate)) continue;
          int landing=nav.Nearest(candidate.landing,.45f);
          if(landing<0 || nav.Component(landing)!=nav.Component(goal)) continue;
          float lowerBound=DistanceXZ(from,launch)+DistanceXZ(candidate.landing,destination)+candidate.seconds*4.6f+1;
          if(lowerBound>=best) continue;
          std::vector<Vec3> after;
          if(!nav.FindPath(candidate.landing,destination,after) || !nav.FindPath(from,launch,candidate.approach)) continue;
          float cost=PathLength(candidate.approach)+PathLength(after)+candidate.seconds*4.6f+1;
          if(cost>=best) continue;
          best=cost;plan=std::move(candidate);found=true;
          // A short approach in the target's direction already gives a useful pursuit exit.
          if(PathLength(plan.approach)<5) return true;
        }
      }
    }
  }
  return found;
}
