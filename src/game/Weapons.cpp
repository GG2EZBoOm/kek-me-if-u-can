#include "game/Weapons.h"

const WeaponInfo& GetWeapon(int w) {
  static const WeaponInfo base[WPN_COUNT] = {
    {"MP",30,1,13,.085f,1.9f,.016f,.004f,4,true},
    {"Schrotflinte",6,8,10,.85f,2.4f,.07f,.05f,1.25f,false},
    {"Granate",1,0,0,.8f,0,0,0,0,false}
  };
  return base[ClampI(w,0,WPN_COUNT-1)];
}
int WeaponPart(u16 b,int slot) { return slot>=0 && slot<WS_COUNT ? (b>>(slot*2))&3 : 0; }
int WeaponPartCount(int slot) { return slot==WS_OPTIC ? 4 : 3; }
const char* WeaponSlotName(int slot) {
  static const char* names[]={"Visierung","Lauf","Mündung","Schulterstütze","Magazin","Vordergriff"};
  return names[ClampI(slot,0,WS_COUNT-1)];
}
const char* WeaponPartName(int w,int slot,int n) {
  static const char* names[2][WS_COUNT][4]={
    {{"Kimme / Korn","Reflex R1","Holo H2","Prisma 2,5×"},
     {"Standard 230 mm","Kompakt 160 mm","Präzision 320 mm"},
     {"Mündungsfeuerdämpfer","Kompensator","Schalldämpfer"},
     {"Teleskopschaft","Leichtschaft","Präzisionsschaft"},
     {"30 Schuss","20 Schuss kurz","50 Schuss Trommel"},
     {"Handschutz","Vertikalgriff","Winkelgriff"}},
    {{"Korn / Ghost Ring","Reflex R1","Holo H2","Prisma 2,5×"},
     {"Standard 470 mm","Kompakt 350 mm","Sport 610 mm"},
     {"Offene Mündung","Enger Choke","Schalldämpfer"},
     {"Walnussschaft","Leichter Polymer-Schaft","Gepolsterter Schaft"},
     {"6 Patronen Rohr","4 Patronen kurz","8 Patronen Verlängerung"},
     {"Walnuss-Pumpe","Taktische Pumpe","Leichte Polymer-Pumpe"}}
  };
  return names[ClampI(w,0,1)][ClampI(slot,0,WS_COUNT-1)][ClampI(n,0,WeaponPartCount(slot)-1)];
}
const char* WeaponPartDescription(int w,int slot,int n) {
  static const char* desc[WS_COUNT][4]={
    {"Leicht und offen. 1,25× Zielansicht.","Freies Sichtfeld, roter Punkt. 1,45×.","Großes Ringabsehen. Schwerer, 1,65×.","2,5× Vergrößerung. Langsamer im Anschlag."},
    {"Ausgewogene Länge und Präzision.","Schneller im Anschlag. Mehr Streuung und Rückstoß.","Engere Streuung. Mehr Gewicht, langsamer im Anschlag."},
    {"Geringes Gewicht, voller Schussknall.","Weniger Rückstoß; lauter. Bei Schrot: engeres Trefferbild.","Leiser, weniger Blitz. Schwer und lang; bleibt hörbar."},
    {"Ausgewogener Anschlag und Rückstoß.","Schneller im Anschlag, dafür mehr Rückstoß.","Ruhiger beim Feuern. Schwerer, langsamer im Anschlag."},
    {"Standardkapazität und Nachladetempo.","Weniger Munition. Leichter und schneller nachgeladen.","Mehr Munition. Schwerer, langsamer nachgeladen."},
    {"Kein Zusatzgewicht; normaler Rückstoß.","Weniger Rückstoß; etwas schwerer im Anschlag.","Schnellerer Anschlag; moderate Rückstoßkontrolle."}
  };
  return desc[ClampI(slot,0,WS_COUNT-1)][ClampI(n,0,WeaponPartCount(slot)-1)];
}
static u16 Put(u16 b,int s,int n) { return (u16)((b&~(3u<<(s*2)))|(n<<(s*2))); }
const char* WeaponPartBlock(int w,u16 b,int slot,int n) {
  b=Put(b,slot,n);
  if(w==WPN_SMG && WeaponPart(b,WS_BARREL)==1 && WeaponPart(b,WS_GRIP)==1)
    return "Vertikalgriff braucht den Standard- oder Präzisions-Handschutz.";
  if(w==WPN_SHOTGUN && WeaponPart(b,WS_BARREL)==1 && WeaponPart(b,WS_MAGAZINE)==2)
    return "Die 8er-Verlängerung passt nur unter Standard- oder Sportlauf.";
  return nullptr;
}
u16 ValidateWeaponBuild(int w,u16 b) {
  if(w<0 || w>=WPN_GRENADE) return 0;
  b &= 0x0fff;
  for(int s=0;s<WS_COUNT;s++) if(WeaponPart(b,s)>=WeaponPartCount(s)) b=Put(b,s,0);
  if(WeaponPart(b,WS_BARREL)==1) {
    if(w==WPN_SMG && WeaponPart(b,WS_GRIP)==1) b=Put(b,WS_GRIP,0);
    if(w==WPN_SHOTGUN && WeaponPart(b,WS_MAGAZINE)==2) b=Put(b,WS_MAGAZINE,0);
  }
  return b;
}
u16 WithWeaponPart(int w,u16 b,int s,int n) {
  b=ValidateWeaponBuild(w,b);
  if(s<0 || s>=WS_COUNT || n<0 || n>=WeaponPartCount(s) || WeaponPartBlock(w,b,s,n)) return b;
  return ValidateWeaponBuild(w,Put(b,s,n));
}
u16 WeaponPreset(int w,int preset) {
  // Standard, compact, stable, quiet. Compatible by construction.
  static const int parts[4][WS_COUNT]={{0,0,0,0,0,0},{1,1,0,1,1,2},{2,2,1,2,2,1},{1,0,2,0,0,2}};
  u16 b=0; for(int s=0;s<WS_COUNT;s++) b=Put(b,s,parts[ClampI(preset,0,3)][s]);
  return ValidateWeaponBuild(w,b);
}
WeaponStats GetWeaponStats(int w,u16 b) {
  WeaponStats s; static_cast<WeaponInfo&>(s)=GetWeapon(w);
  if(w>=WPN_GRENADE) return s;
  b=ValidateWeaponBuild(w,b); s.weight=w==WPN_SMG?2.7f:3.4f; s.adsTime=w==WPN_SMG?.24f:.30f;
  int o=WeaponPart(b,WS_OPTIC), barrel=WeaponPart(b,WS_BARREL), muzzle=WeaponPart(b,WS_MUZZLE);
  int stock=WeaponPart(b,WS_STOCK), mag=WeaponPart(b,WS_MAGAZINE), grip=WeaponPart(b,WS_GRIP);
  const float zoom[]={1.25f,1.45f,1.65f,2.5f}, mass[]={0,.12f,.24f,.36f};
  s.zoom=zoom[o]; s.weight+=mass[o]; s.adsTime+=mass[o]*.14f;
  if(barrel==1) { s.weight-=.35f; s.adsTime-=.035f; s.recoil*=1.15f; s.spreadHip*=1.18f; s.spreadAim*=1.25f; }
  if(barrel==2) { s.weight+=.45f; s.adsTime+=.045f; s.recoil*=.94f; s.spreadHip*=.90f; s.spreadAim*=.78f; }
  if(muzzle==1) { s.weight+=.12f; s.recoil*=w==WPN_SMG?.80f:.94f; s.soundRadius=75;
    if(w==WPN_SHOTGUN) { s.spreadHip*=.78f; s.spreadAim*=.78f; } }
  if(muzzle==2) { s.weight+=w==WPN_SMG?.42f:.65f; s.adsTime+=.05f; s.recoil*=.90f; s.soundRadius=30; s.suppressed=true; }
  if(stock==1) { s.weight-=.30f; s.adsTime-=.025f; s.recoil*=1.18f; }
  if(stock==2) { s.weight+=.35f; s.adsTime+=.03f; s.recoil*=.82f; }
  if(mag==1) { s.magazine=w==WPN_SMG?20:4; s.weight-=.20f; s.adsTime-=.015f; s.reload*=.87f; }
  if(mag==2) { s.magazine=w==WPN_SMG?50:8; s.weight+=w==WPN_SMG?.60f:.22f; s.adsTime+=.04f; s.reload*=1.28f; }
  if(grip==1) { s.weight+=.18f; s.adsTime+=.015f; s.recoil*=.86f; }
  if(grip==2) { s.weight+=.08f; s.adsTime-=.025f; s.recoil*=.95f; }
  s.adsTime=Clamp(s.adsTime,.14f,.55f); s.shellTime=.55f*(mag==1?.92f:mag==2?1.06f:1.0f);
  // Shotgun reload is individual shells, not a magical full tube after a magazine animation.
  if(w==WPN_SHOTGUN) s.reload=.55f+s.shellTime*s.magazine;
  return s;
}
float WeaponMuzzleZ(int w,u16 b) {
  int barrel=WeaponPart(b,WS_BARREL);
  float z=w==WPN_SHOTGUN?-.65f:-.43f;
  z+=barrel==1?.12f:barrel==2?-.14f:0;
  int muzzle=WeaponPart(b,WS_MUZZLE);
  return z-(muzzle==2?(w==WPN_SMG?.19f:.24f):muzzle==1?.045f:0);
}
float WeaponSightHeight(int w,u16 b) { return WeaponPart(b,WS_OPTIC)==0?.067f:.105f; }
