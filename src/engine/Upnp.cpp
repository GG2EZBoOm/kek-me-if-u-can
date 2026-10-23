#include "engine/Upnp.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <chrono>

namespace upnp {

namespace {

struct WsaGuard {
  WsaGuard() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
  ~WsaGuard() { WSACleanup(); }
};

std::string Lower(std::string s) {
  for (char& c : s) c = (char)tolower((unsigned char)c);
  return s;
}

// Wert eines HTTP-Kopfes (ohne Beachtung der Groß-/Kleinschreibung)
std::string Header(const std::string& resp, const char* name) {
  std::string l = Lower(resp), key = Lower(std::string(name)) + ":";
  size_t p = 0;
  while ((p = l.find(key, p)) != std::string::npos) {
    if (p == 0 || l[p - 1] == '\n') {
      size_t s = p + key.size(), e = resp.find('\r', s);
      std::string v = resp.substr(s, e == std::string::npos ? std::string::npos : e - s);
      size_t a = v.find_first_not_of(" \t");
      return a == std::string::npos ? "" : v.substr(a);
    }
    p += key.size();
  }
  return "";
}

std::string XmlValue(const std::string& xml, const std::string& tag, size_t from = 0, size_t* endPos = nullptr) {
  std::string open = "<" + tag + ">", close = "</" + tag + ">";
  size_t a = xml.find(open, from);
  if (a == std::string::npos) {
    // Mit Namensraum-Präfix (z. B. <u:Tag>)
    size_t p = from;
    while ((p = xml.find(":" + tag + ">", p)) != std::string::npos) {
      size_t lt = xml.rfind('<', p);
      if (lt != std::string::npos && xml[lt + 1] != '/') {
        std::string prefix = xml.substr(lt + 1, p - lt - 1);
        size_t s = p + tag.size() + 2;
        size_t e = xml.find("</" + prefix + ":" + tag + ">", s);
        if (e == std::string::npos) return "";
        if (endPos) *endPos = e;
        return xml.substr(s, e - s);
      }
      p++;
    }
    return "";
  }
  size_t s = a + open.size(), e = xml.find(close, s);
  if (e == std::string::npos) return "";
  if (endPos) *endPos = e + close.size();
  std::string v = xml.substr(s, e - s);
  size_t b = v.find_first_not_of(" \t\r\n"), z = v.find_last_not_of(" \t\r\n");
  return b == std::string::npos ? "" : v.substr(b, z - b + 1);
}

bool ParseUrl(const std::string& url, std::string& host, u16& port, std::string& path) {
  std::string u = url;
  if (Lower(u.substr(0, 7)) == "http://") u = u.substr(7);
  else if (u.find("://") != std::string::npos) return false;
  size_t slash = u.find('/');
  std::string hp = slash == std::string::npos ? u : u.substr(0, slash);
  path = slash == std::string::npos ? "/" : u.substr(slash);
  size_t colon = hp.find(':');
  host = colon == std::string::npos ? hp : hp.substr(0, colon);
  port = colon == std::string::npos ? 80 : (u16)atoi(hp.c_str() + colon + 1);
  return !host.empty() && port != 0;
}

// Einfache HTTP/1.1-Anfrage mit "Connection: close"; liefert die komplette Antwort
bool HttpRequest(const std::string& host, u16 port, const std::string& request, std::string& response, std::string* localIp,
                 int timeoutMs) {
  addrinfo hints = {}, *res = nullptr;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return false;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) { freeaddrinfo(res); return false; }
  u_long nb = 1;
  ioctlsocket(s, FIONBIO, &nb);
  connect(s, res->ai_addr, (int)res->ai_addrlen);
  freeaddrinfo(res);
  fd_set wf;
  FD_ZERO(&wf);
  FD_SET(s, &wf);
  timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
  if (select(0, nullptr, &wf, nullptr, &tv) <= 0) { closesocket(s); return false; }
  nb = 0;
  ioctlsocket(s, FIONBIO, &nb);
  DWORD to = (DWORD)timeoutMs;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
  if (localIp) {
    sockaddr_in la = {};
    int len = sizeof(la);
    if (getsockname(s, (sockaddr*)&la, &len) == 0) {
      char buf[32];
      inet_ntop(AF_INET, &la.sin_addr, buf, sizeof(buf));
      *localIp = buf;
    }
  }
  if (send(s, request.data(), (int)request.size(), 0) != (int)request.size()) { closesocket(s); return false; }
  response.clear();
  char buf[4096];
  for (;;) {
    int n = recv(s, buf, sizeof(buf), 0);
    if (n <= 0) break;
    response.append(buf, n);
    if (response.size() > (1 << 20)) break;
  }
  closesocket(s);
  return !response.empty();
}

int HttpStatus(const std::string& resp) {
  size_t sp = resp.find(' ');
  return sp == std::string::npos ? 0 : atoi(resp.c_str() + sp + 1);
}

std::string Body(const std::string& resp) {
  size_t p = resp.find("\r\n\r\n");
  return p == std::string::npos ? "" : resp.substr(p + 4);
}

// SSDP-Suche nach Internet-Gateways; liefert die Beschreibungs-URLs
std::vector<std::string> DiscoverGateways(int waitMs) {
  std::vector<std::string> found;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return found;
  // Multicast kann sonst über einen VPN-Adapter mit niedrigerer Metrik laufen,
  // obwohl die Internet-Standardroute (und unser Spielserver) das Heimnetz nutzt.
  std::string local = LocalIPv4();
  sockaddr_in source = {};
  source.sin_family = AF_INET;
  if (!local.empty() && inet_pton(AF_INET, local.c_str(), &source.sin_addr) == 1) {
    if (bind(s, (sockaddr*)&source, sizeof(source)) != 0 ||
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&source.sin_addr, sizeof(source.sin_addr)) != 0) {
      LOG("UPnP: Netzwerkschnittstelle %s nicht verfügbar (%d)", local.c_str(), WSAGetLastError());
      closesocket(s);
      return found;
    }
  }
  DWORD to = 300;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
  sockaddr_in dst = {};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(1900);
  inet_pton(AF_INET, "239.255.255.250", &dst.sin_addr);
  const char* targets[] = {"urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
                           "urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANPPPConnection:1"};
  for (int rep = 0; rep < 2; rep++)
    for (const char* st : targets) {
      std::string msg = StrFormat("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: %s\r\n\r\n", st);
      sendto(s, msg.data(), (int)msg.size(), 0, (sockaddr*)&dst, sizeof(dst));
    }
  auto t0 = std::chrono::steady_clock::now();
  char buf[2048];
  while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() < waitMs) {
    sockaddr_in from = {};
    int fl = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
    if (n <= 0) continue;
    buf[n] = 0;
    std::string loc = Header(buf, "LOCATION");
    if (!loc.empty() && std::find(found.begin(), found.end(), loc) == found.end()) found.push_back(loc);
  }
  closesocket(s);
  return found;
}

