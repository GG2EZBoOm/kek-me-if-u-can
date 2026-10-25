#include "game/MapDetails.h"
#include "game/MapKit.h"
#include "game/Shapes.h"

namespace {
constexpr u32 IRON=0x394751, CREAM=0xe3d9c3, RED=0xc91b10, YELLOW=0xffcf20;

// Structural detail stays above head height or inside solid furniture. Every
// new floor has normal stairs, generous landings and at least two ways out.
void Beam(MapKit& k, Vec3 a, Vec3 b, float r) {
  Vec3 y=Normalize(b-a), ref=fabsf(y.y)<.9f ? Vec3(0,1,0) : Vec3(1,0,0);
  Vec3 x=Normalize(Cross(y,ref)), z=Cross(x,y); Mat4 m=Mat4::Identity();
  m.m[0]=x.x;m.m[1]=x.y;m.m[2]=x.z;m.m[4]=y.x;m.m[5]=y.y;m.m[6]=y.z;
  m.m[8]=z.x;m.m[9]=z.y;m.m[10]=z.z;m.m[12]=a.x;m.m[13]=a.y;m.m[14]=a.z;
  k.ob.Push(m);k.ob.BoxC({0,Length(b-a)*.5f,0},{r,Length(b-a)*.5f,r});k.ob.Pop();
}

void WindowX(MapKit& k,float x,float z,float y,float width,float height,u32 frame=IRON) {
  k.Glass({x-width*.5f,y,z-.035f},{x+width*.5f,y+height,z+.035f},0x84abb7,.25f);
  k.Style(MAT_METAL,frame);
  for(float xx : {x-width*.5f,x,x+width*.5f}) k.Visual({xx-.045f,y-.08f,z-.07f},{xx+.045f,y+height+.08f,z+.07f});
  for(float yy : {y-.08f,y+height*.55f,y+height-.035f}) k.Visual({x-width*.5f-.08f,yy,z-.07f},{x+width*.5f+.08f,yy+.07f,z+.07f});
}

void WindowZ(MapKit& k,float x,float z,float y,float width,float height,u32 frame=IRON) {
  k.Glass({x-.035f,y,z-width*.5f},{x+.035f,y+height,z+width*.5f},0x84abb7,.25f);
  k.Style(MAT_METAL,frame);
  for(float zz : {z-width*.5f,z,z+width*.5f}) k.Visual({x-.07f,y-.08f,zz-.045f},{x+.07f,y+height+.08f,zz+.045f});
  for(float yy : {y-.08f,y+height*.55f,y+height-.035f}) k.Visual({x-.07f,yy,z-width*.5f-.08f},{x+.07f,yy+.07f,z+width*.5f+.08f});
}

void Table(MapKit& k,Vec3 p,bool beer=false) {
  k.Prop(beer ? "biertisch" : "kantinentisch",p,0);
  if(beer) {
    for(float side : {-1.0f,1.0f}) k.Prop("bierbank",p+Vec3(0,0,side*.75f),0);
  } else {
    for(float side : {-1.0f,1.0f}) k.Prop("kantinenstuhl",p+Vec3(0,0,side*.95f),side<0 ? PI : 0);
  }
  k.Prop(beer ? "masskrug" : "kaffeebecher",p+Vec3(-.45f,.79f,0),.2f);
  k.Prop(beer ? "wiesnbrezn" : "tablett",p+Vec3(.45f,.79f,0),0);
}

void Clock(MapKit& k,Vec3 p,float radius,u32 rim) {
  k.Style(MAT_METAL,rim);k.ob.CylinderZ(p,radius,.12f,48);
  k.Style(MAT_PLAIN,0xefe4cc);k.ob.CylinderZ(p+Vec3(0,0,.08f),radius*.88f,.03f,48);
  k.Style(MAT_METAL,IRON);
  for(int i=0;i<12;i++) {
    float a=i*TAU/12;
    Beam(k,p+Vec3(sinf(a)*radius*.67f,cosf(a)*radius*.67f,.11f),
           p+Vec3(sinf(a)*radius*.79f,cosf(a)*radius*.79f,.11f),.018f);
  }
  Beam(k,p+Vec3(0,0,.13f),p+Vec3(-radius*.34f,radius*.25f,.13f),.026f);
  Beam(k,p+Vec3(0,0,.14f),p+Vec3(radius*.12f,radius*.66f,.14f),.02f);
}

void SlopingRail(MapKit& k,Vec3 a,Vec3 b,float halfWidth,u32 color=IRON) {
  k.Style(MAT_METAL,color);
  for(float side : {-1.0f,1.0f}) {
    Vec3 offset(side*halfWidth,0,0); // All new stairs run along Z.
    Beam(k,a+offset+Vec3(0,1,0),b+offset+Vec3(0,1,0),.04f);
    for(int i=0;i<=5;i++) {
      Vec3 p=Lerp(a,b,i/5.0f)+offset;
      Beam(k,p,p+Vec3(0,1,0),.035f);
    }
  }
}

void CampusRail(MapKit& k,Vec3 a,Vec3 b) {
  const float len=Length(b-a);int posts=std::max(1,(int)ceilf(len/2.4f));
  k.Style(MAT_METAL,IRON);
  for(int i=0;i<=posts;i++) { Vec3 p=Lerp(a,b,(float)i/posts);Beam(k,p,p+Vec3(0,1.05f,0),.035f); }
  for(float y : {.12f,.55f,1.05f}) Beam(k,a+Vec3(0,y,0),b+Vec3(0,y,0),.035f);
  // Continuous collision, without dense prison-like vertical bars.
  k.ClipR((a+b)*.5f+Vec3(0,.53f,0),{.05f,.53f,len*.5f},atan2f(b.x-a.x,b.z-a.z),COL_SOLID);
}

void ParcelCampus(MapKit& k) {
  // A real staff floor over returns, connected to the dispatch roof terrace.
  constexpr float F=4.5f,H=7.7f;
  k.Style(MAT_PLASTER,0xc9c8bb);
  k.WallX(-26,-10.2f,26.8f,27.02f,F,H,{{-24,-21,F+1,F+2.6f,true},{-19.5f,-16.5f,F+1,F+2.6f,true},{-14.8f,-11.2f,F,H-.3f}});
  k.WallX(-26,-10.2f,14.35f,14.57f,F,H,{{-23.5f,-20,F+1,F+2.6f,true},{-16,-12,F+1,F+2.6f,true}});
  k.WallZ(14.57f,26.8f,-26,-25.78f,F,H,{{16,19,F,H-.3f},{22,25,F+1,F+2.6f,true}});
  k.WallZ(14.57f,26.8f,-10.42f,-10.2f,F,H,{{16,19,F+1,F+2.6f,true},{22.2f,25,F,H-.3f}});
  WindowZ(k,-10.31f,17.5f,F+1,3,1.6f);
  for(float x : {-22.5f,-18.0f}) WindowX(k,x,26.91f,F+1,3,1.6f);
  for(float x : {-21.75f,-14.0f}) WindowX(k,x,14.46f,F+1,x<-20 ? 3.5f : 4,1.6f);
  WindowZ(k,-25.89f,23.5f,F+1,3,1.6f);
  k.Slab(-26.3f,14.2f,-9.95f,27.2f,H+.22f,.22f,MAT_METAL,0x697d83);
  k.FloorFinish(-25.78f,14.57f,-10.42f,26.8f,F+.015f,MAT_PARQUET,0xa18a62);
  k.Style(MAT_METAL,IRON);
  for(float x : {-28.5f,28.6f}) {
    k.Stairs({x,0,30},3,2.4f,F/23,23,.52f,0);
    SlopingRail(k,{x,.2f,29.7f},{x,F,18.04f},1.25f);
  }
  k.Slab(-29.8f,16,-25.7f,18.1f,F+.025f,.18f,MAT_METAL,0x75838c);
  k.Slab(25.6f,16,29.9f,18.1f,F+.025f,.18f,MAT_METAL,0x75838c);
  for(Vec2 span : {Vec2(-29.8f,-26),Vec2(26,29.9f)}) CampusRail(k,{span.x,F,16},{span.y,F,16});
  // Recess the crossing between the wings, leaving the main entrance in view.
  for(Vec2 span : {Vec2(-25.8f,-10.4f),Vec2(14.2f,25.8f)}) {
    k.Slab(span.x,26.7f,span.y,31.2f,F+.025f,.22f,MAT_METAL,0x667575);
    CampusRail(k,{span.x,F,31.2f},{span.y,F,31.2f});
  }
  k.Slab(-10.45f,22.2f,14.35f,25,F+.025f,.22f,MAT_METAL,0x667575);
  CampusRail(k,{-10.2f,F,22.2f},{14.1f,F,22.2f});
  CampusRail(k,{-10.2f,F,25},{14.1f,F,25});
  for(float x : {-10.4f,14.2f}) CampusRail(k,{x,F,27.1f},{x,F,31.2f});
  for(float x : {-25.8f,25.8f}) CampusRail(k,{x,F,27.1f},{x,F,31.2f});
  k.Style(MAT_METAL,IRON);
  for(float x : {-25.0f,-12.0f,12.0f,25.0f}) k.Solid({x-.11f,0,30.1f},{x+.11f,F-.2f,30.32f});
  for(float x : {-8.5f,12.5f}) k.Solid({x-.11f,0,24.65f},{x+.11f,F-.2f,24.87f});
  // The new terrace has real seating and clear walking lanes along all sides.
  for(float z : {20.5f,24.5f}) {
    Table(k,{18, F,z});k.Prop("kuebel",{24.3f,F,z},0);
  }
  CampusRail(k,{14.1f,F,14.5f},{26,F,14.5f});
  CampusRail(k,{14.1f,F,14.5f},{14.1f,F,22.1f});
  CampusRail(k,{14.1f,F,25.1f},{14.1f,F,27});
  CampusRail(k,{26,F,18.15f},{26,F,27});
  CampusRail(k,{26,F,14.5f},{26,F,16});
  for(float z : {20.5f,24.5f}) Table(k,{-21.7f,F,z});
  k.Prop("sofa",{-15.5f,F,24.9f},PI);
  k.Prop("couchtisch",{-15.5f,F,23.3f},0);k.Prop("kaffeebecher",{-15.1f,F+.46f,23.3f},0);
  k.Prop("automat",{-24.7f,F,15.4f},0);k.Prop("wasserspender",{-22.8f,F,15.4f},0);
  k.Prop("regal",{-17.3f,F,15.1f},0);k.Prop("pflanze",{-11.3f,F,21},0);
  // Coffee kitchen, meeting table and a softer lounge surface make the room lived in.
  k.Style(MAT_WOOD,0x846847);k.Solid({-21.2f,F,14.8f},{-19,F+.85f,15.65f});
  k.Style(MAT_METAL,IRON);k.Visual({-21.28f,F+.86f,14.75f},{-18.92f,F+.92f,15.7f});
  k.Prop("kaffeemaschine",{-20.6f,F+.92f,15.2f},0);
  k.Prop("kaffeebecher",{-19.6f,F+.92f,15.35f},0);
  k.Prop("tablett",{-19.25f,F+.92f,15.3f},0);
  Table(k,{-18.5f,F,19});
  k.Prop("pflanze",{-16,F,18.2f},0);k.Prop("papierkorb",{-24.6f,F,20.8f},0);
  k.FloorFinish(-18,21.8f,-12.8f,25.5f,F+.03f,MAT_FABRIC,0x6d786a);
  k.ParcelSign("KAFFEE / KURZE PAUSE",{-19.1f,6.45f,14.66f},0,.035f);
  for(float x : {-23.0f,-16.0f}) k.CeilingLight({x,H-.05f,20},7,{2,1.9f,1.7f},.45f);
  // Integrated fascia and window sills replace the oversized floating letters.
  k.Style(MAT_PLAIN,YELLOW);k.Visual({-26.2f,7.19f,27.035f},{-10.05f,7.74f,27.15f});
  k.Sign("PAUSENDECK",{-18,7.46f,27.18f},0,.047f,RED,0,false);
  k.Style(MAT_METAL,IRON);
  for(float x : {-22.5f,-18.0f}) k.Visual({x-1.6f,F+.91f,26.78f},{x+1.6f,F+.98f,27.22f});
  for(float x : {-25.7f,-15.6f,-10.5f}) k.Visual({x-.055f,F+.08f,27.03f},{x+.055f,7.18f,27.10f});
  k.Style(MAT_WOOD,0x9a7e54);
  for(float x=-25.5f;x<-15.2f;x+=.38f) k.Visual({x,F+.12f,27.035f},{x+.22f,F+.81f,27.09f});
  k.ParcelSign("DACHTERRASSE",{20,5.65f,27.35f},0,.047f);
  k.Zone({-25.7f,F,14.6f},{-10.5f,H,26.8f},.4f,"Pausendeck",4);
  k.Zone({14.2f,F,14.5f},{26,8,27.2f},1,"Dachterrasse Leitstelle",4);
  k.Zone({-10.2f,F,22.2f},{14.2f,7.5f,25},1,"Hofbruecke",4);
  // Building-scale detail: service ribs, drainage and a lit, numbered dock rhythm.
  for(float x : {-25.5f,-17.5f,-8.0f,8.0f,17.5f,25.5f}) {
    k.Style(MAT_METAL,0xb8c2c4);k.Visual({x-.12f,.55f,14.24f},{x+.12f,5.08f,14.43f});
  }
  for(float x : {-26.35f,26.35f}) for(float z : {-20.0f,-12.0f,-4.0f,4.0f,12.0f}) {
    k.Style(MAT_METAL,IRON);k.ob.Cylinder({x,0,z},.075f,8.35f,8);
  }
  for(float z : {-14.5f,-5.5f,5.5f}) {
    k.Style(MAT_METAL,IRON);k.Visual({26.3f,4.4f,z-2.7f},{31.8f,4.62f,z+2.7f});
    for(float zz : {z-2.6f,z+2.6f}) {
      k.Style(MAT_PLASTIC,RED);k.Solid({27.1f,0,zz-.13f},{27.36f,.9f,zz+.13f});
    }
    k.CeilingLight({29,4.35f,z},6,{2,1.8f,1.5f},.22f);
  }
  // Dispatch clutter is grouped into workstations, never sprinkled across aisles.
  for(float x : {-19.0f,18.0f}) {
    k.Prop("schreibtisch",{x,0,-39},0);k.Prop("buerostuhl",{x,0,-37.8f},PI);
    k.Prop("monitor",{x, .77f,-39},0);k.Prop("handscanner",{x+.6f,.77f,-39},0);
    k.Prop("aktenstapel",{x-.6f,.77f,-39},0);k.Prop("papierkorb",{x+1.7f,0,-40},0);
  }
  k.w.cameraPath.insert(k.w.cameraPath.end(),{{-32,10,38},{0,5.5f,23},{-12.5f,6.3f,21},{-23,5.5f,20},
      {17,6.4f,23.6f},{-13,5.5f,23.6f},{34,5,15},{29,3,-6}});
}

void Boiler(MapKit& k,Vec3 p,u32 paint) {
  k.Style(MAT_METAL,IRON);k.Solid(p+Vec3(-1.55f,0,-1.15f),p+Vec3(1.55f,.35f,1.15f));
  k.Style(MAT_METAL,paint);k.ob.Cylinder(p+Vec3(0,.35f,0),1.35f,3.5f,32);
  k.Clip(p+Vec3(-1.35f,.35f,-1.35f),p+Vec3(1.35f,3.85f,1.35f),COL_SOLID);
  k.Style(MAT_METAL,0xb5b8b1);
  for(float y : {.6f,1.8f,3.5f}) k.ob.Torus(p+Vec3(0,y,0),1.36f,.065f,32,8);
  k.ob.Frustum(p+Vec3(0,3.85f,0),1.35f,.65f,.48f,32);
  k.Style(MAT_METAL,0x6c5943);k.ob.Cylinder(p+Vec3(0,4.33f,0),.25f,1.6f,16);
  k.Style(MAT_METAL,IRON);k.ob.CylinderZ(p+Vec3(0,1.1f,1.36f),.68f,.15f,24);
  k.Style(MAT_METAL,0xb8965c);k.ob.CylinderZ(p+Vec3(0,1.1f,1.47f),.44f,.08f,24);
  Clock(k,p+Vec3(.7f,2.7f,1.12f),.23f,IRON);
  k.Sign("KESSEL / 04 BAR",p+Vec3(-.2f,2.2f,1.43f),0,.032f,CREAM);
}

void FactoryQuarter(MapKit& k) {
  constexpr float F=3,H=6.5f;
  k.Style(MAT_BRICK,0x87543b);
  k.WallX(-53,-36,31,31.3f,0,H,{{-46,-43,0,2.7f},{-51.7f,-48.2f,4,5.8f,true},{-41,-37.2f,4,5.8f,true}});
  k.WallX(-53,-36,47,47.3f,0,H,{{-48,-45,0,2.8f},{-42,-39,0,2.8f},{-51.8f,-49.3f,1,3.2f,true},{-38.5f,-36.8f,1,3.2f,true}});
  k.WallZ(31.3f,47,-53,-52.7f,0,H,{{39,42,1.2f,4.5f,true}});
  k.WallZ(31.3f,47,-36.3f,-36,0,H,{{39,42,1.2f,4.5f,true}});
  WindowX(k,-49.95f,31.15f,4,3.5f,1.8f);WindowX(k,-39.1f,31.15f,4,3.8f,1.8f);
  WindowX(k,-50.55f,47.15f,1,2.5f,2.2f);WindowX(k,-37.65f,47.15f,1,1.7f,2.2f);
  WindowZ(k,-52.85f,40.5f,1.2f,3,3.3f);WindowZ(k,-36.15f,40.5f,1.2f,3,3.3f);
  k.FloorFinish(-52.7f,31.3f,-36.3f,47,.025f,MAT_CONCRETE,0x8f8b7b);
  // Narrow clerestory slot admits a view of the sky without a fake painted skylight.
  k.Slab(-53.2f,30.8f,-46.2f,47.5f,H+.2f,.2f,MAT_CORRUGATED,0x51635c);
  k.Slab(-42.8f,30.8f,-35.8f,47.5f,H+.2f,.2f,MAT_CORRUGATED,0x51635c);
  k.Glass({-46.2f,H,30.8f},{-42.8f,H+.10f,47.5f},0x9bbcc0,.38f);
  k.Style(MAT_METAL,IRON);
  for(float z : {32.0f,36.0f,40.0f,44.0f,47.0f}) {
    k.Visual({-52.65f,H-.3f,z-.08f},{-36.35f,H-.05f,z+.08f});
    k.Visual({-46.2f,H+.1f,z-.06f},{-42.8f,H+.23f,z+.06f});
  }
  k.Slab(-52.7f,31.3f,-36.3f,34.8f,F+.025f,.18f,MAT_METAL,0x648177);
  for(float x : {-51.2f,-37.8f}) {
    k.Style(MAT_METAL,IRON);k.Stairs({x,0,44},3,2.2f,.2f,15,.62f,0);
    SlopingRail(k,{x,.2f,43.7f},{x,F,34.7f},1.15f);
  }
  k.Railing({-49.95f,F,34.8f},{-39.05f,F,34.8f},1,false);
  Boiler(k,{-47.5f,0,38},0x607c70);Boiler(k,{-41.5f,0,38},0x7c6a50);
  for(float x : {-47.5f,-41.5f}) {
    k.Style(MAT_METAL,0x8d6243);Beam(k,{x,5.9f,38},{x,5.9f,32},.13f);
    Beam(k,{x,5.9f,32},{-36.35f,5.9f,32},.13f);
  }
  k.Prop("werkbank",{-44.5f,0,43.5f},0);k.Prop("werkzeugkiste",{-45,.91f,43.5f},0);
  k.Prop("schutzhelm",{-44,.91f,43.5f},0);k.Prop("eimer",{-42.4f,0,43.5f},0);
  k.Prop("schaltschrank",{-44.6f,F,31.9f},0);k.Prop("kabeltrommel",{-48,F,32.1f},0);
  k.Prop("klemmbrett",{-43,F,32.3f},.4f);k.Prop("feuerloescher",{-52,0,46},0);
  for(Vec3 p : {Vec3(-44.5f,6.3f,37),Vec3(-44.5f,6.3f,44)}) k.CeilingLight(p,11,{2.7f,1.9f,1.1f},.5f);
  k.Sign("KESSELHAUS",{-44.5f,5.5f,47.75f},0,.16f,0xf1d09a,0,false);
  k.Sign("ENERGIE / WARTUNG",{-44.5f,4.6f,47.75f},0,.06f,CREAM);
  // Pilasters, cornices and a clock break up the old, flat frontage.
  for(float x : {-52.7f,-44.5f,-36.3f}) {
    k.Style(MAT_BRICK,0x6e4333);k.Visual({x-.2f,0,47.33f},{x+.2f,6.45f,47.63f});
  }
  k.Style(MAT_SANDSTONE,0xb4a789);k.Visual({-53.2f,6.1f,47.32f},{-35.8f,6.38f,47.7f});
  for(float x : {-34.5f,-25.0f,-17.0f,-2.5f,8.5f,19.0f,34.5f}) {
    k.Style(MAT_BRICK,0x743c2c);k.Visual({x-.19f,.15f,22.04f},{x+.19f,8.85f,22.34f});
  }
  k.Style(MAT_METAL,IRON);
  for(float x : {-35.8f,35.8f}) k.ob.Cylinder({x,.1f,22.4f},.08f,8.9f,8);
  Clock(k,{0,7.6f,22.25f},1.0f,0x384e47);
  for(float x : {-22.0f,0.0f,23.0f}) {
    k.Style(MAT_METAL,0x527164);k.Visual({x-3,3.7f,22.35f},{x+3,3.91f,25.4f});
    k.Style(MAT_METAL,IRON);
    for(float dx : {-2.7f,2.7f}) Beam(k,{x+dx,2.5f,22.37f},{x+dx,3.7f,24.7f},.065f);
  }
  // Pipe gantry and small repair yard visually connect the two industrial buildings.
  k.Style(MAT_METAL,0x8b6951);
  for(float x : {-39.5f,-38.8f}) Beam(k,{x,5.7f,22},{x,5.7f,31},.14f);
  k.Style(MAT_METAL,IRON);
  for(float z : {24.0f,29.0f}) {
    k.Solid({-40.5f,0,z-.12f},{-40.25f,5.8f,z+.12f});Beam(k,{-40.5f,5.55f,z},{-38,5.55f,z},.10f);
  }
  k.Prop("ibc",{-48,0,26.5f},0);k.Prop("fass_blau",{-51,0,27.5f},0);k.Prop("sackkarre",{-45,0,26},.3f);
  k.Zone({-52.7f,0,31.3f},{-36.3f,6.4f,47},.32f,"Kesselhaus",3);
  k.Zone({-52.7f,F-.1f,31.3f},{-36.3f,5.8f,34.8f},.35f,"Kesselwarte",4);
  k.w.cameraPath.insert(k.w.cameraPath.end(),{{-32,9,55},{-45,3,39},{-44.5f,2,45},{-47,2.5f,36},
      {-38.5f,4.7f,33},{-49,3.6f,38},{8,5,32},{-8,5,22}});
}

void Bulbs(MapKit& k,Vec3 a,Vec3 b) {
  k.Style(MAT_METAL,IRON);
  Vec3 prev=a;
  for(int i=1;i<=24;i++) {
    float u=i/24.0f;Vec3 p=Lerp(a,b,u)-Vec3(0,sinf(u*PI)*.55f,0);Beam(k,prev,p,.014f);prev=p;
    if(i%2==0) {
      k.Style(MAT_PLASTIC,i%4 ? 0xf5c472 : 0xe3dca5);k.Glow(.7f);k.ob.Sphere(p-Vec3(0,.10f,0),.065f,8,6);k.Glow(0);k.Style(MAT_METAL,IRON);
    }
  }
}

void AlpineLodge(MapKit& k) {
  constexpr float F=3,H=6.4f;
  k.Style(MAT_WOOD,0x996a40);
  k.WallX(40,58,-11,-10.75f,0,H,{{47,51,0,2.7f},{42,45,1,2.4f,true},{53,56,1,2.4f,true},{42,45,4,5.7f,true},{53,56,4,5.7f,true}});
  k.WallX(40,58,8,8.25f,0,H,{{47,51,0,2.7f},{42,45,1,2.4f,true},{53,56,1,2.4f,true},{47,51,F,H-.3f}});
  k.WallZ(-10.75f,8,40,40.25f,0,H,{{-3,1,0,2.7f},{-9,-6,1,2.4f,true},{3,6,1,2.4f,true}});
  k.WallZ(-10.75f,8,57.75f,58,0,H,{{-3,1,0,2.7f},{-9,-6,1,2.4f,true},{3,6,1,2.4f,true}});
  for(float z : {-10.88f,8.12f}) for(float x : {43.5f,54.5f}) WindowX(k,x,z,1,3,1.4f,0x5c3c29);
  for(float x : {43.5f,54.5f}) WindowX(k,x,-10.88f,4,3,1.7f,0x5c3c29);
  for(float x : {40.12f,57.88f}) for(float z : {-7.5f,4.5f}) WindowZ(k,x,z,1,3,1.4f,0x5c3c29);
  k.FloorFinish(40.25f,-10.75f,57.75f,8,.025f,MAT_PARQUET,0x98734a);
  // Horseshoe loft around the double-height taproom, with two broad stairs.
  k.Slab(40.3f,-10.7f,57.7f,-7.6f,F,.20f,MAT_WOOD,0x9c774e);
  k.Slab(40.3f,-7.6f,43.3f,8,F,.20f,MAT_WOOD,0x9c774e);
  k.Slab(54.7f,-7.6f,57.7f,8,F,.20f,MAT_WOOD,0x9c774e);
  k.Slab(43.3f,4.8f,54.7f,8,F,.20f,MAT_WOOD,0x9c774e);
  for(float x : {44.7f,53.3f}) {
    k.Style(MAT_WOOD,0x704a32);k.Stairs({x,0,2},3,2.5f,.2f,15,.64f,0);
    SlopingRail(k,{x,.2f,1.7f},{x,F,-7.6f},1.3f,0x705034);
  }
  k.Railing({43.3f,F,-7.6f},{43.3f,F,4.8f},1,false);
  k.Railing({54.7f,F,-7.6f},{54.7f,F,4.8f},1,false);
  k.Railing({46.1f,F,-7.6f},{51.9f,F,-7.6f},1,false);
  k.Railing({43.3f,F,4.8f},{54.7f,F,4.8f},1,false);
  // Usable balcony, supported by timber posts and an Alpine gable above it.
  k.Slab(39.5f,8.27f,58.5f,11.3f,F,.23f,MAT_WOOD,0x927147);
  k.Slab(47,7.95f,51,8.32f,F+.02f,.18f,MAT_WOOD,0x927147);
  for(float x : {39.7f,46.0f,52.0f,58.3f}) {
    k.Style(MAT_WOOD,0x5b3b28);k.Solid({x-.12f,0,10.8f},{x+.12f,6.55f,11.05f});
  }
  k.Railing({39.5f,F,11.3f},{58.5f,F,11.3f},1,false);
  k.Railing({39.5f,F,8},{39.5f,F,11.3f},1,false);k.Railing({58.5f,F,8},{58.5f,F,11.3f},1,false);
  // Gabled roof is a closed shell; the void cannot hide a player.
  k.Slab(39.5f,-11.5f,58.5f,11.5f,H+.2f,.20f,MAT_WOOD,0x604432);
  k.Style(MAT_WOOD,0x593f2d);
  ExtrudeConvex(k.ob,{{39.2f,H+.215f},{58.8f,H+.215f},{49,H+3.4f}},-11.8f,11.8f);
  k.Clip({39.2f,H,-11.8f},{58.8f,H+3.4f,11.8f},COL_PLAYER);
  // Braces and contrasting plaster panels give the hut a readable timber frame.
  for(float z : {-11.1f,8.33f}) {
    k.Style(MAT_WOOD,0x523823);
    for(float x : {40.1f,46.1f,51.9f,57.9f}) k.Visual({x-.10f,0,z-.08f},{x+.10f,H,z+.08f});
    k.Visual({40,2.85f,z-.08f},{58,3.12f,z+.08f});
  }
  k.Style(MAT_WOOD,0xc09b65);
  for(float x : {40.2f,43.2f,46.1f,51.9f,54.8f,57.8f}) Beam(k,{x,6.5f,11.86f},{49,9.55f,11.86f},.075f);
  Clock(k,{49,7.5f,11.95f},.7f,0x6e442b);
  k.Sign("ZUR KECKEN ALM",{49,5.8f,11.94f},0,.15f,0xeace8e,0,false);
  k.Sign("STUBE / EMPORE",{49,2.85f,8.5f},0,.045f,CREAM);
  for(Vec3 p : {Vec3(42,0,4.2f),Vec3(56,0,4.2f),Vec3(48.8f,0,-5.8f),Vec3(49,F,-9.3f),Vec3(44,F,9.5f),Vec3(54,F,9.5f)}) Table(k,p,true);
  k.Prop("holzfass",{45.7f,0,6.8f},0);k.Prop("holzfass",{52.3f,0,6.8f},0);k.Prop("masskrug",{45.7f,1.05f,6.8f},0);
  k.Prop("trachtenpuppe",{41.5f,F,-9.2f},0);k.Prop("gartenzwerg",{56.5f,F,-9.2f},0);
  k.Prop("tuba",{42,F,6.5f},0);k.Prop("notenstaender",{42,F,5.3f},.3f);
  for(float x : {41.5f,56.5f}) for(float z : {-9.2f,6.5f}) {
    k.Prop("holzfass",{x,0,z},.2f);k.Prop("getraenkekiste",{x+.8f,0,z},0);
  }
  for(float z : {-5.0f,4.0f}) {k.CeilingLight({49,6.15f,z},11,{2.6f,1.6f,.8f},.5f);Bulbs(k,{40.5f,5.5f,z},{57.5f,5.5f,z});}
  Bulbs(k,{39.8f,5.8f,11},{58.2f,5.8f,11});
  k.Zone({40.3f,0,-10.7f},{57.7f,F,8},.24f,"Almhuette / Stube",3);
  k.Zone({40.3f,F,-10.7f},{57.7f,H,8},.32f,"Almhuette / Empore",4);
  k.Zone({39.5f,F,8},{58.5f,6,11.3f},1,"Almbalkon",4);
  // A short garden trail connects the side entrances to the main fair avenue.
  k.FloorFinish(34,12,59,18,.025f,MAT_TILE,0xa89373);
  for(float x : {37.0f,49.0f,58.0f}) {
    k.Prop("biertisch",{x,0,22.5f},0);k.Prop("bierbank",{x,0,23.3f},0);
    k.Prop("masskrug",{x,.79f,22.5f},0);k.Prop("ballons",{x+2.2f,0,23},0);
  }
  k.w.cameraPath.insert(k.w.cameraPath.end(),{{31,9,23},{49,4,4},{49,1.9f,3},{49,4,-6},
      {44,4.8f,10},{29,4,21},{49,8,29},{49,6,8}});
}

void Pedestal(MapKit& k,Vec3 p,const char* prop,u32 stone=0xb7a58c) {
  k.Style(MAT_MARBLE,stone);k.Solid(p+Vec3(-.5f,0,-.5f),p+Vec3(.5f,.68f,.5f));
  k.Style(MAT_SANDSTONE,CREAM);k.Visual(p+Vec3(-.55f,.68f,-.55f),p+Vec3(.55f,.77f,.55f));
  k.Clip(p+Vec3(-.5f,.68f,-.5f),p+Vec3(.5f,.77f,.5f),COL_SOLID);
  k.Prop(prop,p+Vec3(0,.77f,0),0);
  k.Style(MAT_METAL,0x826339);k.Visual(p+Vec3(-.20f,.43f,.51f),p+Vec3(.20f,.56f,.53f));
}

void Painting(MapKit& k,float x,float z,float y,int scene) {
  // A broad, layered frame with a tiny original landscape; plane faces west.
  k.Style(MAT_GOLD,0xa58443);k.Visual({x-.14f,y,z-1.02f},{x,y+1.6f,z+1.02f});
  k.Style(MAT_WOOD,0x513f2e);k.Visual({x-.18f,y+.09f,z-.93f},{x-.15f,y+1.51f,z+.93f});
  k.Style(MAT_PLAIN,scene%2 ? 0x798b97 : 0xbaa48a);k.Visual({x-.22f,y+.17f,z-.84f},{x-.19f,y+1.43f,z+.84f});
  k.Style(MAT_PLAIN,scene%2 ? 0x485c46 : 0x7b7562);k.Visual({x-.25f,y+.17f,z-.84f},{x-.23f,y+.63f,z+.84f});
  k.Style(MAT_PLAIN,0x4a575b);
  k.ob.Push(Translation({x-.27f,y+.18f,z}));
  if(scene%3==0) {
    k.ob.Tri({0,0,-.8f},{0,0,.65f},{0,.91f,-.2f});
    k.ob.Tri({-.01f,0,-.3f},{-.01f,0,.8f},{-.01f,.65f,.43f});
  } else if(scene%3==1) {
    k.ob.Box({-.015f,0,-.64f},{0,.72f,-.2f});
    k.ob.Box({-.015f,0,-.18f},{0,1.05f,.04f});
    k.ob.Box({-.015f,0,.06f},{0,.53f,.63f});
  } else {
    k.Style(MAT_PLAIN,0x8ca0aa);k.ob.Box({-.01f,.02f,-.8f},{0,.32f,.8f});
    k.Style(MAT_PLAIN,0xe0cbaa);k.ob.Tri({-.02f,.35f,-.45f},{-.02f,.35f,.3f},{-.02f,1,.22f});
  }
  k.ob.Pop();
  k.Style(MAT_PLAIN,0xe2ce9c);k.ob.Sphere({x-.27f,y+1.16f,z+.51f},.12f,12,8);
}

void MuseumQuarter(MapKit& k) {
  constexpr float F=3.8f;
  // The former long empty gallery gains an upper collection and two end stairs.
  k.Slab(27.1f,-18,33, -16,F,.15f,MAT_PARQUET,0xa18a61);
  k.Slab(27.1f,-6.4f,33,6.4f,F,.18f,MAT_PARQUET,0xa18a61);
  k.Slab(27.1f,16,33,18,F,.15f,MAT_PARQUET,0xa18a61);
  for(float side : {-1.0f,1.0f}) {
    float z0=side<0 ? -16 : 6.4f, z1=side<0 ? -6.4f : 16;
    k.Slab(27.1f,z0,28.9f,z1,F,.18f,MAT_PARQUET,0xa18a61);
    k.Slab(32.1f,z0,33,z1,F,.18f,MAT_PARQUET,0xa18a61);
    k.Style(MAT_SANDSTONE,0xcebd99);
    k.Stairs({30.5f,0,side*6.5f},side<0 ? 3 : 2,2.8f,.2f,19,.5f,0);
    SlopingRail(k,{30.5f,.2f,side*6.75f},{30.5f,F,side*16},1.44f,0x8b7959);
    k.Railing({28.9f,F,z0},{28.9f,F,z1},.95f,false);
  }
  k.Railing({28.9f,F,-6.4f},{32.1f,F,-6.4f},.95f,false);
  k.Railing({28.9f,F,6.4f},{32.1f,F,6.4f},.95f,false);
  // Transverse ceiling ribs, wall bases and repeated frames give each bay a scale.
  for(float z=-16;z<=16;z+=4) {
    k.Style(MAT_SANDSTONE,0xd8c9ab);k.Visual({27.16f,3.42f,z-.09f},{32.94f,3.58f,z+.09f});
    k.Visual({27.16f,7.28f,z-.1f},{32.94f,7.49f,z+.1f});
    k.Style(MAT_MARBLE,0xa89b83);k.Visual({32.69f,0,z-.10f},{32.94f,3.4f,z+.10f});
    Painting(k,32.88f,z,4.95f,(int)(z/4+4));
  }
  k.Style(MAT_MARBLE,0x9b8c72);
  k.Visual({32.73f,0,-18},{32.97f,.23f,18});
  for(float z : {-4.3f,4.3f}) {
    Pedestal(k,{31.8f,F,z},z<0 ? "zwinger_vase" : "globus");
    Pedestal(k,{31.8f,0,z},z<0 ? "bueste" : "vase");
    k.Prop("sitzbank",{28.1f,0,z},PI*.5f);
  }
  k.Prop("zwinger_fernrohr",{28.05f,F,0},PI*.5f);
  k.Prop("zwinger_kunstkiste",{31.8f,0,17.1f},0);
  for(float y : {3.35f,7.2f}) for(float z : {-12.0f,0.0f,12.0f}) k.CeilingLight({30,y,z},8,{1.7f,1.55f,1.25f},.28f);
  k.Sign("KUNST / ASTRONOMIE",{32.45f,6.9f,0},-PI*.5f,.055f,0xe1c18b);
  k.Zone({27.2f,F-.1f,-18},{32.9f,7.5f,18},.42f,"Galerie / Obergeschoss",4);
  // Small pavilion collections keep the cross-shaped door axes clear.
  for(float x : {-27.0f,27.0f}) for(float z : {-22.0f,22.0f}) {
    k.FloorFinish(x-3.8f,z-3.8f,x+3.8f,z+3.8f,.025f,MAT_MARBLE,0xb6a68a);
    for(float dx : {-2.9f,2.9f}) {
      // Rear corners are already furnished in the western pavilions.
      if(x>0) Pedestal(k,{x+dx,0,z-2.8f},dx<0 ? "zwinger_vase" : "globus");
      k.Style(MAT_SANDSTONE,0xe1cfaa);
      k.Visual({x+dx-.08f,3.45f,z-3.65f},{x+dx+.08f,3.64f,z+3.65f});
    }
    k.Style(MAT_MARBLE,0x846f50);k.ob.Ring({x,.045f,z},1.05f,1.15f,40,true);
    k.Sign(x<0 ? (z<0 ? "ZEIT & STERNE" : "WEISSES GOLD") : "SKULPTUREN",{x,3.15f,z+3.77f},PI,.052f,0x856333,0,false);
  }
  // Architectural silhouette: cornice bands, roof ribs and courtyard-side urns.
  for(Vec2 p : {Vec2(-27,-22),Vec2(-27,22),Vec2(27,-22),Vec2(27,22),Vec2(0,-39),Vec2(0,39)}) {
    for(float y : {4.45f,7.14f}) {
      k.Style(MAT_SANDSTONE,0xd4c4a4);
      k.Visual({p.x-2.52f,y,p.y-2.52f},{p.x+2.52f,y+.14f,p.y+2.52f});
    }
    for(float dx : {-1.8f,1.8f}) {
      k.Style(MAT_SANDSTONE,0xd4c4a4);
      k.ob.Frustum({p.x+dx,7.55f,p.y+2.12f},.16f,.11f,.34f,12);
      k.ob.Ellipsoid({p.x+dx,8.0f,p.y+2.12f},{.23f,.28f,.23f},12,8);
    }
  }
  // An orangery café beyond the arcade adds an intimate destination off the main axis.
  k.FloorFinish(-24,41.5f,-8,49.5f,.025f,MAT_TILE,0xbbaa8b);
  k.Style(MAT_SANDSTONE,0xd3bf98);
  for(float x : {-23.0f,-18.5f,-14.0f,-9.5f}) for(float z : {42.0f,49.0f}) {
    k.Solid({x-.23f,0,z-.23f},{x+.23f,.35f,z+.23f});
    k.ob.Frustum({x,.35f,z},.17f,.13f,2.8f,16);
    k.Clip({x-.17f,.35f,z-.17f},{x+.17f,3.15f,z+.17f},COL_SOLID);
    k.ob.BoxC({x,3.27f,z},{.3f,.12f,.3f});
  }
  k.Style(MAT_METAL,0x637c6b);
  for(float z : {42.0f,49.0f}) k.Visual({-23.4f,3.4f,z-.15f},{-9.1f,3.61f,z+.15f});
  for(float x : {-23.0f,-20.75f,-18.5f,-16.25f,-14.0f,-11.75f,-9.5f}) {
    Beam(k,{x,3.6f,42},{x,4.55f,45.5f},.07f);Beam(k,{x,4.55f,45.5f},{x,3.6f,49},.07f);
  }
  // Transparent flat panels beneath the airy roof ribs, with wide open sides.
  k.Glass({-23.3f,3.6f,42},{-9.2f,3.68f,49},0xa4bec0,.24f);
  for(float x : {-21.0f,-11.7f}) for(float z : {43.8f,47.2f}) Table(k,{x,0,z});
  for(Vec3 p : {Vec3(-24,0,40),Vec3(-8,0,40),Vec3(-24,0,50),Vec3(-8,0,50)}) k.Prop("zwinger_orange",p,0);
  k.Prop("kaffeemaschine",{-16, .82f,48},0);
  k.Style(MAT_WOOD,0x806846);k.Solid({-17.5f,0,47.5f},{-14.5f,.82f,48.7f});
  k.Prop("wasserflasche",{-15,.82f,48},0);k.Prop("kaffeebecher",{-16.8f,.82f,48},0);
  k.Sign("ORANGERIE-CAFE",{-16,3.05f,41.82f},PI,.10f,0x756044,0,false);
  k.Zone({-24,0,41.5f},{-8,4,49.5f},.8f,"Orangerie-Cafe",3);
  // Restoration corner behind the opposite arcade: tools, canvas and crates.
  for(float x : {12.0f,19.0f}) {
    k.Prop("werkbank",{x,0,45.5f},0);k.Prop("werkzeugkiste",{x-.5f,.91f,45.5f},0);
    k.Prop("zwinger_vase",{x+.5f,.91f,45.5f},0);k.Prop("staffelei",{x-1.2f,0,48},PI);
    k.Prop("zwinger_kunstkiste",{x+2.0f,0,48},0);k.Prop("eimer",{x+1.8f,0,44.3f},0);
  }
  k.Style(MAT_WOOD,0x66553b);
  for(float x : {10.5f,22.0f}) k.Solid({x-.08f,0,49.7f},{x+.08f,3.2f,49.86f});
  k.Sign("RESTAURIERUNG",{16.25f,2.8f,49.67f},PI,.08f,CREAM);
  k.Zone({9.5f,0,43},{23,4,50},1,"Restaurierungshof",3);
  // Basin niches and paired pilasters make the Nymphenbad read as a grotto.
  for(float x : {21.5f,23.3f,31.7f,33.5f}) {
    k.Style(MAT_SANDSTONE,0xb5a184);k.Visual({x-.16f,0,-43.94f},{x+.16f,5.5f,-43.58f});
    k.Style(MAT_SANDSTONE,0xd3bf98);k.ob.BoxC({x,5.4f,-43.56f},{.28f,.14f,.28f});
  }
  k.Style(MAT_SANDSTONE,0xd3bf98);k.Visual({19.6f,5.55f,-44.1f},{35.8f,5.82f,-43.6f});
  k.w.cameraPath.insert(k.w.cameraPath.end(),{{25,12,12},{0,2,0},{28.1f,5.5f,5},{32.7f,5.2f,-3},
      {-6,5,46},{-20,1.6f,45},{14,2.2f,41},{19,1,48}});
}
}

void BuildMapDistricts(World& world) {
  MapKit k(world);
  if(world.id=="paketzentrum") ParcelCampus(k);
  else if(world.id=="nudelfabrik") FactoryQuarter(k);
  else if(world.id=="oktoberfest") AlpineLodge(k);
  else if(world.id=="zwinger") MuseumQuarter(k);
}
