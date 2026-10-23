#pragma once
// Eigener, minimaler OpenGL-3.3-Core-Loader (keine externen Bibliotheken).
#include <windows.h>
#include <GL/gl.h>
#include <cstddef>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_R16F 0x822D
#define GL_R8 0x8229
#define GL_RG 0x8227
#define GL_RG8 0x822B
#define GL_RED 0x1903
#define GL_HALF_FLOAT 0x140B
#define GL_RGB10_A2 0x8059
#define GL_RGBA8 0x8058
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_FUNC_ADD 0x8006
#define GL_MULTISAMPLE 0x809D
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_DEBUG_OUTPUT 0x92E0

#define GL_FUNCS(X)                                                                                         \
  X(void, GenBuffers, (GLsizei n, GLuint * b))                                                              \
  X(void, DeleteBuffers, (GLsizei n, const GLuint* b))                                                      \
  X(void, BindBuffer, (GLenum t, GLuint b))                                                                 \
  X(void, BufferData, (GLenum t, GLsizeiptr s, const void* d, GLenum u))                                    \
  X(void, BufferSubData, (GLenum t, GLintptr o, GLsizeiptr s, const void* d))                               \
  X(void, BindBufferBase, (GLenum t, GLuint i, GLuint b))                                                   \
  X(void, GenVertexArrays, (GLsizei n, GLuint * a))                                                         \
  X(void, DeleteVertexArrays, (GLsizei n, const GLuint* a))                                                 \
  X(void, BindVertexArray, (GLuint a))                                                                      \
  X(void, EnableVertexAttribArray, (GLuint i))                                                              \
  X(void, VertexAttribPointer, (GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p))       \
  X(void, VertexAttribDivisor, (GLuint i, GLuint d))                                                        \
  X(void, DrawArraysInstanced, (GLenum m, GLint f, GLsizei c, GLsizei n))                                   \
  X(void, DrawElementsInstanced, (GLenum m, GLsizei c, GLenum t, const void* i, GLsizei n))                 \
  X(GLuint, CreateShader, (GLenum t))                                                                       \
  X(void, ShaderSource, (GLuint s, GLsizei c, const GLchar* const* str, const GLint* len))                  \
  X(void, CompileShader, (GLuint s))                                                                        \
  X(void, GetShaderiv, (GLuint s, GLenum p, GLint * v))                                                     \
  X(void, GetShaderInfoLog, (GLuint s, GLsizei m, GLsizei * l, GLchar * log))                               \
  X(GLuint, CreateProgram, (void))                                                                          \
  X(void, AttachShader, (GLuint p, GLuint s))                                                               \
  X(void, LinkProgram, (GLuint p))                                                                          \
  X(void, GetProgramiv, (GLuint p, GLenum n, GLint * v))                                                    \
  X(void, GetProgramInfoLog, (GLuint p, GLsizei m, GLsizei * l, GLchar * log))                              \
  X(void, DeleteShader, (GLuint s))                                                                         \
  X(void, DeleteProgram, (GLuint p))                                                                        \
  X(void, UseProgram, (GLuint p))                                                                           \
  X(void, BindAttribLocation, (GLuint p, GLuint i, const GLchar* n))                                        \
  X(GLint, GetUniformLocation, (GLuint p, const GLchar* n))                                                 \
  X(GLuint, GetUniformBlockIndex, (GLuint p, const GLchar* n))                                              \
  X(void, UniformBlockBinding, (GLuint p, GLuint i, GLuint b))                                              \
  X(void, Uniform1i, (GLint l, GLint v))                                                                    \
  X(void, Uniform1f, (GLint l, GLfloat v))                                                                  \
  X(void, Uniform2f, (GLint l, GLfloat a, GLfloat b))                                                       \
  X(void, Uniform3f, (GLint l, GLfloat a, GLfloat b, GLfloat c))                                            \
  X(void, Uniform4f, (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                                 \
  X(void, Uniform3fv, (GLint l, GLsizei c, const GLfloat* v))                                               \
  X(void, Uniform4fv, (GLint l, GLsizei c, const GLfloat* v))                                               \
  X(void, UniformMatrix4fv, (GLint l, GLsizei c, GLboolean t, const GLfloat* v))                            \
  X(void, ActiveTexture, (GLenum t))                                                                        \
  X(void, GenerateMipmap, (GLenum t))                                                                       \
  X(void, GenFramebuffers, (GLsizei n, GLuint * f))                                                         \
  X(void, DeleteFramebuffers, (GLsizei n, const GLuint* f))                                                 \
  X(void, BindFramebuffer, (GLenum t, GLuint f))                                                            \
  X(void, FramebufferTexture2D, (GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l))                       \
  X(GLenum, CheckFramebufferStatus, (GLenum t))                                                             \
  X(void, DrawBuffers, (GLsizei n, const GLenum* b))                                                        \
  X(void, BlitFramebuffer, (GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl)) \
  X(void, GenRenderbuffers, (GLsizei n, GLuint * r))                                                        \
  X(void, DeleteRenderbuffers, (GLsizei n, const GLuint* r))                                                \
  X(void, BindRenderbuffer, (GLenum t, GLuint r))                                                           \
  X(void, RenderbufferStorage, (GLenum t, GLenum f, GLsizei w, GLsizei h))                                  \
  X(void, FramebufferRenderbuffer, (GLenum t, GLenum a, GLenum rt, GLuint r))                               \
  X(void, BlendFuncSeparate, (GLenum a, GLenum b, GLenum c, GLenum d))                                      \
  X(void, BlendEquation, (GLenum m))                                                                        \
  X(void, VertexAttrib4f, (GLuint i, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                           \
  X(void, DisableVertexAttribArray, (GLuint i))

#define GL_DECLARE(ret, name, params) \
  typedef ret(APIENTRY* PFN_gl##name) params; \
  extern PFN_gl##name gl##name;
GL_FUNCS(GL_DECLARE)
#undef GL_DECLARE

bool GLLoadFunctions();
