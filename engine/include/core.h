#pragma once

#include "components.h"
#include "scene.h"
#include "systems.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
namespace criogenio {
class MovementSystem : public ISystem {
public:
  Scene &scene;
  MovementSystem(Scene &s) : scene(s) {}

  void Update(float dt) override {
    auto ids = scene.GetEntitiesWith<Controller>();

    for (int id : ids) {
      auto &ctrl = scene.GetComponent<Controller>(id);
      auto &tr = scene.GetComponent<Transform>(id);
      auto &anim = scene.GetComponent<AnimationState>(id);

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

  void Render(Renderer &) override {}
};

class AnimationSystem : public ISystem {
public:
  Scene &scene;
  AnimationSystem(Scene &s) : scene(s) {};
  std::string FacingToString(Direction d) const {
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

  std::string BuildClipKey(const AnimationState &st) {
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

  void Update(float dt) override {
    auto ids = scene.GetEntitiesWith<AnimatedSprite>();
    for (int id : ids) {
      auto &sprite = scene.GetComponent<AnimatedSprite>(id);
      auto &st = scene.GetComponent<AnimationState>(id);

      if (st.current != st.previous) {
        sprite.SetAnimation(BuildClipKey(st));
      }
      st.previous = st.current;
      sprite.Update(dt);
    }
  }

  void Render(Renderer &) override {}
};

class RenderSystem : public ISystem {
public:
  Scene &scene;

  RenderSystem(Scene &s) : scene(s) {}

  void Update(float) override {}

  void Render(Renderer &renderer) override {
    auto ids = scene.GetEntitiesWith<AnimatedSprite>();
    for (int id : ids) {
      auto &animSprite = scene.GetComponent<AnimatedSprite>(id);
      auto &tr = scene.GetComponent<Transform>(id);

      Rectangle src = animSprite.GetFrame();
      Rectangle dest = {tr.x, tr.y, (float)src.width, (float)src.height};

      renderer.DrawTexturePro(animSprite.texture, src, dest, {0, 0}, 0.0f,
                              WHITE);
    }
  }
};

} // namespace criogenio