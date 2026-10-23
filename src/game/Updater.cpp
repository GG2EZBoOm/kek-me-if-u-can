#include "game/Updater.h"
#include "game/Version.h"
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <chrono>

namespace update {

namespace {

// --- Minimaler JSON-Leser (genügt für die GitHub-Release-Antwort) ----------------------------

struct Json {
  enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Json> a;
  std::vector<std::pair<std::string, Json>> o;

  const Json& operator[](const char* key) const {
    static const Json none;
    for (const auto& kv : o)
      if (kv.first == key) return kv.second;
    return none;
  }
  std::string Text() const { return type == Str ? s : ""; }
};

class JsonReader {
 public:
  JsonReader(const std::string& t) : p_(t.data()), e_(t.data() + t.size()) {}
  bool Parse(Json& out) {
    bool ok = Value(out, 0);
    Ws();
    return ok && p_ == e_;
  }

 private:
  void Ws() {
    while (p_ < e_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) p_++;
  }
  bool Lit(const char* w) {
    size_t n = strlen(w);
    if ((size_t)(e_ - p_) < n || memcmp(p_, w, n) != 0) return false;
    p_ += n;
    return true;
  }
  bool Hex4(u32& v) {
    if (e_ - p_ < 4) return false;
    v = 0;
    for (int i = 0; i < 4; i++) {
      char c = *p_++;
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else return false;
    }
    return true;
  }
  bool String(std::string& out) {
    if (p_ >= e_ || *p_ != '"') return false;
    p_++;
    while (p_ < e_ && *p_ != '"') {
      char c = *p_++;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (p_ >= e_) return false;
      char x = *p_++;
      switch (x) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          u32 cp;
          if (!Hex4(cp)) return false;
          if (cp >= 0xD800 && cp < 0xDC00 && e_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u') {
            p_ += 2;
            u32 lo;
            if (!Hex4(lo)) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
          Utf8Append(out, cp);
          break;
        }
        default: return false;
      }
    }
    if (p_ >= e_) return false;
    p_++;
    return true;
  }
  bool Value(Json& v, int depth) {
    if (depth > 64) return false;
    Ws();
    if (p_ >= e_) return false;
    char c = *p_;
    if (c == '{') {
      p_++;
      v.type = Json::Obj;
      Ws();
      if (p_ < e_ && *p_ == '}') { p_++; return true; }
      for (;;) {
        Ws();
        std::string key;
        if (!String(key)) return false;
        Ws();
        if (p_ >= e_ || *p_++ != ':') return false;
        v.o.emplace_back(std::move(key), Json());
        if (!Value(v.o.back().second, depth + 1)) return false;
        Ws();
        if (p_ < e_ && *p_ == ',') { p_++; continue; }
        if (p_ < e_ && *p_ == '}') { p_++; return true; }
        return false;
      }
    }
    if (c == '[') {
      p_++;
      v.type = Json::Arr;
      Ws();
      if (p_ < e_ && *p_ == ']') { p_++; return true; }
      for (;;) {
        v.a.emplace_back();
        if (!Value(v.a.back(), depth + 1)) return false;
        Ws();
        if (p_ < e_ && *p_ == ',') { p_++; continue; }
        if (p_ < e_ && *p_ == ']') { p_++; return true; }
        return false;
      }
    }
    if (c == '"') {
      v.type = Json::Str;
      return String(v.s);
    }
    if (Lit("true")) { v.type = Json::Bool; v.b = true; return true; }
    if (Lit("false")) { v.type = Json::Bool; return true; }
    if (Lit("null")) return true;
    const char* s = p_;
    while (p_ < e_ && (isdigit((unsigned char)*p_) || *p_ == '-' || *p_ == '+' || *p_ == '.' || *p_ == 'e' || *p_ == 'E')) p_++;
    if (p_ == s) return false;
    v.type = Json::Num;
    v.n = strtod(std::string(s, p_).c_str(), nullptr);
    return true;
  }
  const char* p_;
  const char* e_;
};

// --- SHA-256 über BCrypt ----------------------------------------------------------------------

class Sha256 {
 public:
  Sha256() {
    if (BCryptOpenAlgorithmProvider(&alg_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0)
      BCryptCreateHash(alg_, &hash_, nullptr, 0, nullptr, 0, 0);
  }
  ~Sha256() {
    if (hash_) BCryptDestroyHash(hash_);
    if (alg_) BCryptCloseAlgorithmProvider(alg_, 0);
  }
  void Add(const void* d, size_t n) {
    if (hash_) BCryptHashData(hash_, (PUCHAR)d, (ULONG)n, 0);
  }
  std::string Hex() {
    u8 out[32] = {};
    if (!hash_ || BCryptFinishHash(hash_, out, sizeof(out), 0) != 0) return "";
    std::string h;
    for (u8 b : out) h += StrFormat("%02x", b);
    return h;
  }

 private:
  BCRYPT_ALG_HANDLE alg_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
};

// --- HTTPS über WinHTTP -----------------------------------------------------------------------

struct HInternet {
  HINTERNET h = nullptr;
  HInternet(HINTERNET x = nullptr) : h(x) {}
  ~HInternet() {
    if (h) WinHttpCloseHandle(h);
  }
  operator HINTERNET() const { return h; }
};

// Lädt eine HTTPS-Adresse; Weiterleitungen folgt WinHTTP selbst (nie von HTTPS auf HTTP).
// sink bekommt die Daten stückweise (Rückgabe false bricht ab), total ist Content-Length oder 0.
bool HttpsGet(const std::string& url, const wchar_t* extraHeaders, const std::function<bool(const char*, size_t, u64)>& sink,
              int& status, std::string& err) {
  status = 0;
  std::wstring wurl = Utf8ToWide(url);
  URL_COMPONENTS uc = {sizeof(uc)};
  uc.dwHostNameLength = (DWORD)-1;
  uc.dwUrlPathLength = (DWORD)-1;
  uc.dwExtraInfoLength = (DWORD)-1;
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
    err = "Ungültige Adresse";
    return false;
  }
  std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
  std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
  if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

