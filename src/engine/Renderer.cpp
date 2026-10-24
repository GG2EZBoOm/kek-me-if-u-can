#include "engine/Renderer.h"
#include "engine/WorldText.h"

static const int MAX_LIGHTS = 96;

// ---------------------------------------------------------------------------------------------
// Shader-Quelltexte
// ---------------------------------------------------------------------------------------------

static const char* kNoiseGLSL = R"(
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec3 p) {
  vec3 i = floor(p); vec3 f = fract(p); f = f * f * (3.0 - 2.0 * f);
  float n000 = hash13(i), n100 = hash13(i + vec3(1,0,0)), n010 = hash13(i + vec3(0,1,0)), n110 = hash13(i + vec3(1,1,0));
  float n001 = hash13(i + vec3(0,0,1)), n101 = hash13(i + vec3(1,0,1)), n011 = hash13(i + vec3(0,1,1)), n111 = hash13(i + vec3(1,1,1));
  return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y), mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}
float fbm(vec3 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; } return s; }
)";

static const char* kSkyGLSL = R"(
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyGround;
uniform float uClouds;
uniform float uTime;
vec3 skyColor(vec3 d, bool withSun) {
  float y = d.y;
  vec3 c = mix(uSkyHorizon, uSkyZenith, pow(clamp(y, 0.0, 1.0), 0.55));
  c = mix(c, uSkyGround, smoothstep(0.0, -0.12, y));
  float sd = max(dot(d, uSunDir), 0.0);
  c += uSunColor * 0.06 * pow(sd, 6.0);
  if (withSun) {
    c += uSunColor * 0.25 * pow(sd, 64.0);
    c += uSunColor * 6.0 * smoothstep(0.9993, 0.9997, sd);
    if (y > 0.0 && uClouds > 0.0) {
      vec2 uv = d.xz / (y + 0.08) * 1.6 + vec2(uTime * 0.01, uTime * 0.004);
      float n = fbm(vec3(uv, uTime * 0.01));
      float cl = smoothstep(1.0 - uClouds * 0.75, 1.15 - uClouds * 0.6, n + 0.25) * smoothstep(0.0, 0.18, y);
      vec3 cc = mix(vec3(0.92, 0.94, 0.98), uSunColor * 0.32, 0.3) * (0.75 + 0.35 * sd);
      c = mix(c, cc, cl * 0.85);
    }
  }
  return c;
}
)";

static const char* kLitVS = R"(
in vec3 aPos; in vec3 aNormal; in vec4 aColor; in vec4 aAux;
in vec2 aTexUV; out vec2 vTexUV;
in vec4 aInst0; in vec4 aInst1; in vec4 aInst2;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vWorld; out vec3 vNormal; out vec3 vPat; out vec3 vPatN;
out vec4 vColor; out vec4 vAux; out vec4 vInst;
void main() {
  float c = cos(aInst0.w), s = sin(aInst0.w);
  vec3 lp = aPos * aInst1.z;
  vec3 p = vec3(c * lp.x + s * lp.z, lp.y, -s * lp.x + c * lp.z) + aInst0.xyz;
  vec3 n = vec3(c * aNormal.x + s * aNormal.z, aNormal.y, -s * aNormal.x + c * aNormal.z);
  vec4 wp = uModel * vec4(p, 1.0);
  vWorld = wp.xyz;
  vTexUV = aTexUV;
  vNormal = mat3(uModel) * n;
  vPat = aPos; vPatN = aNormal;
  vColor = vec4(mix(aColor.rgb, aColor.rgb * aInst2.rgb, aInst2.a), aColor.a * aInst1.w);
  vAux = aAux;
  vInst = aInst1;
  gl_Position = uViewProj * wp;
}
)";

static const char* kLitFS = R"(
in vec3 vWorld; in vec3 vNormal; in vec3 vPat; in vec3 vPatN;
in vec2 vTexUV;
in vec4 vColor; in vec4 vAux; in vec4 vInst;
layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;
uniform vec3 uCamPos;
uniform sampler2D uSignAtlas;
uniform vec3 uSkyAmb; uniform vec3 uGroundAmb; uniform vec3 uIndoorAmb;
uniform vec3 uFogColor; uniform float uFogDensity;
uniform sampler2DShadow uShadow0; uniform sampler2DShadow uShadow1;
uniform mat4 uShadowMat0; uniform mat4 uShadowMat1;
uniform vec2 uShadowTexel;
uniform int uLightCount;
uniform int uGlass;
uniform vec3 uHighlightColor;
layout(std140) uniform Lights { vec4 uLightPos[MAX_LIGHTS]; vec4 uLightCol[MAX_LIGHTS]; };

float shadowLookup(sampler2DShadow tex, mat4 m, vec3 wp, float texel) {
  vec4 sp = m * vec4(wp, 1.0);
  vec3 p = sp.xyz / sp.w * 0.5 + 0.5;
  if (p.x < 0.0 || p.y < 0.0 || p.x > 1.0 || p.y > 1.0 || p.z > 1.0) return -1.0;
  float s = 0.0;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) s += texture(tex, vec3(p.xy + vec2(x, y) * texel, p.z - 0.0004));
  return s / 9.0;
}
float shadowAt(vec3 wp, vec3 n) {
  float s = shadowLookup(uShadow0, uShadowMat0, wp + n * 0.03, uShadowTexel.x);
  if (s < 0.0) s = shadowLookup(uShadow1, uShadowMat1, wp + n * 0.08, uShadowTexel.y);
  return s < 0.0 ? 1.0 : s;
}
vec2 planarUV(vec3 p, vec3 n) {
  vec3 a = abs(n);
  if (a.y >= a.x && a.y >= a.z) return p.xz;
  if (a.x >= a.z) return p.zy;
  return p.xy;
}

// Integral eines periodischen Streifens: Fugen bleiben auch unter einem Pixel breit ruhig.
float stripeIntegral(float x, float width) {
  return floor(x) * width + min(fract(x), width);
}
float filteredStripe(float x, float width, float footprint) {
  float fw = max(footprint, 0.0001);
  float p = x + width * 0.5;
  return clamp((stripeIntegral(p + fw * 0.5, width) - stripeIntegral(p - fw * 0.5, width)) / fw, 0.0, 1.0);
}

