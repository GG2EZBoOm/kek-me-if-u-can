#include "game/WeaponModels.h"
#include <array>

namespace {
void Paint(MeshBuilder& b,Material mat,u32 color) { b.Mat(mat); b.ColorHex(color); }
struct Section { float z, width, bottom, top; };
void ShapedBody(MeshBuilder& b,std::initializer_list<Section> sections) {
  std::vector<std::array<Vec3,8>> rings;
  for(auto s:sections) {
    float bevel=std::min(s.width*.3f,(s.top-s.bottom)*.2f);
    rings.push_back({Vec3(-s.width+bevel,s.bottom,s.z),Vec3(s.width-bevel,s.bottom,s.z),
      Vec3(s.width,s.bottom+bevel,s.z),Vec3(s.width,s.top-bevel,s.z),Vec3(s.width-bevel,s.top,s.z),
      Vec3(-s.width+bevel,s.top,s.z),Vec3(-s.width,s.top-bevel,s.z),Vec3(-s.width,s.bottom+bevel,s.z)});
  }
  for(size_t k=1;k<rings.size();k++) for(int i=0;i<8;i++) {
    int j=(i+1)%8; b.Quad(rings[k-1][i],rings[k-1][j],rings[k][j],rings[k][i]);
  }
  for(int i=1;i<7;i++) { b.Tri(rings.front()[0],rings.front()[i+1],rings.front()[i]); b.Tri(rings.back()[0],rings.back()[i],rings.back()[i+1]); }
}
void TubeZ(MeshBuilder& b,Vec3 c,float outer,float inner,float length,int n=24) {
  for(int i=0;i<n;i++) {
    float a=i*TAU/n,d=(i+1)*TAU/n;
    Vec3 v(cosf(a),sinf(a),0),u(cosf(d),sinf(d),0),z(0,0,length*.5f);
    b.Quad(c+v*outer-z,c+u*outer-z,c+u*outer+z,c+v*outer+z);
    b.Quad(c+u*inner-z,c+v*inner-z,c+v*inner+z,c+u*inner+z);
    b.Quad(c+v*inner+z,c+v*outer+z,c+u*outer+z,c+u*inner+z);
    b.Quad(c+u*inner-z,c+u*outer-z,c+v*outer-z,c+v*inner-z);
  }
}
void Rail(MeshBuilder& b,float z0,float z1,float y) {
  Paint(b,MAT_METAL,0x353b41);
  b.Box({-.012f,y,z0},{.012f,y+.006f,z1});
  for(float z=z0;z<z1-.009f;z+=.017f) b.Box({-.015f,y+.006f,z},{.015f,y+.012f,z+.009f});
}
void Grip(MeshBuilder& b,bool wood) {
  Paint(b,wood?MAT_WOOD:MAT_PLASTIC,wood?0x754e31:0x303337);
  b.Push(Translation({0,-.025f,.035f})*RotationX(-.25f));
  b.Cylinder({0,-.12f,0},.022f,.12f,8);
  Paint(b,MAT_LEATHER,0x1c2022);
  for(int i=0;i<6;i++) b.Box({-.019f,-.11f+i*.014f,.018f},{.019f,-.107f+i*.014f,.022f});
  b.Pop();
  Paint(b,MAT_METAL,0x353b40);
  b.Box({-.005f,-.069f,-.052f},{.005f,-.063f,.025f});
  b.Box({-.005f,-.069f,-.055f},{.005f,-.02f,-.049f});
  b.Push(RotationX(-.20f)); b.Box({-.003f,-.055f,-.025f},{.003f,-.018f,-.019f}); b.Pop();
}
void PistolFrame(MeshBuilder& b) {
  Paint(b,MAT_PLASTIC,0x30383c);
  ShapedBody(b,{{-.146f,.018f,-.017f,.014f},{-.130f,.024f,-.020f,.014f},
    {.043f,.024f,-.020f,.014f},{.065f,.018f,-.013f,.014f}});
  // Angled grip, with the magazine seated inside it.
  b.Push(Translation({0,-.018f,.029f})*RotationX(-.22f));
  ShapedBody(b,{{-.025f,.017f,-.112f,0},{-.018f,.023f,-.114f,0},
    {.025f,.023f,-.114f,0},{.033f,.015f,-.107f,-.003f}});
  b.Pop();
  // Open trigger guard and a separate trigger blade/safety.
  Paint(b,MAT_PLASTIC,0x20282d);
  b.Box({-.009f,-.062f,-.072f},{.009f,-.055f,.013f});
  b.Box({-.009f,-.055f,-.072f},{.009f,-.012f,-.065f});
  Paint(b,MAT_METAL,0x4a535b);
  b.Push(RotationX(-.24f)); b.Box({-.004f,-.051f,-.030f},{.004f,-.012f,-.024f}); b.Pop();
  Paint(b,MAT_PLASTIC,0x13191c); b.Box({-.0015f,-.044f,-.033f},{.0015f,-.025f,-.029f});
  Paint(b,MAT_METAL,0x68737a);
  for(float x:{-.025f,.025f}) for(float z:{-.043f,.020f}) b.CylinderX({x,-.005f,z},.003f,.002f,12);
  Paint(b,MAT_METAL,0x181e23);
  b.Box({-.029f,-.006f,-.014f},{-.025f,.003f,.021f}); // slide stop
  b.Box({-.026f,-.038f,.003f},{-.023f,-.027f,.014f}); // magazine release
  for(int i=0;i<3;i++) b.Box({-.020f,-.023f,-.125f+i*.015f},{.020f,-.017f,-.119f+i*.015f});
}
void PistolSlide(MeshBuilder& b) {
  Paint(b,MAT_METAL,0x555f68);
  ShapedBody(b,{{-.167f,.017f,.012f,.044f},{-.159f,.023f,.010f,.051f},
    {.052f,.023f,.010f,.051f},{.061f,.018f,.015f,.043f}});
  Paint(b,MAT_PLAIN,0x11181e);
  b.Box({.0228f,.021f,-.047f},{.0234f,.044f,-.004f});
  b.Box({.005f,.0508f,-.047f},{.022f,.0514f,-.004f});
  Paint(b,MAT_METAL,0x78858e);
  b.Box({.0235f,.023f,-.045f},{.024f,.040f,-.007f}); // chamber visible through port
  Paint(b,MAT_PLAIN,0x222c34);
  for(float x:{-.0233f,.0233f}) for(int i=0;i<7;i++) {
    float z=.005f+i*.0065f;
    b.Box({x-.0005f,.017f,z},{x+.0005f,.043f,z+.0025f});
  }
  Paint(b,MAT_METAL,0x171e25);
  b.Box({-.006f,.050f,-.148f},{.006f,.062f,-.138f});
  Paint(b,MAT_PLAIN,0xe2e6dc); b.Box({-.0017f,.059f,-.1379f},{.0017f,.062f,-.1374f});
}
void PistolOptic(MeshBuilder& b,int n) {
  Paint(b,MAT_METAL,0x171e25);
  if(n==0) {
    b.Box({-.019f,.050f,.037f},{-.0045f,.065f,.049f});
    b.Box({.0045f,.050f,.037f},{.019f,.065f,.049f});
    Paint(b,MAT_PLAIN,0xe2e6dc);
    for(float x:{-.011f,.011f}) b.Box({x-.002f,.059f,.0491f},{x+.002f,.063f,.0496f});
    return;
  }
  b.Box({-.021f,.050f,-.016f},{.021f,.059f,.032f});
  b.Box({-.022f,.057f,-.008f},{-.018f,.105f,.001f});
  b.Box({.018f,.057f,-.008f},{.022f,.105f,.001f});
  b.Box({-.022f,.102f,-.008f},{.022f,.107f,.001f});
  b.CylinderX({.024f,.064f,.017f},.006f,.005f,12);
  Paint(b,MAT_PLAIN,0xff463a); b.Emissive(.9f);
  b.Sphere({0,.084f,.002f},.0015f,10,6); b.Emissive(0);
}
void Body(MeshBuilder& b,int w) {
  if(w==WPN_PISTOL) { PistolFrame(b); return; }
  Paint(b,MAT_METAL,0x41484e);
  ShapedBody(b,{{-.20f,.026f,-.014f,.041f},{-.185f,.031f,-.014f,.050f},{.060f,.031f,-.014f,.050f},{.075f,.025f,-.014f,.040f}});
  Paint(b,MAT_PLASTIC,0x262b2e);
  b.Box({-.031f,-.037f,-.165f},{.031f,-.013f,.068f});
  // Separate ejection port, bolt, retaining pins, selector and receiver seam.
  Paint(b,MAT_PLAIN,0x070a0d);
  b.Box({.0303f,.002f,-.112f},{.031f,.029f,-.025f});
  b.Box({-.0313f,-.017f,-.16f},{-.031f,-.015f,.063f});
  Paint(b,MAT_METAL,0x7c858c);
  for(float x:{-.032f,.032f}) for(float z:{-.145f,.045f}) b.CylinderX({x,-.004f,z},.0045f,.003f,12);
  Paint(b,MAT_METAL,0x1e2225);
  b.CylinderX({-.033f,-.008f,.018f},.009f,.006f,16);
  b.Box({-.038f,-.011f,-.006f},{-.034f,-.005f,.022f});
  Paint(b,MAT_PLAIN,0xb4bec3);
  for(int i=0;i<5;i++) b.Box({-.032f,.007f,-.104f+i*.009f},{-.0315f,.011f,-.101f+i*.009f});
  Grip(b,w==WPN_SHOTGUN);
  Rail(b,-.19f,.063f,.049f);
  if(w==WPN_SMG) {
    Paint(b,MAT_METAL,0x343a40);
    b.Box({-.026f,-.052f,-.14f},{.026f,-.035f,-.08f});
  } else {
    Paint(b,MAT_PLAIN,0x080b0c);
    b.Box({-.016f,-.039f,-.132f},{.016f,-.037f,-.054f});
  }
}
void Optic(MeshBuilder& b,int n) {
  Paint(b,MAT_METAL,0x272e34);
  if(n==0) {
    b.Box({-.020f,.061f,.032f},{-.012f,.079f,.050f});
    b.Box({.012f,.061f,.032f},{.020f,.079f,.050f});
    b.Box({-.020f,.060f,.032f},{.020f,.065f,.050f});
    return;
  }
  b.Box({-.018f,.061f,-.072f},{.018f,.071f,.025f});
  b.CylinderX({.023f,.067f,-.016f},.009f,.014f,12);
  if(n==1 || n==2) {
    float wide=n==1?.025f:.035f, z=n==1?-.028f:-.018f, thick=n==1?.006f:.012f;
    b.Box({-wide,.071f,z-thick},{-wide+.005f,.130f,z+thick});
    b.Box({wide-.005f,.071f,z-thick},{wide,.130f,z+thick});
    b.Box({-wide,.126f,z-thick},{wide,.133f,z+thick});
    b.Box({-wide,.071f,z-thick},{wide,.077f,z+thick});
    Paint(b,MAT_PLASTIC,0x12191e);
    b.Box({-wide,.070f,z-.028f},{wide,.082f,z-.008f});
    // Open aperture, no opaque painted lens blocking the view. Reticle sits on the sight axis.
    Paint(b,MAT_PLAIN,n==1?0xff463a:0xffb149); b.Emissive(.9f);
    b.Sphere({0,.105f,z+.001f},.0021f,10,6);
    if(n==2) TubeZ(b,{0,.105f,z+.001f},.010f,.0092f,.0007f,32);
    b.Emissive(0);
  } else {
    b.Box({-.014f,.070f,-.072f},{.014f,.087f,.032f});
    TubeZ(b,{0,.105f,-.020f},.026f,.020f,.15f,32);
    TubeZ(b,{0,.105f,.060f},.032f,.022f,.022f,32);
    b.Cylinder({0,.127f,-.015f},.014f,.020f,16);
    b.CylinderX({.032f,.105f,-.016f},.014f,.026f,16);
    Paint(b,MAT_PLAIN,0xff634a); b.Emissive(.7f);
    b.Box({-.009f,.1045f,-.088f},{.009f,.1055f,-.087f});
    b.Box({-.0005f,.096f,-.088f},{.0005f,.114f,-.087f}); b.Emissive(0);
  }
}
void Barrel(MeshBuilder& b,int w,int n) {
  float end=WeaponMuzzleZ(w,(u16)(n<<(WS_BARREL*2))), start=-.19f;
  if(w==WPN_PISTOL) {
    Paint(b,MAT_METAL,0x707b83);
    TubeZ(b,{0,.030f,(end-.025f)*.5f},.010f,.0047f,-.025f-end);
    Paint(b,MAT_METAL,0x20282f); b.CylinderZ({0,.007f,end+.012f},.004f,.012f,16);
    return;
  }
  Paint(b,MAT_METAL,0x262f36);
  TubeZ(b,{0,.025f,(start+end)*.5f},w==WPN_SMG?.012f:.018f,w==WPN_SMG?.006f:.010f,start-end);
  if(w==WPN_SMG) {
    float len=n==1?.09f:n==2?.22f:.15f, z=-.20f-len*.5f;
    Paint(b,MAT_METAL,0x3a4147);
    TubeZ(b,{0,.011f,z},.036f,.029f,len,8);
    Paint(b,MAT_PLAIN,0x0c1115);
    for(int i=0;i<(n==1?2:n==2?6:4);i++) for(float x:{-.034f,.034f})
      b.Box({x-.001f,-.001f,-.218f-i*.031f},{x+.001f,.011f,-.204f-i*.031f});
    Rail(b,-.20f-len,-.20f,.046f);
  }
  Paint(b,MAT_METAL,0x313940);
  b.Box({-.013f,.038f,end+.020f},{.013f,.055f,end+.044f});
  b.Box({-.002f,.055f,end+.026f},{.002f,.067f,end+.030f});
  Paint(b,MAT_PLAIN,0xe5dfbc); b.Box({-.0016f,.064f,end+.0301f},{.0016f,.067f,end+.0305f});
}
void Muzzle(MeshBuilder& b,int w,int n) {
  // Local origin at the selected barrel tip; no duplicate barrel below attachments.
  Paint(b,MAT_METAL,0x232c32);
  if(w==WPN_PISTOL) {
    if(n==0) return;
    float len=n==2?.15f:.035f, radius=n==2?.019f:.015f;
    TubeZ(b,{0,.030f,-len*.5f},radius,.0055f,len,32);
    Paint(b,MAT_METAL,0x4d5960);
    for(int i=0;i<(n==2?6:2);i++) TubeZ(b,{0,.030f,-.008f-i*len/(n==2?7:3)},radius+.001f,radius,.002f,24);
    return;
  }
  float len=n==2?(w==0?.19f:.24f):n==1?.045f:.02f, radius=n==2?(w==0?.026f:.041f):.020f;
  TubeZ(b,{0,.025f,-len*.5f},radius,w==0?.007f:.011f,len,32);
  Paint(b,MAT_METAL,n==2?0x4e5457:0x75808a);
  for(int i=0;i<(n==2?8:2);i++) TubeZ(b,{0,.025f,-.012f-i*len/(n==2?9:3)},radius+.001f,radius,.002f,24);
  if(n==1) {
    Paint(b,MAT_PLAIN,0x070a0d);
    for(int i=0;i<3;i++) for(float x:{-radius,radius}) b.Box({x-.001f,.021f,-.010f-i*.011f},{x+.001f,.030f,-.005f-i*.011f});
  }
}
void Stock(MeshBuilder& b,int w,int n) {
  if(w==WPN_PISTOL) return;
  Paint(b,w==1&&n==0?MAT_WOOD:MAT_PLASTIC,w==1&&n==0?0x785235:n==2?0x626653:0x30383c);
  if(n==1) {
    b.Box({-.012f,.003f,.074f},{.012f,.018f,.292f});
    b.Box({-.012f,-.058f,.074f},{.012f,-.043f,.292f});
  } else {
    ShapedBody(b,{{.075f,.018f,-.025f,.018f},{.13f,.022f,-.045f,.018f},{.23f,.029f,-.074f,.026f},{.30f,.028f,-.077f,.025f}});
  }
  if(n==2) { Paint(b,MAT_LEATHER,0x292f2c); ShapedBody(b,{{.13f,.022f,.015f,.026f},{.15f,.030f,.015f,.044f},{.24f,.030f,.020f,.044f},{.265f,.023f,.021f,.032f}}); }
  Paint(b,MAT_LEATHER,0x151b1c);
  ShapedBody(b,{{.296f,.029f,-.079f,.027f},{.312f,.030f,-.078f,.027f},{.317f,.024f,-.073f,.021f}});
  for(int i=0;i<8;i++) b.Box({-.023f,-.068f+i*.011f,.315f},{.023f,-.065f+i*.011f,.318f});
  Paint(b,MAT_METAL,0x4c565d);
  for(float x:{-.031f,.031f}) b.CylinderX({x,-.030f,.258f},.005f,.003f,12);
}
void Magazine(MeshBuilder& b,int w,int n) {
  Paint(b,MAT_METAL,0x262e35);
  if(w==WPN_PISTOL) {
    float len=n==2?.151f:n==1?.106f:.117f;
    b.Push(Translation({0,-.018f,.029f})*RotationX(-.22f));
    b.Box({-.015f,-len,-.018f},{.015f,-.005f,.023f});
    Paint(b,MAT_PLASTIC,0x161e24);
    b.Box({-.025f,-len-.007f,-.025f},{.025f,-len+.002f,.033f});
    Paint(b,MAT_PLAIN,0x080c10);
    for(int i=0;i<6;i++) for(float x:{-.0155f,.0155f}) b.CylinderX({x,-.025f-i*.015f,.005f},.002f,.001f,8);
    b.Pop(); return;
  }
  if(w==1) {
    float len=n==1?.26f:n==2?.48f:.37f;
    b.CylinderZ({0,-.019f,-.17f-len*.5f},.013f,len,20);
    b.CylinderZ({0,-.019f,-.17f-len},.016f,.018f,20);
    return;
  }
  if(n==2) {
    b.Box({-.019f,-.100f,-.135f},{.019f,-.045f,-.09f});
    b.CylinderZ({0,-.15f,-.112f},.073f,.057f,32);
    Paint(b,MAT_PLASTIC,0x373e42); b.CylinderZ({0,-.15f,-.080f},.064f,.006f,32);
    Paint(b,MAT_METAL,0x6c787e); b.CylinderZ({0,-.15f,-.075f},.012f,.008f,20);
  } else {
    float len=n==1?.104f:.165f;
    b.Push(Translation({0,-.045f,-.11f})*RotationX(.14f));
    b.Box({-.018f,-len,-.024f},{.018f,0,.024f});
    Paint(b,MAT_PLASTIC,0x4a5256);
    b.Box({-.020f,-len-.005f,-.026f},{.020f,-len+.006f,.026f});
    for(float x:{-.0185f,.0185f}) for(int i=0;i<3;i++) b.Box({x-.0005f,-len+.015f,-.016f+i*.012f},{x+.0005f,-.020f,-.013f+i*.012f});
    b.Pop();
  }
}
void Foregrip(MeshBuilder& b,int w,int n) {
  if(w==WPN_PISTOL) {
    b.Push(Translation({0,-.018f,.029f})*RotationX(-.22f));
    Paint(b,n==1?MAT_LEATHER:MAT_PLASTIC,n==1?0x3e4843:n==2?0x655a49:0x252e33);
    float wide=n==2?.0235f:n==1?.026f:.024f;
    for(float x:{-wide,wide}) {
      b.Box({x-.001f,-.105f,-.016f},{x+.001f,-.024f,.026f});
      Paint(b,MAT_PLAIN,0x172025);
      for(int i=0;i<9;i++) b.Box({x-.0013f,-.096f+i*.008f,-.012f},{x+.0013f,-.094f+i*.008f,.022f});
    }
    b.Pop(); return;
  }
  if(w==1) {
    Paint(b,n==0?MAT_WOOD:MAT_PLASTIC,n==0?0x805735:n==1?0x42494c:0x2d3539);
    TubeZ(b,{0,-.019f,-.345f},.032f,.014f,.14f,20);
    Paint(b,n==0?MAT_WOOD:MAT_PLASTIC,n==0?0x4c3322:0x171e22);
    for(int i=0;i<8;i++) TubeZ(b,{0,-.019f,-.405f+i*.017f},.0335f,.031f,.004f,20);
  }
  if(n==0 || (w==1 && n==2)) return;
  Paint(b,MAT_PLASTIC,0x333d42);
  float z=w==1?-.345f:-.25f;
  b.Box({-.018f,-.043f,z-.024f},{.018f,-.026f,z+.024f});
  b.Push(Translation({0,-.04f,z})*RotationX(n==1?.10f:-.60f));
  b.Cylinder({0,n==1?-.085f:-.05f,0},.018f,n==1?.085f:.05f,12);
  Paint(b,MAT_LEATHER,0x1f292e);
  for(int i=0;i<4;i++) b.Cylinder({0,-.015f-i*.015f,0},.0185f,.003f,12);
  b.Pop();
}
}

