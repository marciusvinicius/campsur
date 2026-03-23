#pragma once

namespace criogenio::box3d {

struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;
};

/// First-person camera (Y up). Yaw rotates around +Y; pitch looks up/down.
class FPCamera {
public:
  Vec3 position{};
  float yaw = 0.f;
  float pitch = 0.f;
  float fovDeg = 70.f;
  float nearPlane = 0.05f;
  float farPlane = 120.f;

  /// `dx`/`dy` are pointer deltas (e.g. SDL); positive dx = move mouse right → look right.
  void AddLookDelta(float dx, float dy, float mouseSensitivity);
  void GetViewMatrixColumnMajor(float out[16]) const;
  void GetProjectionColumnMajor(float out[16], int viewportW, int viewportH) const;

  Vec3 Forward() const;
  Vec3 ForwardOnXZPlane() const;
  Vec3 RightOnXZPlane() const;
};

} // namespace criogenio::box3d