void main() {
  vec3 N = normalize(vNormal);
  vec3 V = normalize(uCamPos - vWorld);
  int mat = int(vAux.x * 255.0 + 0.5);
  vec3 alb = vColor.rgb;
  alb *= alb;  // Vertexfarben sind sRGB
  if (mat == 24) {
    float ink = texture(uSignAtlas,vTexUV).r;
    if (vAux.w < 0.5) {
      if (ink < 0.45) discard;
    } else {
      // Match the matte board's color conversion in the restored renderer.
      vec3 board = vec3(29.0,36.0,48.0)/255.0;
      alb = mix(board*board,alb,ink);
    }
  }
  float spec = 0.08, shin = 24.0, refl = 0.0;
  vec3 emit = vec3(0.0);
  vec3 P = vPat;
  vec2 uv = planarUV(P, vPatN);

  if (mat == 1) {  // Holz
    float g = vnoise(vec3(uv.x * 2.5, uv.y * 28.0, P.y * 3.0));
    float g2 = vnoise(vec3(uv.x * 0.6, uv.y * 4.0, 1.0));
    alb *= 0.78 + 0.3 * g + 0.12 * g2; spec = 0.18; shin = 32.0;
  } else if (mat == 2) {  // Fliesen
    vec2 q = uv / 0.3; vec2 f = fract(q);
    float e = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
    alb *= 0.92 + 0.12 * hash12(floor(q));
    alb = mix(alb * 0.55, alb, smoothstep(0.015, 0.035, e)); spec = 0.6; shin = 80.0; refl = 0.05;
  } else if (mat == 3) {  // Gras
    float n = fbm(P * 0.9);
    alb *= 0.6 + 0.65 * n;
    alb *= 0.8 + 0.35 * vnoise(P * 22.0);
    spec = 0.02;
  } else if (mat == 4) {  // Sandstein-Quader
    vec2 q = uv / vec2(1.25, 0.55); float row = floor(q.y);
    q.x += mod(row, 2.0) * 0.5;
    vec2 f = fract(q); vec2 id = floor(q);
    float ex = min(f.x, 1.0 - f.x) * 1.25, ey = min(f.y, 1.0 - f.y) * 0.55;
    float m = smoothstep(0.004, 0.018, min(ex, ey));
    float w = fbm(P * 2.5);
    alb *= (0.88 + 0.16 * hash12(id + row * 7.31)) * (0.82 + 0.35 * w);
    alb = mix(alb * 0.62, alb, m); spec = 0.05;
  } else if (mat == 5) {  // Teppich
    alb *= 0.88 + 0.2 * vnoise(P * 70.0) + 0.06 * vnoise(P * 4.0); spec = 0.01;
  } else if (mat == 6) {  // Metall
    alb *= 0.92 + 0.1 * vnoise(vec3(uv * vec2(2.0, 120.0), 0.0)); spec = 0.9; shin = 72.0; refl = 0.18;
  } else if (mat == 7) {  // Stoff
    alb *= 0.86 + 0.1 * (sin(uv.x * 420.0) * sin(uv.y * 420.0) * 0.5 + 0.5) + 0.08 * vnoise(P * 9.0); spec = 0.02;
  } else if (mat == 8) {  // Beton
    alb *= 0.78 + 0.3 * fbm(P * 3.0);
    alb *= 1.0 - 0.25 * step(0.94, hash13(floor(P * 55.0))); spec = 0.05;
  } else if (mat == 10) {  // Bildschirm
    float sl = 0.85 + 0.15 * sin(vWorld.y * 500.0);
    emit = alb * 2.2 * sl; alb *= 0.15; spec = 0.8; shin = 128.0;
  } else if (mat == 11) {  // Blätter
    alb *= 0.55 + 0.7 * fbm(P * 5.0); spec = 0.04;
  } else if (mat == 12) {  // Marmor
    float v = abs(sin(P.x * 2.3 + P.z * 1.6 + P.y * 0.8 + fbm(P * 1.1) * 7.0));
    alb = mix(alb, alb * 0.62, pow(1.0 - v, 28.0));
    float v2 = abs(sin(P.z * 3.1 - P.x * 0.9 + fbm(P * 2.3 + 5.0) * 5.0));
    alb = mix(alb, alb * 0.78, pow(1.0 - v2, 40.0));
    alb *= 0.95 + 0.07 * vnoise(P * 6.0); spec = 0.55; shin = 110.0; refl = 0.1;
  } else if (mat == 13) {  // Kunststoff
    spec = 0.45; shin = 56.0;
  } else if (mat == 14) {  // Papier
    alb *= 0.95 + 0.06 * vnoise(P * 35.0); spec = 0.04;
  } else if (mat == 15) {  // Wasser
    float t = uTime;
    vec3 wn = vec3(vnoise(vec3(P.xz * 1.8, t * 0.6)) - 0.5, 0.0, vnoise(vec3(P.xz * 1.8 + 9.0, t * 0.6)) - 0.5);
    N = normalize(N + wn * 0.5); spec = 1.2; shin = 160.0; refl = 0.55;
  } else if (mat == 16) {  // Spiegel / Chrom
    alb *= 0.25; spec = 1.6; shin = 300.0; refl = 0.85;
  } else if (mat == 17) {  // Asphalt
    alb *= 0.7 + 0.35 * vnoise(P * 18.0) * fbm(P * 1.5) + 0.1; spec = 0.08;
  } else if (mat == 18) {  // Parkett
    vec2 q = uv / vec2(0.9, 0.14); float row = floor(q.y);
    q.x += hash12(vec2(row, 3.7)) * 3.0;
    vec2 f = fract(q); vec2 pid = vec2(floor(q.x), row);
    float tint = 0.78 + 0.32 * hash12(pid);
    float grain = vnoise(vec3(uv.x * 4.0, uv.y * 45.0, row));
    float edge = smoothstep(0.0, 0.04, f.y) * smoothstep(0.0, 0.04, 1.0 - f.y) * smoothstep(0.0, 0.006, f.x) * smoothstep(0.0, 0.006, 1.0 - f.x);
    alb *= tint * (0.84 + 0.22 * grain) * mix(0.5, 1.0, edge); spec = 0.35; shin = 60.0; refl = 0.04;
  } else if (mat == 19) {  // Putz
    alb *= 0.93 + 0.09 * fbm(P * 1.8); spec = 0.03;
  } else if (mat == 20) {  // Gold
    spec = 1.3; shin = 90.0; refl = 0.4;
  } else if (mat == 21) {  // Leder
    alb *= 0.85 + 0.18 * vnoise(P * 28.0); spec = 0.35; shin = 36.0;
  } else if (mat == 22) {  // Backstein (Läuferverband, 25 x 7,5 cm mit Fuge)
    vec2 q = uv / vec2(0.25, 0.075); float row = floor(q.y);
    vec2 footprint = fwidth(q);
    float detail = 1.0 - smoothstep(0.2, 0.7, max(footprint.x, footprint.y));
    q.x += mod(row, 2.0) * 0.5;
    vec2 id = floor(q);
    float mortar = 1.0 - (1.0 - filteredStripe(q.x, 0.04, footprint.x)) *
                         (1.0 - filteredStripe(q.y, 0.1333, footprint.y));
    mortar = mix(0.168, mortar, detail);
    vec3 brick = alb * (0.76 + 0.34 * hash12(id + row * 3.17)) * (0.88 + 0.22 * vnoise(P * 9.0));
    brick = mix(brick, brick * vec3(0.5, 0.46, 0.44), step(0.93, hash12(id * 1.7 + 4.1)));  // dunkel gebrannte Steine
    brick = mix(alb * 0.89, brick, detail);
    alb = mix(brick, vec3(0.14, 0.13, 0.11) * (0.85 + 0.2 * vnoise(P * 3.0)), mortar);
    spec = 0.04;
  } else if (mat == 23) {  // Wellblech: Welle über die Normale, Schmutzschlieren
    vec3 a = abs(vPatN);
    vec3 tU = (a.y >= a.x && a.y >= a.z) ? vec3(1.0, 0.0, 0.0) : (a.x >= a.z ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));
    float detail = 1.0 - smoothstep(0.15, 0.5, fwidth(uv.x) / 0.076);
    N = normalize(N + tU * cos(uv.x * (6.2831 / 0.076)) * (0.38 * detail));
    alb *= 0.9 + 0.12 * vnoise(vec3(uv.x * 3.0, uv.y * 0.4, 0.0)) - 0.14 * smoothstep(0.55, 0.9, fbm(vec3(uv.x * 4.0, uv.y * 0.6, 3.0)));
    spec = 0.55; shin = 44.0; refl = 0.06;
  }
  emit += alb * vAux.z * 6.0;
  // Metalle spiegeln in ihrer eigenen Farbe: dunkler Stahl glänzt dezent, helles Metall bleibt hell
  vec3 metalTint = mat == 6 ? mix(vec3(0.18), vec3(1.0), clamp(alb * 2.2, 0.0, 1.0)) : vec3(1.0);

  float sky = vAux.y * vInst.x;
  if (uGlass == 1 && dot(N, V) < 0.0) N = -N;
  float up = N.y * 0.5 + 0.5;
  vec3 ambOut = mix(uGroundAmb, uSkyAmb, up);
  vec3 amb = mix(uIndoorAmb * (0.7 + 0.3 * up), ambOut, sky);
  vec3 col = alb * amb;

  float ndl = dot(N, uSunDir);
  float sh = 0.0;
  if (ndl > 0.0) {
    sh = shadowAt(vWorld, N);
    vec3 H = normalize(V + uSunDir);
    col += uSunColor * sh * ndl * (alb + spec * metalTint * pow(max(dot(N, H), 0.0), shin));
  }
  for (int i = 0; i < uLightCount; i++) {
    vec3 L = uLightPos[i].xyz - vWorld;
    float d2 = dot(L, L);
    float r = uLightPos[i].w;
    if (d2 > r * r) continue;
    float d = sqrt(d2);
    L /= d;
    float x = d / r;
    float win = clamp(1.0 - x * x * x * x, 0.0, 1.0);
    float att = win * win / (d2 + 1.0);
    float nl = max(dot(N, L), 0.0);
    vec3 H = normalize(V + L);
    col += uLightCol[i].rgb * att * nl * (alb + spec * metalTint * pow(max(dot(N, H), 0.0), shin));
  }
  float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);
  if (refl > 0.0) {
    vec3 R = reflect(-V, N);
    vec3 env = mix(uIndoorAmb * 2.5, skyColor(R, false), sky);
    col += env * (refl + (1.0 - refl) * fres * 0.5) * (mat == 20 ? alb * 2.0 : metalTint);
  }
  col += emit;
  if (vInst.y > 0.0) {
    float rim = 0.25 + 0.75 * pow(1.0 - max(dot(N, V), 0.0), 2.0);
    col += uHighlightColor * vInst.y * rim;
  }

  if (uGlass == 1) {
    vec3 R = reflect(-V, N);
    vec3 env = mix(uIndoorAmb * 3.0, skyColor(R, true), max(sky, 0.35));
    vec3 H = normalize(V + uSunDir);
    vec3 sunSpec = uSunColor * pow(max(dot(N, H), 0.0), 400.0) * 4.0 * max(ndl, 0.0);
    float a = clamp(vColor.a * 0.35 + fres * 0.65, 0.06, 0.92);
    col = alb * 0.25 * amb + env * (0.2 + 0.8 * fres) + sunSpec;
    oColor = vec4(col, a);
    oNormal = vec4(N * 0.5 + 0.5, 0.0);
    return;
  }
  float dist = length(vWorld - uCamPos);
  float fog = 1.0 - exp(-max(dist - 12.0, 0.0) * uFogDensity);
  col = mix(col, uFogColor, fog * mix(0.25, 1.0, sky));
  oColor = vec4(col, vColor.a);
  oNormal = vec4(N * 0.5 + 0.5, 1.0);
}
)";

