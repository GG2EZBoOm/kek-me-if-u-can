#include "engine/Upnp.h"
#include <winsock2.h>
#include <ws2tcpip.h>
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
  int a, b, c, d;
  char tail;
  if (sscanf(text.c_str(), "%d.%d.%d.%d%c", &a, &b, &c, &d, &tail) != 4) return false;
  if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) return false;
  ip = ((u32)a << 24) | ((u32)b << 16) | ((u32)c << 8) | (u32)d;
  return true;
}

bool IsPrivateIPv4(const std::string& text) {
  u32 ip;
  if (!ParseIPv4(text, ip)) return true;
  u32 a = ip >> 24, b = (ip >> 16) & 255;
  return a == 10 || a == 127 || a == 0 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 169 && b == 254) ||
         (a == 100 && b >= 64 && b <= 127);
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
    error_.clear();
  }
  if (thread_.joinable()) thread_.join();
  thread_ = std::thread([this]() {
    WsaGuard wsa;
    std::string resp, ip, err;
    const char* req = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nUser-Agent: KekMeIfYouCan\r\nConnection: close\r\n\r\n";
    if (HttpRequest("api.ipify.org", 80, req, resp, nullptr, 4000) && HttpStatus(resp) == 200) {
      std::string body = Body(resp);
      // erstes IPv4-artiges Wort im Text (auch bei chunked-Kodierung)
      size_t i = 0;
      while (i < body.size()) {
        size_t j = i;
        while (j < body.size() && (isdigit((unsigned char)body[j]) || body[j] == '.')) j++;
        u32 tmp;
        if (j > i && ParseIPv4(body.substr(i, j - i), tmp)) { ip = body.substr(i, j - i); break; }
        i = j + 1;
      }
      if (ip.empty()) err = "Unerwartete Antwort";
    } else {
      err = "Keine Verbindung zu api.ipify.org";
    }
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
