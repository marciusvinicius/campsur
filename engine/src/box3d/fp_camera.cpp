#include "box3d/fp_camera.h"
#include <cmath>
#include <cstring>

namespace criogenio::box3d {

namespace {
constexpr float kHalfPi = 1.57079632679f;
}

void FPCamera::AddLookDelta(float dx, float dy, float mouseSensitivity) {
  yaw -= dx * mouseSensitivity;
  pitch -= dy * mouseSensitivity;
  if (pitch > kHalfPi - 0.01f)
    pitch = kHalfPi - 0.01f;
  if (pitch < -kHalfPi + 0.01f)
    pitch = -kHalfPi + 0.01f;
}

Vec3 FPCamera::Forward() const {
  float cp = std::cos(pitch);
  return Vec3{cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw)};
}

Vec3 FPCamera::ForwardOnXZPlane() const {
  return Vec3{std::sin(yaw), 0.f, std::cos(yaw)};
}

Vec3 FPCamera::RightOnXZPlane() const {
  return Vec3{std::cos(yaw), 0.f, -std::sin(yaw)};
}

void FPCamera::GetViewMatrixColumnMajor(float out[16]) const {
  Vec3 f = Forward();
  float fl = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
  if (fl > 1e-6f) {
    f.x /= fl;
    f.y /= fl;
    f.z /= fl;
  }
  Vec3 up{0.f, 1.f, 0.f};
  Vec3 s{f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x};
  float sl = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
  if (sl < 1e-4f) {
    up = {0.f, 0.f, 1.f};
    s = {f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x};
    sl = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
  }
  if (sl > 1e-6f) {
    s.x /= sl;
    s.y /= sl;
    s.z /= sl;
  }
  Vec3 u{s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x};

  std::memset(out, 0, 16 * sizeof(float));
  out[0] = s.x;
  out[1] = u.x;
  out[2] = -f.x;
  out[3] = 0.f;
  out[4] = s.y;
  out[5] = u.y;
  out[6] = -f.y;
  out[7] = 0.f;
  out[8] = s.z;
  out[9] = u.z;
  out[10] = -f.z;
  out[11] = 0.f;
  out[12] = -(s.x * position.x + s.y * position.y + s.z * position.z);
  out[13] = -(u.x * position.x + u.y * position.y + u.z * position.z);
  out[14] = f.x * position.x + f.y * position.y + f.z * position.z;
  out[15] = 1.f;
}

void FPCamera::GetProjectionColumnMajor(float out[16], int viewportW,
                                        int viewportH) const {
  float aspect =
      viewportH > 0 ? static_cast<float>(viewportW) / static_cast<float>(viewportH) : 1.f;
  float f = 1.f / std::tan((fovDeg * 3.14159265f / 180.f) * 0.5f);
  float zn = nearPlane;
  float zf = farPlane;
  std::memset(out, 0, 16 * sizeof(float));
  out[0] = f / aspect;
  out[5] = f;
  out[10] = (zf + zn) / (zn - zf);
  out[11] = -1.f;
  out[14] = (2.f * zf * zn) / (zn - zf);
  out[15] = 0.f;
}

} // namespace criogenio::box3d
