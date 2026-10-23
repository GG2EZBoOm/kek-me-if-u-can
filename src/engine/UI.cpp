#include "engine/UI.h"
#include "engine/Platform.h"
#include "engine/GL.h"
#include <windows.h>

static const char* kUIVS = R"(
layout(location = 0) in vec2 aP;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aC;
layout(location = 3) in vec4 aRect;
layout(location = 4) in float aMode;
uniform vec2 uScreen;
out vec2 vUV; out vec4 vC; out vec4 vRect; out float vMode;
void main() {
  vUV = aUV; vC = aC; vRect = aRect; vMode = aMode;
  gl_Position = vec4(aP.x / uScreen.x * 2.0 - 1.0, 1.0 - aP.y / uScreen.y * 2.0, 0.0, 1.0);
}
)";

// Modi: 0 Schrift, 1 Rechteck/Rahmen, 2 Schatten, 3 Milchglas, 4 Kreisbogen
static const char* kUIFS = R"(
in vec2 vUV; in vec4 vC; in vec4 vRect; in float vMode;
out vec4 oColor;
uniform sampler2D uFont;
uniform sampler2D uBackdrop;
uniform int uHasBackdrop;
uniform vec2 uScreen;
float sdRound(vec2 p, vec2 h, float r) {
  vec2 q = abs(p) - h + r;
  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
  vec4 c = vC;
  if (vMode < 0.5) {
    c.a *= texture(uFont, vUV).r;
  } else if (vMode < 1.5) {
    float d = sdRound(vUV, vRect.xy, vRect.z);
    float a = clamp(0.5 - d, 0.0, 1.0);
    if (vRect.w > 0.0) a *= clamp(0.5 + d + vRect.w, 0.0, 1.0);
    c.a *= a;
    c.rgb += (fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
  } else if (vMode < 2.5) {
    float d = sdRound(vUV, vRect.xy, vRect.z);
    c.a *= 1.0 - smoothstep(-vRect.w, vRect.w, d);
  } else if (vMode < 3.5) {
    float d = sdRound(vUV, vRect.xy, vRect.z);
    float a = clamp(0.5 - d, 0.0, 1.0);
    vec3 col;
    if (uHasBackdrop == 1) {
      vec3 b = texture(uBackdrop, gl_FragCoord.xy / uScreen).rgb;
      float l = dot(b, vec3(0.299, 0.587, 0.114));
      b = mix(vec3(l), b, 0.8);
      col = mix(b, vC.rgb, vC.a);
      float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
      col += (n - 0.5) / 255.0;
      c = vec4(col, a * vRect.w);
    } else {
      c = vec4(vC.rgb, a * vRect.w * 0.92);
    }
  } else {
    float r = length(vUV);
    float d = abs(r - vRect.x) - vRect.y;
    float a = clamp(0.5 - d, 0.0, 1.0);
    float ang = fract(atan(vUV.x, -vUV.y) / 6.2831853 - vRect.w + 1.0);
    a *= clamp((vRect.z - ang) * r * 6.2831853 + 0.5, 0.0, 1.0);
    c.a *= a;
  }
  if (c.a <= 0.001) discard;
  oColor = c;
}
)";

enum GlyphSetId { SET_FULL = 0, SET_LATIN, SET_TITLE, SET_ICONS };

static std::vector<u32> GlyphSet(int set) {
  std::vector<u32> cps;
  if (set == SET_ICONS) {
    for (u32 c = 0xE700; c < 0xEA00; c++) cps.push_back(c);
    return cps;
  }
  if (set == SET_TITLE) {
    for (u32 c = 32; c < 96; c++) cps.push_back(c);
    for (u32 c : {0xC4u, 0xD6u, 0xDCu}) cps.push_back(c);
    return cps;
  }
  for (u32 c = 32; c < 127; c++) cps.push_back(c);
  for (u32 c = 160; c < 256; c++) cps.push_back(c);
  if (set == SET_FULL) {
    const u32 extra[] = {0x20AC, 0x2013, 0x2014, 0x201E, 0x201C, 0x201D, 0x2018, 0x2019, 0x2022, 0x2026,
                         0x2190, 0x2191, 0x2192, 0x2193, 0x2713, 0x2717, 0x25B6, 0x25C0, 0x2605, 0x2665, 0x266A};
    for (u32 c : extra) cps.push_back(c);
  }
  return cps;
}

bool UI::Init() {
  if (!shader_.Build("ui", kUIVS, kUIFS)) return false;
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, x));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, u));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVertex), (void*)offsetof(UIVertex, c));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, rect));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, mode));
  glBindVertexArray(0);
  return true;
}