static const char* kDepthVS = R"(
in vec3 aPos; in vec4 aInst0; in vec4 aInst1;
in vec4 aAux; out float vMaterial;
uniform mat4 uViewProj; uniform mat4 uModel;
void main() {
  vMaterial = aAux.x*255.0;
  float c = cos(aInst0.w), s = sin(aInst0.w);
  vec3 lp = aPos * aInst1.z;
  vec3 p = vec3(c * lp.x + s * lp.z, lp.y, -s * lp.x + c * lp.z) + aInst0.xyz;
  gl_Position = uViewProj * (uModel * vec4(p, 1.0));
}
)";
static const char* kDepthFS = R"(
in float vMaterial;
void main() { if(vMaterial>23.5 && vMaterial<24.5) discard; }
)";

static const char* kFullscreenVS = R"(
out vec2 vUV;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUV = p;
  gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);
}
)";

static const char* kSkyFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
void main() {
  vec4 p = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
  vec3 d = normalize(p.xyz / p.w - uCamPos);
  oColor = vec4(skyColor(d, true), 1.0);
}
)";

static const char* kSsaoFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform mat4 uProj;
uniform mat4 uInvProj;
uniform mat4 uView;
uniform vec3 uKernel[16];
uniform float uRadius;
vec3 viewPos(vec2 uv) {
  float z = texture(uDepth, uv).r * 2.0 - 1.0;
  vec4 p = uInvProj * vec4(uv * 2.0 - 1.0, z, 1.0);
  return p.xyz / p.w;
}
void main() {
  float d = texture(uDepth, vUV).r;
  if (d >= 0.99999) { oColor = vec4(1.0); return; }
  vec3 P = viewPos(vUV);
  vec3 N = normalize(mat3(uView) * (texture(uNormal, vUV).xyz * 2.0 - 1.0));
  float a = hash12(gl_FragCoord.xy) * 6.2831;
  vec3 rv = vec3(cos(a), sin(a), 0.0);
  vec3 T = normalize(rv - N * dot(rv, N));
  vec3 B = cross(N, T);
  mat3 TBN = mat3(T, B, N);
  float occ = 0.0;
  float radius = uRadius;
  for (int i = 0; i < 16; i++) {
    vec3 S = P + TBN * uKernel[i] * radius;
    vec4 o = uProj * vec4(S, 1.0);
    vec2 suv = o.xy / o.w * 0.5 + 0.5;
    if (suv.x < 0.0 || suv.y < 0.0 || suv.x > 1.0 || suv.y > 1.0) continue;
    float sz = viewPos(suv).z;
    float range = smoothstep(0.0, 1.0, radius / max(abs(P.z - sz), 1e-4));
    occ += (sz >= S.z + 0.025 ? 1.0 : 0.0) * range;
  }
  float ao = 1.0 - occ / 16.0;
  oColor = vec4(pow(clamp(ao, 0.0, 1.0), 1.6));
}
)";