  std::wstring agent = L"KekMeIfYouCan/" + Utf8ToWide(KEK_VERSION_STR);
  HInternet session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session.h) session.h = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h) {
    err = "WinHTTP nicht verfügbar";
    return false;
  }
  WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
  protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
  if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
  }
  HInternet conn(WinHttpConnect(session, host.c_str(), uc.nPort, 0));
  HInternet req(conn.h ? WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE)
                       : nullptr);
  if (!req.h) {
    err = "Verbindung fehlgeschlagen";
    return false;
  }
  if (!WinHttpSendRequest(req, extraHeaders ? extraHeaders : WINHTTP_NO_ADDITIONAL_HEADERS, extraHeaders ? (DWORD)-1L : 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(req, nullptr)) {
    DWORD e = GetLastError();
    err = e == ERROR_WINHTTP_NAME_NOT_RESOLVED || e == ERROR_WINHTTP_CANNOT_CONNECT ? "Keine Internetverbindung"
          : e == ERROR_WINHTTP_TIMEOUT                                             ? "Zeitüberschreitung"
                                                                                   : StrFormat("Verbindungsfehler %lu", e);
    return false;
  }
  DWORD code = 0, sz = sizeof(code);
  WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
                      WINHTTP_NO_HEADER_INDEX);
  status = (int)code;
  DWORD len = 0;
  sz = sizeof(len);
  if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &len, &sz,
                           WINHTTP_NO_HEADER_INDEX))
    len = 0;
  std::vector<char> buf(64 * 1024);
  for (;;) {
    DWORD got = 0;
    if (!WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &got)) {
      err = "Übertragung abgebrochen";
      return false;
    }
    if (got == 0) break;
    if (!sink(buf.data(), got, len)) return false;
  }
  return true;
}