void UI::Shutdown() {
  if (atlas_) glDeleteTextures(1, &atlas_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  shader_.Destroy();
}

void UI::BuildFonts() {
  // Kandidaten der Reihe nach; matchLen = so viele Zeichen des Namens müssen übereinstimmen
  struct Face { const wchar_t* name; int weight; int matchLen; };
  struct Spec { Face faces[3]; float size; int set; };
  const Spec specs[FONT_COUNT] = {
      {{{L"Segoe UI", FW_NORMAL, 0}, {}, {}}, 15, SET_FULL},
      {{{L"Segoe UI", FW_NORMAL, 0}, {}, {}}, 18, SET_FULL},
      {{{L"Segoe UI Semibold", FW_SEMIBOLD, 0}, {}, {}}, 23, SET_FULL},
      {{{L"Bahnschrift SemiBold", FW_SEMIBOLD, 11}, {L"Bahnschrift", FW_SEMIBOLD, 11}, {L"Segoe UI", FW_BOLD, 0}}, 36, SET_FULL},
      {{{L"Bahnschrift Bold", FW_BOLD, 11}, {L"Bahnschrift", FW_BOLD, 11}, {L"Segoe UI Black", FW_BLACK, 0}}, 78, SET_LATIN},
      {{{L"Bahnschrift SemiBold", FW_SEMIBOLD, 11}, {L"Bahnschrift", FW_SEMIBOLD, 11}, {L"Segoe UI Semibold", FW_SEMIBOLD, 0}}, 14, SET_LATIN},
      {{{L"Bahnschrift SemiBold", FW_SEMIBOLD, 11}, {L"Bahnschrift", FW_SEMIBOLD, 11}, {L"Segoe UI", FW_BOLD, 0}}, 46, SET_LATIN},
      {{{L"Bahnschrift Bold Condensed", FW_BOLD, 11}, {L"Bahnschrift", FW_BOLD, 11}, {L"Segoe UI Black", FW_BLACK, 0}}, 132, SET_TITLE},
      {{{L"Segoe Fluent Icons", FW_NORMAL, 18}, {L"Segoe MDL2 Assets", FW_NORMAL, 17}, {}}, 20, SET_ICONS},
      {{{L"Segoe Fluent Icons", FW_NORMAL, 18}, {L"Segoe MDL2 Assets", FW_NORMAL, 17}, {}}, 34, SET_ICONS},
  };
  struct Bmp { int w, h; std::vector<u8> px; };
  struct Pending { FontId font; u32 cp; Glyph g; Bmp bmp; };
  std::vector<Pending> pend;
  HDC dc = CreateCompatibleDC(nullptr);
  MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
  size_t area = 0;
  for (int f = 0; f < FONT_COUNT; f++) {
    int px = (int)roundf(specs[f].size * scale_);
    HFONT font = nullptr;
    for (const Face& fc : specs[f].faces) {
      if (!fc.name) break;
      HFONT cand = CreateFontW(-px, 0, 0, 0, fc.weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, fc.name);
      if (fc.matchLen == 0) { font = cand; break; }
      HGDIOBJ prev = SelectObject(dc, cand);
      wchar_t got[64] = {};
      GetTextFaceW(dc, 63, got);
      SelectObject(dc, prev);
      if (_wcsnicmp(got, fc.name, fc.matchLen) == 0) { font = cand; break; }
      DeleteObject(cand);
    }
    if (!font) {
      fonts_[f].glyphs.clear();
      continue;
    }
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    fonts_[f].glyphs.clear();
    fonts_[f].ascent = (float)tm.tmAscent;
    fonts_[f].lineHeight = (float)tm.tmHeight;
    std::vector<u32> cps = GlyphSet(specs[f].set);
    std::vector<WORD> gidx(cps.size(), 0xFFFF);
    if (specs[f].set == SET_ICONS) {
      std::wstring ws;
      for (u32 c : cps) ws.push_back((wchar_t)c);
      GetGlyphIndicesW(dc, ws.c_str(), (int)ws.size(), gidx.data(), GGI_MARK_NONEXISTING_GLYPHS);
    }
    for (size_t ci = 0; ci < cps.size(); ci++) {
      u32 cp = cps[ci];
      if (specs[f].set == SET_ICONS && gidx[ci] == 0xFFFF) continue;  // Symbol fehlt in der Schrift
      GLYPHMETRICS gm;
      DWORD size = GetGlyphOutlineW(dc, cp, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &mat);
      Pending p;
      p.font = (FontId)f;
      p.cp = cp;
      p.g = Glyph{};
      p.g.adv = (float)gm.gmCellIncX;
      if (size == GDI_ERROR) continue;
      if (size > 0 && gm.gmBlackBoxX > 0 && gm.gmBlackBoxY > 0) {
        std::vector<u8> buf(size);
        GetGlyphOutlineW(dc, cp, GGO_GRAY8_BITMAP, &gm, size, buf.data(), &mat);
        int w = gm.gmBlackBoxX, h = gm.gmBlackBoxY;
        int pitch = (w + 3) & ~3;
        p.bmp.w = w;
        p.bmp.h = h;
        p.bmp.px.resize(w * h);
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) p.bmp.px[y * w + x] = (u8)std::min(255, buf[y * pitch + x] * 255 / 64);
        p.g.w = (float)w;
        p.g.h = (float)h;
        p.g.xoff = (float)gm.gmptGlyphOrigin.x;
        p.g.yoff = (float)(tm.tmAscent - gm.gmptGlyphOrigin.y);
        area += (size_t)(w + 2) * (h + 2);
      } else {
        p.bmp.w = p.bmp.h = 0;
      }
      pend.push_back(std::move(p));
    }
    SelectObject(dc, old);
    DeleteObject(font);
  }
  DeleteDC(dc);

  int aw = 1024;
  while ((size_t)aw * aw < area * 1.35) aw *= 2;
  int ah = aw;
  // Höhe absteigend sortieren für den Regal-Packer
  std::vector<int> order(pend.size());
  for (size_t i = 0; i < pend.size(); i++) order[i] = (int)i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return pend[a].bmp.h > pend[b].bmp.h; });
  std::vector<u8> atlas((size_t)aw * ah, 0);
  int cx = 1, cy = 1, rowH = 0;
  for (int idx : order) {
    Pending& p = pend[idx];
    if (p.bmp.w > 0) {
      if (cx + p.bmp.w + 1 >= aw) { cx = 1; cy += rowH + 2; rowH = 0; }
      if (cy + p.bmp.h + 1 >= ah) break;
      for (int y = 0; y < p.bmp.h; y++) memcpy(&atlas[(size_t)(cy + y) * aw + cx], &p.bmp.px[y * p.bmp.w], p.bmp.w);
      p.g.u0 = (float)cx / aw;
      p.g.v0 = (float)cy / ah;
      p.g.u1 = (float)(cx + p.bmp.w) / aw;
      p.g.v1 = (float)(cy + p.bmp.h) / ah;
      cx += p.bmp.w + 2;
      rowH = std::max(rowH, p.bmp.h);
    }
    fonts_[p.font].glyphs[p.cp] = p.g;
  }
  if (!atlas_) glGenTextures(1, &atlas_);
  glBindTexture(GL_TEXTURE_2D, atlas_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aw, ah, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  atlasW_ = aw;
  atlasH_ = ah;
  builtScale_ = scale_;
}