WeaponPose MakeWeaponPose(int w,u16 build,float remaining,float duration,float fire,bool empty) {
  WeaponPose p;
  p.leftHand={0,-.05f,w==WPN_SMG?-.25f:-.345f};
  if(WeaponPart(build,WS_GRIP)==1) p.leftHand.y-=.03f;
  if(w==WPN_PISTOL) p.leftHand={-.037f,-.057f,.017f};
  if(w==WPN_SHOTGUN) {
    float t=GetWeaponStats(w,build).interval-fire;
    p.pump=fire>0 ? std::max(0.0f,sinf(Saturate((t-.10f)/.42f)*PI))*.085f : 0;
    p.leftHand.z+=p.pump;
  } else if(w==WPN_PISTOL) {
    float elapsed=GetWeaponStats(w,build).interval-fire;
    p.bolt=empty?.033f:fire>0?std::max(0.0f,sinf(Saturate(elapsed/.09f)*PI))*.033f:0;
  } else p.bolt=fire>0 ? Saturate(fire/.085f)*.04f : 0;
  if(remaining<=0 || duration<=0) return p;
  float t=Saturate(1-remaining/duration);
  p.reload=std::min(Saturate(t/.12f),Saturate((1-t)/.14f));
  if(w==WPN_SMG || w==WPN_PISTOL) {
    // Left hand removes the old magazine, travels to the pouch, then seats the replacement.
    float drop=t<.52f?Saturate((t-.16f)/.18f):1-Saturate((t-.56f)/.20f);
    p.magazineOffset={-.035f*drop,-.30f*drop,.04f*drop}; p.magazineRoll=-.35f*drop;
    p.magazineVisible=!(t>.35f&&t<.52f);
    Vec3 magHand=p.magazineOffset+Vec3(-.025f,-.12f,-.11f);
    if(w==WPN_PISTOL) magHand=p.magazineOffset+Vec3(-.022f,-.125f,.048f);
    float reach=t<.16f?Saturate(t/.16f):1-Saturate((t-.79f)/.15f);
    p.leftHand=Lerp(p.leftHand,magHand,reach);
    if(w==WPN_PISTOL) p.bolt=empty?.033f*(1-Saturate((t-.85f)/.07f)):0;
    else p.bolt=std::max(p.bolt,sinf(Saturate((t-.79f)/.13f)*PI)*.055f);
  } else {
    float elapsed=duration-remaining;
    float cycle=Saturate(elapsed/.35f);
    if(elapsed>.35f) cycle=1-Saturate(fmodf(elapsed-.35f,GetWeaponStats(w,build).shellTime)/GetWeaponStats(w,build).shellTime);
    Vec3 loading(-.018f,-.066f,-.095f),pouch(-.11f,-.24f,.08f);
    p.leftHand=Lerp(p.leftHand,Lerp(loading,pouch,cycle),p.reload);
    p.shellVisible=elapsed>.35f && remaining>.2f;
    p.shell=cycle;
  }
  return p;
}
void WeaponModels::Init() {
  MeshData d; MeshBuilder b(&d);
  for(int w=0;w<FIREARM_COUNT;w++) {
    Body(b,w); body_[w].Upload(d); d.Clear();
    if(w==WPN_PISTOL) PistolSlide(b);
    else {
      Paint(b,MAT_METAL,0x444c53);
      b.Box({.0315f,.004f,-.108f},{.0325f,.027f,-.028f});
      b.CylinderX({.040f,.016f,-.024f},.006f,.018f,12);
    }
    bolt_[w].Upload(d); d.Clear();
    for(int s=0;s<WS_COUNT;s++) for(int n=0;n<WeaponPartCount(s);n++) {
      switch(s) {
        case WS_OPTIC: if(w==WPN_PISTOL) PistolOptic(b,n); else Optic(b,n); break;
        case WS_BARREL: Barrel(b,w,n); break;
        case WS_MUZZLE: Muzzle(b,w,n); break;
        case WS_STOCK: Stock(b,w,n); break;
        case WS_MAGAZINE: Magazine(b,w,n); break;
        case WS_GRIP: Foregrip(b,w,n); break;
      }
      parts_[w][s][n].Upload(d); d.Clear();
    }
  }
  Paint(b,MAT_PLASTIC,0xb12f25); b.CylinderZ({0,0,0},.010f,.053f,16);
  Paint(b,MAT_GOLD,0xbca66a); b.CylinderZ({0,0,.028f},.011f,.010f,16); shell_.Upload(d);
}
void WeaponModels::Draw(Renderer& r,int w,u16 build,const Mat4& model,float sky,bool vm,const WeaponPose& pose,int selected) const {
  if(w<0 || w>=FIREARM_COUNT) return;
  build=ValidateWeaponBuild(w,build);
  DrawItem it; it.model=model; it.sky=sky; it.viewModel=vm; it.castShadow=!vm; it.mesh=&body_[w]; r.Draw(it);
  for(int s=0;s<WS_COUNT;s++) {
    if(vm && s==WS_STOCK) continue;  // shoulder/cheek support sits behind the first-person eye
    int n=WeaponPart(build,s); it.mesh=&parts_[w][s][n]; it.model=model; it.highlight=s==selected?.18f:0;
    if(s==WS_MUZZLE) it.model=model*Translation({0,0,WeaponMuzzleZ(w,(u16)(build&~(3<<(WS_MUZZLE*2))))});
    if(s==WS_OPTIC && w==WPN_PISTOL) it.model=model*Translation({0,0,pose.bolt});
    if(s==WS_MAGAZINE && w!=WPN_SHOTGUN) {
      if(!pose.magazineVisible) continue;
      it.model=model*Translation(pose.magazineOffset)*RotationZ(pose.magazineRoll);
    }
    if(s==WS_GRIP && w==WPN_SHOTGUN) it.model=model*Translation({0,0,pose.pump});
    r.Draw(it);
  }
  it.highlight=0; it.mesh=&bolt_[w]; it.model=model*Translation({0,0,w==WPN_SHOTGUN?pose.pump*.6f:pose.bolt});
  if(w==WPN_PISTOL) {
    float length=WeaponPart(build,WS_BARREL)==1?.91f:WeaponPart(build,WS_BARREL)==2?1.135f:1;
    it.model=it.model*Translation({0,0,.061f})*Scaling({1,1,length})*Translation({0,0,-.061f});
  }
  r.Draw(it);
  if(pose.shellVisible) { it.mesh=&shell_; it.model=model*Translation(pose.leftHand+Vec3(.015f,.018f,-.010f))*RotationX(-.2f); r.Draw(it); }
}
