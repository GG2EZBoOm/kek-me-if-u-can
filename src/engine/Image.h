#pragma once
#include "engine/Common.h"

// Schreibt ein RGBA8-Bild als PNG (unkomprimierte Deflate-Blöcke). Zeilen von oben nach unten.
bool WritePNG(const std::string& path, int w, int h, const u8* rgba);