std::string Soap(const std::string& serviceType, const std::string& action, const std::string& args) {
  return "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" +
         action + " xmlns:u=\"" + serviceType + "\">" + args + "</u:" + action + "></s:Body></s:Envelope>\r\n";
}

bool SoapCall(const std::string& host, u16 port, const std::string& path, const std::string& serviceType, const std::string& action,
              const std::string& args, std::string& response, std::string* localIp = nullptr) {
  std::string body = Soap(serviceType, action, args);
  std::string req = StrFormat(
      "POST %s HTTP/1.1\r\nHOST: %s:%d\r\nCONTENT-TYPE: text/xml; charset=\"utf-8\"\r\nSOAPACTION: \"%s#%s\"\r\n"
      "CONTENT-LENGTH: %d\r\nConnection: close\r\n\r\n",
      path.c_str(), host.c_str(), port, serviceType.c_str(), action.c_str(), (int)body.size());
  req += body;
  return HttpRequest(host, port, req, response, localIp, 2500);
}

std::string SoapError(const std::string& resp) {
  std::string code = XmlValue(resp, "errorCode"), desc = XmlValue(resp, "errorDescription");
  if (code.empty()) return StrFormat("HTTP %d", HttpStatus(resp));
  return code + (desc.empty() ? "" : " " + desc);
}

}  // namespace

bool ParseIPv4(const std::string& text, u32& ip) {
  if (text.size() > 15) return false;
  size_t pos = 0;
  u32 value = 0;
  for (int part = 0; part < 4; part++) {
    unsigned octet = 0, digits = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
      octet = octet * 10 + (text[pos++] - '0'); digits++;
      if (octet > 255 || digits > 3) return false;
    }
    if (!digits) return false;
    value = (value << 8) | octet;
    if (part < 3) { if (pos >= text.size() || text[pos++] != '.') return false; }
    else if (pos != text.size()) return false;
  }
  ip = value;
  return true;
}

