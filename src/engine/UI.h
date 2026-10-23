#pragma once
// 2D-Oberfläche: Schrift-Atlas (GDI-gerastert), Batch-Renderer mit SDF-Rechtecken, Milchglas, Icons,
// animierte Immediate-Mode-Widgets.
#include "engine/Common.h"
#include "engine/Math.h"
#include "engine/Shader.h"
#include <unordered_map>

enum FontId {
  FONT_SMALL = 0,  // Fließtext klein (Segoe UI)
  FONT_NORMAL,     // Fließtext
  FONT_MEDIUM,     // Schaltflächen, Zeilen (Segoe UI Semibold)
  FONT_LARGE,      // Überschriften (Bahnschrift)
  FONT_HUGE,       // Ansagen
  FONT_LABEL,      // kleine Großbuchstaben-Beschriftungen
  FONT_NUM,        // große Zahlen (Zeit, Munition, Lebenspunkte)
  FONT_TITLE,      // Logo
  FONT_ICON,       // Symbole (Segoe Fluent Icons / MDL2)
  FONT_ICON_L,
  FONT_COUNT
};

enum Align { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

struct Color {
  u8 r = 255, g = 255, b = 255, a = 255;
  constexpr Color() = default;
  constexpr Color(u8 r_, u8 g_, u8 b_, u8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
  static constexpr Color Hex(u32 rgb, u8 a = 255) { return Color((u8)((rgb >> 16) & 255), (u8)((rgb >> 8) & 255), (u8)(rgb & 255), a); }
  Color Alpha(float f) const { return Color(r, g, b, (u8)Clamp(a * f, 0.0f, 255.0f)); }
  static Color Lerp(Color x, Color y, float t) {
    return Color((u8)::Lerp(x.r, y.r, t), (u8)::Lerp(x.g, y.g, t), (u8)::Lerp(x.b, y.b, t), (u8)::Lerp(x.a, y.a, t));
  }
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool Contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
  Vec2 Center() const { return Vec2(x + w * 0.5f, y + h * 0.5f); }
  Rect Inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

// Farbwelt der Oberfläche
namespace theme {
constexpr Color Accent = Color::Hex(0xFFB627);    // Bernstein
constexpr Color Accent2 = Color::Hex(0xFF7A1A);   // Orange (Verläufe)
constexpr Color Text = Color::Hex(0xF5F6FA);
constexpr Color Dim = Color::Hex(0xAAB1BF);
constexpr Color Faint = Color::Hex(0x6E7685);
constexpr Color Glass = Color::Hex(0x0B0F17, 182);  // Tönung über dem weichgezeichneten Hintergrund
constexpr Color Line = Color(255, 255, 255, 22);
constexpr Color Danger = Color::Hex(0xFF5A4E);
constexpr Color Ok = Color::Hex(0x5AD16F);
}  // namespace theme

// Symbole aus der Schrift "Segoe Fluent Icons" bzw. "Segoe MDL2 Assets"
namespace icon {
constexpr const char* Play = "";
constexpr const char* Settings = "";
constexpr const char* People = "";
constexpr const char* Globe = "";
constexpr const char* Back = "";
constexpr const char* Close = "";
constexpr const char* Refresh = "";
constexpr const char* Power = "";
constexpr const char* Lock = "";
constexpr const char* Unlock = "";
constexpr const char* Chat = "";
constexpr const char* Timer = "";
constexpr const char* Star = "";
constexpr const char* Warning = "";
constexpr const char* Info = "";
constexpr const char* Check = "";
constexpr const char* Game = "";
constexpr const char* Volume = "";
constexpr const char* Copy = "";
constexpr const char* Paste = "";
constexpr const char* Download = "";
constexpr const char* Left = "";
constexpr const char* Right = "";
constexpr const char* Down = "";
constexpr const char* Up = "";
constexpr const char* Contact = "";
constexpr const char* Home = "";
constexpr const char* Pin = "";
constexpr const char* Edit = "";
constexpr const char* Monitor = "";
constexpr const char* Keyboard = "";
constexpr const char* Mouse = "";
constexpr const char* Wifi = "";
constexpr const char* Link = "";
constexpr const char* World = "";
constexpr const char* Bolt = "";
constexpr const char* History = "";
constexpr const char* Mic = "";
constexpr const char* Eye = "";
constexpr const char* Help = "";
constexpr const char* Flag = "";
constexpr const char* Target = "";
constexpr const char* Search = "";
constexpr const char* Package = "";
constexpr const char* Send = "";
constexpr const char* Trophy = "";
}  // namespace icon

class UI {
 public:
  bool Init();
  void Shutdown();
  // Pro Frame aufrufen, baut bei Größenänderung die Schriften neu.
  void Begin(int width, int height, bool inputEnabled, float dt = -1.0f);
  void End();

  float Scale() const { return scale_; }
  float S(float v) const { return v * scale_; }
  int W() const { return width_; }
  int H() const { return height_; }
  float Dt() const { return dt_; }
  // Weichgezeichnetes Bild für Milchglas-Flächen (0 = keins, dann deckende Tönung)
  void SetBackdrop(unsigned tex) { backdrop_ = tex; }
  // Globale Deckkraft (Bildschirm-Überblendungen)
  void SetAlpha(float a) { alpha_ = Saturate(a); }
  float GetAlpha() const { return alpha_; }

  // Zeichnen
  void Rect_(const Rect& r, Color c, float radius = 0);
  void RectGrad(const Rect& r, Color top, Color bottom, float radius = 0);
  void RectGradH(const Rect& r, Color left, Color right, float radius = 0);
  void Border(const Rect& r, Color c, float radius, float width);
  void Shadow(const Rect& r, float radius, float blur, Color c);
  void Glass(const Rect& r, float radius, float opacity = 1.0f, Color tint = theme::Glass);
  // Milchglas-Tafel mit Schatten, Rand und Glanzkante
  void Panel(const Rect& r, float radius, float opacity = 1.0f, Color tint = theme::Glass);
  void Circle(Vec2 c, float radius, Color col) { Rect_({c.x - radius, c.y - radius, radius * 2, radius * 2}, col, radius); }
  void Ring(Vec2 c, float radius, float width, Color col) { Border({c.x - radius, c.y - radius, radius * 2, radius * 2}, col, radius, width); }
  // Kreisbogen ab 12 Uhr im Uhrzeigersinn, frac 0..1
  void Arc(Vec2 c, float radius, float width, float frac, Color col, float start = 0);
  void Line(Vec2 a, Vec2 b, float width, Color c);
  void GradientV(const Rect& r, Color top, Color bottom);
  void GradientH(const Rect& r, Color left, Color right);
  float Text(FontId f, float x, float y, const std::string& s, Color c, Align align = ALIGN_LEFT, float tracking = 0);
  float TextShadow(FontId f, float x, float y, const std::string& s, Color c, Align align = ALIGN_LEFT, float tracking = 0);
  // Text vertikal zentriert in einem Rechteck
  void TextIn(FontId f, const Rect& r, const std::string& s, Color c, Align align = ALIGN_CENTER, float tracking = 0);
  // Beschriftung in Großbuchstaben mit Sperrung
  float Label(float x, float y, const std::string& s, Color c, Align align = ALIGN_LEFT);
  float Icon(float x, float y, const char* ic, Color c, FontId f = FONT_ICON, Align align = ALIGN_LEFT);
  void IconIn(FontId f, const Rect& r, const char* ic, Color c);
  // Tastenkappe ("E", "Leertaste" ...) – gibt die Breite zurück
  float KeyCap(float x, float y, const std::string& key, float height = 0, Color c = theme::Text);
  float KeyCapWidth(const std::string& key, float height = 0);
  Vec2 Measure(FontId f, const std::string& s, float tracking = 0);
  float LineHeight(FontId f) const { return fonts_[f].lineHeight; }
  // Zeilenumbruch auf maximale Breite
  std::vector<std::string> Wrap(FontId f, const std::string& s, float maxWidth);
  void PushClip(const Rect& r);
  void PopClip();
  static std::string Upper(const std::string& s);

  // Weiche Animation eines Wertes je Kennung (0..1-Übergänge, Hover usw.)
  float Anim(const char* id, float target, float speed = 12.0f) { return Anim(Hash(id), target, speed); }
  float Anim(u32 id, float target, float speed = 12.0f);

  // Widgets (Rückgabe true bei Interaktion)
  enum ButtonStyle { BTN_PRIMARY = 0, BTN_SECONDARY, BTN_GHOST, BTN_DANGER };
  bool Button(const char* id, const Rect& r, const std::string& label, bool primary = false, bool enabled = true);
  bool ButtonEx(const char* id, const Rect& r, const std::string& label, const char* ic, int style, bool enabled = true);
  bool IconButton(const char* id, const Rect& r, const char* ic, bool enabled = true);
  // Großer Menüeintrag mit Symbol, Titel und Untertitel
  bool MenuItem(const char* id, const Rect& r, const std::string& label, const std::string& sub, const char* ic, bool accent = false);
  bool Slider(const char* id, const Rect& r, const std::string& label, float* v, float lo, float hi, const char* fmt = "%.2f");
  bool Checkbox(const char* id, const Rect& r, const std::string& label, bool* v);
  bool Selector(const char* id, const Rect& r, const std::string& label, int* index, const std::vector<std::string>& options);
  bool TextField(const char* id, const Rect& r, std::string* text, int maxLen, const std::string& placeholder = "",
                 const char* ic = nullptr);
  bool Hovered(const Rect& r) const { return input_ && r.Contains(mouse_); }
  bool AnyActive() const { return active_ != 0 || focus_ != 0; }
  void ClearFocus() { focus_ = 0; }
  void Focus(const char* id) { focus_ = Hash(id); }
  bool Focused(const char* id) const { return focus_ == Hash(id); }
  Vec2 Mouse() const { return mouse_; }
  bool Clicked() const { return clicked_; }
  float Time() const { return time_; }
  // Wird aufgerufen, wenn ein Widget angeklickt wird (z. B. für Klick-Sound)
  std::function<void()> onClick;
  std::function<void()> onHover;

 private:
  struct Glyph {
    float u0, v0, u1, v1;
    float w, h, xoff, yoff, adv;
  };
  struct FontData {
    std::unordered_map<u32, Glyph> glyphs;
    float lineHeight = 0, ascent = 0;
  };
  struct UIVertex {
    float x, y, u, v;
    u8 c[4];
    float rect[4];
    float mode;
  };
  void BuildFonts();
  void Flush();
  void Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c, const float* rect,
            float mode);
  void Quad4(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const Color* cols,
             const float* rect, float mode);
  const Glyph* GetGlyph(FontId f, u32 cp) const;
  u32 Hash(const char* id) const;
  void RowBackground(u32 h, const Rect& r, bool hover);

  Shader shader_;
  GLuint vao_ = 0, vbo_ = 0, atlas_ = 0;
  unsigned backdrop_ = 0;
  int atlasW_ = 0, atlasH_ = 0;
  FontData fonts_[FONT_COUNT];
  std::vector<UIVertex> verts_;
  std::vector<Rect> clipStack_;
  std::unordered_map<u32, float> anim_;
  int width_ = 1, height_ = 1;
  float scale_ = 1, builtScale_ = 0;
  float alpha_ = 1;
  bool input_ = true;
  Vec2 mouse_;
  bool mouseDown_ = false, clicked_ = false, released_ = false;
  u32 hot_ = 0, active_ = 0, focus_ = 0, lastHot_ = 0;
  float time_ = 0, dt_ = 0;
  float caretBlink_ = 0;
};
