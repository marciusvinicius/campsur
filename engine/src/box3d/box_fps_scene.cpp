#include "box3d/box_fps_scene.h"
#include "box3d/fp_camera.h"
#include "box3d/gl_box_renderer.h"
#include <box2d/math_functions.h>
#include <cassert>
#include <cfloat>
#include <cmath>

namespace criogenio::box3d {

namespace {
constexpr uint64_t kWallCategory = 0x1u;
constexpr uint64_t kPlayerCategory = 0x2u;
} // namespace

bool BoxFpsScene::GatherPlane(b2ShapeId shapeId, const b2PlaneResult* planeResult,
                              void* context) {
  (void)shapeId;
  assert(planeResult->hit);
  auto* self = static_cast<BoxFpsScene*>(context);
  if (self->planeCount_ >= kMaxPlanes)
    return true;
  self->planes_[self->planeCount_] = {planeResult->plane, FLT_MAX, 0.f, true};
  self->planeCount_ += 1;
  return true;
}

BoxFpsScene::BoxFpsScene() {
  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = {0.f, 0.f};
  world_ = b2CreateWorld(&def);

  playerTransform_ = {{0.f, 0.f}, b2Rot_identity};
  velocity_ = {0.f, 0.f};
  playerCapsuleLocal_.center1 = {0.f, 0.f};
  playerCapsuleLocal_.center2 = {0.f, 0.f};
  playerCapsuleLocal_.radius = 0.35f;

  const float w = 18.f;
  const float h = 14.f;
  const float t = 0.5f;
  AddStaticObstacle(0.f, -h, w, t, 1.25f, 0.35f, 0.4f, 0.45f);
  AddStaticObstacle(0.f, h, w, t, 1.25f, 0.35f, 0.4f, 0.45f);
  AddStaticObstacle(-w, 0.f, t, h, 1.25f, 0.35f, 0.4f, 0.45f);
  AddStaticObstacle(w, 0.f, t, h, 1.25f, 0.35f, 0.4f, 0.45f);

  AddStaticObstacle(-6.f, 4.f, 2.f, 2.f, 1.5f, 0.85f, 0.35f, 0.2f);
  AddStaticObstacle(5.f, -3.f, 3.f, 1.5f, 2.f, 0.25f, 0.55f, 0.75f);
  AddStaticObstacle(0.f, 0.f, 1.5f, 4.f, 1.2f, 0.5f, 0.5f, 0.55f);
}

BoxFpsScene::~BoxFpsScene() {
  if (b2World_IsValid(world_))
    b2DestroyWorld(world_);
}

void BoxFpsScene::AddStaticObstacle(float centerX, float centerZ, float halfW, float halfD,
                                    float halfHeightY, float r, float g, float b) {
  b2BodyDef bodyDef = b2DefaultBodyDef();
  bodyDef.position = {centerX, centerZ};
  b2BodyId body = b2CreateBody(world_, &bodyDef);

  b2Polygon box = b2MakeBox(halfW, halfD);
  b2ShapeDef shapeDef = b2DefaultShapeDef();
  shapeDef.filter.categoryBits = kWallCategory;
  shapeDef.filter.maskBits = ~0ull;
  b2CreatePolygonShape(body, &shapeDef, &box);

  obstacles_.push_back(Obstacle{centerX, centerZ, halfW, halfD, halfHeightY, r, g, b});
}

void BoxFpsScene::Step(float dt, b2Vec2 wishVelocityXZ) {
  velocity_ = wishVelocityXZ;

  b2Vec2 target = b2Add(playerTransform_.p, b2MulSV(dt, velocity_));
  b2QueryFilter collideFilter = {kPlayerCategory, kWallCategory};
  b2QueryFilter castFilter = {kPlayerCategory, kWallCategory};

  const float tolerance = 0.0004f;
  for (int iteration = 0; iteration < 5; ++iteration) {
    planeCount_ = 0;

    b2Capsule mover;
    mover.center1 = b2TransformPoint(playerTransform_, playerCapsuleLocal_.center1);
    mover.center2 = b2TransformPoint(playerTransform_, playerCapsuleLocal_.center2);
    mover.radius = playerCapsuleLocal_.radius;

    b2World_CollideMover(world_, &mover, collideFilter, GatherPlane, this);
    b2PlaneSolverResult solved =
        b2SolvePlanes(b2Sub(target, playerTransform_.p), planes_, planeCount_);

    float fraction = b2World_CastMover(world_, &mover, solved.translation, castFilter);
    b2Vec2 delta = b2MulSV(fraction, solved.translation);
    playerTransform_.p = b2Add(playerTransform_.p, delta);

    if (b2LengthSquared(delta) < tolerance * tolerance)
      break;
  }

  velocity_ = b2ClipVector(velocity_, planes_, planeCount_);
  b2World_Step(world_, dt, 4);
}

void BoxFpsScene::SyncCamera(FPCamera& cam, float eyeHeight) const {
  cam.position.x = playerTransform_.p.x;
  cam.position.y = eyeHeight;
  cam.position.z = playerTransform_.p.y;
}

void BoxFpsScene::DrawWorld(GlBoxRenderer& renderer) const {
  renderer.DrawBox(Vec3{0.f, -0.02f, 0.f}, Vec3{20.f, 0.02f, 16.f}, 0.12f, 0.14f, 0.18f);

  for (const Obstacle& o : obstacles_) {
    renderer.DrawBox(Vec3{o.x, o.halfH, o.z}, Vec3{o.halfW, o.halfH, o.halfD}, o.r, o.g, o.b);
  }
}

} // namespace criogenio::box3d
