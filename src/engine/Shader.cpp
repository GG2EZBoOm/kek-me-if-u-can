#include "engine/Shader.h"

static GLuint Compile(const char* name, GLenum type, const std::string& src) {
  GLuint s = glCreateShader(type);
  const char* p = src.c_str();
  glShaderSource(s, 1, &p, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    LOG("Shader-Fehler (%s, %s):\n%s", name, type == GL_VERTEX_SHADER ? "VS" : "FS", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

bool Shader::Build(const char* name, const std::string& vs, const std::string& fs, const char* defines) {
  std::string header = "#version 330 core\n";
  header += defines;
  header += "\n";
  GLuint v = Compile(name, GL_VERTEX_SHADER, header + "#define VERTEX\n" + vs);
  GLuint f = Compile(name, GL_FRAGMENT_SHADER, header + "#define FRAGMENT\n" + fs);
  if (!v || !f) return false;
  prog_ = glCreateProgram();
  glAttachShader(prog_, v);
  glAttachShader(prog_, f);
  glBindAttribLocation(prog_, 0, "aPos");
  glBindAttribLocation(prog_, 1, "aNormal");
  glBindAttribLocation(prog_, 2, "aColor");
  glBindAttribLocation(prog_, 3, "aAux");
  glBindAttribLocation(prog_, 4, "aInst0");
  glBindAttribLocation(prog_, 5, "aInst1");
  glBindAttribLocation(prog_, 6, "aInst2");
  glBindAttribLocation(prog_, 7, "aTexUV");
  glLinkProgram(prog_);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
    LOG("Shader-Link-Fehler (%s):\n%s", name, log);
    return false;
  }
  locs_.clear();
  return true;
}

void Shader::Destroy() {
  if (prog_) glDeleteProgram(prog_);
  prog_ = 0;
}

GLint Shader::Loc(const char* name) {
  auto it = locs_.find(name);
  if (it != locs_.end()) return it->second;
  GLint l = glGetUniformLocation(prog_, name);
  locs_[name] = l;
  return l;
}

void Shader::BindBlock(const char* blockName, GLuint binding) {
  GLuint idx = glGetUniformBlockIndex(prog_, blockName);
  if (idx != 0xFFFFFFFFu) glUniformBlockBinding(prog_, idx, binding);
}
