#include "engine/Platform.h"
#include "engine/GL.h"
#include <windows.h>
#include <mmsystem.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
typedef HGLRC(WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFN_wglSwapIntervalEXT)(int);

namespace plat {

static HWND g_hwnd = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_glrc = nullptr;
static int g_width = 0, g_height = 0;
static bool g_hidden = false;
static bool g_quit = false;
static bool g_focused = true;
static bool g_captured = false;
static bool g_fullscreen = false;
static WINDOWPLACEMENT g_prevPlacement = {sizeof(WINDOWPLACEMENT)};
static PFN_wglSwapIntervalEXT g_swapInterval = nullptr;

static bool g_keyDown[256];
static bool g_keyPrev[256];
static bool g_keyFramePressed[256];
static bool g_keyTap[256];
static bool g_mouseDown[MOUSE_COUNT];
static bool g_mousePrev[MOUSE_COUNT];
static bool g_mouseFramePressed[MOUSE_COUNT];
static bool g_mouseTap[MOUSE_COUNT];
static Vec2 g_mousePos;
static Vec2 g_mouseDelta, g_mouseAccum;
static int g_wheel = 0, g_wheelAccum = 0;
static std::string g_text, g_textAccum;
static LARGE_INTEGER g_qpcFreq, g_qpcStart;

static void UpdateClip() {
  if (g_captured && g_focused) {
    RECT r;
    GetClientRect(g_hwnd, &r);
    POINT a = {r.left, r.top}, b = {r.right, r.bottom};
    ClientToScreen(g_hwnd, &a);
    ClientToScreen(g_hwnd, &b);
    RECT clip = {a.x, a.y, b.x, b.y};
    ClipCursor(&clip);
  } else {
    ClipCursor(nullptr);
  }
}

static void ClearInput() {
  memset(g_keyDown, 0, sizeof(g_keyDown));
  memset(g_mouseDown, 0, sizeof(g_mouseDown));
}

static void MouseButtonEvent(int b, bool down) {
  if (down) {
    if (!g_mouseDown[b]) { g_mouseFramePressed[b] = true; g_mouseTap[b] = true; }
    SetCapture(g_hwnd);
  } else {
    ReleaseCapture();
  }
  g_mouseDown[b] = down;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CLOSE: g_quit = true; return 0;
    case WM_SIZE:
      g_width = LOWORD(lp);
      g_height = HIWORD(lp);
      UpdateClip();
      return 0;
    case WM_MOVE: UpdateClip(); return 0;
    case WM_SETFOCUS: g_focused = true; UpdateClip(); return 0;
    case WM_KILLFOCUS:
      g_focused = false;
      ClearInput();
      ClipCursor(nullptr);
      return 0;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
      int vk = (int)wp;
      if (vk == VK_RETURN && (lp & (1 << 29))) {  // Alt+Enter
        SetFullscreen(!g_fullscreen);
        return 0;
      }
      if (vk < 256) {
        if (!(lp & (1 << 30))) { g_keyFramePressed[vk] = true; g_keyTap[vk] = true; }
        g_keyDown[vk] = true;
      }
      if (msg == WM_SYSKEYDOWN && vk != VK_F4) return 0;
      break;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP:
      if (wp < 256) g_keyDown[wp] = false;
      if (msg == WM_SYSKEYUP) return 0;
      break;
    case WM_SYSCHAR: return 0;
    case WM_CHAR:
      if (wp >= 32 && wp != 127) Utf8Append(g_textAccum, (u32)wp);
      return 0;
    case WM_LBUTTONDOWN: MouseButtonEvent(MOUSE_LEFT, true); return 0;
    case WM_LBUTTONUP: MouseButtonEvent(MOUSE_LEFT, false); return 0;
    case WM_RBUTTONDOWN: MouseButtonEvent(MOUSE_RIGHT, true); return 0;
    case WM_RBUTTONUP: MouseButtonEvent(MOUSE_RIGHT, false); return 0;
    case WM_MBUTTONDOWN: MouseButtonEvent(MOUSE_MIDDLE, true); return 0;
    case WM_MBUTTONUP: MouseButtonEvent(MOUSE_MIDDLE, false); return 0;
    case WM_XBUTTONDOWN: MouseButtonEvent(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? MOUSE_X1 : MOUSE_X2, true); return TRUE;
    case WM_XBUTTONUP: MouseButtonEvent(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? MOUSE_X1 : MOUSE_X2, false); return TRUE;
    case WM_MOUSEMOVE:
      g_mousePos = Vec2((float)(short)LOWORD(lp), (float)(short)HIWORD(lp));
      return 0;
    case WM_MOUSEWHEEL: g_wheelAccum += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA; return 0;
    case WM_INPUT: {
      RAWINPUT raw;
      UINT size = sizeof(raw);
      if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1) {
        if (raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
          g_mouseAccum.x += (float)raw.data.mouse.lLastX;
          g_mouseAccum.y += (float)raw.data.mouse.lLastY;
        }
      }
      break;
    }
    case WM_SETCURSOR:
      if (g_captured && LOWORD(lp) == HTCLIENT) { SetCursor(nullptr); return TRUE; }
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool CreateGLContext() {
  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;
  int pf = ChoosePixelFormat(g_hdc, &pfd);
  if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) { LOG("Pixelformat fehlgeschlagen"); return false; }
  HGLRC legacy = wglCreateContext(g_hdc);
  if (!legacy) { LOG("wglCreateContext fehlgeschlagen"); return false; }
  wglMakeCurrent(g_hdc, legacy);
  auto createAttribs = (PFN_wglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");
  if (!createAttribs) { LOG("wglCreateContextAttribsARB fehlt"); return false; }
  int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                   WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
  g_glrc = createAttribs(g_hdc, nullptr, attribs);
  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(legacy);
  if (!g_glrc) { LOG("OpenGL 3.3 Core wird nicht unterstützt"); return false; }
  wglMakeCurrent(g_hdc, g_glrc);
  if (!GLLoadFunctions()) return false;
  g_swapInterval = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
  LOG("OpenGL: %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
  return true;
}

bool Init(const char* title, int width, int height, bool hidden) {
  QueryPerformanceFrequency(&g_qpcFreq);
  QueryPerformanceCounter(&g_qpcStart);
  g_hidden = hidden;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  timeBeginPeriod(1);

  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc = {sizeof(wc)};
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  // Programmsymbol aus den Ressourcen (res/app.rc), passend für Taskleiste und Titelleiste
  wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
  wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
  if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.lpszClassName = L"KekMeIfYouCanEngine";
  RegisterClassExW(&wc);

  DWORD style = WS_OVERLAPPEDWINDOW;
  RECT r = {0, 0, width, height};
  AdjustWindowRect(&r, style, FALSE);
  int ww = r.right - r.left, wh = r.bottom - r.top;
  int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
  int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
  std::wstring wtitle = Utf8ToWide(title);
  g_hwnd = CreateWindowExW(0, wc.lpszClassName, wtitle.c_str(), style, sx > 0 ? sx : 0, sy > 0 ? sy : 0, ww, wh,
                           nullptr, nullptr, inst, nullptr);
  if (!g_hwnd) { LOG("Fenster konnte nicht erstellt werden"); return false; }
  g_width = width;
  g_height = height;
  g_hdc = GetDC(g_hwnd);
  if (!CreateGLContext()) return false;

  RAWINPUTDEVICE rid = {};
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.hwndTarget = g_hwnd;
  RegisterRawInputDevices(&rid, 1, sizeof(rid));

  if (!hidden) {
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    RECT cr;
    GetClientRect(g_hwnd, &cr);
    g_width = cr.right - cr.left;
    g_height = cr.bottom - cr.top;
  }
  return true;
}

void Shutdown() {
  ClipCursor(nullptr);
  if (g_glrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(g_glrc); }
  if (g_hwnd) { ReleaseDC(g_hwnd, g_hdc); DestroyWindow(g_hwnd); }
  timeEndPeriod(1);
}

bool PumpEvents() {
  memcpy(g_keyPrev, g_keyDown, sizeof(g_keyDown));
  memcpy(g_mousePrev, g_mouseDown, sizeof(g_mouseDown));
  memset(g_keyFramePressed, 0, sizeof(g_keyFramePressed));
  memset(g_mouseFramePressed, 0, sizeof(g_mouseFramePressed));
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) g_quit = true;
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  g_mouseDelta = g_mouseAccum;
  g_mouseAccum = Vec2();
  g_wheel = g_wheelAccum;
  g_wheelAccum = 0;
  g_text.swap(g_textAccum);
  g_textAccum.clear();
  if (!g_focused) g_mouseDelta = Vec2();
  return !g_quit;
}

void Present() { SwapBuffers(g_hdc); }
void SetVSync(bool on) { if (g_swapInterval) g_swapInterval(on ? 1 : 0); }
int Width() { return g_width; }
int Height() { return g_height; }
bool IsFocused() { return g_focused; }
bool IsHidden() { return g_hidden; }
bool IsFullscreen() { return g_fullscreen; }
void RequestQuit() { g_quit = true; }

void SetClipboardText(const std::string& text) {
  if (!OpenClipboard(g_hwnd)) return;
  EmptyClipboard();
  std::wstring w = Utf8ToWide(text);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
  if (mem) {
    memcpy(GlobalLock(mem), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(mem);
    if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
  }
  CloseClipboard();
}

std::string GetClipboardText() {
  std::string out;
  if (!OpenClipboard(g_hwnd)) return out;
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (h) {
    const wchar_t* w = (const wchar_t*)GlobalLock(h);
    if (w) out = WideToUtf8(w);
    GlobalUnlock(h);
  }
  CloseClipboard();
  return out;
}

void SetFullscreen(bool on) {
  if (on == g_fullscreen || g_hidden) return;
  DWORD style = GetWindowLongW(g_hwnd, GWL_STYLE);
  if (on) {
    MONITORINFO mi = {sizeof(mi)};
    GetWindowPlacement(g_hwnd, &g_prevPlacement);
    GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
    SetWindowLongW(g_hwnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  } else {
    SetWindowLongW(g_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(g_hwnd, &g_prevPlacement);
    SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
  g_fullscreen = on;
  UpdateClip();
}

bool KeyDown(int vk) { return vk >= 0 && vk < 256 && g_keyDown[vk]; }
bool KeyPressed(int vk) { return vk >= 0 && vk < 256 && g_keyFramePressed[vk]; }
bool KeyReleased(int vk) { return vk >= 0 && vk < 256 && !g_keyDown[vk] && g_keyPrev[vk]; }
bool ConsumeKeyTap(int vk) {
  if (vk < 0 || vk >= 256) return false;
  bool t = g_keyTap[vk];
  g_keyTap[vk] = false;
  return t;
}
bool MouseDown(int b) { return b >= 0 && b < MOUSE_COUNT && g_mouseDown[b]; }
bool MousePressed(int b) { return b >= 0 && b < MOUSE_COUNT && g_mouseFramePressed[b]; }
bool MouseReleased(int b) { return b >= 0 && b < MOUSE_COUNT && !g_mouseDown[b] && g_mousePrev[b]; }
bool ConsumeMouseTap(int b) {
  if (b < 0 || b >= MOUSE_COUNT) return false;
  bool t = g_mouseTap[b];
  g_mouseTap[b] = false;
  return t;
}
Vec2 MousePos() { return g_mousePos; }
Vec2 MouseDelta() { return g_mouseDelta; }
int WheelDelta() { return g_wheel; }
const std::string& TextInput() { return g_text; }

void SetMouseCaptured(bool captured) {
  if (captured == g_captured || g_hidden) return;
  g_captured = captured;
  if (captured) {
    RECT r;
    GetClientRect(g_hwnd, &r);
    POINT c = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
    ClientToScreen(g_hwnd, &c);
    SetCursorPos(c.x, c.y);
    SetCursor(nullptr);
  } else {
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
  }
  UpdateClip();
  g_mouseAccum = Vec2();
}
bool IsMouseCaptured() { return g_captured; }

double Time() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)(now.QuadPart - g_qpcStart.QuadPart) / (double)g_qpcFreq.QuadPart;
}

std::string KeyName(int vk) {
  switch (vk) {
    case VK_SPACE: return "Leertaste";
    case VK_SHIFT: return "Umschalt";
    case VK_CONTROL: return "Strg";
    case VK_TAB: return "Tab";
    case VK_ESCAPE: return "Esc";
    case VK_RETURN: return "Enter";
  }
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, (char)vk);
  if (vk >= VK_F1 && vk <= VK_F12) return StrFormat("F%d", vk - VK_F1 + 1);
  return StrFormat("#%d", vk);
}

}  // namespace plat
