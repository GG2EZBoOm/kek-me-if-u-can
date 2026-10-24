#pragma once
#include "engine/Mesh.h"

// Antialiased Windows TrueType glyphs shared by all world signs. Geometry is
// independent of OpenGL so dedicated servers can still build the same maps.
bool WorldTextReady();
float WorldTextWidth(const char* text, float height);
void BuildWorldText(MeshBuilder& b, const char* text, float height, float maxWidth, bool backing);
GLuint CreateWorldTextTexture();
