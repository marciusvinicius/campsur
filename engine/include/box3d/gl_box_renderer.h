#pragma once

#include "box3d/fp_camera.h"

namespace criogenio {
class ApplicationWindow;
}

namespace criogenio::box3d {

/// Minimal OpenGL 3.3 core renderer: shaded axis-aligned boxes (engine **Box3D** view).
class GlBoxRenderer {
public:
  /// Forwards to `ApplicationWindow::PrepareOpenGLAttributes()` (OpenGL windows only).
  static void ConfigureGlAttributes();

  bool Init(ApplicationWindow& window);
  void Shutdown();

  void Resize(int width, int height);
  void ClearFrame(float r, float g, float b);
  void SetCamera(const FPCamera& cam);

  /// World-space direction from surface toward the light (need not be normalized).
  /// `ambient` and `diffuse` scale the base color; use diffuse 0 and ambient (1,1,1) for flat shading.
  void SetDirectionalLight(Vec3 towardLight, Vec3 ambient, float diffuse);

  /// Axis-aligned box: center, half-extents along X/Y/Z, RGB in 0..1.
  void DrawBox(Vec3 center, Vec3 halfExtents, float cr, float cg, float cb);

  void Present(ApplicationWindow& window);

  bool Ok() const { return ok_; }

private:
  bool LoadGl();
  bool CompileProgram();
  void Mat4Mul(float out[16], const float a[16], const float b[16]) const;
  void Mat4Translate(float m[16], float tx, float ty, float tz) const;
  void Mat4Scale(float m[16], float sx, float sy, float sz) const;

  void* window_ = nullptr;
  void* glContext_ = nullptr;
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned program_ = 0;
  int locModel_ = -1;
  int locView_ = -1;
  int locProj_ = -1;
  int locBaseColor_ = -1;
  int locLightDir_ = -1;
  int locAmbient_ = -1;
  int locDiffuse_ = -1;
  int viewportW_ = 0;
  int viewportH_ = 0;
  float proj_[16]{};
  float view_[16]{};
  Vec3 lightDirTo_{0.f, 1.f, 0.f};
  Vec3 ambient_{1.f, 1.f, 1.f};
  float diffuse_ = 0.f;
  bool ok_ = false;
};

} // namespace criogenio::box3d
