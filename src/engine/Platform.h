#pragma once
// Fenster, OpenGL-Kontext, Eingabe und Zeitmessung (Win32).
#include "engine/Common.h"
#include "engine/Math.h"

namespace plat {

enum MouseButton { MOUSE_LEFT = 0, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_X1, MOUSE_X2, MOUSE_COUNT };

bool Init(const char* title, int width, int height, bool hidden);
void Shutdown();
// Verarbeitet Fensternachrichten. Gibt false zurück, wenn das Fenster geschlossen wurde.
bool PumpEvents();
void Present();
void SetVSync(bool on);

int Width();
int Height();
bool IsFocused();
bool IsHidden();
void SetFullscreen(bool on);
bool IsFullscreen();
void RequestQuit();
// Zwischenablage (UTF-8)
void SetClipboardText(const std::string& text);
std::string GetClipboardText();

// Tastatur (Virtual-Key-Codes von Windows)
bool KeyDown(int vk);
bool KeyPressed(int vk);   // in diesem Frame gedrückt
bool KeyReleased(int vk);
// Taste wurde seit dem letzten Aufruf von ConsumeKeyTap gedrückt (verliert keine kurzen Tipper)
bool ConsumeKeyTap(int vk);

bool MouseDown(int b);
bool MousePressed(int b);
bool MouseReleased(int b);
bool ConsumeMouseTap(int b);
Vec2 MousePos();
Vec2 MouseDelta();  // Roh-Mausbewegung seit letztem Frame
int WheelDelta();
const std::string& TextInput();  // UTF-8-Zeichen dieses Frames
void SetMouseCaptured(bool captured);
bool IsMouseCaptured();

double Time();
std::string KeyName(int vk);

}  // namespace plat
