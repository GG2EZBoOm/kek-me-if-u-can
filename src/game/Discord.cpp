#include "game/Discord.h"
#include <windows.h>

namespace {

enum Op { OP_HANDSHAKE = 0, OP_FRAME = 1, OP_CLOSE = 2, OP_PING = 3, OP_PONG = 4 };

std::string Esc(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': break;
      case '\t': o += ' '; break;
      default:
        if ((u8)c < 0x20) break;
        o += c;
    }
  }
  return o;
}

// Liest den ersten Zeichenkettenwert zu "key" (genügt für die wenigen Felder, die wir brauchen)
std::string JsonStr(const std::string& j, const char* key, size_t from = 0) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = j.find(k, from);
  while (p != std::string::npos) {
    size_t c = p + k.size();
    while (c < j.size() && (j[c] == ' ' || j[c] == ':')) c++;
    if (c < j.size() && j[c] == '"') {
      std::string out;
      for (size_t i = c + 1; i < j.size(); i++) {
        if (j[i] == '\\' && i + 1 < j.size()) {
          char n = j[++i];
          if (n == 'u' && i + 4 < j.size()) {
            Utf8Append(out, (u32)strtoul(j.substr(i + 1, 4).c_str(), nullptr, 16));
            i += 4;
          } else {
            out += n == 'n' ? '\n' : n;
          }
        } else if (j[i] == '"') {
          return out;
        } else {
          out += j[i];
        }
      }
      return out;
    }
    p = j.find(k, p + 1);
  }
  return "";
}

std::string Clip(const std::string& s, size_t maxBytes) {
  if (s.size() <= maxBytes) return s;
  size_t n = maxBytes;
  while (n > 0 && ((u8)s[n] & 0xC0) == 0x80) n--;  // nicht mitten im UTF-8-Zeichen abschneiden
  return s.substr(0, n);
}

// Discord darf das Spiel über "discord-<id>://" starten, wenn ein Freund auf "Beitreten" klickt
void RegisterLaunchCommand(const std::string& appId) {
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring key = L"Software\\Classes\\discord-" + Utf8ToWide(appId);
  std::wstring cmd = std::wstring(L"\"") + exe + L"\"";
  wchar_t cur[MAX_PATH * 2] = {};
  DWORD sz = sizeof(cur);
  if (RegGetValueW(HKEY_CURRENT_USER, (key + L"\\shell\\open\\command").c_str(), nullptr, RRF_RT_REG_SZ, nullptr, cur, &sz) ==
          ERROR_SUCCESS &&
      cmd == cur)
    return;
  std::wstring desc = L"URL:Run game " + Utf8ToWide(appId) + L" protocol";
  auto set = [](const std::wstring& k, const wchar_t* name, const std::wstring& v) {
    RegSetKeyValueW(HKEY_CURRENT_USER, k.c_str(), name, REG_SZ, v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
  };
  set(key, nullptr, desc);
  set(key, L"URL Protocol", L"");
  set(key + L"\\DefaultIcon", nullptr, exe);
  set(key + L"\\shell\\open\\command", nullptr, cmd);
  LOG("Discord: Startbefehl registriert");
}

}  // namespace

DiscordRpc::~DiscordRpc() {
  if (ready_) ClearActivity();
  Close();
}

void DiscordRpc::Init(const std::string& appId) {
  appId_ = appId;
  RegisterLaunchCommand(appId);
  nextTry_ = 0;
}

bool DiscordRpc::Open() {
  for (int i = 0; i < 10; i++) {
    std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
    HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      pipe_ = h;
      return true;
    }
  }
  return false;
}

void DiscordRpc::Close() {
  if (pipe_) CloseHandle((HANDLE)pipe_);
  pipe_ = nullptr;
  ready_ = false;
  lastActivity_.clear();
}

bool DiscordRpc::Write(int op, const std::string& json) {
  if (!pipe_) return false;
  std::vector<u8> buf(8 + json.size());
  u32 hdr[2] = {(u32)op, (u32)json.size()};
  memcpy(buf.data(), hdr, 8);
  memcpy(buf.data() + 8, json.data(), json.size());
  DWORD written = 0;
  if (!WriteFile((HANDLE)pipe_, buf.data(), (DWORD)buf.size(), &written, nullptr) || written != buf.size()) {
    Close();
    return false;
  }
  return true;
}

bool DiscordRpc::ReadFrame(int& op, std::string& json) {
  if (!pipe_) return false;
  DWORD avail = 0;
  if (!PeekNamedPipe((HANDLE)pipe_, nullptr, 0, nullptr, &avail, nullptr)) {
    Close();
    return false;
  }
  if (avail < 8) return false;
  u32 hdr[2];
  DWORD got = 0;
  if (!ReadFile((HANDLE)pipe_, hdr, 8, &got, nullptr) || got != 8) { Close(); return false; }
  op = (int)hdr[0];
  json.resize(hdr[1]);
  size_t off = 0;
  while (off < json.size()) {
    if (!ReadFile((HANDLE)pipe_, &json[off], (DWORD)(json.size() - off), &got, nullptr) || got == 0) { Close(); return false; }
    off += got;
  }
  return true;
}