static const char* kBlurFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uTex;
uniform sampler2D uDepth;
uniform vec2 uTexel;
float lin(float d) { return 1.0 / (1.0 - d + 1e-5); }
void main() {
  float cd = lin(texture(uDepth, vUV).r);
  float sum = 0.0, wsum = 0.0;
  for (int x = -2; x <= 2; x++)
    for (int y = -2; y <= 2; y++) {
      vec2 uv = vUV + vec2(x, y) * uTexel;
      float dd = lin(texture(uDepth, uv).r);
      float w = 1.0 / (1.0 + abs(dd - cd) * 4.0 / cd);
      sum += texture(uTex, uv).r * w;
      wsum += w;
    }
  oColor = vec4(sum / wsum);
}
)";

static const char* kKawaseFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uOffset;
void main() {
  vec2 o = uTexel * uOffset;
  vec3 c = texture(uTex, vUV + vec2(-o.x, -o.y)).rgb + texture(uTex, vUV + vec2(o.x, -o.y)).rgb +
           texture(uTex, vUV + vec2(-o.x, o.y)).rgb + texture(uTex, vUV + vec2(o.x, o.y)).rgb;
  oColor = vec4(c * 0.25, 1.0);
}
)";

static const char* kBloomDownFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform int uPrefilter;
vec3 s(vec2 o) { return texture(uTex, vUV + o * uTexel).rgb; }
void main() {
  vec3 a = s(vec2(-2, 2)), b = s(vec2(0, 2)), c = s(vec2(2, 2));
  vec3 d = s(vec2(-2, 0)), e = s(vec2(0, 0)), f = s(vec2(2, 0));
  vec3 g = s(vec2(-2, -2)), h = s(vec2(0, -2)), i = s(vec2(2, -2));
  vec3 j = s(vec2(-1, 1)), k = s(vec2(1, 1)), l = s(vec2(-1, -1)), m = s(vec2(1, -1));
  vec3 col = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
  if (uPrefilter == 1) {
    float br = max(col.r, max(col.g, col.b));
    float knee = 0.6, thr = 1.4;
    float soft = clamp(br - thr + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contrib = max(soft, br - thr) / max(br, 1e-4);
    col *= contrib;
    col = min(col, vec3(40.0));
  }
  oColor = vec4(col, 1.0);
}
)";

static const char* kBloomUpFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uTex;
uniform vec2 uTexel;
void main() {
  vec3 c = texture(uTex, vUV).rgb * 4.0;
  c += texture(uTex, vUV + vec2(-1, 0) * uTexel).rgb * 2.0;
  c += texture(uTex, vUV + vec2(1, 0) * uTexel).rgb * 2.0;
  c += texture(uTex, vUV + vec2(0, -1) * uTexel).rgb * 2.0;
  c += texture(uTex, vUV + vec2(0, 1) * uTexel).rgb * 2.0;
  c += texture(uTex, vUV + vec2(-1, -1) * uTexel).rgb;
  c += texture(uTex, vUV + vec2(1, -1) * uTexel).rgb;
  c += texture(uTex, vUV + vec2(-1, 1) * uTexel).rgb;
  c += texture(uTex, vUV + vec2(1, 1) * uTexel).rgb;
  oColor = vec4(c / 16.0, 1.0);
}
)";

static const char* kCompositeFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uHdr;
uniform sampler2D uAO;
uniform sampler2D uBloom;
uniform float uAOStrength;
uniform float uBloomStrength;
uniform float uExposure;
uniform float uSaturation;
uniform float uVignette;
uniform float uDamage;
uniform float uBlackout;
uniform vec3 uTint;
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
  vec3 c = texture(uHdr, vUV).rgb;
  float ao = texture(uAO, vUV).r;
  c *= mix(1.0, ao, uAOStrength);
  c += texture(uBloom, vUV).rgb * uBloomStrength;
  c *= uExposure * uTint;
  c = aces(c);
  float l = dot(c, vec3(0.299, 0.587, 0.114));
  c = mix(vec3(l), c, uSaturation);
  vec2 q = vUV - 0.5;
  float r = dot(q, q);
  c *= 1.0 - uVignette * smoothstep(0.1, 0.55, r);
  c = mix(c, vec3(0.75, 0.02, 0.02), uDamage * smoothstep(0.05, 0.5, r));
  c = pow(c, vec3(1.0 / 2.2));
  c += (hash12(gl_FragCoord.xy + fract(uTime) * 100.0) - 0.5) / 255.0;
  c *= 1.0 - uBlackout;
  oColor = vec4(c, dot(c, vec3(0.299, 0.587, 0.114)));
}
)";

static const char* kFxaaFS = R"(
in vec2 vUV;
out vec4 oColor;
uniform sampler2D uTex;
uniform vec2 uInvSize;
uniform int uEnabled;
void main() {
  if (uEnabled == 0) { oColor = vec4(texture(uTex, vUV).rgb, 1.0); return; }
  vec3 rgbNW = texture(uTex, vUV + vec2(-1.0, -1.0) * uInvSize).rgb;
  vec3 rgbNE = texture(uTex, vUV + vec2(1.0, -1.0) * uInvSize).rgb;
  vec3 rgbSW = texture(uTex, vUV + vec2(-1.0, 1.0) * uInvSize).rgb;
  vec3 rgbSE = texture(uTex, vUV + vec2(1.0, 1.0) * uInvSize).rgb;
  vec3 rgbM = texture(uTex, vUV).rgb;
  vec3 luma = vec3(0.299, 0.587, 0.114);
  float lNW = dot(rgbNW, luma), lNE = dot(rgbNE, luma), lSW = dot(rgbSW, luma), lSE = dot(rgbSE, luma), lM = dot(rgbM, luma);
  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
  float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
  float dirReduce = max((lNW + lNE + lSW + lSE) * (0.25 / 8.0), 1.0 / 128.0);
  float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
  dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * uInvSize;
  vec3 rgbA = 0.5 * (texture(uTex, vUV + dir * (1.0 / 3.0 - 0.5)).rgb + texture(uTex, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
  vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uTex, vUV + dir * -0.5).rgb + texture(uTex, vUV + dir * 0.5).rgb);
  float lB = dot(rgbB, luma);
  oColor = vec4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
)";