bool IsPrivateIPv4(const std::string& text) {
  u32 ip;
  if (!ParseIPv4(text, ip)) return true;
  u32 a = ip >> 24, b = (ip >> 16) & 255;
  return a == 10 || a == 127 || a == 0 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 169 && b == 254) ||
         (a == 100 && b >= 64 && b <= 127);
}

bool IsPublicIPv4(const std::string& text) {
  u32 ip;
  if (!ParseIPv4(text, ip) || IsPrivateIPv4(text)) return false;
  u32 a = ip >> 24, b = (ip >> 16) & 255, c = (ip >> 8) & 255;
  if (a >= 224 || (a == 198 && (b == 18 || b == 19))) return false;
  if ((a == 192 && b == 0 && c == 2) || (a == 198 && b == 51 && c == 100) || (a == 203 && b == 0 && c == 113)) return false;
  if (a == 192 && b == 0 && c == 0 && ip != 0xc0000009u && ip != 0xc000000au) return false;
  return true;
}

std::string LocalIPv4() {
  WsaGuard wsa;
  // UDP-"Verbindung" sendet nichts, legt aber die ausgehende Schnittstelle fest
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return "";
  sockaddr_in dst = {};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(53);
  inet_pton(AF_INET, "192.0.2.1", &dst.sin_addr);  // Dokumentationsnetz, wird nie kontaktiert
  std::string out;
  if (connect(s, (sockaddr*)&dst, sizeof(dst)) == 0) {
    sockaddr_in la = {};
    int len = sizeof(la);
    if (getsockname(s, (sockaddr*)&la, &len) == 0) {
      char buf[32];
      inet_ntop(AF_INET, &la.sin_addr, buf, sizeof(buf));
      out = buf;
    }
  }
  closesocket(s);
  return out == "0.0.0.0" ? "" : out;
}

// --- PortMapper ------------------------------------------------------------------------------

PortMapper::~PortMapper() {
  Remove();
  Join();
}

void PortMapper::Join() {
  if (thread_.joinable()) thread_.join();
}

void PortMapper::Start(u16 port, const std::string& description) {
  Join();
  {
    std::lock_guard<std::mutex> l(mu_);
    status_ = Status();
    status_.state = Status::Working;
    status_.message = "Suche Router …";
  }
  thread_ = std::thread([this, port, description]() { Run(port, description); });
}

Status PortMapper::GetStatus() const {
  std::lock_guard<std::mutex> l(mu_);
  return status_;
}

