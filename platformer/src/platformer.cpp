#include "platformer.h"
#include "components.h"
#include "ecs_registry.h"
#include "input.h"
#include "keys.h"

void PlatformerInitSystem::Update(float dt) {
  (void)dt;
  auto ids = world.GetEntitiesWith<criogenio::Controller, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    if (world.HasComponent<criogenio::RigidBody>(id))
      continue;
    world.AddComponent<criogenio::RigidBody>(id);
    criogenio::BoxCollider& col = world.AddComponent<criogenio::BoxCollider>(id);
    col.width = 20.f;
    col.height = 32.f;
    col.offsetX = -10.f;
    col.offsetY = -16.f;
    world.AddComponent<criogenio::Grounded>(id);
    world.AddComponent<criogenio::AnimatedSprite>(id, playerAnimId);
    world.AddComponent<criogenio::AnimationState>(id);
    criogenio::AnimationState* st = world.GetComponent<criogenio::AnimationState>(id);
    if (st) {
      st->facing = criogenio::Direction::RIGHT;
      st->current = criogenio::AnimState::IDLE;
    }
  }
}

void PlatformerInitSystem::Render(criogenio::Renderer&) {}

void PlatformerMovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<criogenio::Controller, criogenio::RigidBody, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto* ctrl = world.GetComponent<criogenio::Controller>(id);
    auto* rb = world.GetComponent<criogenio::RigidBody>(id);
    auto* tr = world.GetComponent<criogenio::Transform>(id);
    auto* anim = world.GetComponent<criogenio::AnimationState>(id);
    auto* grounded = world.GetComponent<criogenio::Grounded>(id);
    if (!ctrl || !rb || !tr)
      continue;

    rb->velocity.x = ctrl->velocity.x * moveSpeed;
    if (anim) {
      if (ctrl->velocity.x > 0)
        anim->facing = criogenio::Direction::RIGHT;
      else if (ctrl->velocity.x < 0)
        anim->facing = criogenio::Direction::LEFT;
    }

    bool jumpRequested = (ctrl->velocity.y > 0.5f);
    if (jumpRequested && grounded && grounded->value) {
      rb->velocity.y = -jumpForce;
      ctrl->velocity.y = 0.0f;
    }
    if (jumpRequested)
      ctrl->velocity.y = 0.0f;

    tr->x += rb->velocity.x * dt;

    if (anim) {
      if (rb->velocity.y < -1.0f || rb->velocity.y > 1.0f)
        anim->current = criogenio::AnimState::JUMPING;
      else if (ctrl->velocity.x != 0)
        anim->current = criogenio::AnimState::WALKING;
      else
        anim->current = criogenio::AnimState::IDLE;
    }
  }
}

void PlatformerMovementSystem::Render(criogenio::Renderer&) {}

void CameraFollowSystem::Update(float dt) {
  criogenio::Camera2D* cam = world.GetActiveCamera();
  if (!cam)
    return;
  auto ids = world.GetEntitiesWith<criogenio::Controller, criogenio::Transform>();
  if (ids.empty())
    return;
  criogenio::CameraFollow2DConfig cfg{};
  cfg.deadzoneHalfWidth = 64.f;
  cfg.deadzoneHalfHeight = 40.f;
  cfg.smoothingSpeed = 10.f;
  (void)criogenio::UpdateCameraFollow2D(world, cam, ids[0], 20.f, 32.f, dt, cfg);
}

void CameraFollowSystem::Render(criogenio::Renderer&) {}

void PlatformerEngine::OnFrame(float dt) {
  (void)dt;
  criogenio::PlayerInput input = {};
  using namespace criogenio;
  if (Input::IsKeyDown(static_cast<int>(Key::Right)) || Input::IsKeyDown(static_cast<int>(Key::D)))
    input.move_x += 1.f;
  if (Input::IsKeyDown(static_cast<int>(Key::Left)) || Input::IsKeyDown(static_cast<int>(Key::A)))
    input.move_x -= 1.f;
  if (Input::IsKeyPressed(static_cast<int>(Key::Space)))
    input.move_y = 1.f;
  if (GetNetworkMode() == NetworkMode::Client)
    SendInputAsClient(input);
  else if (GetNetworkMode() == NetworkMode::Server)
    SetServerPlayerInput(input);
}