static const char* kParticleVS = R"(
in vec4 aInst0; in vec4 aInst1;
uniform mat4 uViewProj;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
out vec4 vColor; out vec2 vUV;
void main() {
  vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
  vUV = c;
  vec3 p = aInst0.xyz + (uCamRight * c.x + uCamUp * c.y) * aInst0.w;
  vColor = aInst1;
  gl_Position = uViewProj * vec4(p, 1.0);
}
)";
static const char* kParticleFS = R"(
in vec4 vColor; in vec2 vUV;
layout(location = 0) out vec4 oColor;
void main() {
  float r = dot(vUV, vUV);
  if (r > 1.0) discard;
  oColor = vec4(vColor.rgb, vColor.a * (1.0 - r * r));
}
)";

// ---------------------------------------------------------------------------------------------

void Camera::Update(float aspect) {
  Vec3 f = Forward();
  Vec3 up(0, 1, 0);
  if (roll != 0) {
    Vec3 r = Right();
    up = Normalize(up * cosf(roll) + r * sinf(roll));
  }
  view = LookAt(pos, pos + f, up);
  proj = Perspective(fovDeg * DEG2RAD, aspect, zNear, zFar);
  viewProj = proj * view;
  invViewProj = Inverse(viewProj);
}

static GLuint MakeTex(int w, int h, GLenum ifmt, GLenum fmt, GLenum type, bool linear) {
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

bool Renderer::Init(const RenderSettings& s) {
  settings_ = s;
  std::string defs = StrFormat("#define MAX_LIGHTS %d\n", MAX_LIGHTS);
  std::string common = std::string(kNoiseGLSL) + kSkyGLSL;
  bool ok = true;
  ok &= lit_.Build("lit", kLitVS, common + kLitFS, defs.c_str());
  ok &= depth_.Build("depth", kDepthVS, kDepthFS);
  ok &= sky_.Build("sky", kFullscreenVS, common + kSkyFS);
  ok &= ssao_.Build("ssao", kFullscreenVS, std::string(kNoiseGLSL) + kSsaoFS);
  ok &= blur_.Build("blur", kFullscreenVS, kBlurFS);
  ok &= bloomDown_.Build("bloomDown", kFullscreenVS, kBloomDownFS);
  ok &= bloomUp_.Build("bloomUp", kFullscreenVS, kBloomUpFS);
  ok &= composite_.Build("composite", kFullscreenVS, std::string(kNoiseGLSL) + "uniform float uTime;\n" + kCompositeFS);
  ok &= fxaa_.Build("fxaa", kFullscreenVS, kFxaaFS);
  ok &= kawase_.Build("kawase", kFullscreenVS, kKawaseFS);
  ok &= particle_.Build("particle", kParticleVS, kParticleFS);
  if (!ok) return false;
  lit_.Use();
  lit_.BindBlock("Lights", 0);
  lit_.Set("uShadow0", 0);
  lit_.Set("uShadow1", 1);
  lit_.Set("uSignAtlas", 3);
  signTex_ = CreateWorldTextTexture();

  glGenVertexArrays(1, &emptyVao_);
  glGenBuffers(1, &instanceVbo_);
  glGenBuffers(1, &lightUbo_);
  glBindBuffer(GL_UNIFORM_BUFFER, lightUbo_);
  glBufferData(GL_UNIFORM_BUFFER, MAX_LIGHTS * 32, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, lightUbo_);

  glGenVertexArrays(1, &particleVao_);
  glGenBuffers(1, &particleVbo_);
  glBindVertexArray(particleVao_);
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
  glVertexAttribDivisor(4, 1);
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
  glVertexAttribDivisor(5, 1);
  glBindVertexArray(0);

  Rng rng(1234);
  for (int i = 0; i < 16; i++) {
    Vec3 v(rng.Range(-1, 1), rng.Range(-1, 1), rng.Range(0.1f, 1));
    v = Normalize(v) * rng.Range(0.2f, 1.0f);
    float sc = (float)i / 16.0f;
    v *= Lerp(0.15f, 1.0f, sc * sc);
    ssaoKernel_[i] = v;
  }
  CreateShadowMaps();
  return true;
}

void Renderer::CreateShadowMaps() {
  for (int i = 0; i < 2; i++) {
    if (shadowTex_[i]) glDeleteTextures(1, &shadowTex_[i]);
    if (shadowFbo_[i]) glDeleteFramebuffers(1, &shadowFbo_[i]);
  }
  shadowRes_[0] = std::min(2048, settings_.shadowSize);
  shadowRes_[1] = settings_.shadowSize;
  for (int i = 0; i < 2; i++) {
    int r = shadowRes_[i];
    shadowTex_[i] = MakeTex(r, r, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, true);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo_[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_[i], 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::ApplySettings(const RenderSettings& s) {
  bool shadowChanged = s.shadowSize != settings_.shadowSize;
  settings_ = s;
  if (shadowChanged) CreateShadowMaps();
}

void Renderer::Shutdown() {
  if (signTex_) glDeleteTextures(1, &signTex_);
  signTex_ = 0;
  DestroyTargets();
  world_.Destroy();
  glass_.Destroy();
  decalMesh_.Destroy();
  ClearPropMeshes();
}

void Renderer::DestroyTargets() {
  auto kill = [](Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    for (GLuint& c : t.color) if (c) glDeleteTextures(1, &c);
    if (t.depth) glDeleteTextures(1, &t.depth);
    t = Target();
  };
  kill(hdr_); kill(ao_); kill(aoBlur_); kill(ldr_);
  for (auto& b : bloom_) kill(b);
  for (auto& b : back_) kill(b);
}

void Renderer::CreateTargets(int w, int h) {
  DestroyTargets();
  width_ = w;
  height_ = h;
  hdr_.w = w; hdr_.h = h;
  glGenFramebuffers(1, &hdr_.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_.fbo);
  hdr_.color[0] = MakeTex(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_.color[0], 0);
  hdr_.color[1] = MakeTex(w, h, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_BYTE, false);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, hdr_.color[1], 0);
  hdr_.depth = MakeTex(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, hdr_.depth, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) LOG("HDR-Framebuffer unvollständig");

  auto simple = [](Target& t, int tw, int th, GLenum ifmt, GLenum fmt, GLenum type) {
    t.w = tw; t.h = th;
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    t.color[0] = MakeTex(tw, th, ifmt, fmt, type, true);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color[0], 0);
  };
  int hw = std::max(1, w / 2), hh = std::max(1, h / 2);
  simple(ao_, hw, hh, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
  simple(aoBlur_, hw, hh, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
  simple(ldr_, w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
  simple(back_[0], hw, hh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
  simple(back_[1], std::max(1, w / 4), std::max(1, h / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
  simple(back_[2], std::max(1, w / 4), std::max(1, h / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
  bloomLevels_ = 0;
  int bw = hw, bh = hh;
  for (int i = 0; i < 5 && bw >= 8 && bh >= 8; i++) {
    simple(bloom_[i], bw, bh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomLevels_++;
    bw /= 2; bh /= 2;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::SetWorld(const MeshData& opaque, const MeshData& transparent) {
  world_.Upload(opaque);
  glass_.Upload(transparent);
}

int Renderer::AddPropMesh(const MeshData& d) {
  auto m = std::make_unique<Mesh>();
  m->Upload(d);
  propMeshes_.push_back(std::move(m));
  instances_.resize(propMeshes_.size());
  return (int)propMeshes_.size() - 1;
}

void Renderer::ClearPropMeshes() {
  for (auto& m : propMeshes_) m->Destroy();
  propMeshes_.clear();
  instances_.clear();
}

void Renderer::Begin(const Camera& cam, const Environment& env, int width, int height, float time) {
  showWorld_ = true;
  cam_ = cam;
  env_ = env;
  time_ = time;
  if (width != width_ || height != height_ || !hdr_.fbo) CreateTargets(std::max(1, width), std::max(1, height));
  cam_.Update((float)width_ / (float)height_);
  for (auto& v : instances_) v.clear();
  items_.clear();
  particles_.clear();
  stats_ = RenderStats();
}

void Renderer::SetLights(const std::vector<PointLight>& lights) {
  // Lichter nach Relevanz (Abstand zur Kamera, Sichtkegel) auswählen
  struct Cand { float score; int idx; };
  std::vector<Cand> c;
  c.reserve(lights.size());
  Vec3 fwd = cam_.Forward();
  for (int i = 0; i < (int)lights.size(); i++) {
    Vec3 d = lights[i].pos - cam_.pos;
    float dist = Length(d);
    if (dist > 70 + lights[i].radius) continue;
    float behind = Dot(d, fwd) < -lights[i].radius ? 3.0f : 1.0f;
    c.push_back({(dist - lights[i].radius) * behind, i});
  }
  std::sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
  lightCount_ = std::min((int)c.size(), MAX_LIGHTS);
  std::vector<float> buf(MAX_LIGHTS * 8, 0.0f);
  for (int i = 0; i < lightCount_; i++) {
    const PointLight& l = lights[c[i].idx];
    float* p = &buf[i * 4];
    p[0] = l.pos.x; p[1] = l.pos.y; p[2] = l.pos.z; p[3] = l.radius;
    float* q = &buf[MAX_LIGHTS * 4 + i * 4];
    q[0] = l.color.x; q[1] = l.color.y; q[2] = l.color.z; q[3] = 0;
  }
  glBindBuffer(GL_UNIFORM_BUFFER, lightUbo_);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, buf.size() * sizeof(float), buf.data());
  stats_.lights = lightCount_;
}

void Renderer::Prop(int meshId, const PropInstance& inst) {
  if (meshId < 0 || meshId >= (int)instances_.size()) return;
  instances_[meshId].push_back(inst);
}

void Renderer::Draw(const DrawItem& item) {
  if (item.mesh && item.mesh->Valid()) items_.push_back(item);
}

void Renderer::AddDecal(Vec3 pos, Vec3 normal, float size, Vec4 color) {
  DecalRec d{pos, normal, size, color};
  const int maxDecals = 300;
  if ((int)decals_.size() < maxDecals) decals_.push_back(d);
  else { decals_[decalHead_] = d; decalHead_ = (decalHead_ + 1) % maxDecals; }
  decalsDirty_ = true;
}

void Renderer::ClearDecals() {
  decals_.clear();
  decalHead_ = 0;
  decalsDirty_ = true;
}

void Renderer::RebuildDecalMesh() {
  MeshData md;
  MeshBuilder b(&md);
  for (const DecalRec& d : decals_) {
    Vec3 n = Normalize(d.normal);
    Vec3 t = fabsf(n.y) > 0.9f ? Vec3(1, 0, 0) : Normalize(Cross(Vec3(0, 1, 0), n));
    Vec3 bt = Cross(n, t);
    Vec3 c = d.pos + n * 0.004f;
    b.Color(d.color.xyz(), d.color.w);
    const int seg = 8;
    for (int i = 0; i < seg; i++) {
      float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
      Vec3 p0 = c + (t * cosf(a0) + bt * sinf(a0)) * d.size;
      Vec3 p1 = c + (t * cosf(a1) + bt * sinf(a1)) * d.size;
      b.Tri(c, p0, p1);
    }
  }
  // Dreiecke auf die Normale ausrichten (Wicklung egal, da ohne Culling gezeichnet)
  decalMesh_.Upload(md, true);
  decalsDirty_ = false;
}

void Renderer::ComputeShadowMatrices() {
  Vec3 sd = Normalize(env_.sunDir);
  Vec3 up = fabsf(sd.y) > 0.95f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
  Mat4 rot = LookAt(Vec3(0), -sd, up);
  // Kaskade 0: um die Kamera, auf Texel eingerastet
  {
    float r = 22.0f;
    Vec3 center = cam_.pos + cam_.Forward() * 10.0f;
    Vec3 lc = TransformPoint(rot, center);
    float texel = 2 * r / shadowRes_[0];
    lc.x = floorf(lc.x / texel) * texel;
    lc.y = floorf(lc.y / texel) * texel;
    float depthR = 250.0f;
    Mat4 proj = Ortho(lc.x - r, lc.x + r, lc.y - r, lc.y + r, -lc.z - depthR, -lc.z + depthR);
    shadowViewProj_[0] = proj * rot;
  }
  // Kaskade 1: gesamte Welt
  {
    Vec3 c = (env_.worldMin + env_.worldMax) * 0.5f;
    float r = Length(env_.worldMax - env_.worldMin) * 0.5f;
    Vec3 lc = TransformPoint(rot, c);
    Mat4 proj = Ortho(lc.x - r, lc.x + r, lc.y - r, lc.y + r, -lc.z - r - 50, -lc.z + r + 50);
    shadowViewProj_[1] = proj * rot;
  }
  shadowMat_[0] = shadowViewProj_[0];
  shadowMat_[1] = shadowViewProj_[1];
}

void Renderer::SetConstInstance(float sky, Vec3 tint, float tintAmount, float highlight) {
  glDisableVertexAttribArray(4);
  glDisableVertexAttribArray(5);
  glDisableVertexAttribArray(6);
  glVertexAttrib4f(4, 0, 0, 0, 0);
  glVertexAttrib4f(5, sky, highlight, 1, 1);
  glVertexAttrib4f(6, tint.x, tint.y, tint.z, tintAmount);
}

void Renderer::DrawInstances(Shader& sh) {
  // alle Instanzen in einen Puffer laden
  size_t total = 0;
  for (auto& v : instances_) total += v.size();
  if (total == 0) return;
  std::vector<InstanceData> data;
  data.reserve(total);
  for (auto& v : instances_)
    for (const PropInstance& p : v) {
      InstanceData d = {{p.pos.x, p.pos.y, p.pos.z, p.yaw, p.sky, p.highlight, p.scale, p.alpha, p.tint.x, p.tint.y,
                         p.tint.z, p.tintAmount}};
      data.push_back(d);
    }
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
  size_t bytes = data.size() * sizeof(InstanceData);
  if (bytes > instanceCap_) {
    instanceCap_ = bytes * 2;
    glBufferData(GL_ARRAY_BUFFER, instanceCap_, nullptr, GL_STREAM_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data.data());
  sh.Set("uModel", Mat4::Identity());
  size_t offset = 0;
  for (size_t m = 0; m < instances_.size(); m++) {
    size_t n = instances_[m].size();
    if (!n) continue;
    const Mesh* mesh = propMeshes_[m].get();
    glBindVertexArray(mesh->Vao());
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    for (int a = 0; a < 3; a++) {
      glEnableVertexAttribArray(4 + a);
      glVertexAttribPointer(4 + a, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(offset * sizeof(InstanceData) + a * 16));
      glVertexAttribDivisor(4 + a, 1);
    }
    mesh->DrawInstanced((int)n);
    for (int a = 0; a < 3; a++) {
      glVertexAttribDivisor(4 + a, 0);
      glDisableVertexAttribArray(4 + a);
    }
    stats_.drawCalls++;
    stats_.instances += (int)n;
    offset += n;
  }
  glBindVertexArray(0);
}

void Renderer::DrawItems(Shader& sh, bool viewModel, bool shadowPass) {
  for (const DrawItem& it : items_) {
    if (it.viewModel != viewModel) continue;
    if (shadowPass && !it.castShadow) continue;
    glBindVertexArray(it.mesh->Vao());
    SetConstInstance(it.sky, it.tint, it.tintAmount, it.highlight);
    sh.Set("uModel", it.model);
    it.mesh->Draw();
    stats_.drawCalls++;
  }
  glBindVertexArray(0);
}

void Renderer::DrawScene(Shader& sh, bool shadowPass) {
  sh.Set("uModel", Mat4::Identity());
  if (showWorld_ && world_.Valid()) {
    glBindVertexArray(world_.Vao());
    SetConstInstance(1, Vec3(1), 0, 0);
    world_.Draw();
    stats_.drawCalls++;
  }
  DrawInstances(sh);
  DrawItems(sh, false, shadowPass);
}

void Renderer::SetCommonUniforms(Shader& sh) {
  sh.Set("uSunDir", Normalize(env_.sunDir));
  sh.Set("uSunColor", env_.sunColor);
  sh.Set("uSkyZenith", env_.skyZenith);
  sh.Set("uSkyHorizon", env_.skyHorizon);
  sh.Set("uSkyGround", env_.groundColor);
  sh.Set("uClouds", env_.clouds);
  sh.Set("uTime", time_);
  sh.Set("uCamPos", cam_.pos);
}

void Renderer::Fullscreen() {
  glBindVertexArray(emptyVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::Render(const ScreenFX& fx) {
  fx_ = fx;
  if (decalsDirty_) RebuildDecalMesh();
  ComputeShadowMatrices();
  glActiveTexture(GL_TEXTURE0 + 3);
  glBindTexture(GL_TEXTURE_2D, signTex_);
  glActiveTexture(GL_TEXTURE0);

  // --- Schattenkarten ---
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.5f, 3.0f);
  depth_.Use();
  for (int i = 0; i < 2; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_[i]);
    glViewport(0, 0, shadowRes_[i], shadowRes_[i]);
    glClear(GL_DEPTH_BUFFER_BIT);
    depth_.Set("uViewProj", shadowViewProj_[i]);
    DrawScene(depth_, true);
  }
  glDisable(GL_POLYGON_OFFSET_FILL);

  // --- Hauptpass ---
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_.fbo);
  glViewport(0, 0, width_, height_);
  GLenum bufs2[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
  glDrawBuffers(2, bufs2);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  lit_.Use();
  SetCommonUniforms(lit_);
  lit_.Set("uViewProj", cam_.viewProj);
  lit_.Set("uSkyAmb", env_.skyAmbient);
  lit_.Set("uGroundAmb", env_.groundAmbient);
  lit_.Set("uIndoorAmb", env_.indoorAmbient);
  lit_.Set("uFogColor", env_.fogColor);
  lit_.Set("uFogDensity", env_.fogDensity);
  lit_.Set("uShadowMat0", shadowMat_[0]);
  lit_.Set("uShadowMat1", shadowMat_[1]);
  lit_.Set("uShadowTexel", Vec2(1.0f / shadowRes_[0], 1.0f / shadowRes_[1]));
  lit_.Set("uLightCount", lightCount_);
  lit_.Set("uGlass", 0);
  lit_.Set("uHighlightColor", Vec3(1.0f, 0.75f, 0.2f));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, shadowTex_[0]);
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, shadowTex_[1]);
  glActiveTexture(GL_TEXTURE0);
  DrawScene(lit_, false);

  // --- SSAO (vor Himmel, Glas und Waffe) ---
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  bool aoOn = settings_.ssao;
  if (aoOn) {
    glBindFramebuffer(GL_FRAMEBUFFER, ao_.fbo);
    glViewport(0, 0, ao_.w, ao_.h);
    ssao_.Use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_.depth);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, hdr_.color[1]);
    ssao_.Set("uDepth", 0);
    ssao_.Set("uNormal", 1);
    ssao_.Set("uProj", cam_.proj);
    ssao_.Set("uInvProj", Inverse(cam_.proj));
    ssao_.Set("uView", cam_.view);
    ssao_.Set("uRadius", 0.55f);
    glUniform3fv(ssao_.Loc("uKernel"), 16, &ssaoKernel_[0].x);
    Fullscreen();
    glBindFramebuffer(GL_FRAMEBUFFER, aoBlur_.fbo);
    blur_.Use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ao_.color[0]);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, hdr_.depth);
    blur_.Set("uTex", 0);
    blur_.Set("uDepth", 1);
    blur_.Set("uTexel", Vec2(1.0f / ao_.w, 1.0f / ao_.h));
    Fullscreen();
    glActiveTexture(GL_TEXTURE0);
  }

  // --- Himmel, Decals, Glas, Partikel ---
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_.fbo);
  glViewport(0, 0, width_, height_);
  GLenum buf1 = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &buf1);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_LEQUAL);
  sky_.Use();
  SetCommonUniforms(sky_);
  sky_.Set("uInvViewProj", cam_.invViewProj);
  Fullscreen();

  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  lit_.Use();
  if (showWorld_ && decalMesh_.Valid()) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -4.0f);
    lit_.Set("uModel", Mat4::Identity());
    glBindVertexArray(decalMesh_.Vao());
    SetConstInstance(0.6f, Vec3(1), 0, 0);
    decalMesh_.Draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
  if (showWorld_ && glass_.Valid()) {
    lit_.Set("uGlass", 1);
    lit_.Set("uModel", Mat4::Identity());
    glBindVertexArray(glass_.Vao());
    SetConstInstance(1, Vec3(1), 0, 0);
    glass_.Draw();
    lit_.Set("uGlass", 0);
  }
  glBindVertexArray(0);

  if (!particles_.empty()) {
    particle_.Use();
    particle_.Set("uViewProj", cam_.viewProj);
    particle_.Set("uCamRight", cam_.Right());
    Vec3 camUp = Cross(cam_.Right(), cam_.Forward());
    particle_.Set("uCamUp", camUp);
    std::vector<float> pdata;
    for (int pass = 0; pass < 2; pass++) {
      pdata.clear();
      for (const Particle& p : particles_) {
        if (p.additive != (pass == 1)) continue;
        float d[8] = {p.pos.x, p.pos.y, p.pos.z, p.size, p.color.x, p.color.y, p.color.z, p.color.w};
        pdata.insert(pdata.end(), d, d + 8);
      }
      if (pdata.empty()) continue;
      if (pass == 1) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glBindVertexArray(particleVao_);
      glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
      glBufferData(GL_ARRAY_BUFFER, pdata.size() * sizeof(float), pdata.data(), GL_STREAM_DRAW);
      glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(pdata.size() / 8));
      stats_.drawCalls++;
    }
    glBindVertexArray(0);
  }
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);

  // --- Waffe in der Hand (eigene Projektion) ---
  bool hasVM = false;
  for (const DrawItem& it : items_) if (it.viewModel) { hasVM = true; break; }
  if (hasVM) {
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    lit_.Use();
    Mat4 vmProj = Perspective(fx_.viewModelFov * DEG2RAD, (float)width_ / height_, 0.01f, 10.0f);
    lit_.Set("uViewProj", vmProj * cam_.view);
    DrawItems(lit_, true, false);
    glDisable(GL_CULL_FACE);
  }
  glDisable(GL_DEPTH_TEST);

  // --- Bloom ---
  if (settings_.bloom && bloomLevels_ > 0) {
    bloomDown_.Use();
    bloomDown_.Set("uTex", 0);
    for (int i = 0; i < bloomLevels_; i++) {
      glBindFramebuffer(GL_FRAMEBUFFER, bloom_[i].fbo);
      glViewport(0, 0, bloom_[i].w, bloom_[i].h);
      GLuint src = i == 0 ? hdr_.color[0] : bloom_[i - 1].color[0];
      int sw = i == 0 ? width_ : bloom_[i - 1].w, shh = i == 0 ? height_ : bloom_[i - 1].h;
      glBindTexture(GL_TEXTURE_2D, src);
      bloomDown_.Set("uTexel", Vec2(1.0f / sw, 1.0f / shh));
      bloomDown_.Set("uPrefilter", i == 0 ? 1 : 0);
      Fullscreen();
    }
    bloomUp_.Use();
    bloomUp_.Set("uTex", 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = bloomLevels_ - 1; i > 0; i--) {
      glBindFramebuffer(GL_FRAMEBUFFER, bloom_[i - 1].fbo);
      glViewport(0, 0, bloom_[i - 1].w, bloom_[i - 1].h);
      glBindTexture(GL_TEXTURE_2D, bloom_[i].color[0]);
      bloomUp_.Set("uTexel", Vec2(1.0f / bloom_[i].w, 1.0f / bloom_[i].h));
      Fullscreen();
    }
    glDisable(GL_BLEND);
  }

  // --- Zusammensetzen ---
  glBindFramebuffer(GL_FRAMEBUFFER, ldr_.fbo);
  glViewport(0, 0, width_, height_);
  composite_.Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdr_.color[0]);
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, aoOn ? aoBlur_.color[0] : 0);
  glActiveTexture(GL_TEXTURE0 + 2);
  glBindTexture(GL_TEXTURE_2D, settings_.bloom && bloomLevels_ ? bloom_[0].color[0] : 0);
  composite_.Set("uHdr", 0);
  composite_.Set("uAO", 1);
  composite_.Set("uBloom", 2);
  composite_.Set("uAOStrength", aoOn ? 0.85f : 0.0f);
  composite_.Set("uBloomStrength", settings_.bloom ? 0.07f : 0.0f);
  composite_.Set("uExposure", env_.exposure);
  composite_.Set("uSaturation", fx_.saturation);
  composite_.Set("uVignette", fx_.vignette);
  composite_.Set("uDamage", fx_.damage);
  composite_.Set("uBlackout", fx_.blackout);
  composite_.Set("uTint", fx_.tint);
  composite_.Set("uTime", time_);
  Fullscreen();
  glActiveTexture(GL_TEXTURE0);

  // --- Weichgezeichneter Hintergrund für die Oberfläche (Dual-Kawase) ---
  {
    kawase_.Use();
    kawase_.Set("uTex", 0);
    struct Pass { GLuint src; int sw, sh; Target* dst; float off; };
    const Pass passes[] = {{ldr_.color[0], width_, height_, &back_[0], 1.0f},
                           {back_[0].color[0], back_[0].w, back_[0].h, &back_[1], 1.0f},
                           {back_[1].color[0], back_[1].w, back_[1].h, &back_[2], 1.5f},
                           {back_[2].color[0], back_[2].w, back_[2].h, &back_[1], 2.5f},
                           {back_[1].color[0], back_[1].w, back_[1].h, &back_[2], 3.5f}};
    for (const Pass& p : passes) {
      glBindFramebuffer(GL_FRAMEBUFFER, p.dst->fbo);
      glViewport(0, 0, p.dst->w, p.dst->h);
      glBindTexture(GL_TEXTURE_2D, p.src);
      kawase_.Set("uTexel", Vec2(1.0f / p.sw, 1.0f / p.sh));
      kawase_.Set("uOffset", p.off);
      Fullscreen();
    }
  }

  // --- FXAA auf den Bildschirm ---
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width_, height_);
  fxaa_.Use();
  glBindTexture(GL_TEXTURE_2D, ldr_.color[0]);
  fxaa_.Set("uTex", 0);
  fxaa_.Set("uInvSize", Vec2(1.0f / width_, 1.0f / height_));
  fxaa_.Set("uEnabled", settings_.fxaa ? 1 : 0);
  Fullscreen();
  glBindVertexArray(0);
}

void Renderer::ReadBackbuffer(std::vector<u8>& rgba, int& w, int& h) {
  w = width_;
  h = height_;
  if (w <= 0 || h <= 0) {  // vor dem ersten 3D-Bild (z. B. Intro-Video): aktuelle Bildfläche
    GLint vp[4] = {};
    glGetIntegerv(GL_VIEWPORT, vp);
    w = vp[2];
    h = vp[3];
  }
  rgba.resize((size_t)w * h * 4);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  std::vector<u8> tmp((size_t)w * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
  for (int y = 0; y < h; y++) memcpy(&rgba[(size_t)y * w * 4], &tmp[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
  for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
}
