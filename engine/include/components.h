#pragma once
#include "animation_state.h"
#include "raylib.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using ComponentTypeId = std::size_t;

namespace criogenio {

enum Direction { UP, DOWN, LEFT, RIGHT };

class ComponentTypeRegistry {
public:
  static ComponentTypeId NewId() {
    static ComponentTypeId lastId = 0;
    return lastId++;
  }
};

template <typename T> ComponentTypeId GetComponentTypeId() {
  static ComponentTypeId id = ComponentTypeRegistry::NewId();
  return id;
}

class Component {
public:
  virtual ~Component() = default;
};

class Transform : public Component {
public:
  float x = 0;
  float y = 0;

  Transform() = default;
  Transform(float x, float y) : x(x), y(y) {}
};

class Sprite : public Component {
public:
  Sprite() {
    loaded = true;
    // Add this on the constructor later
    texture = LoadTextureFromImage(GenImageColor(32, 32, WHITE));
  }
  Texture2D texture;
  bool loaded = false;
  std::string path;
};

struct Animation {
  std::vector<Rectangle> frames;
  float frameTime = 0.1f;
};

class AnimatedSprite : public Component {
public:
  struct Animation {
    std::vector<Rectangle> frames;
    float frameSpeed = 0.2f;
  };

  std::unordered_map<std::string, Animation> animations;
  std::string currentAnim = "";
  float timer = 0.0f;
  int frameIndex = 0;

  Texture2D texture;

  AnimatedSprite(const std::string &initialAnim,
                 const std::vector<Rectangle> &frames, float speed,
                 Texture2D tex)
      : texture(tex) {
    AddAnimation(initialAnim, frames, speed);
    SetAnimation(initialAnim);
  }

  void AddAnimation(const std::string &name,
                    const std::vector<Rectangle> &frames, float speed) {
    animations[name] = Animation{frames, speed};
  }

  void SetAnimation(const std::string &name) {
    if (currentAnim == name)
      return;

    currentAnim = name;
    frameIndex = 0;
    timer = 0.0f;
  }

  void Update(float dt) {
    if (animations.empty() || currentAnim.empty())
      return;
    auto it = animations.find(currentAnim);
    if (it == animations.end())
      return;

    auto &anim = it->second;
    if (anim.frames.empty())
      return;

    timer += dt;
    if (timer >= anim.frameSpeed) {
      timer = 0;
      frameIndex = (frameIndex + 1) % anim.frames.size();
    }
  }

  Rectangle GetFrame() const {

    const auto &anim = animations.at(currentAnim);
    if (anim.frames.empty()) {
      TraceLog(LOG_ERROR, "AnimatedSprite: animation '%s' has 0 frames!",
               currentAnim.c_str());
      return {0, 0, 0, 0}; // prevent crash
    }
    return anim.frames[frameIndex];
  }
};

class Collider : public Component {
public:
  float width = 20;
  float height = 20;
  bool isTrigger = false; // If true, no physics resolution
};

class CameraComponent : public Component {

public:
  Camera2D cam;
  bool active = false;
};

class Controller : public Component {
public:
  float speed = 200.0f;
  Direction direction = UP;
  Controller(float speed) : speed(speed) {}
};


class AIController : public Component {
public:
    float speed = 200.0f;
    Direction direction = UP;
    AIBrainState brainState = FRIENDLY;
    Vector2 target;
};


class InteractableComponent : public Component {
public:

};

class Name : public Component {
public:
  std::string name;
  Name(const std::string &name) : name(name) {}
};

class AnimationState : public Component {
public:
  AnimState current = AnimState::IDLE;
  AnimState previous = AnimState::IDLE;
  Direction facing = DOWN;
};

} // namespace criogenio