void UI::Begin(int width, int height, bool inputEnabled, float dt) {
  width_ = std::max(1, width);
  height_ = std::max(1, height);
  scale_ = Clamp(height_ / 900.0f, 0.7f, 3.0f);
  if (fabsf(scale_ - builtScale_) > 0.04f) BuildFonts();
  input_ = inputEnabled;
  mouse_ = plat::MousePos();
  bool down = plat::MouseDown(plat::MOUSE_LEFT);
  clicked_ = input_ && plat::MousePressed(plat::MOUSE_LEFT);
  released_ = mouseDown_ && !down;
  mouseDown_ = down;
  float now = (float)plat::Time();
  dt_ = dt >= 0 ? dt : Clamp(now - time_, 0.0f, 0.1f);
  time_ = now;
  lastHot_ = hot_;
  hot_ = 0;
  alpha_ = 1;
  if (clicked_) focus_ = 0;  // Fokus wird vom angeklickten Textfeld neu gesetzt
  verts_.clear();
  clipStack_.clear();
}

void UI::End() {
  Flush();
  if (!mouseDown_) active_ = 0;
  if (hot_ && hot_ != lastHot_ && onHover) onHover();
}

float UI::Anim(u32 id, float target, float speed) {
  auto it = anim_.find(id);
  if (it == anim_.end()) it = anim_.emplace(id, target).first;
  it->second = Damp(it->second, target, speed, dt_);
  if (fabsf(it->second - target) < 0.001f) it->second = target;
  return it->second;
}

void UI::Flush() {
  if (verts_.empty()) return;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (!clipStack_.empty()) {
    const Rect& c = clipStack_.back();
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)c.x, (int)(height_ - c.y - c.h), (int)std::max(0.0f, c.w), (int)std::max(0.0f, c.h));
  }
  shader_.Use();
  shader_.Set("uScreen", Vec2((float)width_, (float)height_));
  shader_.Set("uFont", 0);
  shader_.Set("uBackdrop", 1);
  shader_.Set("uHasBackdrop", backdrop_ ? 1 : 0);
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, backdrop_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlas_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(UIVertex), verts_.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts_.size());
  glBindVertexArray(0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  verts_.clear();
}

void UI::PushClip(const Rect& r) {
  Flush();
  Rect c = r;
  if (!clipStack_.empty()) {
    const Rect& p = clipStack_.back();
    float x0 = std::max(c.x, p.x), y0 = std::max(c.y, p.y);
    float x1 = std::min(c.x + c.w, p.x + p.w), y1 = std::min(c.y + c.h, p.y + p.h);
    c = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
  }
  clipStack_.push_back(c);
}

void UI::PopClip() {
  Flush();
  if (!clipStack_.empty()) clipStack_.pop_back();
}

void UI::Quad4(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const Color* cols,
               const float* rect, float mode) {
  UIVertex v[4];
  float xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1};
  float us[4] = {u0, u1, u1, u0}, vs[4] = {v0, v0, v1, v1};
  for (int i = 0; i < 4; i++) {
    const Color& c = cols[i];
    v[i].x = xs[i]; v[i].y = ys[i]; v[i].u = us[i]; v[i].v = vs[i];
    v[i].c[0] = c.r; v[i].c[1] = c.g; v[i].c[2] = c.b;
    v[i].c[3] = mode == 3 ? c.a : (u8)(c.a * alpha_);
    for (int k = 0; k < 4; k++) v[i].rect[k] = rect ? rect[k] : 0.0f;
    if (mode == 3) v[i].rect[3] *= alpha_;
    v[i].mode = mode;
  }
  verts_.push_back(v[0]); verts_.push_back(v[1]); verts_.push_back(v[2]);
  verts_.push_back(v[0]); verts_.push_back(v[2]); verts_.push_back(v[3]);
}

void UI::Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c,
              const float* rect, float mode) {
  Color cols[4] = {c, c, c, c};
  Quad4(x0, y0, x1, y1, u0, v0, u1, v1, cols, rect, mode);
}

