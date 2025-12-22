#include "core.h"
#include "components.h"
#include "math.h"

namespace criogenio {

void MovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<Controller>();

  for (int id : ids) {
    auto &ctrl = world.GetComponent<Controller>(id);
    auto &tr = world.GetComponent<Transform>(id);
    auto &anim = world.GetComponent<AnimationState>(id);

    float dx = 0, dy = 0;

    if (IsKeyDown(KEY_RIGHT)) {
      dx += 1;
      anim.facing = RIGHT;
    }
    if (IsKeyDown(KEY_LEFT)) {
      dx -= 1;
      anim.facing = LEFT;
    }
    if (IsKeyDown(KEY_UP)) {
      dy -= 1;
      anim.facing = UP;
    }
    if (IsKeyDown(KEY_DOWN)) {
      dy += 1;
      anim.facing = DOWN;
    }

    if (dx != 0 || dy != 0) {
      tr.x += dx * ctrl.speed * dt;
      tr.y += dy * ctrl.speed * dt;
      anim.current = AnimState::WALKING;
    } else {
      anim.current = AnimState::IDLE;
    }
  }
}

void MovementSystem::Render(Renderer &renderer) {}

void AIMovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<AIController>();

  for (int id : ids) {
    auto &ctrl = world.GetComponent<AIController>(id);
    auto &tr = world.GetComponent<Transform>(id);
    auto &anim = world.GetComponent<AnimationState>(id);

    if (ctrl.entityTarget == 0 and ctrl.entityTarget != id) {
      return;
    }

    const float arriveRadius = 0.01f;

    auto &targetTrasnform = world.GetComponent<Transform>(ctrl.entityTarget);
    float dx = targetTrasnform.x - tr.x;
    float dy = targetTrasnform.y - tr.y;

    float distSq = dx * dx + dy * dy;

    if (distSq > arriveRadius * arriveRadius) {
      float dist = sqrtf(distSq);

      float dirX = dx / dist;
      float dirY = dy / dist;

      float step = ctrl.speed * dt;

      // Prevent overshoot
      if (step >= dist) {
        tr.x = targetTrasnform.x;
        tr.y = targetTrasnform.y;
      } else {
        tr.x += dirX * step;
        tr.y += dirY * step;
      }
      // --- Facing logic ---
      if (fabsf(dx) > fabsf(dy))
        anim.facing = (dx > 0) ? Direction::RIGHT : Direction::LEFT;
      else
        anim.facing = (dy > 0) ? Direction::DOWN : Direction::UP;

      anim.current = AnimState::WALKING;
    } else {
      tr.x = targetTrasnform.x;
      tr.y = targetTrasnform.y;
      anim.current = AnimState::IDLE;
    }
  }
}

void AIMovementSystem::Render(Renderer &renderer) {};

std::string AnimationSystem::FacingToString(Direction d) const {
  switch (d) {
  case UP:
    return "up";
  case DOWN:
    return "down";
  case LEFT:
    return "left";
  case RIGHT:
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
  for (int id : ids) {
    auto &sprite = world.GetComponent<AnimatedSprite>(id);
    auto &st = world.GetComponent<AnimationState>(id);
    sprite.SetAnimation(BuildClipKey(st));
    st.previous = st.current;
    sprite.Update(dt);
  }
}

void AnimationSystem::Render(Renderer &renderer) {}

void RenderSystem::Update(float) {}

void RenderSystem::Render(Renderer &renderer) {
  auto ids = world.GetEntitiesWith<AnimatedSprite>();
  for (int id : ids) {
    auto &animSprite = world.GetComponent<AnimatedSprite>(id);
    auto &tr = world.GetComponent<Transform>(id);

    Rectangle src = animSprite.GetFrame();
    Rectangle dest = {tr.x, tr.y, (float)src.width, (float)src.height};

    renderer.DrawTexturePro(animSprite.texture, src, dest, {0, 0}, 0.0f, WHITE);
  }
}

void AnimationSystem::OnWorldLoaded(World &world) {
  for (int e : world.GetEntitiesWith<AnimatedSprite>()) {
    auto &sprite = world.GetComponent<AnimatedSprite>(e);

    sprite.texture = LoadTexture(sprite.texturePath.c_str());

    // Load animations from asset database
    // AnimationDatabase::Apply(sprite);
    sprite.SetAnimation(sprite.currentAnim);
  }
}
} // namespace criogenio
