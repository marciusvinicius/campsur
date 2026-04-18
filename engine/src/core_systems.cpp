#include "core_systems.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "components.h"
#include "graphics_types.h"
#include "input.h"
#include "keys.h"
#include "math.h"
#include "resources.h"
#include "terrain.h"
#include <cmath>

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

    if (Input::IsKeyDown(static_cast<int>(Key::Right))) {
      dx += 1;
      anim->facing = Direction::RIGHT;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::Left))) {
      dx -= 1;
      anim->facing = Direction::LEFT;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::Up))) {
      dy -= 1;
      anim->facing = Direction::UP;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::Down))) {
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

void MovementSystem3D::Update(float dt) {
  auto ids = world.GetEntitiesWith<PlayerController3D, Transform3D>();

  for (ecs::EntityId id : ids) {
    auto *ctrl = world.GetComponent<PlayerController3D>(id);
    auto *tr = world.GetComponent<Transform3D>(id);
    if (!ctrl || !tr)
      continue;

    float moveX = 0.0f;
    float moveY = 0.0f;
    float moveZ = 0.0f;

    if (Input::IsKeyDown(static_cast<int>(Key::W)))
      moveZ -= 1.0f;
    if (Input::IsKeyDown(static_cast<int>(Key::S)))
      moveZ += 1.0f;
    if (Input::IsKeyDown(static_cast<int>(Key::A)))
      moveX -= 1.0f;
    if (Input::IsKeyDown(static_cast<int>(Key::D)))
      moveX += 1.0f;
    if (Input::IsKeyDown(static_cast<int>(Key::Space)))
      moveY += 1.0f;
    if (Input::IsKeyDown(static_cast<int>(Key::LeftCtrl)))
      moveY -= 1.0f;

    float len = std::sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
    if (len > 1e-5f) {
      moveX /= len;
      moveY /= len;
      moveZ /= len;
    }

    tr->x += moveX * ctrl->moveSpeed * dt;
    tr->z += moveZ * ctrl->moveSpeed * dt;
    tr->y += moveY * ctrl->verticalSpeed * dt;
  }
}

void MovementSystem3D::Render(Renderer &renderer) { (void)renderer; }

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
  case AnimState::RUNNING:
    return "walk_" + FacingToString(st.facing);
  case AnimState::JUMPING:
    return "jump_" + FacingToString(st.facing);
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

    float sx = (tr->scale_x > 0.0f) ? tr->scale_x : 1.0f;
    float sy = (tr->scale_y > 0.0f) ? tr->scale_y : 1.0f;
    Rect src = animSprite->GetFrame();
    Rect dest = {tr->x, tr->y, src.width * sx, src.height * sy};
    Vec2 origin = {dest.width * 0.5f, dest.height * 0.5f};

    renderer.DrawTexturePro(texture->texture, src, dest, origin, tr->rotation,
                            Colors::White);
  }
}

void AnimationSystem::OnWorldLoaded(World &world) {
  // Animation database should be pre-populated by editor or game code
  // Sprites just reference animations by ID at this point
}

void GravitySystem::Update(float dt) {
  auto *gravity = world.GetGlobalComponent<Gravity>();
  float strength = gravity ? gravity->strength : 980.0f;

  auto ids = world.GetEntitiesWith<Transform, RigidBody>();
  for (ecs::EntityId id : ids) {
    auto *rb = world.GetComponent<RigidBody>(id);
    auto *tr = world.GetComponent<Transform>(id);
    if (!rb || !tr)
      continue;

    rb->velocity.y += strength * dt;
    tr->y += rb->velocity.y * dt;
  }
}

void GravitySystem::Render(Renderer &renderer) {}

