#include "game/Movement.h"

MoveConfig MoveConfigFor(int propType) {
  MoveConfig c;
  if (propType >= 0) {
    c.maxSpeed = 4.4f;
    c.sprintMul = 1.42f;
    c.jumpVel = 6.9f;
  }
  return c;
}

static Vec3 PropHalf(int propType) {
  const PropDef& d = props::Get(propType);
  return Max(d.colHalf, Vec3(0.07f, 0.06f, 0.07f));
}

OBox PlayerBox(const MoveState& s, int propType) {
  if (propType < 0) {
    float hy = s.crouching ? 0.6f : 0.9f;
    return OBox(s.pos + Vec3(0, hy, 0), Vec3(0.32f, hy, 0.32f), 0);
  }
  const PropDef& d = props::Get(propType);
  Vec3 h = PropHalf(propType);
  Vec3 off(d.colCenter.x, h.y, d.colCenter.z);
  return OBox(s.pos + RotateY(off, s.colYaw), h, s.colYaw);
}

float EyeHeight(const MoveState& s, int propType) {
  if (propType < 0) return s.crouching ? 1.0f : 1.62f;
  return PropHalf(propType).y * 2.0f;
}

Vec3 EyePos(const MoveState& s, int propType) { return s.pos + Vec3(0, EyeHeight(s, propType), 0); }

Vec3 PropPivot(const MoveState& s, int propType) {
  float top = propType < 0 ? 1.7f : PropHalf(propType).y * 2.0f;
  return s.pos + Vec3(0, top + 0.35f, 0);
}

float PropCamDistance(int propType) {
  if (propType < 0) return 2.6f;
  Vec3 h = PropHalf(propType);
  return Clamp(2.2f + 1.3f * std::max(h.x, std::max(h.y, h.z)), 2.2f, 6.0f);
}

Vec3 ThirdPersonCamera(const World& w, Vec3 pivot, float yaw, float pitch, float dist) {
  Vec3 dir = DirFromYawPitch(yaw, pitch);
  RayHit h = w.Raycast(pivot, -dir, dist + 0.3f, COL_PLAYER, false);
  float d = dist;
  if (h.hit) d = std::max(0.25f, h.t - 0.25f);
  return pivot - dir * d;
}

namespace {

struct Candidates {
  std::vector<const OBox*> boxes;
};

void Gather(const MoveEnv& env, Vec3 mn, Vec3 mx, Candidates& out) {
  static thread_local std::vector<int> idx;
  out.boxes.clear();
  env.world->Query(mn, mx, idx);
  for (int i : idx) {
    const Collider& c = env.world->cols[i];
    if (c.flags & COL_PLAYER) out.boxes.push_back(&c.box);
  }
  if (env.others) {
    for (const OBox& b : *env.others) {
      Vec3 bmn, bmx;
      b.Bounds(bmn, bmx);
      if (bmx.x < mn.x || bmn.x > mx.x || bmx.y < mn.y || bmn.y > mx.y || bmx.z < mn.z || bmn.z > mx.z) continue;
      out.boxes.push_back(&b);
    }
  }
}

OBox Shrink(const OBox& b, float lift) {
  OBox r = b;
  r.c.y += lift * 0.5f;
  r.h.y = std::max(0.01f, r.h.y - lift * 0.5f);
  r.h.x = std::max(0.01f, r.h.x - 0.004f);
  r.h.z = std::max(0.01f, r.h.z - 0.004f);
  return r;
}

bool BlockedBy(const Candidates& c, const OBox& b) {
  for (const OBox* o : c.boxes)
    if (OverlapOBox(b, *o, nullptr)) return true;
  return false;
}

}  // namespace

bool BoxBlocked(const MoveEnv& env, const OBox& b, float lift) {
  Vec3 mn, mx;
  b.Bounds(mn, mx);
  Candidates c;
  Gather(env, mn - Vec3(0.1f), mx + Vec3(0.1f), c);
  return BlockedBy(c, Shrink(b, lift));
}

