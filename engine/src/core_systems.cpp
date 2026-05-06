#include "core_systems.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "components.h"
#include "gameplay_tags.h"
#include "graphics_types.h"
#include "input.h"
#include "keys.h"
#include "math.h"
#include "resources.h"
#include "terrain.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <vector>

namespace criogenio {

namespace {

PlayerMovementAxisOverrideFn g_playerMovementAxisOverride = nullptr;
PlayerRunHeldOverrideFn g_playerRunHeldOverride = nullptr;
struct WorldMovementInputProvider {
  WorldMovementAxisProviderFn axisFn = nullptr;
  WorldRunHeldProviderFn runHeldFn = nullptr;
  void *user = nullptr;
};
std::unordered_map<World *, WorldMovementInputProvider> g_worldInputProviders;
CoreSystemPerfCounters g_coreSystemPerfCounters;

void RecordPerfSample(uint64_t &samples, double &avgMs, double &maxMs, double sampleMs) {
  ++samples;
  const double n = static_cast<double>(samples);
  avgMs += (sampleMs - avgMs) / n;
  if (sampleMs > maxMs)
    maxMs = sampleMs;
}

bool NameLooksLikePlayer(const Name *nm) {
  if (!nm)
    return false;
  static const char k[] = "player";
  const std::string &s = nm->name;
  if (s.size() != sizeof(k) - 1)
    return false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(s[i])) !=
        static_cast<unsigned char>(k[i]))
      return false;
  }
  return true;
}

std::string FacingToken(Direction d) {
  switch (d) {
  case Direction::UP:
    return "up";
  case Direction::DOWN:
    return "down";
  case Direction::LEFT:
    return "left";
  case Direction::RIGHT:
    return "right";
  default:
    return "down";
  }
}

std::string BuildClipKeyFromState(const AnimationState &st) {
  const std::string f = FacingToken(st.facing);
  switch (st.current) {
  case AnimState::IDLE:
    return "idle_" + f;
  case AnimState::WALKING:
    return "walk_" + f;
  case AnimState::RUNNING:
    return "run_" + f;
  case AnimState::JUMPING:
    return "jump_" + f;
  case AnimState::ATTACKING:
    return "attack_" + f;
  default:
    return "idle_down";
  }
}

const AnimationClip *PickClipForSprite(const AnimationDef *def, const AnimationState &st,
                                       const std::string &preferredKey) {
  if (!def || def->clips.empty())
    return nullptr;
  for (const auto &c : def->clips) {
    if (c.name == preferredKey)
      return &c;
  }
  for (const auto &c : def->clips) {
    if (c.state == AnimState::Count)
      continue;
    if (c.state == st.current && c.direction == st.facing)
      return &c;
  }
  for (const auto &c : def->clips) {
    if (c.state == AnimState::Count)
      continue;
    if (c.state == st.current)
      return &c;
  }
  if (st.current == AnimState::RUNNING || st.current == AnimState::JUMPING ||
      st.current == AnimState::ATTACKING) {
    for (const auto &c : def->clips) {
      if (c.state == AnimState::Count)
        continue;
      if (c.state == AnimState::WALKING && c.direction == st.facing)
        return &c;
    }
    for (const auto &c : def->clips) {
      if (c.state == AnimState::Count)
        continue;
      if (c.state == AnimState::WALKING)
        return &c;
    }
  }
  for (const auto &c : def->clips) {
    if (c.state == AnimState::Count)
      continue;
    if (c.state == AnimState::IDLE && c.direction == st.facing)
      return &c;
  }
  for (const auto &c : def->clips) {
    if (c.state == AnimState::Count)
      continue;
    if (c.state == AnimState::IDLE)
      return &c;
  }
  for (const auto &c : def->clips) {
    if (c.state != AnimState::Count)
      return &c;
  }
  return &def->clips[0];
}

std::string ResolveClipKeyForAnimation(AssetId animId, const AnimationState &st) {
  const std::string preferred = BuildClipKeyFromState(st);
  const auto *def = AnimationDatabase::instance().getAnimation(animId);
  if (const auto *clip = PickClipForSprite(def, st, preferred))
    return clip->name;
  return preferred;
}

/** Base layer from `SpriteRenderLayer` plus a large bias for the player (`Name` or `PlayerTag`). */
int EffectiveSpriteDrawOrder(const World &world, ecs::EntityId id) {
  int order = 0;
  if (auto *rl = world.GetComponent<SpriteRenderLayer>(id))
    order += rl->layer;
  if (NameLooksLikePlayer(world.GetComponent<Name>(id)) ||
      world.HasComponent<PlayerTag>(id))
    order += 100000;
  return order;
}

} // namespace

void SetPlayerMovementAxisOverride(PlayerMovementAxisOverrideFn fn) {
  g_playerMovementAxisOverride = fn;
}