static bool AABBOverlap(float ax, float ay, float aw, float ah,
                        float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// Resolve dynamic AABB against a platform rect. Returns true if collision was resolved.
static bool ResolveAgainstPlatform(float &trX, float &trY, RigidBody *rb,
    float dynLeft, float dynTop, float dynRight, float dynBottom,
    float colOffX, float colOffY, float colW, float colH,
    float platLeft, float platTop, float platRight, float platBottom,
    bool isPlatform, bool *outGrounded) {
  if (!AABBOverlap(dynLeft, dynTop, colW, colH, platLeft, platTop,
                   platRight - platLeft, platBottom - platTop))
    return false;

  if (isPlatform) {
    if (rb->velocity.y <= 0)
      return false;
    if (dynBottom <= platTop + 1.0f)
      return false;
    trY = platTop - colOffY - colH;
    rb->velocity.y = 0;
    if (outGrounded)
      *outGrounded = true;
    return true;
  }

  float overlapTop = dynBottom - platTop;
  float overlapBottom = platBottom - dynTop;
  float overlapLeft = dynRight - platLeft;
  float overlapRight = platRight - dynLeft;
  float minOverlap = overlapTop;
  int resolve = 0;
  if (overlapBottom < minOverlap) {
    minOverlap = overlapBottom;
    resolve = 1;
  }
  if (overlapLeft < minOverlap) {
    minOverlap = overlapLeft;
    resolve = 2;
  }
  if (overlapRight < minOverlap) {
    minOverlap = overlapRight;
    resolve = 3;
  }
  if (resolve == 0) {
    trY = platTop - colOffY - colH;
    rb->velocity.y = 0;
    if (outGrounded)
      *outGrounded = true;
  } else if (resolve == 1) {
    trY = platBottom - colOffY;
    rb->velocity.y = 0;
  } else if (resolve == 2) {
    trX = platLeft - colOffX - colW;
    rb->velocity.x = 0;
  } else {
    trX = platRight - colOffX;
    rb->velocity.x = 0;
  }
  return true;
}

void CollisionSystem::Update(float dt) {
  (void)dt;
  auto dynamics = world.GetEntitiesWith<Transform, RigidBody, BoxCollider>();
  auto statics = world.GetEntitiesWith<Transform, BoxCollider>();
  Terrain2D *terrain = world.GetTerrain();
  const int ts = terrain ? terrain->tileset.tileSize : 0;
  const float ox = terrain ? terrain->origin.x : 0;
  const float oy = terrain ? terrain->origin.y : 0;

  for (ecs::EntityId dynId : dynamics) {
    auto *grounded = world.GetComponent<Grounded>(dynId);
    if (grounded)
      grounded->value = false;
    auto *rb = world.GetComponent<RigidBody>(dynId);
    auto *tr = world.GetComponent<Transform>(dynId);
    auto *col = world.GetComponent<BoxCollider>(dynId);
    if (!rb || !tr || !col)
      continue;

    float dynLeft = tr->x + col->offsetX;
    float dynTop = tr->y + col->offsetY;
    float dynRight = dynLeft + col->width;
    float dynBottom = dynTop + col->height;
    bool resolved = false;

    // Collision against entity platforms
    for (ecs::EntityId statId : statics) {
      if (statId == dynId || world.HasComponent<RigidBody>(statId))
        continue;
      auto *platTr = world.GetComponent<Transform>(statId);
      auto *platCol = world.GetComponent<BoxCollider>(statId);
      if (!platTr || !platCol)
        continue;
      float platLeft = platTr->x + platCol->offsetX;
      float platTop = platTr->y + platCol->offsetY;
      float platRight = platLeft + platCol->width;
      float platBottom = platTop + platCol->height;
      if (ResolveAgainstPlatform(tr->x, tr->y, rb, dynLeft, dynTop, dynRight, dynBottom,
          col->offsetX, col->offsetY, col->width, col->height,
          platLeft, platTop, platRight, platBottom, platCol->isPlatform,
          grounded ? &grounded->value : nullptr)) {
        resolved = true;
        break;
      }
    }

    // Collision against terrain tiles (solid tiles act as platforms)
    if (!resolved && terrain && ts > 0 && !terrain->layers.empty()) {
      int minTx = static_cast<int>(std::floor((dynLeft - ox) / ts));
      int maxTx = static_cast<int>(std::floor((dynRight - ox - 0.001f) / ts));
      int minTy = static_cast<int>(std::floor((dynTop - oy) / ts));
      int maxTy = static_cast<int>(std::floor((dynBottom - oy - 0.001f) / ts));

      for (int ty = minTy; ty <= maxTy && !resolved; ty++) {
        for (int tx = minTx; tx <= maxTx && !resolved; tx++) {
          if (terrain->GetTile(0, tx, ty) < 0)
            continue;
          float platLeft = ox + tx * ts;
          float platTop = oy + ty * ts;
          float platRight = platLeft + ts;
          float platBottom = platTop + ts;
          dynLeft = tr->x + col->offsetX;
          dynTop = tr->y + col->offsetY;
          dynRight = dynLeft + col->width;
          dynBottom = dynTop + col->height;
          if (ResolveAgainstPlatform(tr->x, tr->y, rb, dynLeft, dynTop, dynRight, dynBottom,
              col->offsetX, col->offsetY, col->width, col->height,
              platLeft, platTop, platRight, platBottom, false,
              grounded ? &grounded->value : nullptr)) {
            resolved = true;
          }
        }
      }
    }
  }
}

void CollisionSystem::Render(Renderer &renderer) { (void)renderer; }

void SpriteSystem::Update(float dt) {}

void SpriteSystem::Render(Renderer &renderer) {
  auto ids = world.GetEntitiesWith<Sprite>();
  for (ecs::EntityId id : ids) {
    auto *sprite = world.GetComponent<Sprite>(id);
    auto *tr = world.GetComponent<Transform>(id);

    // Load texture from asset manager
    if (!sprite->atlas)
      continue;

    auto texture = sprite->atlas;

    float sx = (tr->scale_x > 0.0f) ? tr->scale_x : 1.0f;
    float sy = (tr->scale_y > 0.0f) ? tr->scale_y : 1.0f;
    Rect src = {sprite->spriteX, sprite->spriteY, (float)sprite->spriteSize,
                (float)sprite->spriteSize};
    float w = (float)sprite->spriteSize * sx;
    float h = (float)sprite->spriteSize * sy;
    Rect dest = {tr->x, tr->y, w, h};
    Vec2 origin = {dest.width * 0.5f, dest.height * 0.5f};

    renderer.DrawTexturePro(texture->texture, src, dest, origin, tr->rotation,
                            Colors::White);
  }
}

} // namespace criogenio