std::wstring ExePath() {
  wchar_t p[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
  return std::wstring(p, n);
}

std::wstring DirOf(const std::wstring& p) {
  size_t s = p.find_last_of(L"\\/");
  return s == std::wstring::npos ? L"" : p.substr(0, s + 1);
}

bool SameName(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

bool FileExists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

}  // namespace

// --- Hilfen -----------------------------------------------------------------------------------

int CompareVersions(const std::string& a, const std::string& b) {
  auto parts = [](const std::string& v) {
    std::vector<int> p;
    size_t i = (!v.empty() && (v[0] == 'v' || v[0] == 'V')) ? 1 : 0;
    while (i < v.size() && isdigit((unsigned char)v[i])) {
      int n = 0;
      while (i < v.size() && isdigit((unsigned char)v[i])) n = n * 10 + (v[i++] - '0');
      p.push_back(n);
      if (i < v.size() && v[i] == '.') i++;
      else break;
    }
    while (p.size() < 3) p.push_back(0);
    return p;
  };
  std::vector<int> x = parts(a), y = parts(b);
  for (size_t i = 0; i < std::max(x.size(), y.size()); i++) {
    int u = i < x.size() ? x[i] : 0, w = i < y.size() ? y[i] : 0;
    if (u != w) return u < w ? -1 : 1;
  }
  return 0;
}

bool ParseRelease(const std::string& json, Release& out, std::string& err) {
  Json j;
  if (!JsonReader(json).Parse(j) || j.type != Json::Obj) {
    err = "Antwort von GitHub nicht lesbar";
    return false;
  }
  out = Release();
  out.tag = j["tag_name"].Text();
  out.version = !out.tag.empty() && (out.tag[0] == 'v' || out.tag[0] == 'V') ? out.tag.substr(1) : out.tag;
  if (out.version.empty() || !isdigit((unsigned char)out.version[0])) {
    err = "Release ohne Versionsnummer";
    return false;
  }
  const std::string prefix = std::string("https://github.com/") + kRepo + "/releases/download/";
  for (const Json& a : j["assets"].a) {
    Release::Asset as;
    as.name = a["name"].Text();
    as.url = a["browser_download_url"].Text();
    as.size = a["size"].type == Json::Num ? (u64)a["size"].n : 0;
    std::string d = a["digest"].Text();
    if (d.rfind("sha256:", 0) == 0) as.sha256 = d.substr(7);
    // Nur Dateien aus genau diesem Repository annehmen
    if (as.name.empty() || as.url.rfind(prefix, 0) != 0) continue;
    out.assets.push_back(as);
  }
  return true;
}

std::string Sha256Hex(const void* data, size_t size) {
  Sha256 h;
  h.Add(data, size);
  return h.Hex();
}

bool RestartSelf() {
  std::wstring exe = ExePath();
  std::wstring cmd = GetCommandLineW();
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    LOG("Update: Neustart fehlgeschlagen (%lu)", GetLastError());
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  LOG("Update: neue Version gestartet");
  return true;
}

void CleanupOldFiles(int retries) {
  std::wstring dir = DirOf(ExePath());
  for (int attempt = 0; attempt <= retries; attempt++) {
    int left = 0;
    for (const wchar_t* pattern : {L"*.exe.old", L"*.exe.new"}) {
      WIN32_FIND_DATAW fd;
      HANDLE f = FindFirstFileW((dir + pattern).c_str(), &fd);
      if (f == INVALID_HANDLE_VALUE) continue;
      do {
        if (!DeleteFileW((dir + fd.cFileName).c_str())) left++;
      } while (FindNextFileW(f, &fd));
      FindClose(f);
    }
    if (left == 0 || attempt == retries) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // die alte EXE beendet sich gerade noch
  }
}

// --- Updater ----------------------------------------------------------------------------------

Updater::~Updater() {
  cancel_ = true;
  if (thread_.joinable()) thread_.join();
}

void Updater::Set(Status::State st, const std::string& msg, float progress) {
  std::lock_guard<std::mutex> l(mu_);
  status_.state = st;
  if (!msg.empty() || st != Status::Downloading) status_.message = msg;
  if (progress >= 0) status_.progress = progress;
}

Status Updater::GetStatus() const {
  std::lock_guard<std::mutex> l(mu_);
  return status_;
}

bool Updater::Busy() const {
  std::lock_guard<std::mutex> l(mu_);
  return busy_;
}

void Updater::SetPreview(const Status& s) {
  std::lock_guard<std::mutex> l(mu_);
  status_ = s;
}

void Updater::Start(const std::string& currentVersion, bool install) {
  {
    std::lock_guard<std::mutex> l(mu_);
    if (busy_ || status_.state == Status::Installed) return;
    busy_ = true;
    status_ = Status();
    status_.state = Status::Checking;
  }
  if (thread_.joinable()) thread_.join();
  thread_ = std::thread([this, currentVersion, install]() { Work(currentVersion, install); });
}

Status Updater::Run(const std::string& currentVersion, bool install) {
  Start(currentVersion, install);
  if (thread_.joinable()) thread_.join();
  return GetStatus();
}

void Updater::Work(std::string current, bool install) {
  auto finish = [&](Status::State st, const std::string& msg) {
    Set(st, msg);
    std::lock_guard<std::mutex> l(mu_);
    busy_ = false;
  };
  CleanupOldFiles(6);

  // 1. Neueste Version abfragen
  std::string body, err;
  int code = 0;
  bool ok = HttpsGet(std::string("https://api.github.com/repos/") + kRepo + "/releases/latest",
                     L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n",
                     [&](const char* d, size_t n, u64) {
                       body.append(d, n);
                       return !cancel_ && body.size() < (4u << 20);
                     },
                     code, err);
  if (!ok) return finish(Status::Failed, err);
  if (code == 404) return finish(Status::UpToDate, "Noch keine Version veröffentlicht");
  if (code == 403 || code == 429) return finish(Status::Failed, "GitHub-Abfragelimit erreicht – später erneut");
  if (code != 200) return finish(Status::Failed, StrFormat("GitHub antwortet mit %d", code));
  Release rel;
  if (!ParseRelease(body, rel, err)) return finish(Status::Failed, err);
  {
    std::lock_guard<std::mutex> l(mu_);
    status_.latest = rel.version;
  }
  if (CompareVersions(rel.version, current) <= 0) {
    LOG("Update: Version %s ist aktuell", current.c_str());
    return finish(Status::UpToDate, "");
  }
  LOG("Update: Version %s verfügbar (installiert: %s)", rel.version.c_str(), current.c_str());
  if (!install) return finish(Status::UpToDate, "Version " + rel.version + " verfügbar");

  // 2. Welche Dateien werden ersetzt? Die laufende EXE immer, die zweite EXE nur, wenn sie danebenliegt.
  std::wstring self = ExePath(), dir = DirOf(self);
  std::wstring selfName = self.substr(dir.size());
  bool selfIsCli = SameName(selfName, L"KekMeIfYouCanCLI.exe");
  struct Job {
    const Release::Asset* asset;
    std::wstring target;
  };
  std::vector<Job> jobs;
  for (const Release::Asset& a : rel.assets) {
    std::wstring name = Utf8ToWide(a.name);
    bool gui = SameName(name, L"KekMeIfYouCan.exe"), cli = SameName(name, L"KekMeIfYouCanCLI.exe");
    if (!gui && !cli) continue;
    std::wstring target = dir + name;
    if ((gui && !selfIsCli) || (cli && selfIsCli)) target = self;  // auch eine umbenannte EXE wird ersetzt
    else if (!FileExists(target)) continue;
    jobs.push_back({&a, target});
  }
  bool hasSelf = false;
  for (const Job& j : jobs) hasSelf |= SameName(j.target, self);
  if (!hasSelf) return finish(Status::Failed, "Release enthält keine passende EXE");

  // 3. Herunterladen und prüfen
  u64 totalBytes = 0, doneBytes = 0;
  for (const Job& j : jobs) totalBytes += j.asset->size;
  Set(Status::Downloading, "", 0);
  for (const Job& j : jobs) {
    std::wstring tmp = j.target + L".new";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return finish(Status::Failed, "Keine Schreibrechte im Spielordner");
    Sha256 hash;
    u64 got = 0;
    std::string head;
    ok = HttpsGet(j.asset->url, nullptr,
                  [&](const char* d, size_t n, u64 total) {
                    if (head.size() < 2) head.append(d, std::min<size_t>(n, 2 - head.size()));
                    hash.Add(d, n);
                    got += n;
                    if (cancel_ || fwrite(d, 1, n, f) != n) return false;
                    u64 all = totalBytes ? totalBytes : total;
                    if (all) Set(Status::Downloading, "", (float)((double)(doneBytes + got) / (double)all));
                    return got <= (64ull << 20);
                  },
                  code, err);
    bool written = fclose(f) == 0;
    std::string digest = hash.Hex();
    std::string problem;
    if (!ok) problem = err.empty() ? "Download abgebrochen" : err;
    else if (code != 200) problem = StrFormat("Download: HTTP %d", code);
    else if (!written) problem = "Datei konnte nicht gespeichert werden";
    else if (j.asset->size && got != j.asset->size) problem = "Download unvollständig";
    else if (head != "MZ") problem = "Download ist keine Windows-Programmdatei";
    else if (!j.asset->sha256.empty() && _stricmp(digest.c_str(), j.asset->sha256.c_str()) != 0) problem = "Prüfsumme stimmt nicht";
    if (!problem.empty()) {
      DeleteFileW(tmp.c_str());
      for (const Job& o : jobs) DeleteFileW((o.target + L".new").c_str());
      LOG("Update: %s (%s)", problem.c_str(), j.asset->name.c_str());
      return finish(Status::Failed, problem);
    }
    LOG("Update: %s geladen, SHA-256 %s%s", j.asset->name.c_str(), digest.c_str(), j.asset->sha256.empty() ? " (ohne Vergleichswert)" : " geprüft");
    doneBytes += got;
  }

  // 4. Austauschen: laufende EXE -> *.old, neue Datei an ihren Platz; bei Fehlern zurückrollen
  std::vector<std::wstring> swapped;
  for (const Job& j : jobs) {
    std::wstring old = j.target + L".old", tmp = j.target + L".new";
    DeleteFileW(old.c_str());
    bool moved = MoveFileExW(j.target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
    if (!moved || !MoveFileExW(tmp.c_str(), j.target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
      DWORD e = GetLastError();
      if (moved) MoveFileExW(old.c_str(), j.target.c_str(), MOVEFILE_REPLACE_EXISTING);
      for (const std::wstring& t : swapped) {
        DeleteFileW(t.c_str());
        MoveFileExW((t + L".old").c_str(), t.c_str(), MOVEFILE_REPLACE_EXISTING);
      }
      for (const Job& o : jobs) DeleteFileW((o.target + L".new").c_str());
      LOG("Update: Austausch fehlgeschlagen (%lu)", e);
      return finish(Status::Failed, e == ERROR_ACCESS_DENIED ? "Datei gesperrt oder keine Schreibrechte" : "Austausch fehlgeschlagen");
    }
    swapped.push_back(j.target);
  }
  LOG("Update auf Version %s installiert", rel.version.c_str());
  Set(Status::Installed, "", 1.0f);
  std::lock_guard<std::mutex> l(mu_);
  busy_ = false;
}

}  // namespace update