void UI::Rect_(const Rect& r, Color c, float radius) {
  if (c.a == 0 || r.w <= 0 || r.h <= 0) return;
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), 0};
  Quad(r.x, r.y, r.x + r.w, r.y + r.h, -hw, -hh, hw, hh, c, rc, 1);
}

void UI::RectGrad(const Rect& r, Color top, Color bottom, float radius) {
  if (r.w <= 0 || r.h <= 0) return;
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), 0};
  Color cols[4] = {top, top, bottom, bottom};
  Quad4(r.x, r.y, r.x + r.w, r.y + r.h, -hw, -hh, hw, hh, cols, rc, 1);
}

void UI::RectGradH(const Rect& r, Color left, Color right, float radius) {
  if (r.w <= 0 || r.h <= 0) return;
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), 0};
  Color cols[4] = {left, right, right, left};
  Quad4(r.x, r.y, r.x + r.w, r.y + r.h, -hw, -hh, hw, hh, cols, rc, 1);
}

void UI::Border(const Rect& r, Color c, float radius, float width) {
  if (c.a == 0) return;
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), std::max(width, 0.5f)};
  Quad(r.x, r.y, r.x + r.w, r.y + r.h, -hw, -hh, hw, hh, c, rc, 1);
}

void UI::Shadow(const Rect& r, float radius, float blur, Color c) {
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), blur};
  float e = blur * 2;
  Quad(r.x - e, r.y - e, r.x + r.w + e, r.y + r.h + e, -hw - e, -hh - e, hw + e, hh + e, c, rc, 2);
}

void UI::Glass(const Rect& r, float radius, float opacity, Color tint) {
  if (r.w <= 0 || r.h <= 0 || opacity <= 0) return;
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float rc[4] = {hw, hh, std::min(radius, std::min(hw, hh)), opacity};
  Quad(r.x, r.y, r.x + r.w, r.y + r.h, -hw, -hh, hw, hh, tint, rc, 3);
}

void UI::Panel(const Rect& r, float radius, float opacity, Color tint) {
  Shadow({r.x, r.y + S(6), r.w, r.h}, radius, S(18), Color(0, 0, 0, (u8)(110 * opacity)));
  Glass(r, radius, opacity, tint);
  RectGrad({r.x, r.y, r.w, std::min(r.h, S(70))}, Color(255, 255, 255, (u8)(14 * opacity)), Color(255, 255, 255, 0), radius);
  Border(r, Color(255, 255, 255, (u8)(26 * opacity)), radius, std::max(1.0f, S(1)));
}

void UI::Arc(Vec2 c, float radius, float width, float frac, Color col, float start) {
  if (frac <= 0 || col.a == 0) return;
  float e = radius + width;
  float rc[4] = {radius, width * 0.5f, Saturate(frac), start - floorf(start)};
  Quad(c.x - e, c.y - e, c.x + e, c.y + e, -e, -e, e, e, col, rc, 4);
}

void UI::Line(Vec2 a, Vec2 b, float width, Color c) {
  Vec2 d = b - a;
  float len = Length(d);
  if (len < 0.001f) return;
  Vec2 n = Vec2(-d.y, d.x) * (width * 0.5f / len);
  UIVertex v[4];
  Vec2 p[4] = {a + n, b + n, b - n, a - n};
  for (int i = 0; i < 4; i++) {
    v[i] = UIVertex{};
    v[i].x = p[i].x; v[i].y = p[i].y;
    v[i].c[0] = c.r; v[i].c[1] = c.g; v[i].c[2] = c.b; v[i].c[3] = (u8)(c.a * alpha_);
    v[i].rect[0] = 1e5f; v[i].rect[1] = 1e5f;
    v[i].mode = 1;
  }
  verts_.push_back(v[0]); verts_.push_back(v[1]); verts_.push_back(v[2]);
  verts_.push_back(v[0]); verts_.push_back(v[2]); verts_.push_back(v[3]);
}

void UI::GradientV(const Rect& r, Color top, Color bottom) {
  float rc[4] = {1e5f, 1e5f, 0, 0};
  Color cols[4] = {top, top, bottom, bottom};
  Quad4(r.x, r.y, r.x + r.w, r.y + r.h, 0, 0, 0, 0, cols, rc, 1);
}

void UI::GradientH(const Rect& r, Color left, Color right) {
  float rc[4] = {1e5f, 1e5f, 0, 0};
  Color cols[4] = {left, right, right, left};
  Quad4(r.x, r.y, r.x + r.w, r.y + r.h, 0, 0, 0, 0, cols, rc, 1);
}

const UI::Glyph* UI::GetGlyph(FontId f, u32 cp) const {
  auto it = fonts_[f].glyphs.find(cp);
  if (it != fonts_[f].glyphs.end()) return &it->second;
  if (f == FONT_ICON || f == FONT_ICON_L) return nullptr;
  it = fonts_[f].glyphs.find('?');
  return it != fonts_[f].glyphs.end() ? &it->second : nullptr;
}

Vec2 UI::Measure(FontId f, const std::string& s, float tracking) {
  float w = 0, maxW = 0;
  int lines = 1;
  size_t i = 0;
  while (i < s.size()) {
    u32 cp = Utf8Next(s.data(), s.size(), i);
    if (cp == '\n') { maxW = std::max(maxW, w); w = 0; lines++; continue; }
    const Glyph* g = GetGlyph(f, cp);
    if (g) w += g->adv + tracking;
  }
  if (w > 0) w -= tracking;
  return Vec2(std::max(maxW, w), lines * fonts_[f].lineHeight);
}

