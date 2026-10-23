#include "engine/GL.h"
#include "engine/Common.h"

#define GL_DEFINE(ret, name, params) PFN_gl##name gl##name = nullptr;
GL_FUNCS(GL_DEFINE)
#undef GL_DEFINE

static void* GetGLProc(const char* name) {
  void* p = (void*)wglGetProcAddress(name);
  if (p == nullptr || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1) {
    static HMODULE mod = LoadLibraryA("opengl32.dll");
    p = (void*)GetProcAddress(mod, name);
  }
  return p;
}

bool GLLoadFunctions() {
  bool ok = true;
#define GL_LOADFN(ret, name, params)                                \
  gl##name = (PFN_gl##name)GetGLProc("gl" #name);                  \
  if (!gl##name) { LOG("GL-Funktion fehlt: gl%s", #name); ok = false; }
  GL_FUNCS(GL_LOADFN)
#undef GL_LOADFN
  return ok;
}