PlayerMovementAxisOverrideFn GetPlayerMovementAxisOverride() {
  return g_playerMovementAxisOverride;
}

void SetPlayerRunHeldOverride(PlayerRunHeldOverrideFn fn) { g_playerRunHeldOverride = fn; }

void SetWorldMovementInputProvider(World &world, WorldMovementAxisProviderFn axisFn,
                                   WorldRunHeldProviderFn runHeldFn, void *user) {
  if (!axisFn && !runHeldFn) {
    g_worldInputProviders.erase(&world);
    return;
  }
  g_worldInputProviders[&world] = {axisFn, runHeldFn, user};
}

void ClearWorldMovementInputProvider(World &world) { g_worldInputProviders.erase(&world); }

const CoreSystemPerfCounters &GetCoreSystemPerfCounters() { return g_coreSystemPerfCounters; }

void ResetCoreSystemPerfCounters() { g_coreSystemPerfCounters = {}; }

void MovementSystem::Update(float dt) {
  const auto t0 = std::chrono::high_resolution_clock::now();
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<Controller>(ids);

  for (ecs::EntityId id : ids) {
    auto *ctrl = world.GetComponent<Controller>(id);
    auto *tr = world.GetComponent<Transform>(id);
    auto *anim = world.GetComponent<AnimationState>(id);
    if (const auto *nm = world.GetComponent<Name>(id)) {
      if (nm->name != "player")
        continue;
    }

    if (!ctrl || !tr || !anim)
      continue;

    if (Input::IsGameplayInputSuppressed() || ctrl->movement_frozen) {
      anim->current = AnimState::IDLE;
      continue;
    }

    float dx = 0, dy = 0;
    bool usedOverride = false;
    auto wit = g_worldInputProviders.find(&world);
    if (wit != g_worldInputProviders.end() && wit->second.axisFn) {
      usedOverride = wit->second.axisFn(wit->second.user, world, id, &dx, &dy);
    }
    if (!usedOverride && g_playerMovementAxisOverride) {
      usedOverride = g_playerMovementAxisOverride(world, id, &dx, &dy);
    }

    if (!usedOverride) {
      if (Input::IsKeyDown(static_cast<int>(Key::Right)) ||
          Input::IsKeyDown(static_cast<int>(Key::D))) {
        dx += 1;
        anim->facing = Direction::RIGHT;
      }
      if (Input::IsKeyDown(static_cast<int>(Key::Left)) ||
          Input::IsKeyDown(static_cast<int>(Key::A))) {
        dx -= 1;
        anim->facing = Direction::LEFT;
      }
      if (Input::IsKeyDown(static_cast<int>(Key::Up)) ||
          Input::IsKeyDown(static_cast<int>(Key::W))) {
        dy -= 1;
        anim->facing = Direction::UP;
      }
      if (Input::IsKeyDown(static_cast<int>(Key::Down)) ||
          Input::IsKeyDown(static_cast<int>(Key::S))) {
        dy += 1;
        anim->facing = Direction::DOWN;
      }
    } else {
      if (dx > 0.01f)
        anim->facing = Direction::RIGHT;
      else if (dx < -0.01f)
        anim->facing = Direction::LEFT;
      else if (dy < -0.01f)
        anim->facing = Direction::UP;
      else if (dy > 0.01f)
        anim->facing = Direction::DOWN;
    }

    bool run = false;
    if (wit != g_worldInputProviders.end() && wit->second.runHeldFn) {
      run = wit->second.runHeldFn(wit->second.user);
    } else if (g_playerRunHeldOverride) {
      run = g_playerRunHeldOverride();
    } else {
      run = Input::IsKeyDown(static_cast<int>(Key::LeftShift)) ||
            Input::IsKeyDown(static_cast<int>(Key::RightShift));
    }
    const float runMul = run ? 1.55f : 1.f;

    if (dx != 0 || dy != 0) {
      const float stepX = dx * ctrl->velocity.x * runMul * dt;
      const float stepY = dy * ctrl->velocity.y * runMul * dt;
      Terrain2D *terrain = world.GetTerrain();
      const bool useTileCol =
          terrain && ctrl->tile_collision_w > 0.f && ctrl->tile_collision_h > 0.f &&
          !terrain->tmxMeta.collisionSolid.empty();
      if (useTileCol) {
        const float ox = ctrl->tile_collision_offset_x;
        const float oy = ctrl->tile_collision_offset_y;
        const float cw = ctrl->tile_collision_w;
        const float ch = ctrl->tile_collision_h;
        const float x0 = tr->x;
        const float y0 = tr->y;
        const float tryX = x0 + stepX;
        if (!terrain->TmxFootprintOverlapsSolid(tryX + ox, y0 + oy, cw, ch))
          tr->x = tryX;
        const float tryY = y0 + stepY;
        if (!terrain->TmxFootprintOverlapsSolid(tr->x + ox, tryY + oy, cw, ch))
          tr->y = tryY;
      } else {
        tr->x += stepX;
        tr->y += stepY;
      }
      anim->current = run ? AnimState::RUNNING : AnimState::WALKING;
    } else {
      anim->current = AnimState::IDLE;
    }
  }
  const auto t1 = std::chrono::high_resolution_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  RecordPerfSample(g_coreSystemPerfCounters.movementSamples, g_coreSystemPerfCounters.movementAvgMs,
                   g_coreSystemPerfCounters.movementMaxMs, ms);
}

