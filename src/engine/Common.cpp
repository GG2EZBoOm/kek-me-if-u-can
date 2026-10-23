#include "engine/Common.h"
#include <windows.h>
#include <cstdarg>
#include <mutex>

static FILE* g_logFile = nullptr;
static bool g_logStdout = false;
static std::mutex g_logMutex;

void LogInit(const char* path, bool toStdout) {
  g_logStdout = toStdout;
  if (path && *path) g_logFile = fopen(path, "w");
}

void Log(const char* fmt, ...) {
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
  va_end(ap);
  size_t n = strlen(buf);
  if (n == 0 || buf[n - 1] != '\n') { buf[n] = '\n'; buf[n + 1] = 0; }
  std::lock_guard<std::mutex> lock(g_logMutex);
  if (g_logFile) { fputs(buf, g_logFile); fflush(g_logFile); }
  if (g_logStdout) { fputs(buf, stdout); fflush(stdout); }
  OutputDebugStringA(buf);
}

std::string ExeDir() {
  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring w(path, n);
  size_t p = w.find_last_of(L"\\/");
  if (p != std::wstring::npos) w.resize(p + 1);
  return WideToUtf8(w);
}

std::string StrFormat(const char* fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return buf;
}

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}

u32 Utf8Next(const char* s, size_t len, size_t& i) {
  u8 c = (u8)s[i++];
  if (c < 0x80) return c;
  int extra = 0;
  u32 cp = 0;
  if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
  else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
  else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
  else return '?';
  while (extra-- > 0 && i < len) {
    u8 cc = (u8)s[i];
    if ((cc & 0xC0) != 0x80) return '?';
    cp = (cp << 6) | (cc & 0x3F);
    i++;
  }
  return cp;
}

void Utf8Append(std::string& out, u32 cp) {
  if (cp < 0x80) out += (char)cp;
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
  }
}

void Utf8PopBack(std::string& s) {
  while (!s.empty()) {
    u8 c = (u8)s.back();
    s.pop_back();
    if ((c & 0xC0) != 0x80) break;
  }
}

bool ReadFileText(const std::string& path, std::string& out) {
  FILE* f = _wfopen(Utf8ToWide(path).c_str(), L"rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  out.resize(n > 0 ? n : 0);
  if (n > 0) fread(out.data(), 1, n, f);
  fclose(f);
  return true;
}

bool WriteFileText(const std::string& path, const std::string& text) {
  FILE* f = _wfopen(Utf8ToWide(path).c_str(), L"wb");
  if (!f) return false;
  fwrite(text.data(), 1, text.size(), f);
  fclose(f);
  return true;
}
