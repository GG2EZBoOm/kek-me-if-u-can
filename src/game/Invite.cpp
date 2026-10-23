#include "game/Invite.h"
#include "engine/Upnp.h"

namespace {
const char kAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";  // ohne 0/O/1/I

int CharValue(char c) {
  c = (char)toupper((unsigned char)c);
  for (int i = 0; i < 32; i++)
    if (kAlphabet[i] == c) return i;
  return -1;
}
}  // namespace

std::string EncodeInvite(u32 ip, u16 port) {
  // 48 Bit Nutzdaten + 2 Bit Prüfsumme = 50 Bit = 10 Zeichen
  u64 v = ((u64)ip << 16) | port;
  u32 sum = 0;
  for (int i = 0; i < 6; i++) sum += (u32)((v >> (i * 8)) & 255);
  v = (v << 2) | (sum & 3);
  char out[10];
  for (int i = 9; i >= 0; i--) {
    out[i] = kAlphabet[v & 31];
    v >>= 5;
  }
  return "KEK-" + std::string(out, 5) + "-" + std::string(out + 5, 5);
}

bool DecodeInvite(const std::string& text, u32& ip, u16& port) {
  std::string up;
  for (char c : text) up += (char)toupper((unsigned char)c);
  size_t p = up.find("KEK");
  while (p != std::string::npos) {
    std::string chars;
    for (size_t i = p + 3; i < up.size() && chars.size() < 10; i++) {
      char c = up[i];
      if (c == '-' || c == ' ') continue;
      if (CharValue(c) < 0) break;
      chars += c;
    }
    if (chars.size() == 10) {
      u64 v = 0;
      for (char c : chars) v = (v << 5) | (u64)CharValue(c);
      u32 check = (u32)(v & 3);
      v >>= 2;
      u32 sum = 0;
      for (int i = 0; i < 6; i++) sum += (u32)((v >> (i * 8)) & 255);
      if ((sum & 3) == check) {
        ip = (u32)(v >> 16);
        port = (u16)(v & 0xFFFF);
        return port != 0 && ip != 0;
      }
    }
    p = up.find("KEK", p + 3);
  }
  return false;
}

std::string ResolveJoinInput(const std::string& text) {
  u32 ip;
  u16 port;
  if (DecodeInvite(text, ip, port))
    return StrFormat("%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
  // IPv4 (optional mit Port) irgendwo im Text
  for (size_t i = 0; i < text.size(); i++) {
    if (!isdigit((unsigned char)text[i]) || (i > 0 && (isdigit((unsigned char)text[i - 1]) || text[i - 1] == '.'))) continue;
    size_t j = i;
    while (j < text.size() && (isdigit((unsigned char)text[j]) || text[j] == '.')) j++;
    std::string cand = text.substr(i, j - i);
    u32 tmp;
    if (upnp::ParseIPv4(cand, tmp)) {
      if (j < text.size() && text[j] == ':') {
        size_t k = j + 1;
        while (k < text.size() && isdigit((unsigned char)text[k])) k++;
        if (k > j + 1) cand = text.substr(i, k - i);
      }
      if (cand.size() != text.size()) return cand;
      break;
    }
  }
  std::string t = text;
  while (!t.empty() && isspace((unsigned char)t.back())) t.pop_back();
  while (!t.empty() && isspace((unsigned char)t.front())) t.erase(t.begin());
  return t;
}

std::string InviteMessage(const std::string& address, const std::string& code, const std::string& mapName) {
  return "Kek Me If You Can – spiel mit mir auf \"" + mapName + "\"! Code: " + code + " (Adresse " + address +
         ") · Im Spiel: Server beitreten → Code einfügen";
}