float UI::Text(FontId f, float x, float y, const std::string& s, Color c, Align align, float tracking) {
  float w = Measure(f, s, tracking).x;
  float sx = align == ALIGN_CENTER ? x - w * 0.5f : (align == ALIGN_RIGHT ? x - w : x);
  sx = floorf(sx + 0.5f);
  y = floorf(y + 0.5f);
  float cx = sx;
  size_t i = 0;
  while (i < s.size()) {
    u32 cp = Utf8Next(s.data(), s.size(), i);
    if (cp == '\n') { cx = sx; y += fonts_[f].lineHeight; continue; }
    const Glyph* g = GetGlyph(f, cp);
    if (!g) continue;
    if (g->w > 0) {
      float x0 = floorf(cx + g->xoff + 0.5f), y0 = y + g->yoff;
      Quad(x0, y0, x0 + g->w, y0 + g->h, g->u0, g->v0, g->u1, g->v1, c, nullptr, 0);
    }
    cx += g->adv + tracking;
  }
  return w;
}

float UI::TextShadow(FontId f, float x, float y, const std::string& s, Color c, Align align, float tracking) {
  float o = std::max(1.0f, S(1.5f));
  Text(f, x + o * 0.5f, y + o, s, Color(0, 0, 0, (u8)(c.a * 0.55f)), align, tracking);
  return Text(f, x, y, s, c, align, tracking);
}

void UI::TextIn(FontId f, const Rect& r, const std::string& s, Color c, Align align, float tracking) {
  float y = r.y + (r.h - fonts_[f].lineHeight) * 0.5f;
  float x = align == ALIGN_CENTER ? r.x + r.w * 0.5f : (align == ALIGN_RIGHT ? r.x + r.w : r.x);
  Text(f, x, y, s, c, align, tracking);
}

std::string UI::Upper(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    u8 ch = (u8)s[i];
    if (ch >= 'a' && ch <= 'z') o += (char)(ch - 32);
    else if (ch == 0xC3 && i + 1 < s.size()) {
      u8 n = (u8)s[i + 1];
      if (n == 0xA4 || n == 0xB6 || n == 0xBC) n -= 0x20;  // ä ö ü -> Ä Ö Ü
      o += (char)ch;
      o += (char)n;
      i++;
    } else o += (char)ch;
  }
  return o;
}

float UI::Label(float x, float y, const std::string& s, Color c, Align align) {
  return Text(FONT_LABEL, x, y, Upper(s), c, align, S(1.4f));
}

float UI::Icon(float x, float y, const char* ic, Color c, FontId f, Align align) {
  return Text(f, x, y, ic, c, align);
}

void UI::IconIn(FontId f, const Rect& r, const char* ic, Color c) {
  // Symbole sitzen in Segoe-Symbolschriften optisch etwas tief – an der Glyphenbox zentrieren
  size_t i = 0;
  std::string s(ic);
  u32 cp = Utf8Next(s.data(), s.size(), i);
  const Glyph* g = GetGlyph(f, cp);
  if (!g) return;
  float x0 = floorf(r.x + (r.w - g->w) * 0.5f + 0.5f), y0 = floorf(r.y + (r.h - g->h) * 0.5f + 0.5f);
  if (g->w > 0) Quad(x0, y0, x0 + g->w, y0 + g->h, g->u0, g->v0, g->u1, g->v1, c, nullptr, 0);
}

float UI::KeyCapWidth(const std::string& key, float height) {
  float h = height > 0 ? height : S(24);
  return std::max(h, Measure(FONT_LABEL, Upper(key), S(0.6f)).x + S(14));
}

float UI::KeyCap(float x, float y, const std::string& key, float height, Color c) {
  float h = height > 0 ? height : S(24);
  float w = KeyCapWidth(key, h);
  Rect r = {x, y, w, h};
  Rect_({r.x, r.y + S(2), r.w, r.h}, Color(0, 0, 0, (u8)(90 * c.a / 255)), S(5));
  RectGrad(r, Color(255, 255, 255, (u8)(46 * c.a / 255)), Color(255, 255, 255, (u8)(22 * c.a / 255)), S(5));
  Border(r, Color(255, 255, 255, (u8)(70 * c.a / 255)), S(5), std::max(1.0f, S(1)));
  TextIn(FONT_LABEL, r, Upper(key), c, ALIGN_CENTER, S(0.6f));
  return w;
}

std::vector<std::string> UI::Wrap(FontId f, const std::string& s, float maxWidth) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    std::string test = line.empty() ? word : line + " " + word;
    if (Measure(f, test).x > maxWidth && !line.empty()) { lines.push_back(line); line = word; }
    else line = test;
    word.clear();
  };
  for (char ch : s) {
    if (ch == ' ') flushWord();
    else if (ch == '\n') { flushWord(); lines.push_back(line); line.clear(); }
    else word += ch;
  }
  flushWord();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

u32 UI::Hash(const char* id) const {
  u32 h = 2166136261u;
  for (const char* p = id; *p; p++) h = (h ^ (u8)*p) * 16777619u;
  return h ? h : 1;
}

