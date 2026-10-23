#pragma once
// Einladungscodes ("KEK-XXXXX-XXXXX") für Server-Adressen: IPv4 + Port, gut abtippbar und in Discord teilbar.
#include "engine/Common.h"

std::string EncodeInvite(u32 ip, u16 port);
// Findet einen Code auch mitten in einem eingefügten Text
bool DecodeInvite(const std::string& text, u32& ip, u16& port);
// Eingabe des Beitreten-Feldes auflösen: Code -> "a.b.c.d:port", IPv4 in längerem Text -> Adresse, sonst unverändert
std::string ResolveJoinInput(const std::string& text);
// Zeile zum Kopieren nach Discord
std::string InviteMessage(const std::string& address, const std::string& code, const std::string& mapName);