void MovementSystem::Render(Renderer &renderer) {}

void MovementSystem3D::Update(float dt) {
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<PlayerController3D, Transform3D>(ids);

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
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<AIController>(ids);

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
  return FacingToken(d);
}

std::string AnimationSystem::BuildClipKey(const AnimationState &st) {
  return BuildClipKeyFromState(st);
}

void AnimationSystem::Update(float dt) {
  const auto t0 = std::chrono::high_resolution_clock::now();
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<AnimatedSprite>(ids);
  for (ecs::EntityId id : ids) {
    auto *sprite = world.GetComponent<AnimatedSprite>(id);
    auto *st = world.GetComponent<AnimationState>(id);

    if (!sprite || !st)
      continue;

    if (!sprite->hasCachedSelection || sprite->cachedState != st->current ||
        sprite->cachedFacing != st->facing) {
      sprite->SetClip(ResolveClipKeyForAnimation(sprite->animationId, *st));
      sprite->cachedState = st->current;
      sprite->cachedFacing = st->facing;
      sprite->hasCachedSelection = true;
    }
    st->previous = st->current;
    sprite->Update(dt);
  }
  const auto t1 = std::chrono::high_resolution_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  RecordPerfSample(g_coreSystemPerfCounters.animationSamples,
                   g_coreSystemPerfCounters.animationAvgMs, g_coreSystemPerfCounters.animationMaxMs,
                   ms);
}

void AnimationSystem::Render(Renderer &renderer) {}

void RenderSystem::Update(float) {}

void RenderSystem::Render(Renderer &renderer) {
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<AnimatedSprite>(ids);
  std::sort(ids.begin(), ids.end(), [&](ecs::EntityId a, ecs::EntityId b) {
    const int oa = EffectiveSpriteDrawOrder(world, a);
    const int ob = EffectiveSpriteDrawOrder(world, b);
    if (oa != ob)
      return oa < ob;
    return a < b;
  });
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
    if (src.width <= 0.f || src.height <= 0.f)
      continue;
    Rect dest = {tr->x, tr->y, src.width * sx, src.height * sy};
    // Transform is top-left across gameplay/collision/camera code, so keep render anchored too.
    Vec2 origin = {0.f, 0.f};

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

  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<Transform, RigidBody>(ids);
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
  static thread_local std::vector<ecs::EntityId> dynamics;
  static thread_local std::vector<ecs::EntityId> statics;
  world.GetEntitiesWith<Transform, RigidBody, BoxCollider>(dynamics);
  world.GetEntitiesWith<Transform, BoxCollider>(statics);
  Terrain2D *terrain = world.GetTerrain();
  const int tsx = terrain ? terrain->GridStepX() : 0;
  const int tsy = terrain ? terrain->GridStepY() : 0;
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
    if (!resolved && terrain && tsx > 0 && tsy > 0 && !terrain->layers.empty()) {
      int minTx = static_cast<int>(std::floor((dynLeft - ox) / tsx));
      int maxTx = static_cast<int>(std::floor((dynRight - ox - 0.001f) / tsx));
      int minTy = static_cast<int>(std::floor((dynTop - oy) / tsy));
      int maxTy = static_cast<int>(std::floor((dynBottom - oy - 0.001f) / tsy));

      for (int ty = minTy; ty <= maxTy && !resolved; ty++) {
        for (int tx = minTx; tx <= maxTx && !resolved; tx++) {
          if (!terrain->CellHasTile(0, tx, ty))
            continue;
          float platLeft = ox + tx * tsx;
          float platTop = oy + ty * tsy;
          float platRight = platLeft + tsx;
          float platBottom = platTop + tsy;
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
  static thread_local std::vector<ecs::EntityId> ids;
  world.GetEntitiesWith<Sprite>(ids);
  std::sort(ids.begin(), ids.end(), [&](ecs::EntityId a, ecs::EntityId b) {
    const int oa = EffectiveSpriteDrawOrder(world, a);
    const int ob = EffectiveSpriteDrawOrder(world, b);
    if (oa != ob)
      return oa < ob;
    return a < b;
  });
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