void UpdatePropRotation(const MoveEnv& env, MoveState& s, int propType) {
  if (propType < 0 || s.rotLock) return;
  float target = WrapAngle(s.yaw + PI);
  float diff = AngleDiff(s.colYaw, target);
  if (fabsf(diff) < 0.0005f) return;
  float tries[3] = {diff, Clamp(diff, -0.25f, 0.25f), Clamp(diff, -0.08f, 0.08f)};
  for (float step : tries) {
    MoveState t = s;
    t.colYaw = WrapAngle(s.colYaw + step);
    if (!BoxBlocked(env, PlayerBox(t, propType), 0.05f)) {
      s.colYaw = t.colYaw;
      return;
    }
  }
}

bool FindFreeSpot(const MoveEnv& env, MoveState& s, int propType) {
  if (!BoxBlocked(env, PlayerBox(s, propType), 0.02f)) return true;
  for (float up = 0.05f; up <= 0.6f; up += 0.05f) {
    MoveState t = s;
    t.pos.y += up;
    if (!BoxBlocked(env, PlayerBox(t, propType), 0.0f)) { s = t; return true; }
  }
  for (float r = 0.15f; r <= 1.5f; r += 0.15f) {
    for (int i = 0; i < 12; i++) {
      float a = TAU * i / 12;
      MoveState t = s;
      t.pos += Vec3(cosf(a) * r, 0.02f, sinf(a) * r);
      if (!BoxBlocked(env, PlayerBox(t, propType), 0.0f)) { s = t; return true; }
    }
  }
  return false;
}

bool ApplyMovement(const MoveEnv& env, MoveState& s, const UserCmd& cmd, int propType, bool frozen) {
  if (propType >= 0 && !frozen && (cmd.buttons & BTN_LOCK) && !(s.lastButtons & BTN_LOCK)) s.rotLock = !s.rotLock;
  bool ground = s.onGround;
  PlayerMove(env, s, cmd, propType, frozen, TICK_DT);
  if (propType >= 0) UpdatePropRotation(env, s, propType);
  return ground && !s.onGround && s.vel.y > 3.0f;
}

