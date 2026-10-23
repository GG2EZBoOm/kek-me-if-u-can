#pragma once
// Grundtypen und Hilfsfunktionen der Engine.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

#define COUNTOF(a) (int)(sizeof(a) / sizeof((a)[0]))

void LogInit(const char* path, bool toStdout);
void Log(const char* fmt, ...);
#define LOG(...) Log(__VA_ARGS__)

std::string ExeDir();
std::string StrFormat(const char* fmt, ...);
std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
// Dekodiert einen UTF-8-Codepunkt ab s[i] und rückt i weiter.
u32 Utf8Next(const char* s, size_t len, size_t& i);
void Utf8Append(std::string& out, u32 cp);
// Entfernt das letzte UTF-8-Zeichen.
void Utf8PopBack(std::string& s);
bool ReadFileText(const std::string& path, std::string& out);
bool WriteFileText(const std::string& path, const std::string& text);
