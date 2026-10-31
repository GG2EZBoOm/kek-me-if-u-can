// The garden paths and the museum delivery route share the same courtyard footprint.
#include "ZwingerCourts.h"
#include "game/MapKit.h"
#include "game/Shapes.h"

namespace {
constexpr float cellar = -4.4f;
constexpr u32 honey = 0xb19870, limestone = 0xcbb99a, bronze = 0x715737;
constexpr u32 green = 0x284f48, garnet = 0x823b49, ivory = 0xe4d5ac;

void Paving(MapKit& k) {
  // The solid slab is also the cellar ceiling. Each stairwell is an actual hole.
  auto slab = [&](float x0, float z0, float x1, float z1) {
    k.Slab(x0, z0, x1, z1, 0, .42f, MAT_CONCRETE, 0x9d9077, 0xb5a083);
  };
  slab(-46,-52,42,-31);
  slab(-46,-31,-10,-19); slab(-6,-31,42,-19);
  slab(-46,-19,42,19);
  slab(-46,19,6,31); slab(10,19,42,31);
  slab(-46,31,42,52);
  // Cross axes read as paths even from the terraces above.
  k.FloorFinish(-23.7f,-2.6f,26.2f,2.6f,.012f,MAT_TILE,0xb0a28a);
  for (int side : {-1,1}) {
    float z0 = side < 0 ? -34.5f : 2.6f, z1 = side < 0 ? -2.6f : 34.5f;
    k.FloorFinish(-2.65f,z0,2.65f,z1,.012f,MAT_TILE,0xb0a28a);
    for (float x : {-2.6f,2.5f}) k.FloorFinish(x,z0,x+.10f,z1,.022f,MAT_MARBLE,0x716953);
  }
}

void Stairwell(MapKit& k, int side) {
  const float x = side * 8.f;
  const float z0 = side < 0 ? -31.f : 19.f, z1 = side < 0 ? -19.f : 31.f;
  k.Style(MAT_SANDSTONE, honey);
  k.Stairs({x,cellar,side*19.f}, side < 0 ? 3 : 2, 4.f, -cellar/22, 22, 12.f/22, cellar);
  for (float dx : {-2.12f,2.12f}) {
    k.Style(MAT_SANDSTONE, honey);
    k.Solid({x+dx-.15f,cellar,z0-side*.045f},{x+dx+.15f,.24f,z1-side*.045f});
    k.Style(MAT_METAL, bronze);
    for (float z = z0+.2f; z < z1; z += 1.45f)
      k.Solid({x+dx-.045f,.24f,z-.045f},{x+dx+.045f,1.04f,z+.045f});
    k.Solid({x+dx-.07f,.96f,z0},{x+dx+.07f,1.06f,z1});
  }
  const float end = side*18.84f;
  k.Style(MAT_SANDSTONE, limestone);
  k.Solid({x-2.22f,0,end-.15f},{x+2.22f,1.03f,end+.15f});
  k.Style(MAT_METAL, green);
  const float signZ = end-side*.17f;
  k.Visual({x-1.57f,.23f,signZ-.022f},{x+1.57f,.84f,signZ+.022f});
  k.Sign("PORZELLANDEPOT",{x,.535f,signZ-side*.03f},side<0 ? 0 : PI,.040f,ivory,0,false);
  // Entry markers face visitors approaching the top of the stairs.
  for (float dx : {-2.12f,2.12f}) {
    float z = side*30.8f;
    k.Style(MAT_SANDSTONE, limestone);
    k.Solid({x+dx-.27f,0,z-.26f},{x+dx+.27f,1.25f,z+.26f});
    k.Style(MAT_GOLD,0x9d8150); k.ob.Frustum({x+dx,1.25f,z},.25f,.12f,.19f,12);
    k.Style(MAT_GLASS,0xebcb89); k.ob.Ellipsoid({x+dx,1.67f,z},{.19f,.24f,.19f},12,8);
  }
}

void DisplayRack(MapKit& k, float x, float z, bool porcelain) {
  // A solid dark back masks movement; the objects on the open shelves can be copied.
  k.Style(MAT_WOOD,0x614735);
  k.Solid({x-1.75f,cellar,z-.42f},{x+1.75f,cellar+2.28f,z-.30f});
  for (float dx : {-1.72f,1.72f})
    k.Solid({x+dx-.045f,cellar,z-.30f},{x+dx+.045f,cellar+2.32f,z+.53f});
  for (float y : {.30f,1.27f,2.25f}) {
    k.Style(MAT_WOOD,0x916d46);
    k.Solid({x-1.67f,cellar+y-.08f,z-.30f},{x+1.67f,cellar+y,z+.52f});
  }
  k.Style(MAT_METAL,0x375550);
  k.Visual({x-1.60f,cellar+1.34f,z-.445f},{x+1.60f,cellar+2.13f,z-.427f});
  k.Sign(porcelain ? "MEISSEN" : "HOFDEPOT",{x,cellar+1.76f,z-.46f},PI,.065f,ivory,0,false);
  for (float dx : {-1.48f,1.48f}) {
    k.Style(MAT_METAL,bronze);
    k.Visual({x+dx-.04f,cellar+.12f,z-.45f},{x+dx+.04f,cellar+1.25f,z-.42f});
  }
  for (float dx : {-1.02f,.92f}) {
    k.Prop(porcelain ? "zwinger_vase" : "zwinger_kanne", {x+dx,cellar+.30f,z+.09f},dx*.3f);
    k.Prop(porcelain ? "zwinger_kanne" : "zwinger_vase", {x+dx,cellar+1.27f,z+.08f},-dx*.4f);
  }
}

void VaultRib(MapKit& k, float centerX, float z) {
  // Shallow elliptical ribs over each aisle. The crown meets the slab above.
  constexpr int segments = 14;
  for (int i=0; i<segments; ++i) {
    float a = PI*i/segments, b = PI*(i+1)/segments;
    auto point = [&](float angle, float radius, float rise) {
      return Vec2(radius*cosf(angle), 2.67f+rise*sinf(angle));
    };
    k.Style(MAT_SANDSTONE,i%2 ? 0xb8a27f : 0xc8b28d);
    k.ob.Push(Translation({centerX,cellar,z}));
    ExtrudeConvex(k.ob,{point(a,4.77f,1.07f),point(a,5.f,1.34f),
                        point(b,5.f,1.34f),point(b,4.77f,1.07f)},-.16f,.16f);
    k.ob.Pop();
  }
}

void Depot(MapKit& k) {
  k.Slab(-10,-19,10,19,cellar,.24f,MAT_TILE,0x928472);
  k.Style(MAT_BRICK,0x987151);
  k.WallZ(-19,19,-10.28f,-10,cellar,-.42f);
  k.WallZ(-19,19,10,10.28f,cellar,-.42f);
  k.WallX(-6,10,-19.25f,-19,cellar,-.42f);
  k.WallX(-10,6,19,19.25f,cellar,-.42f);
  Stairwell(k,-1); Stairwell(k,1);
  // Vault undersides span the rooms, with the carved ribs sitting just below them.
  for (float x : {-5.f,5.f}) {
    k.Style(MAT_PLASTER,0xc8bba3);
    auto vault = [&](float a,float z) {
      return Vec3(x+5.0f*cosf(a),cellar+2.76f+1.18f*sinf(a),z);
    };
    for (int i=0;i<20;++i) {
      float a=PI*i/20,b=PI*(i+1)/20;
      k.ob.Quad(vault(a,-19),vault(a,19),vault(b,19),vault(b,-19));
      // Close the space above the vault so jumping disguises cannot hide inside the ceiling.
      Vec3 l=vault(b,-19), r=vault(a,19);
      k.Clip({l.x,(r.y+l.y)*.5f,l.z},{r.x,-.42f,r.z},COL_SOLID);
    }
  }
  for (float x : {-2.3f,2.3f}) {
    k.FloorFinish(x-.40f,-18.9f,x+.40f,18.9f,cellar+.014f,MAT_TILE,0x57655e);
    k.FloorFinish(x-.035f,-18.9f,x+.035f,18.9f,cellar+.025f,MAT_MARBLE,0xb6a47b);
  }
  for (float z : {-15.f,-5.f,5.f,15.f}) {
    for (float x : {-9.8f,0.f,9.8f}) {
      k.Style(MAT_SANDSTONE,0xab9677);
      k.Solid({x-.3f,cellar,z-.31f},{x+.3f,cellar+2.75f,z+.31f});
      k.Visual({x-.42f,cellar+2.5f,z-.41f},{x+.42f,cellar+2.73f,z+.41f});
    }
    VaultRib(k,-5,z); VaultRib(k,5,z);
    for (float x : {-5.f,5.f}) {
      k.Style(MAT_METAL,bronze); k.ob.Cylinder({x,-.99f,z},.09f,.58f,12);
      k.ob.Frustum({x,-1.16f,z},.39f,.12f,.19f,16);
      k.Style(MAT_GLASS,0xffdd9f); k.Glow(.65f);
      k.ob.Ellipsoid({x,-1.19f,z},{.28f,.10f,.28f},12,6); k.Glow(0);
      k.Light({x,-1.4f,z},9,{2.5f,1.83f,1.15f});
    }
  }
  for (float z : {-12.f,0.f,12.f}) {
    DisplayRack(k,-4.7f,z,true); DisplayRack(k,5.f,z+2.0f,false);
  }
  for (float side : {-1.f,1.f}) {
    k.Prop("sackkarre",{side*8,cellar,-side*8},side*.2f);
    for (float z : {-8.6f,8.6f}) {
      k.Prop("zwinger_kunstkiste",{side*8.8f,cellar,z+side*2.0f},side*.13f);
      k.Prop("zwinger_urne",{side*7.4f,cellar,z+side*2.4f},0);
    }
    k.Sign(side<0 ? "ZUM KRONENTOR" : "ZUM STERNENGARTEN",
           {side*1.2f,-1.8f,side*18.95f},side<0 ? 0 : PI,.065f,ivory,0,false);
  }
  for (float z : {-9.f,8.f}) {
    k.Prop("werkbank",{.35f,cellar,z},0);
    k.Prop("zwinger_kanne",{-.19f,cellar+.91f,z},.2f);
    k.Prop("klemmbrett",{.8f,cellar+.91f,z},-.18f);
    k.Prop("zwinger_kunstkiste",{1.0f,cellar,z+1.65f},-.2f);
  }
  k.Zone({-10.25f,cellar-.02f,-19.2f},{10.25f,-.12f,19.2f},.30f,"Porzellandepot",5);
}

// Solid hedge panels have exactly the same footprint for sight, bullets and movement.
void RoseWall(MapKit& k, Vec3 center, float halfLength, float yaw, u32 blossom) {
  k.Style(MAT_SANDSTONE,0xa89976);
  k.SolidR(center+Vec3(0,.16f,0),{halfLength+.07f,.16f,.51f},yaw);
  k.Style(MAT_LEAVES,0x374e34);
  k.VisualR(center+Vec3(0,1.09f,0),{halfLength-.12f,.77f,.31f},yaw);
  k.ClipR(center+Vec3(0,1.15f,0),{halfLength,.83f,.49f},yaw,COL_SOLID);
  k.ob.Push(Translation(center)*RotationY(yaw));
  int clumps=int(halfLength*4);
  for (int i=0;i<clumps;++i) {
    float x=(2*(i+.5f)/clumps-1)*(halfLength-.15f);
    for (int j=0;j<3;++j) {
      k.Style(MAT_LEAVES,j==2 ? (i%3 ? 0x5e7648 : 0x67824d) : (i%2 ? 0x416342 : 0x4b6840));
      k.ob.Ellipsoid({x,.65f+.50f*j+.04f*cosf(i*2+j),.04f*sinf(i+j)},{.41f,.34f,.45f},9,5);
    }
    for (int side : {-1,1}) {
      Vec3 bud(x,.95f+.67f*fabsf(sinf(i*1.8f+side)),side*.415f);
      k.Style(MAT_FABRIC,blossom);
      for (int petal=0;petal<5;++petal) {
        float a=petal*TAU/5;
        k.ob.Ellipsoid(bud+Vec3(.057f*cosf(a),.057f*sinf(a),0),{.060f,.059f,.035f},6,4);
      }
      k.Style(MAT_PLAIN,0xdfb875); k.ob.Sphere(bud+Vec3(0,0,side*.036f),.030f,6,4);
    }
  }
  k.ob.Pop();
}

void RoseGarden(MapKit& k) {
  k.FloorFinish(-17.5f,3.5f,-4.5f,16.5f,.018f,MAT_GRASS,0x687446);
  k.FloorFinish(-16.9f,8.85f,-5.f,11.15f,.035f,MAT_CONCRETE,0xbaab87);
  // Offset openings give both teams a choice between a fast crossing and two blind turns.
  RoseWall(k,{-14,0,5},2.45f,0,0xcb758b);
  RoseWall(k,{-7,0,6.7f},2.05f,PI*.5f,0xd5a669);
  RoseWall(k,{-14.8f,0,13},2.15f,PI*.5f,0xce8393);
  RoseWall(k,{-8.9f,0,14.9f},2.8f,0,0xbd6177);
  for (float x : {-13.2f,-9.8f}) {
    k.Style(MAT_METAL,green);
    for (float z : {8.55f,11.45f}) {
      k.Solid({x-.08f,0,z-.08f},{x+.08f,2.14f,z+.08f});
    }
    k.ob.Push(Translation({x,2.1f,10})*RotationY(PI*.5f));
    for (int i=0; i<12; ++i) {
      float a=PI*i/12, b=PI*(i+1)/12;
      auto ring = [](float r,float t){ return Vec2(r*cosf(t),r*sinf(t)); };
      ExtrudeConvex(k.ob,{ring(1.40f,a),ring(1.50f,a),ring(1.50f,b),ring(1.40f,b)},-.08f,.08f);
    }
    k.ob.Pop();
  }
  k.Style(MAT_METAL,green);
  for (float a : {.28f,.85f,1.57f,2.29f,2.86f}) {
    Vec3 p(-11.5f,2.1f+1.47f*sinf(a),10+1.47f*cosf(a));
    k.ob.BoxC(p,{1.78f,.035f,.035f});
  }
  // Wisteria hangs from the upper lattice, above head height throughout the path.
  for (int i=0;i<33;++i) {
    float a=.50f+2.1f*((i*7)%17)/16.f;
    Vec3 p(-13.25f+3.45f*((i*5)%19)/18.f,2.1f+1.47f*sinf(a),10+1.47f*cosf(a));
    k.Style(MAT_LEAVES,i%2 ? 0x4e723e : 0x6c874a);
    k.ob.Ellipsoid(p,{.36f,.11f,.24f},8,5);
    k.Style(MAT_FABRIC,i%3 ? 0xa891b1 : 0x8e739c);
    k.ob.Ellipsoid(p-Vec3(0,.14f,0),{.09f,.18f,.09f},8,5);
    k.ob.Ellipsoid(p-Vec3(0,.31f,0),{.055f,.095f,.055f},8,5);
  }
  k.Prop("zwinger_orange",{-16.3f,0,7.25f},0);
  k.Prop("zwinger_orange",{-6.2f,0,12.1f},0);
  k.Prop("kuebel",{-11.5f,0,6.3f},.3f); k.Prop("kuebel",{-13.4f,0,13.2f},-.1f);
  k.Prop("parkbank",{-10.2f,0,13},PI);
  k.Zone({-17.5f,0,3.5f},{-4.5f,3.75f,16.5f},1,"Rosenboskett",3);
}

void MusicStage(MapKit& k) {
  constexpr float deck = .72f;
  k.Style(MAT_WOOD,0x74563e);
  k.Solid({6.5f,0,-14.5f},{15.5f,deck,-7.5f});
  k.FloorFinish(6.5f,-14.5f,15.5f,-7.5f,deck+.012f,MAT_PARQUET,0xb68e58);
  k.Stairs({11,0,-5.9f},3,3.6f,.18f,4,.40f,0);
  k.Stairs({17.1f,0,-12},1,2.6f,.18f,4,.40f,0);
  // A split backdrop leaves a playable backstage lane and a side exit.
  for (float x : {8.3f,13.7f}) {
    k.Style(MAT_FABRIC,garnet);
    k.Solid({x-1.58f,deck,-13.1f},{x+1.58f,3.45f,-12.82f});
    for (int i=0; i<8; ++i) {
      k.Style(MAT_FABRIC,i%2 ? 0x924957 : 0x753547);
      k.ob.Ellipsoid({x-1.4f+i*.40f,2.07f,-12.81f},{.20f,1.35f,.11f},8,6);
    }
  }
  for (float x : {6.7f,15.3f}) for (float z : {-14.3f,-7.7f}) {
    k.Style(MAT_WOOD,0x675238);
    k.Solid({x-.10f,deck,z-.10f},{x+.10f,4.07f,z+.10f});
    k.Style(MAT_GOLD,0xc8a260); k.ob.Sphere({x,4.17f,z},.15f,12,8);
  }
  // A striped tensile canopy, pitched along the ridge. Show the lining from the stage, too.
  for (int i=0; i<10; ++i) {
    float left=6.15f+.97f*i, right=left+.97f;
    k.Style(MAT_FABRIC,i%2 ? ivory : 0x426b65);
    for (float edge : {-14.75f,-7.25f}) {
      auto cloth = [edge](float x,float t) {
        return Vec3(x,3.96f+1.39f*t-.23f*sinf(PI*t),edge+(-11-edge)*t);
      };
      for (int j=0;j<5;++j) {
        Vec3 a=cloth(left,j/5.f), b=cloth(right,j/5.f), c=cloth(right,(j+1)/5.f), d=cloth(left,(j+1)/5.f);
        k.ob.Quad(a,b,c,d); k.ob.Quad(d,c,b,a);
      }
      for (int j=0;j<5;++j) {
        float u=j/5.f, v=(j+1)/5.f;
        Vec3 top=cloth(left+.97f*u,0), next=cloth(left+.97f*v,0);
        Vec3 low=top-Vec3(0,.16f+.16f*sinf(PI*u),0), lowNext=next-Vec3(0,.16f+.16f*sinf(PI*v),0);
        k.ob.Quad(top,next,lowNext,low); k.ob.Quad(low,lowNext,next,top);
      }
    }
  }
  k.Style(MAT_WOOD,0x6b5437);
  k.Visual({6.59f,3.54f,-7.82f},{15.41f,3.70f,-7.58f});
  k.Style(MAT_METAL,green);
  k.Visual({9.22f,3.13f,-7.75f},{12.78f,3.65f,-7.56f});
  k.Sign("HOFKONZERTE",{11,3.40f,-7.54f},0,.041f,ivory,0,false);
  k.Prop("zwinger_cello",{8.15f,deck,-11.65f},.13f);
  k.Prop("zwinger_cello",{8.95f,deck,-9.75f},-.31f);
  k.Prop("notenstaender",{9.1f,deck,-10.75f},.32f);
  k.Prop("tuba",{13.1f,deck,-10.7f},-.4f); k.Prop("pauke",{14.15f,deck,-11.75f},0);
  k.Prop("notenstaender",{12.9f,deck,-9.25f},-.36f);
  k.Prop("sitzbank",{7.6f,0,-4.8f},PI); k.Prop("sitzbank",{14.5f,0,-4.5f},PI);
  k.Zone({5.9f,0,-15},{16,5.5f,-5.6f},.9f,"Hofbühne",3);
}

void StarGarden(MapKit& k) {
  const Vec3 pivot(11,2.42f,10);
  k.FloorFinish(4.7f,3.7f,17.3f,16.3f,.018f,MAT_MARBLE,0x8f8269);
  k.Style(MAT_METAL,green); k.ob.Ring({11,.033f,10},2.85f,3.12f,64);
  for (int i=0; i<24; ++i) {
    float angle=TAU*i/24;
    k.Style(MAT_GOLD,0xb39a64);
    k.ob.BoxR(Vec3(11,.045f,10)+RotateY({0,0,3.37f},angle),{.045f,.008f,i%3 ? .14f : .28f},angle);
  }
  k.Style(MAT_SANDSTONE,limestone);
  k.ob.Frustum({11,0,10},1.02f,.83f,.38f,24);
  k.ob.Frustum({11,.38f,10},.50f,.29f,.64f,20);
  k.ob.Frustum({11,1.02f,10},.29f,.68f,.24f,20);
  k.ClipR({11,.63f,10},{.74f,.63f,.74f},0,COL_SOLID);
  // The armillary sphere is an open sculpture. Only its pedestal obstructs the path.
  k.Style(MAT_METAL,green);
  k.ob.Push(Translation(pivot)*RotationZ(.38f));
  k.ob.Torus({},1.37f,.075f,48,8);
  k.ob.Push(RotationX(PI*.5f)); k.ob.Torus({},1.39f,.075f,48,8); k.ob.Pop();
  k.ob.Pop();
  AnimatedPart dial; dial.pivot=pivot; dial.speed=.18f;
  MeshBuilder stars(&dial.mesh); stars.Push(Translation(pivot)*RotationZ(-.46f));
  stars.Mat(MAT_GOLD); stars.ColorHex(0xcda651); stars.Torus({},1.12f,.075f,48,8);
  stars.Push(RotationX(PI*.5f)); stars.Torus({},1.10f,.045f,40,8); stars.Pop();
  stars.Sphere({0,0,0},.30f,20,12);
  for (int i=0; i<4; ++i) {
    float a=TAU*i/4;
    stars.ColorHex(i%2 ? 0x4b8790 : 0xb76c4e); stars.Sphere({1.12f*cosf(a),0,1.12f*sinf(a)},.13f,12,8);
  }
  stars.Pop(); k.w.anims.push_back(std::move(dial));
  for (float side : {-1.f,1.f}) {
    float x = 11+side*4.4f;
    k.Style(MAT_METAL,green);
    k.Solid({x-.38f,0,8.1f},{x+.38f,1.18f,11.9f});
    k.Style(MAT_WOOD,0x98764c);
    k.Solid({x-.53f,1.18f,8.f},{x+.53f,1.29f,12.f});
    k.Prop("globus",{x,1.29f,9},0);
    k.Prop("zwinger_kanne",{x,1.29f,11.1f},side*.5f);
    k.Prop("zwinger_fernrohr",{11+side*3.1f,0,10+side*4.1f},side*1.1f);
    k.Prop("zwinger_urne",{11-side*3.3f,0,10+side*4.2f},0);
  }
  k.Zone({4.6f,0,3.6f},{17.4f,4.2f,16.4f},1,"Sternengarten",3);
}
} // namespace

void BuildZwingerCourts(MapKit& k) {
  Paving(k); Depot(k); RoseGarden(k); MusicStage(k); StarGarden(k);
}
