#include "application_window.h"
#include "box3d/gl_box_renderer.h"
#include <SDL3/SDL_video.h>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef APIENTRY
#define APIENTRY
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace criogenio::box3d {

namespace {

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;
using GLchar = char;

enum : GLenum {
  GL_DEPTH_TEST = 0x0B71,
  GL_TRIANGLES = 0x0004,
  GL_ARRAY_BUFFER = 0x8892,
  GL_STATIC_DRAW = 0x88E4,
  GL_FLOAT = 0x1406,
  GL_FALSE = 0,
  GL_TRUE = 1,
  GL_COLOR_BUFFER_BIT = 0x00004000,
  GL_DEPTH_BUFFER_BIT = 0x00000100,
  GL_COMPILE_STATUS = 0x8B81,
  GL_LINK_STATUS = 0x8B82,
  GL_FRAGMENT_SHADER = 0x8B30,
  GL_VERTEX_SHADER = 0x8B31,
};

struct GlApi {
  void(APIENTRY* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
  void(APIENTRY* Clear)(GLbitfield);
  void(APIENTRY* Viewport)(GLint, GLint, GLsizei, GLsizei);
  void(APIENTRY* Enable)(GLenum);
  GLuint(APIENTRY* CreateShader)(GLenum);
  void(APIENTRY* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
  void(APIENTRY* CompileShader)(GLuint);
  void(APIENTRY* GetShaderiv)(GLuint, GLenum, GLint*);
  void(APIENTRY* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
  GLuint(APIENTRY* CreateProgram)();
  void(APIENTRY* AttachShader)(GLuint, GLuint);
  void(APIENTRY* LinkProgram)(GLuint);
  void(APIENTRY* GetProgramiv)(GLuint, GLenum, GLint*);
  void(APIENTRY* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
  void(APIENTRY* DeleteShader)(GLuint);
  void(APIENTRY* DeleteProgram)(GLuint);
  void(APIENTRY* UseProgram)(GLuint);
  GLint(APIENTRY* GetUniformLocation)(GLuint, const GLchar*);
  void(APIENTRY* UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
  void(APIENTRY* Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
  void(APIENTRY* Uniform1f)(GLint, GLfloat);
  void(APIENTRY* GenVertexArrays)(GLsizei, GLuint*);
  void(APIENTRY* BindVertexArray)(GLuint);
  void(APIENTRY* DeleteVertexArrays)(GLsizei, const GLuint*);
  void(APIENTRY* GenBuffers)(GLsizei, GLuint*);
  void(APIENTRY* BindBuffer)(GLenum, GLuint);
  void(APIENTRY* BufferData)(GLenum, ptrdiff_t, const void*, GLenum);
  void(APIENTRY* DeleteBuffers)(GLsizei, const GLuint*);
  void(APIENTRY* VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                      const void*);
  void(APIENTRY* EnableVertexAttribArray)(GLuint);
  void(APIENTRY* DrawArrays)(GLenum, GLint, GLsizei);
} glb;

static void* LoadGlProc(const char* name) {
  if (SDL_FunctionPointer fp = SDL_GL_GetProcAddress(name))
    return reinterpret_cast<void*>(fp);
#if defined(__linux__) && !defined(__ANDROID__)
  if (void* p = dlsym(RTLD_DEFAULT, name))
    return p;
#endif
  return nullptr;
}

#define SDLGL(sym)                                                                           \
  do {                                                                                       \
    glb.sym = reinterpret_cast<decltype(glb.sym)>(LoadGlProc("gl" #sym));                   \
    if (!glb.sym) {                                                                          \
      std::fprintf(stderr, "OpenGL: missing function gl%s\n", #sym);                        \
      return false;                                                                          \
    }                                                                                        \
  } while (0)

static void PushVert(std::vector<GLfloat>& mesh, GLfloat px, GLfloat py, GLfloat pz, GLfloat nx,
                     GLfloat ny, GLfloat nz) {
  mesh.push_back(px);
  mesh.push_back(py);
  mesh.push_back(pz);
  mesh.push_back(nx);
  mesh.push_back(ny);
  mesh.push_back(nz);
}

/// Two CCW triangles on a quad; `n` is face outward normal; corners ordered CCW seen from outside.
static void PushQuad(std::vector<GLfloat>& mesh, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat ax,
                     GLfloat ay, GLfloat az, GLfloat bx, GLfloat by, GLfloat bz, GLfloat cx,
                     GLfloat cy, GLfloat cz, GLfloat dx, GLfloat dy, GLfloat dz) {
  PushVert(mesh, ax, ay, az, nx, ny, nz);
  PushVert(mesh, bx, by, bz, nx, ny, nz);
  PushVert(mesh, cx, cy, cz, nx, ny, nz);
  PushVert(mesh, ax, ay, az, nx, ny, nz);
  PushVert(mesh, cx, cy, cz, nx, ny, nz);
  PushVert(mesh, dx, dy, dz, nx, ny, nz);
}

static void BuildUnitCubeLitMesh(std::vector<GLfloat>& out) {
  out.clear();
  out.reserve(36 * 6);
  // +Y
  PushQuad(out, 0.f, 1.f, 0.f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
           0.5f, 0.5f);
  // -Y
  PushQuad(out, 0.f, -1.f, 0.f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f,
           -0.5f, -0.5f, -0.5f);
  // +Z
  PushQuad(out, 0.f, 0.f, 1.f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
           -0.5f, 0.5f);
  // -Z
  PushQuad(out, 0.f, 0.f, -1.f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
           -0.5f, -0.5f, -0.5f);
  // +X
  PushQuad(out, 1.f, 0.f, 0.f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
           0.5f, -0.5f);
  // -X
  PushQuad(out, -1.f, 0.f, 0.f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
           -0.5f, 0.5f, 0.5f);
}

static const char* kVs = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vNormal;
out vec3 vFragPos;
void main() {
  vec4 wp = uModel * vec4(aPos, 1.0);
  vFragPos = wp.xyz;
  vNormal = mat3(transpose(inverse(uModel))) * aNormal;
  gl_Position = uProj * uView * wp;
}
)glsl";

static const char* kFs = R"glsl(
#version 330 core
in vec3 vNormal;
in vec3 vFragPos;
uniform vec3 uBaseColor;
uniform vec3 uLightDirTo;
uniform vec3 uAmbient;
uniform float uDiffuse;
out vec4 FragColor;
void main() {
  vec3 n = normalize(vNormal);
  vec3 L = normalize(uLightDirTo);
  float ndl = max(dot(n, L), 0.0);
  vec3 c = uBaseColor * (uAmbient + uDiffuse * ndl);
  FragColor = vec4(c, 1.0);
}
)glsl";

static bool CompileOk(GLuint shader, const char* label) {
  GLint ok = 0;
  glb.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok)
    return true;
  char buf[1024];
  glb.GetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
  std::fprintf(stderr, "%s shader: %s\n", label, buf);
  return false;
}

static bool LinkOk(GLuint prog) {
  GLint ok = 0;
  glb.GetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (ok)
    return true;
  char buf[1024];
  glb.GetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
  std::fprintf(stderr, "program link: %s\n", buf);
  return false;
}

} // namespace

void GlBoxRenderer::ConfigureGlAttributes() { ApplicationWindow::PrepareOpenGLAttributes(); }

bool GlBoxRenderer::LoadGl() {
  SDLGL(ClearColor);
  SDLGL(Clear);
  SDLGL(Viewport);
  SDLGL(Enable);
  SDLGL(CreateShader);
  SDLGL(ShaderSource);
  SDLGL(CompileShader);
  SDLGL(GetShaderiv);
  SDLGL(GetShaderInfoLog);
  SDLGL(CreateProgram);
  SDLGL(AttachShader);
  SDLGL(LinkProgram);
  SDLGL(GetProgramiv);
  SDLGL(GetProgramInfoLog);
  SDLGL(DeleteShader);
  SDLGL(DeleteProgram);
  SDLGL(UseProgram);
  SDLGL(GetUniformLocation);
  SDLGL(UniformMatrix4fv);
  SDLGL(Uniform3f);
  SDLGL(Uniform1f);
  SDLGL(GenVertexArrays);
  SDLGL(BindVertexArray);
  SDLGL(DeleteVertexArrays);
  SDLGL(GenBuffers);
  SDLGL(BindBuffer);
  SDLGL(BufferData);
  SDLGL(DeleteBuffers);
  SDLGL(VertexAttribPointer);
  SDLGL(EnableVertexAttribArray);
  SDLGL(DrawArrays);
  return true;
}

bool GlBoxRenderer::CompileProgram() {
  GLuint vs = glb.CreateShader(GL_VERTEX_SHADER);
  const char* vsrc = kVs;
  glb.ShaderSource(vs, 1, &vsrc, nullptr);
  glb.CompileShader(vs);
  if (!CompileOk(vs, "vertex")) {
    glb.DeleteShader(vs);
    return false;
  }
  GLuint fs = glb.CreateShader(GL_FRAGMENT_SHADER);
  const char* fsrc = kFs;
  glb.ShaderSource(fs, 1, &fsrc, nullptr);
  glb.CompileShader(fs);
  if (!CompileOk(fs, "fragment")) {
    glb.DeleteShader(vs);
    glb.DeleteShader(fs);
    return false;
  }
  program_ = glb.CreateProgram();
  glb.AttachShader(program_, vs);
  glb.AttachShader(program_, fs);
  glb.LinkProgram(program_);
  glb.DeleteShader(vs);
  glb.DeleteShader(fs);
  if (!LinkOk(program_)) {
    glb.DeleteProgram(program_);
    program_ = 0;
    return false;
  }
  locModel_ = glb.GetUniformLocation(program_, "uModel");
  locView_ = glb.GetUniformLocation(program_, "uView");
  locProj_ = glb.GetUniformLocation(program_, "uProj");
  locBaseColor_ = glb.GetUniformLocation(program_, "uBaseColor");
  locLightDir_ = glb.GetUniformLocation(program_, "uLightDirTo");
  locAmbient_ = glb.GetUniformLocation(program_, "uAmbient");
  locDiffuse_ = glb.GetUniformLocation(program_, "uDiffuse");
  return locModel_ >= 0 && locView_ >= 0 && locProj_ >= 0 && locBaseColor_ >= 0 &&
         locLightDir_ >= 0 && locAmbient_ >= 0 && locDiffuse_ >= 0;
}

void GlBoxRenderer::Mat4Translate(float m[16], float tx, float ty, float tz) const {
  m[0] = 1.f;
  m[1] = m[2] = m[3] = m[4] = 0.f;
  m[5] = 1.f;
  m[6] = m[7] = m[8] = m[9] = 0.f;
  m[10] = 1.f;
  m[11] = 0.f;
  m[12] = tx;
  m[13] = ty;
  m[14] = tz;
  m[15] = 1.f;
}

void GlBoxRenderer::Mat4Scale(float m[16], float sx, float sy, float sz) const {
  std::memset(m, 0, 64);
  m[0] = sx;
  m[5] = sy;
  m[10] = sz;
  m[15] = 1.f;
}

void GlBoxRenderer::Mat4Mul(float out[16], const float a[16], const float b[16]) const {
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      float s = 0.f;
      for (int k = 0; k < 4; ++k)
        s += a[i + k * 4] * b[k + j * 4];
      out[i + j * 4] = s;
    }
  }
}

void GlBoxRenderer::SetDirectionalLight(Vec3 towardLight, Vec3 ambient, float diffuse) {
  float len = std::sqrt(towardLight.x * towardLight.x + towardLight.y * towardLight.y +
                        towardLight.z * towardLight.z);
  if (len > 1e-6f) {
    lightDirTo_.x = towardLight.x / len;
    lightDirTo_.y = towardLight.y / len;
    lightDirTo_.z = towardLight.z / len;
  } else {
    lightDirTo_ = {0.f, 1.f, 0.f};
  }
  ambient_ = ambient;
  diffuse_ = diffuse;
}

bool GlBoxRenderer::Init(ApplicationWindow& app) {
  window_ = app.GetPlatformWindowHandle();
  auto* sdlWindow = static_cast<SDL_Window*>(window_);
  if (!sdlWindow)
    return false;

  SDL_GLContext ctx = SDL_GL_CreateContext(sdlWindow);
  if (!ctx) {
    std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
    return false;
  }
  glContext_ = ctx;
  SDL_GL_MakeCurrent(sdlWindow, ctx);
  SDL_GL_SetSwapInterval(1);

  if (!LoadGl())
    return false;
  if (!CompileProgram())
    return false;

  std::vector<GLfloat> mesh;
  BuildUnitCubeLitMesh(mesh);

  glb.GenVertexArrays(1, &vao_);
  glb.GenBuffers(1, &vbo_);
  glb.BindVertexArray(vao_);
  glb.BindBuffer(GL_ARRAY_BUFFER, vbo_);
  glb.BufferData(GL_ARRAY_BUFFER, static_cast<ptrdiff_t>(mesh.size() * sizeof(GLfloat)),
                 mesh.data(), GL_STATIC_DRAW);
  glb.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
  glb.EnableVertexAttribArray(0);
  glb.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                          reinterpret_cast<void*>(3 * sizeof(GLfloat)));
  glb.EnableVertexAttribArray(1);
  glb.BindVertexArray(0);

  {
    int dw = 0, dh = 0;
    app.GetDrawableSize(dw, dh);
    Resize(dw, dh);
  }

  ok_ = true;
  return true;
}

void GlBoxRenderer::Shutdown() {
  if (vbo_) {
    glb.DeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_) {
    glb.DeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (program_) {
    glb.DeleteProgram(program_);
    program_ = 0;
  }
  if (glContext_) {
    SDL_GL_DestroyContext(static_cast<SDL_GLContext>(glContext_));
    glContext_ = nullptr;
  }
  window_ = nullptr;
  ok_ = false;
}

void GlBoxRenderer::Resize(int width, int height) {
  viewportW_ = width;
  viewportH_ = height;
  if (height <= 0)
    height = 1;
  glb.Viewport(0, 0, width, height);
}

void GlBoxRenderer::ClearFrame(float r, float g0, float b) {
  glb.Enable(GL_DEPTH_TEST);
  glb.ClearColor(r, g0, b, 1.f);
  glb.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GlBoxRenderer::SetCamera(const FPCamera& cam) {
  cam.GetProjectionColumnMajor(proj_, viewportW_, viewportH_);
  cam.GetViewMatrixColumnMajor(view_);
}

void GlBoxRenderer::DrawBox(Vec3 center, Vec3 halfExtents, float cr, float cg, float cb) {
  float s[16], t[16], model[16];
  Mat4Translate(t, center.x, center.y, center.z);
  Mat4Scale(s, halfExtents.x * 2.f, halfExtents.y * 2.f, halfExtents.z * 2.f);
  Mat4Mul(model, t, s);

  glb.UseProgram(program_);
  glb.UniformMatrix4fv(locModel_, 1, GL_FALSE, model);
  glb.UniformMatrix4fv(locView_, 1, GL_FALSE, view_);
  glb.UniformMatrix4fv(locProj_, 1, GL_FALSE, proj_);
  glb.Uniform3f(locBaseColor_, cr, cg, cb);
  glb.Uniform3f(locLightDir_, lightDirTo_.x, lightDirTo_.y, lightDirTo_.z);
  glb.Uniform3f(locAmbient_, ambient_.x, ambient_.y, ambient_.z);
  glb.Uniform1f(locDiffuse_, diffuse_);
  glb.BindVertexArray(vao_);
  glb.DrawArrays(GL_TRIANGLES, 0, 36);
  glb.BindVertexArray(0);
}

void GlBoxRenderer::Present(ApplicationWindow& app) {
  auto* w = static_cast<SDL_Window*>(app.GetPlatformWindowHandle());
  if (w)
    SDL_GL_SwapWindow(w);
}

} // namespace criogenio::box3d
