#include "core_systems.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "components.h"
#include "math.h"
#include "resources.h"

namespace criogenio {

void MovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<Controller>();

  for (ecs::EntityId id : ids) {
    auto *ctrl = world.GetComponent<Controller>(id);
    auto *tr = world.GetComponent<Transform>(id);
    auto *anim = world.GetComponent<AnimationState>(id);

    if (!ctrl || !tr || !anim)
      continue;

    float dx = 0, dy = 0;

    if (IsKeyDown(KEY_RIGHT)) {
      dx += 1;
      anim->facing = Direction::RIGHT;
    }
    if (IsKeyDown(KEY_LEFT)) {
      dx -= 1;
      anim->facing = Direction::LEFT;
    }
    if (IsKeyDown(KEY_UP)) {
      dy -= 1;
      anim->facing = Direction::UP;
    }
    if (IsKeyDown(KEY_DOWN)) {
      dy += 1;
      anim->facing = Direction::DOWN;
    }

    if (dx != 0 || dy != 0) {
      tr->x += dx * ctrl->velocity.x * dt;
      tr->y += dy * ctrl->velocity.y * dt;
      anim->current = AnimState::WALKING;
    } else {
      anim->current = AnimState::IDLE;
    }
  }
}

void MovementSystem::Render(Renderer &renderer) {}

void AIMovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<AIController>();

  for (ecs::EntityId id : ids) {
    auto *ctrl = world.GetComponent<AIController>(id);
    auto *tr = world.GetComponent<Transform>(id);
    auto *anim = world.GetComponent<AnimationState>(id);

    if (!ctrl || !tr || !anim)
      continue;

    if (ctrl->entityTarget <= 0 and ctrl->entityTarget != id) {
      return;
    }

    const float arriveRadius = 0.01f;

    auto *targetTrasnform = world.GetComponent<Transform>(ctrl->entityTarget);
    if (!targetTrasnform)
      continue;

    float dx = targetTrasnform->x - tr->x;
    float dy = targetTrasnform->y - tr->y;

    float distSq = dx * dx + dy * dy;

    if (distSq > arriveRadius * arriveRadius) {
      float dist = sqrtf(distSq);

      float dirX = dx / dist;
      float dirY = dy / dist;

      float step = ctrl->velocity.x * dt; // assuming velocity.x is speed

      // Prevent overshoot
      if (step >= dist) {
        tr->x = targetTrasnform->x;
        tr->y = targetTrasnform->y;
      } else {
        tr->x += dirX * step;
        tr->y += dirY * step;
      }
      // --- Facing logic ---
      if (fabsf(dx) > fabsf(dy))
        anim->facing = (dx > 0) ? Direction::RIGHT : Direction::LEFT;
      else
        anim->facing = (dy > 0) ? Direction::DOWN : Direction::UP;

      anim->current = AnimState::WALKING;
    } else {
      tr->x = targetTrasnform->x;
      tr->y = targetTrasnform->y;
      anim->current = AnimState::IDLE;
    }
  }
}

void AIMovementSystem::Render(Renderer &renderer) {};

std::string AnimationSystem::FacingToString(Direction d) const {
  switch (d) {
  case Direction::UP:
    return "up";
  case Direction::DOWN:
    return "down";
  case Direction::LEFT:
    return "left";
  case Direction::RIGHT:
    return "right";
  }
  return "down";
}

std::string AnimationSystem::BuildClipKey(const AnimationState &st) {
  switch (st.current) {
  case AnimState::IDLE:
    return "idle_" + FacingToString(st.facing);
  case AnimState::WALKING:
    return "walk_" + FacingToString(st.facing);
  case AnimState::ATTACKING:
    return "attack_" + FacingToString(st.facing);
  }
  return "idle_down";
}

void AnimationSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<AnimatedSprite>();
  for (ecs::EntityId id : ids) {
    auto *sprite = world.GetComponent<AnimatedSprite>(id);
    auto *st = world.GetComponent<AnimationState>(id);

    if (!sprite || !st)
      continue;

    sprite->SetClip(BuildClipKey(*st));
    st->previous = st->current;
    sprite->Update(dt);
  }
}

void AnimationSystem::Render(Renderer &renderer) {}

void RenderSystem::Update(float) {}

void RenderSystem::Render(Renderer &renderer) {
  auto ids = world.GetEntitiesWith<AnimatedSprite>();
  for (ecs::EntityId id : ids) {
    auto *animSprite = world.GetComponent<AnimatedSprite>(id);
    auto *tr = world.GetComponent<Transform>(id);

    if (!animSprite || !tr)
      continue;

    // Look up animation definition from database
    const auto *animDef =
        AnimationDatabase::instance().getAnimation(animSprite->animationId);
    if (!animDef)
      continue;

    // Load texture from asset manager
    auto texture =
        AssetManager::instance().load<TextureResource>(animDef->texturePath);
    if (!texture)
      continue;

    Rectangle src = animSprite->GetFrame();
    Rectangle dest = {tr->x, tr->y, (float)src.width, (float)src.height};

    renderer.DrawTexturePro(texture->texture, src, dest, {0, 0}, 0.0f, WHITE);
  }
}

void AnimationSystem::OnWorldLoaded(World &world) {
  // Animation database should be pre-populated by editor or game code
  // Sprites just reference animations by ID at this point
}

void GravitySystem::Update(float dt) {
  auto gravity = world.GetGlobalComponent<Gravity>();
  auto ids = world.GetEntitiesWith<Transform, Controller, RigidBody>();

  for (ecs::EntityId id : ids) {
  }
}

void GravitySystem::Render(Renderer &renderer) {}

} // namespace criogenio
