#include "game/Bot.h"

bool BotManager::FollowDrop(const Player& p,Brain& b,GameMode mode,UserCmd& cmd) {
  auto finish=[&]() {
    b.dropStage=0;b.path.clear();b.hasGoal=false;b.repath=0;b.wantJump=false;
  };
  if(!b.dropStage) return false;
  b.dropTimer+=TICK_DT;
  b.errYaw=b.errPitch=0;
  // Use exactly the movement commands that were tested by SimulateBotDrop.
  cmd.buttons=0;b.pitch=-.15f;
  if(b.dropStage==1) {
    if(b.dropTimer>8 || fabsf(p.ms.pos.y-b.drop.takeoff.y)>.7f) {finish();return false;}
    float distance=DistanceXZ(p.ms.pos,b.drop.takeoff);
    if(distance>1.1f) {
      Steer(p,b,FollowPath(p,b),true,cmd,false);
      return true;
    }
    Vec3 delta=b.drop.takeoff-p.ms.pos;delta.y=0;
    Vec3 direction=Normalize(delta);
    float speed=Length(Vec3(p.ms.vel.x,0,p.ms.vel.z));
    if(distance<.10f && speed<.12f && p.ms.onGround && !(p.ms.lastButtons&BTN_JUMP)) {
      BotDropPlan checked=b.drop;
      if(!SimulateBotDrop(*world_,nav_,p.ms,mode,checked)) {finish();return false;}
      int planned=nav_.Nearest(b.drop.landing,.45f),actual=nav_.Nearest(checked.landing,.45f);
      if(fabsf(checked.landing.y-b.drop.landing.y)>.3f || nav_.Component(planned)!=nav_.Component(actual)) {finish();return false;}
      b.drop=std::move(checked);b.dropStage=2;b.dropTimer=0;b.dropAirborne=false;
      b.yaw=b.drop.yaw;cmd.buttons=BTN_FORWARD|(b.drop.jump ? BTN_JUMP : 0);
      return true;
    }
    // Stop precisely before the edge. Never teleport the body to its launch marker.
    if(distance>.06f && Dot(p.ms.vel,direction)*.15f<distance-.06f) {
      b.yaw=atan2f(-direction.x,-direction.z);cmd.buttons=BTN_FORWARD;
    }
    return true;
  }
  b.yaw=b.drop.yaw;
  if(b.dropStage==2) {
    b.dropAirborne|=!p.ms.onGround;
    if(b.dropAirborne && p.ms.onGround && b.drop.takeoff.y-p.ms.pos.y>1.2f) {b.dropStage=3;b.dropTimer=0;return true;}
    if(b.dropTimer>b.drop.seconds+.7f) {finish();return false;}
    cmd.buttons=BTN_FORWARD;
    return true;
  }
  // Bleed off momentum on the validated landing patch before resuming the chase.
  if(b.dropTimer>.6f) {finish();return false;}
  return true;
}