void PlayerMove(const MoveEnv& env, MoveState& s, const UserCmd& cmd, int propType, bool frozen, float dt) {
  MoveConfig cfg = MoveConfigFor(propType);
  s.yaw = cmd.yaw;
  s.pitch = Clamp(cmd.pitch, -1.55f, 1.55f);
  u16 b = frozen ? 0 : cmd.buttons;

  if (propType < 0) {
    bool want = (b & BTN_CROUCH) != 0;
    if (want) s.crouching = true;
    else if (s.crouching) {
      MoveState t = s;
      t.crouching = false;
      if (!BoxBlocked(env, PlayerBox(t, propType), 0.02f)) s.crouching = false;
    }
  } else {
    s.crouching = false;
  }

  Vec3 fwd(-sinf(s.yaw), 0, -cosf(s.yaw)), right(cosf(s.yaw), 0, -sinf(s.yaw));
  float fm = ((b & BTN_FORWARD) ? 1.0f : 0.0f) - ((b & BTN_BACK) ? 1.0f : 0.0f);
  float sm = ((b & BTN_RIGHT) ? 1.0f : 0.0f) - ((b & BTN_LEFT) ? 1.0f : 0.0f);
  Vec3 wish = fwd * fm + right * sm;
  float wl = Length(wish);
  if (wl > 0) wish = wish / wl;
  bool sprint = (b & BTN_SPRINT) && fm > 0 && s.stamina > 0.02f && !s.crouching;
  float speed = cfg.maxSpeed * (sprint ? cfg.sprintMul : 1.0f) * (s.crouching ? cfg.crouchMul : 1.0f);
  if (sprint && wl > 0) s.stamina = std::max(0.0f, s.stamina - cfg.staminaDrain * dt);
  else s.stamina = std::min(1.0f, s.stamina + cfg.staminaRegen * dt);

  if (s.onGround) {
    float sp = sqrtf(s.vel.x * s.vel.x + s.vel.z * s.vel.z);
    if (sp > 0.0001f) {
      float drop = std::max(sp, cfg.stopSpeed) * cfg.friction * dt;
      float ns = std::max(sp - drop, 0.0f);
      s.vel.x *= ns / sp;
      s.vel.z *= ns / sp;
    }
  }
  if (wl > 0) {
    float cur = s.vel.x * wish.x + s.vel.z * wish.z;
    float add = speed - cur;
    if (add > 0) {
      float acc = (s.onGround ? cfg.accel : cfg.airAccel) * speed * dt;
      if (acc > add) acc = add;
      s.vel.x += wish.x * acc;
      s.vel.z += wish.z * acc;
    }
  }
  bool jumped = false;
  if ((b & BTN_JUMP) && !(s.lastButtons & BTN_JUMP) && s.onGround) {
    s.vel.y = cfg.jumpVel * (s.crouching ? 0.85f : 1.0f);
    s.onGround = false;
    jumped = true;
  }
  s.lastButtons = b;
  s.vel.y -= cfg.gravity * dt;
  if (s.vel.y < -40.0f) s.vel.y = -40.0f;

  bool wasGround = s.onGround;
  s.onGround = false;
  Vec3 delta = s.vel * dt;
  OBox startBox = PlayerBox(s, propType);
  Vec3 mn, mx;
  startBox.Bounds(mn, mx);
  Vec3 mn2 = Min(mn, mn + delta), mx2 = Max(mx, mx + delta);
  Candidates cand;
  Gather(env, mn2 - Vec3(0.6f, cfg.stepHeight + 0.3f, 0.6f), mx2 + Vec3(0.6f, 0.6f, 0.6f), cand);

  int steps = std::max(1, (int)ceilf(Length(delta) / 0.08f));
  Vec3 sd = delta / (float)steps;
  for (int st = 0; st < steps; st++) {
    s.pos += sd;
    for (int iter = 0; iter < 5; iter++) {
      bool any = false;
      for (const OBox* o : cand.boxes) {
        OBox box = PlayerBox(s, propType);
        Vec3 mtv;
        if (!OverlapOBox(box, *o, &mtv)) continue;
        any = true;
        float top = o->c.y + o->h.y;
        float feet = box.c.y - box.h.y;
        bool horiz = fabsf(mtv.y) < 1e-6f;
        if (horiz && top - feet > 0 && top - feet <= cfg.stepHeight && (wasGround || s.onGround) && s.vel.y <= 0.5f) {
          MoveState t = s;
          t.pos.y += (top - feet) + 0.002f;
          if (!BlockedBy(cand, Shrink(PlayerBox(t, propType), 0.0f))) {
            s.pos = t.pos;
            s.onGround = true;
            if (s.vel.y < 0) s.vel.y = 0;
            continue;
          }
        }
        s.pos += mtv;
        Vec3 n = Normalize(mtv);
        float vn = Dot(s.vel, n);
        if (vn < 0) s.vel -= n * vn;
        if (n.y > 0.7f) s.onGround = true;
      }
      if (!any) break;
    }
  }

  // Beim Treppab-Laufen am Boden bleiben
  if (!s.onGround && wasGround && !jumped && s.vel.y <= 0) {
    OBox box = PlayerBox(s, propType);
    float feet = box.c.y - box.h.y;
    OBox probe = box;
    probe.c.y = feet - cfg.stepHeight * 0.5f;
    probe.h.y = cfg.stepHeight * 0.5f;
    probe.h.x -= 0.01f;
    probe.h.z -= 0.01f;
    float best = -1e9f;
    for (const OBox* o : cand.boxes) {
      if (!OverlapOBox(probe, *o, nullptr)) continue;
      float top = o->c.y + o->h.y;
      if (top <= feet + 0.01f) best = std::max(best, top);
    }
    if (best > -1e8f) {
      s.pos.y += best - feet;
      s.onGround = true;
      s.vel.y = 0;
    }
  }
}