void PortMapper::Run(u16 port, std::string desc) {
  WsaGuard wsa;
  auto fail = [&](const std::string& msg) {
    std::lock_guard<std::mutex> l(mu_);
    status_.state = Status::Failed;
    status_.message = msg;
    if (status_.localIp.empty()) status_.localIp = LocalIPv4();
    LOG("UPnP: %s", msg.c_str());
  };
  std::vector<std::string> locations = DiscoverGateways(2500);
  if (locations.empty()) return fail("Kein UPnP-Router gefunden");
  for (const std::string& loc : locations) {
    std::string host, path;
    u16 hport;
    if (!ParseUrl(loc, host, hport, path)) continue;
    std::string resp;
    std::string req = StrFormat("GET %s HTTP/1.1\r\nHOST: %s:%d\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str(), hport);
    if (!HttpRequest(host, hport, req, resp, nullptr, 2500) || HttpStatus(resp) != 200) continue;
    std::string xml = Body(resp);
    std::string base = XmlValue(xml, "URLBase");
    // Dienst WANIPConnection oder WANPPPConnection suchen
    size_t p = 0;
    std::string ctrl, stype;
    while ((p = xml.find("<service>", p)) != std::string::npos) {
      size_t e = xml.find("</service>", p);
      if (e == std::string::npos) break;
      std::string block = xml.substr(p, e - p);
      std::string st = XmlValue(block, "serviceType");
      if (st.find("WANIPConnection") != std::string::npos || st.find("WANPPPConnection") != std::string::npos) {
        stype = st;
        ctrl = XmlValue(block, "controlURL");
        break;
      }
      p = e;
    }
    if (ctrl.empty()) continue;
    std::string chost = host, cpath = ctrl;
    u16 cport = hport;
    if (Lower(ctrl.substr(0, 7)) == "http://") {
      ParseUrl(ctrl, chost, cport, cpath);
    } else {
      if (!base.empty()) ParseUrl(base, chost, cport, path);
      if (cpath.empty() || cpath[0] != '/') cpath = "/" + cpath;
    }
    std::string localIp;
    std::string r;
    std::string ext;
    if (SoapCall(chost, cport, cpath, stype, "GetExternalIPAddress", "", r, &localIp) && HttpStatus(r) == 200)
      ext = XmlValue(r, "NewExternalIPAddress");
    if (localIp.empty()) localIp = LocalIPv4();
    {
      std::lock_guard<std::mutex> l(mu_);
      status_.externalIp = ext;
      status_.localIp = localIp;
      status_.message = "Gebe Port frei …";
    }
    std::string err;
    for (int lease : {0, 7200}) {
      std::string args = StrFormat(
          "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>"
          "<NewInternalPort>%d</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled>"
          "<NewPortMappingDescription>%s</NewPortMappingDescription><NewLeaseDuration>%d</NewLeaseDuration>",
          port, port, localIp.c_str(), desc.c_str(), lease);
      if (SoapCall(chost, cport, cpath, stype, "AddPortMapping", args, r) && HttpStatus(r) == 200) {
        std::lock_guard<std::mutex> l(mu_);
        controlHost_ = chost;
        controlPath_ = cpath;
        controlPort_ = cport;
        serviceType_ = stype;
        port_ = port;
        mapped_ = true;
        status_.state = Status::Mapped;
        status_.message = IsPrivateIPv4(ext) && !ext.empty() ? "Port offen, aber der Router hat keine öffentliche IPv4 (Carrier-NAT/DS-Lite)"
                                                             : "UDP-Port automatisch freigegeben";
        LOG("UPnP: Port %d -> %s:%d freigegeben (extern %s)", port, localIp.c_str(), port, ext.c_str());
        return;
      }
      err = SoapError(r);
    }
    return fail("Router lehnt Portfreigabe ab (" + err + ")");
  }
  fail("Router unterstützt keine Portfreigabe per UPnP");
}

void PortMapper::Remove() {
  Join();
  std::string host, path, stype;
  u16 cport = 0, port = 0;
  {
    std::lock_guard<std::mutex> l(mu_);
    if (!mapped_) return;
    mapped_ = false;
    host = controlHost_;
    path = controlPath_;
    stype = serviceType_;
    cport = controlPort_;
    port = port_;
    status_ = Status();
  }
  WsaGuard wsa;
  std::string r;
  SoapCall(host, cport, path, stype, "DeletePortMapping",
           StrFormat("<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>", port), r);
  LOG("UPnP: Portfreigabe %d entfernt", port);
}

// --- PublicIpLookup --------------------------------------------------------------------------

PublicIpLookup::~PublicIpLookup() {
  if (thread_.joinable()) thread_.join();
}

void PublicIpLookup::Start() {
  {
    std::lock_guard<std::mutex> l(mu_);
    if (busy_) return;
    busy_ = true;
    result_.clear();
    error_.clear();
  }
  if (thread_.joinable()) thread_.join();
  thread_ = std::thread([this]() {
    std::string body, ip, err;
    HINTERNET session = WinHttpOpen(L"KekMeIfYouCan", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) WinHttpSetTimeouts(session, 4000, 4000, 4000, 4000);
    HINTERNET conn = session ? WinHttpConnect(session, L"api.ipify.org", INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", L"/", nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(req, nullptr);
    DWORD status = 0, size = sizeof(status);
    ok = ok && WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) && status == 200;
    while (ok) {
      char buf[64]; DWORD got = 0;
      if (!WinHttpReadData(req, buf, sizeof(buf), &got)) { ok = false; break; }
      if (!got) break;
      body.append(buf, got);
      if (body.size() > 64) { ok = false; break; }
    }
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    if (session) WinHttpCloseHandle(session);
    while (!body.empty() && isspace((unsigned char)body.back())) body.pop_back();
    while (!body.empty() && isspace((unsigned char)body.front())) body.erase(body.begin());
    if (ok && IsPublicIPv4(body)) ip = body;
    else err = ok ? "Keine gültige öffentliche IPv4 erhalten" : "Öffentliche IP konnte nicht ermittelt werden – erneut versuchen";
    std::lock_guard<std::mutex> l(mu_);
    result_ = ip;
    error_ = err;
    busy_ = false;
  });
}

bool PublicIpLookup::Busy() const {
  std::lock_guard<std::mutex> l(mu_);
  return busy_;
}

std::string PublicIpLookup::Result() const {
  std::lock_guard<std::mutex> l(mu_);
  return result_;
}

std::string PublicIpLookup::Error() const {
  std::lock_guard<std::mutex> l(mu_);
  return error_;
}

}  // namespace upnp
