#pragma once
#include "engine/Common.h"
#include "engine/Math.h"
#include "engine/GL.h"
#include <unordered_map>

class Shader {
 public:
  // Quelltexte ohne #version; ein gemeinsamer Header wird vorangestellt.
  bool Build(const char* name, const std::string& vs, const std::string& fs, const char* defines = "");
  void Destroy();
  void Use() const { glUseProgram(prog_); }
  GLint Loc(const char* name);
  void Set(const char* n, int v) { glUniform1i(Loc(n), v); }
  void Set(const char* n, float v) { glUniform1f(Loc(n), v); }
  void Set(const char* n, Vec2 v) { glUniform2f(Loc(n), v.x, v.y); }
  void Set(const char* n, Vec3 v) { glUniform3f(Loc(n), v.x, v.y, v.z); }
  void Set(const char* n, Vec4 v) { glUniform4f(Loc(n), v.x, v.y, v.z, v.w); }
  void Set(const char* n, const Mat4& m) { glUniformMatrix4fv(Loc(n), 1, GL_FALSE, m.m); }
  void BindBlock(const char* blockName, GLuint binding);
  GLuint Id() const { return prog_; }

 private:
  GLuint prog_ = 0;
  std::unordered_map<std::string, GLint> locs_;
};