void DiscordRpc::Send(const std::string& cmd, const std::string& argsJson, const char* evt) {
  std::string j = "{\"cmd\":\"" + cmd + "\",\"args\":" + argsJson;
  if (evt) j += std::string(",\"evt\":\"") + evt + "\"";
  j += ",\"nonce\":\"" + std::to_string(++nonce_) + "\"}";
  Write(OP_FRAME, j);
}

void DiscordRpc::Update(double now) {
  if (appId_.empty()) return;
  if (!pipe_) {
    if (now < nextTry_) return;
    nextTry_ = now + 12.0;
    if (!Open()) return;
    Write(OP_HANDSHAKE, "{\"v\":1,\"client_id\":\"" + appId_ + "\"}");
  }
  int op;
  std::string json;
  for (int guard = 0; guard < 16 && ReadFrame(op, json); guard++) {
    if (op == OP_PING) Write(OP_PONG, json);
    else if (op == OP_CLOSE) {
      LOG("Discord: Verbindung geschlossen (%s)", json.c_str());
      Close();
      return;
    } else if (op == OP_FRAME) Handle(json);
  }
}

void DiscordRpc::Handle(const std::string& json) {
  std::string evt = JsonStr(json, "evt");
  std::string cmd = JsonStr(json, "cmd");
  if (cmd == "DISPATCH" && evt == "READY") {
    ready_ = true;
    size_t u = json.find("\"user\"");
    std::string gname = u == std::string::npos ? "" : JsonStr(json, "global_name", u);
    userName_ = !gname.empty() ? gname : (u == std::string::npos ? "" : JsonStr(json, "username", u));
    LOG("Discord verbunden als %s", userName_.c_str());
    Send("SUBSCRIBE", "{}", "ACTIVITY_JOIN");
    Send("SUBSCRIBE", "{}", "ACTIVITY_JOIN_REQUEST");
  } else if (evt == "ACTIVITY_JOIN") {
    std::string secret = JsonStr(json, "secret");
    LOG("Discord: Beitreten angefordert");
    if (onJoin && !secret.empty()) onJoin(secret);
  } else if (evt == "ACTIVITY_JOIN_REQUEST") {
    size_t u = json.find("\"user\"");
    std::string id = u == std::string::npos ? "" : JsonStr(json, "id", u);
    std::string gname = u == std::string::npos ? "" : JsonStr(json, "global_name", u);
    std::string name = !gname.empty() ? gname : JsonStr(json, "username", u == std::string::npos ? 0 : u);
    if (onJoinRequest && !id.empty()) onJoinRequest(id, name);
  } else if (evt == "ERROR") {
    LOG("Discord-Fehler: %s", JsonStr(json, "message").c_str());
  }
}

void DiscordRpc::SetActivity(const DiscordActivity& a) {
  if (!ready_) return;
  std::string act = "{";
  auto field = [&](const char* k, const std::string& v) {
    if (v.size() < 2) return;
    if (act.size() > 1) act += ",";
    act += std::string("\"") + k + "\":\"" + Esc(Clip(v, 127)) + "\"";
  };
  field("details", a.details);
  field("state", a.state);
  if (a.startTime > 0) act += StrFormat("%s\"timestamps\":{\"start\":%lld}", act.size() > 1 ? "," : "", a.startTime);
  if (!a.largeImage.empty())
    act += StrFormat("%s\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"}", act.size() > 1 ? "," : "", Esc(a.largeImage).c_str(),
                     Esc(a.largeText).c_str());
  if (!a.partyId.empty() && a.partyMax > 0) {
    act += StrFormat("%s\"party\":{\"id\":\"%s\",\"size\":[%d,%d]}", act.size() > 1 ? "," : "", Esc(a.partyId).c_str(),
                     std::max(1, a.partySize), a.partyMax);
    if (!a.joinSecret.empty()) act += StrFormat(",\"secrets\":{\"join\":\"%s\"}", Esc(a.joinSecret).c_str());
  }
  act += StrFormat("%s\"instance\":false}", act.size() > 1 ? "," : "");
  if (act == lastActivity_) return;
  lastActivity_ = act;
  Send("SET_ACTIVITY", StrFormat("{\"pid\":%lu,\"activity\":%s}", GetCurrentProcessId(), act.c_str()));
}

void DiscordRpc::ClearActivity() {
  if (!ready_) return;
  lastActivity_.clear();
  Send("SET_ACTIVITY", StrFormat("{\"pid\":%lu}", GetCurrentProcessId()));
}

void DiscordRpc::RespondJoinRequest(const std::string& userId, bool accept) {
  if (!ready_) return;
  Send(accept ? "SEND_ACTIVITY_JOIN_INVITE" : "CLOSE_ACTIVITY_REQUEST", "{\"user_id\":\"" + Esc(userId) + "\"}");
}