// --- Widgets ---------------------------------------------------------------------------------



bool UI::Button(const char* id, const Rect& r, const std::string& label, bool primary, bool enabled) {
  return ButtonEx(id, r, label, nullptr, primary ? BTN_PRIMARY : BTN_SECONDARY, enabled);
}

bool UI::ButtonEx(const char* id, const Rect& r, const std::string& label, const char* ic, int style, bool enabled) {
  u32 h = Hash(id);
  bool hover = enabled && Hovered(r);
  if (hover) hot_ = h;
  bool pressed = false;
  if (hover && clicked_) active_ = h;
  if (active_ == h && released_ && hover) { pressed = true; if (onClick) onClick(); }
  float t = Anim(h, hover ? 1.0f : 0.0f, 16.0f);
  bool down = active_ == h && hover;
  float rad = S(10);
  Rect rr = r;
  if (down) rr = {r.x + S(1), r.y + S(1), r.w - S(2), r.h - S(2)};
  Color tc = theme::Text;
  if (style == BTN_PRIMARY) {
    Shadow({rr.x, rr.y + S(4), rr.w, rr.h}, rad, S(10 + 8 * t), Color::Hex(0xFF8A1A, (u8)(60 + 70 * t)));
    Color top = Color::Lerp(Color::Hex(0xFFC444), Color::Hex(0xFFD266), t);
    Color bot = Color::Lerp(Color::Hex(0xFF9A1F), Color::Hex(0xFFAE33), t);
    if (!enabled) top = bot = Color::Hex(0x5a5246);
    RectGrad(rr, top, bot, rad);
    Border(rr, Color(255, 255, 255, (u8)(60 + 40 * t)), rad, std::max(1.0f, S(1)));
    tc = enabled ? Color::Hex(0x1a1206) : Color::Hex(0x9a9286);
  } else if (style == BTN_GHOST) {
    Rect_(rr, Color(255, 255, 255, (u8)(18 * t)), rad);
    tc = Color::Lerp(theme::Dim, theme::Text, t);
  } else {
    Color tint = style == BTN_DANGER ? Color::Hex(0x3a0f0f, 200) : Color::Hex(0x121826, 195);
    Glass(rr, rad, 1.0f, tint);
    Rect_(rr, Color(255, 255, 255, (u8)(14 + 20 * t)), rad);
    Color bc = style == BTN_DANGER ? theme::Danger : theme::Accent;
    Border(rr, Color::Lerp(Color(255, 255, 255, 34), bc.Alpha(0.85f), t), rad, std::max(1.0f, S(1.2f)));
    tc = enabled ? theme::Text : theme::Faint;
  }
  float iw = 0;
  if (ic) iw = S(30);
  Vec2 sz = Measure(FONT_MEDIUM, label);
  float total = sz.x + iw;
  float x = rr.x + (rr.w - total) * 0.5f;
  if (ic) IconIn(FONT_ICON, {x, rr.y, S(22), rr.h}, ic, tc);
  Text(FONT_MEDIUM, x + iw, rr.y + (rr.h - LineHeight(FONT_MEDIUM)) * 0.5f, label, tc);
  return pressed;
}

bool UI::IconButton(const char* id, const Rect& r, const char* ic, bool enabled) {
  u32 h = Hash(id);
  bool hover = enabled && Hovered(r);
  if (hover) hot_ = h;
  bool pressed = false;
  if (hover && clicked_) active_ = h;
  if (active_ == h && released_ && hover) { pressed = true; if (onClick) onClick(); }
  float t = Anim(h, hover ? 1.0f : 0.0f, 16.0f);
  float rad = std::min(r.w, r.h) * 0.5f;
  Rect_(r, Color(255, 255, 255, (u8)(16 + 30 * t)), rad);
  Border(r, Color::Lerp(Color(255, 255, 255, 30), theme::Accent.Alpha(0.9f), t), rad, std::max(1.0f, S(1)));
  IconIn(FONT_ICON, r, ic, enabled ? Color::Lerp(theme::Dim, theme::Text, t) : theme::Faint);
  return pressed;
}

