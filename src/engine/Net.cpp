#include "engine/Net.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace net {

static bool g_init = false;

bool Init() {
  if (g_init) return true;
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d) != 0) return false;
  g_init = true;
  return true;
}

void Shutdown() {
  if (g_init) WSACleanup();
  g_init = false;
}

std::string Address::ToString() const {
  return StrFormat("%u.%u.%u.%u:%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
}

bool Address::Parse(const std::string& text, u16 defaultPort, Address& out) {
  std::string host = text;
  u16 port = defaultPort;
  size_t c = text.rfind(':');
  if (c != std::string::npos) {
    host = text.substr(0, c);
    port = (u16)atoi(text.c_str() + c + 1);
  }
  while (!host.empty() && host.back() == ' ') host.pop_back();
  while (!host.empty() && host.front() == ' ') host.erase(host.begin());
  if (host.empty() || host == "localhost") host = "127.0.0.1";
  Init();
  addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
  sockaddr_in* sa = (sockaddr_in*)res->ai_addr;
  out.ip = ntohl(sa->sin_addr.s_addr);
  out.port = port;
  freeaddrinfo(res);
  return true;
}

bool Socket::Open(u16 port, bool broadcast) {
  Close();
  if (!Init()) return false;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return false;
  u_long nb = 1;
  ioctlsocket(s, FIONBIO, &nb);
  if (broadcast) {
    BOOL b = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&b, sizeof(b));
  }
  int bufSize = 1 << 20;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));
  // ICMP "Port unreachable" darf den Socket nicht lahmlegen
  BOOL noReset = FALSE;
  DWORD bytes = 0;
  WSAIoctl(s, _WSAIOW(IOC_VENDOR, 12), &noReset, sizeof(noReset), nullptr, 0, &bytes, nullptr, nullptr);
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(port);
  if (bind(s, (sockaddr*)&a, sizeof(a)) != 0) {
    closesocket(s);
    return false;
  }
  int len = sizeof(a);
  getsockname(s, (sockaddr*)&a, &len);
  port_ = ntohs(a.sin_port);
  sock_ = (uintptr_t)s;
  return true;
}

void Socket::Close() {
  if (IsOpen()) closesocket((SOCKET)sock_);
  sock_ = ~(uintptr_t)0;
}

bool Socket::Send(const Address& to, const void* data, int size) {
  if (!IsOpen()) return false;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(to.ip);
  a.sin_port = htons(to.port);
  return sendto((SOCKET)sock_, (const char*)data, size, 0, (sockaddr*)&a, sizeof(a)) == size;
}

int Socket::Recv(Address& from, void* buf, int maxSize) {
  if (!IsOpen()) return 0;
  sockaddr_in a = {};
  int len = sizeof(a);
  int n = recvfrom((SOCKET)sock_, (char*)buf, maxSize, 0, (sockaddr*)&a, &len);
  if (n <= 0) return 0;
  from.ip = ntohl(a.sin_addr.s_addr);
  from.port = ntohs(a.sin_port);
  return n;
}

}  // namespace net
