#pragma once

#include <box2d/box2d.h>
#include <cstdint>
#include <vector>

namespace criogenio::box3d {

class FPCamera;
class GlBoxRenderer;

/// Small arena: **Box2D** handles XZ locomotion (capsule mover); **Box3D** (`GlBoxRenderer`) draws boxes.
class BoxFpsScene {
public:
  BoxFpsScene();
  ~BoxFpsScene();

  BoxFpsScene(const BoxFpsScene&) = delete;
  BoxFpsScene& operator=(const BoxFpsScene&) = delete;

  void AddStaticObstacle(float centerX, float centerZ, float halfW, float halfD, float halfHeightY,
                         float r, float g, float b);

  /// Planar wish velocity in world XZ (maps to Box2D x,y).
  void Step(float dt, b2Vec2 wishVelocityXZ);

  b2Vec2 PlayerPlanePosition() const { return playerTransform_.p; }
  void SetPlayerPlanePosition(b2Vec2 xz) { playerTransform_.p = xz; }

  void SyncCamera(FPCamera& cam, float eyeHeight) const;

  void DrawWorld(GlBoxRenderer& renderer) const;

  b2WorldId WorldId() const { return world_; }

private:
  static bool GatherPlane(b2ShapeId shapeId, const b2PlaneResult* planeResult, void* context);

  b2WorldId world_{};
  b2Transform playerTransform_;
  b2Vec2 velocity_{};
  b2Capsule playerCapsuleLocal_{};

  static constexpr int kMaxPlanes = 32;
  b2CollisionPlane planes_[kMaxPlanes]{};
  int planeCount_ = 0;

  struct Obstacle {
    float x, z, halfW, halfD, halfH;
    float r, g, b;
  };
  std::vector<Obstacle> obstacles_;
};

} // namespace criogenio::box3d