bool UI::MenuItem(const char* id, const Rect& r, const std::string& label, const std::string& sub, const char* ic, bool accent) {
  u32 h = Hash(id);
  bool hover = Hovered(r);
  if (hover) hot_ = h;
  bool pressed = false;
  if (hover && clicked_) active_ = h;
  if (active_ == h && released_ && hover) { pressed = true; if (onClick) onClick(); }
  float t = Anim(h, hover ? 1.0f : 0.0f, 14.0f);
  float rad = S(12);
  float slide = S(10) * t;
  if (t > 0.01f) {
    Glass(r, rad, t, Color::Hex(0x121826, 130));
    RectGradH(r, Color(255, 255, 255, (u8)(26 * t)), Color(255, 255, 255, 0), rad);
    Border(r, Color(255, 255, 255, (u8)(30 * t)), rad, std::max(1.0f, S(1)));
    Rect_({r.x, r.y + r.h * 0.2f, S(3), r.h * 0.6f}, theme::Accent.Alpha(t), S(2));
  }
  // Symbol in abgerundetem Quadrat
  float is = r.h - S(18);
  Rect ib = {r.x + S(12) + slide, r.y + (r.h - is) * 0.5f, is, is};
  if (accent) {
    Shadow({ib.x, ib.y + S(3), ib.w, ib.h}, S(10), S(8), Color::Hex(0xFF8A1A, (u8)(70 + 60 * t)));
    RectGrad(ib, Color::Hex(0xFFC444), Color::Hex(0xFF8F1F), S(10));
    IconIn(FONT_ICON_L, ib, ic, Color::Hex(0x1a1206));
  } else {
    Rect_(ib, Color(255, 255, 255, (u8)(20 + 22 * t)), S(10));
    Border(ib, Color(255, 255, 255, (u8)(30 + 40 * t)), S(10), std::max(1.0f, S(1)));
    IconIn(FONT_ICON_L, ib, ic, Color::Lerp(theme::Dim, accent ? theme::Text : theme::Accent, t));
  }
  float tx = ib.x + ib.w + S(18);
  float lh = LineHeight(FONT_MEDIUM), sh = LineHeight(FONT_SMALL);
  float ty = r.y + (r.h - lh - sh) * 0.5f;
  TextShadow(FONT_MEDIUM, tx, ty, label, theme::Text);
  Text(FONT_SMALL, tx, ty + lh - S(2), sub, Color::Lerp(theme::Faint, theme::Dim, 0.5f + 0.5f * t));
  if (t > 0.01f) IconIn(FONT_ICON, {r.x + r.w - S(44) + S(6) * t, r.y, S(30), r.h}, icon::Right, theme::Accent.Alpha(t));
  return pressed;
}

void UI::RowBackground(u32 h, const Rect& r, bool hover) {
  float t = Anim(h ^ 0x9e3779b9u, hover ? 1.0f : 0.0f, 16.0f);
  float rad = S(10);
  Rect_(r, Color(255, 255, 255, (u8)(10 + 12 * t)), rad);
  Border(r, Color(255, 255, 255, (u8)(16 + 20 * t)), rad, std::max(1.0f, S(1)));
  if (t > 0.01f) Rect_({r.x, r.y + r.h * 0.25f, S(3), r.h * 0.5f}, theme::Accent.Alpha(t), S(2));
}

bool UI::Slider(const char* id, const Rect& r, const std::string& label, float* v, float lo, float hi, const char* fmt) {
  u32 h = Hash(id);
  bool hover = Hovered(r);
  if (hover) hot_ = h;
  RowBackground(h, r, hover || active_ == h);
  Text(FONT_NORMAL, r.x + S(16), r.y + (r.h - LineHeight(FONT_NORMAL)) * 0.5f, label, theme::Text);
  Rect track = {r.x + r.w * 0.47f, r.y + r.h * 0.5f - S(2), r.w * 0.36f, S(4)};
  if (hover && clicked_) active_ = h;
  bool changed = false;
  if (active_ == h && mouseDown_) {
    float tt = Saturate((mouse_.x - track.x) / track.w);
    float nv = lo + (hi - lo) * tt;
    if (nv != *v) { *v = nv; changed = true; }
  }
  float t = Saturate((*v - lo) / (hi - lo));
  float k = Anim(h + 7, hover || active_ == h ? 1.0f : 0.0f, 16.0f);
  Rect_(track, Color(255, 255, 255, 36), S(2));
  RectGradH({track.x, track.y, std::max(track.h, track.w * t), track.h}, theme::Accent2, theme::Accent, S(2));
  Vec2 kc(track.x + track.w * t, track.y + track.h * 0.5f);
  if (k > 0.01f) Circle(kc, S(13) * k, theme::Accent.Alpha(0.22f * k));
  Circle(kc, S(8), Color(250, 250, 255));
  Ring(kc, S(8), S(2), theme::Accent.Alpha(0.6f + 0.4f * k));
  char buf[64];
  snprintf(buf, sizeof(buf), fmt, *v);
  Rect pill = {r.x + r.w - S(62), r.y + r.h * 0.5f - S(13), S(50), S(26)};
  Rect_(pill, Color(255, 255, 255, 18), S(13));
  TextIn(FONT_LABEL, pill, buf, theme::Text);
  return changed;
}

bool UI::Checkbox(const char* id, const Rect& r, const std::string& label, bool* v) {
  u32 h = Hash(id);
  bool hover = Hovered(r);
  if (hover) hot_ = h;
  bool changed = false;
  if (hover && clicked_) { *v = !*v; changed = true; if (onClick) onClick(); }
  RowBackground(h, r, hover);
  Text(FONT_NORMAL, r.x + S(16), r.y + (r.h - LineHeight(FONT_NORMAL)) * 0.5f, label, theme::Text);
  float t = Anim(h + 3, *v ? 1.0f : 0.0f, 18.0f);
  Rect sw = {r.x + r.w - S(62), r.y + r.h * 0.5f - S(12), S(46), S(24)};
  Rect_(sw, Color::Lerp(Color(255, 255, 255, 34), theme::Accent, t), S(12));
  if (t > 0.01f) RectGradH(sw, theme::Accent2.Alpha(t), theme::Accent.Alpha(t), S(12));
  float kx = Lerp(sw.x + S(12), sw.x + sw.w - S(12), t);
  Shadow({kx - S(9), sw.y + S(4), S(18), S(18)}, S(9), S(3), Color(0, 0, 0, 80));
  Circle(Vec2(kx, sw.y + S(12)), S(9), Color(250, 250, 255));
  return changed;
}

bool UI::Selector(const char* id, const Rect& r, const std::string& label, int* index, const std::vector<std::string>& options) {
  u32 h = Hash(id);
  bool hover = Hovered(r);
  if (hover) hot_ = h;
  bool changed = false;
  int n = (int)options.size();
  RowBackground(h, r, hover);
  Text(FONT_NORMAL, r.x + S(16), r.y + (r.h - LineHeight(FONT_NORMAL)) * 0.5f, label, theme::Text);
  float bs = r.h - S(14);
  Rect left = {r.x + r.w * 0.47f, r.y + S(7), bs, bs};
  Rect right = {r.x + r.w - S(12) - bs, r.y + S(7), bs, bs};
  if (n > 0 && clicked_) {
    if (Hovered(left)) { *index = (*index - 1 + n) % n; changed = true; }
    else if (Hovered(right) || Hovered(Rect{left.x + left.w, r.y, right.x - left.x - left.w, r.h})) { *index = (*index + 1) % n; changed = true; }
    if (changed && onClick) onClick();
  }
  for (int s = 0; s < 2; s++) {
    Rect b = s ? right : left;
    bool bh = Hovered(b);
    Rect_(b, bh ? theme::Accent.Alpha(0.25f) : Color(255, 255, 255, 14), S(8));
    IconIn(FONT_ICON, b, s ? icon::Right : icon::Left, bh ? theme::Accent : theme::Dim);
  }
  if (n > 0) {
    int idx = ClampI(*index, 0, n - 1);
    Rect mid = {left.x + left.w, r.y - (n <= 8 ? S(4) : 0), right.x - left.x - left.w, r.h};
    TextIn(FONT_NORMAL, mid, options[idx], theme::Text);
    if (n <= 8 && n > 1) {
      float dw = S(5), gap = S(4);
      float total = n * dw + (n - 1) * gap;
      float dx = mid.x + mid.w * 0.5f - total * 0.5f;
      for (int i = 0; i < n; i++) {
        bool on = i == idx;
        Rect_({dx + i * (dw + gap), r.y + r.h - S(12), dw, S(3)}, on ? theme::Accent : Color(255, 255, 255, 50), S(1.5f));
      }
    }
  }
  return changed;
}

bool UI::TextField(const char* id, const Rect& r, std::string* text, int maxLen, const std::string& placeholder, const char* ic) {
  u32 h = Hash(id);
  bool hover = Hovered(r);
  if (hover) hot_ = h;
  if (hover && clicked_) focus_ = h;
  bool focused = focus_ == h;
  bool changed = false;
  if (focused) {
    const std::string& in = plat::TextInput();
    size_t i = 0;
    while (i < in.size()) {
      u32 cp = Utf8Next(in.data(), in.size(), i);
      int count = 0;
      size_t j = 0;
      while (j < text->size()) { Utf8Next(text->data(), text->size(), j); count++; }
      if (count < maxLen) { Utf8Append(*text, cp); changed = true; }
    }
    if (plat::KeyPressed(VK_BACK) && !text->empty()) { Utf8PopBack(*text); changed = true; }
    if (plat::KeyDown(VK_CONTROL) && plat::KeyPressed('V') && OpenClipboard(nullptr)) {
      HANDLE hd = GetClipboardData(CF_UNICODETEXT);
      if (hd) {
        const wchar_t* w = (const wchar_t*)GlobalLock(hd);
        if (w) {
          std::string s = WideToUtf8(w);
          for (char ch : s) if ((u8)ch >= 32 && (int)text->size() < maxLen * 2) *text += ch;
          changed = true;
          GlobalUnlock(hd);
        }
      }
      CloseClipboard();
    }
    if (plat::KeyPressed(VK_RETURN) || plat::KeyPressed(VK_ESCAPE)) focus_ = 0;
  }
  float f = Anim(h + 11, focused ? 1.0f : 0.0f, 16.0f);
  float rad = S(10);
  if (f > 0.01f) Shadow(r, rad, S(8), theme::Accent.Alpha(0.25f * f));
  Glass(r, rad, 1.0f, Color::Hex(0x070a10, (u8)(170 + 40 * f)));
  Border(r, Color::Lerp(Color(255, 255, 255, (u8)(hover ? 60 : 30)), theme::Accent, f), rad, std::max(1.0f, S(1.4f)));
  float tx = r.x + S(14);
  if (ic) {
    IconIn(FONT_ICON, {r.x + S(8), r.y, S(30), r.h}, ic, Color::Lerp(theme::Faint, theme::Accent, f));
    tx = r.x + S(44);
  }
  float ty = r.y + (r.h - LineHeight(FONT_NORMAL)) * 0.5f;
  PushClip({tx - S(2), r.y, r.x + r.w - tx - S(8), r.h});
  if (text->empty() && !focused) Text(FONT_NORMAL, tx, ty, placeholder, theme::Faint);
  float w = Measure(FONT_NORMAL, *text).x;
  float maxW = r.x + r.w - tx - S(14);
  float off = std::max(0.0f, w - maxW);
  Text(FONT_NORMAL, tx - off, ty, *text, theme::Text);
  if (focused && fmodf(time_, 1.0f) < 0.55f) Rect_({tx - off + w + S(1), ty + S(3), S(2), LineHeight(FONT_NORMAL) - S(6)}, theme::Accent);
  PopClip();
  return changed;
}
