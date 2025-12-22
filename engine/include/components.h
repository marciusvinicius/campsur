#pragma once
#include "animation_state.h"
#include "raylib.h"
#include "serialization.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using ComponentTypeId = std::size_t;

namespace criogenio {

enum Direction { UP, DOWN, LEFT, RIGHT };

struct ISerializableComponent {
  virtual ~ISerializableComponent() = default;

  virtual std::string TypeName() const = 0;
  virtual SerializedComponent Serialize() const = 0;
  virtual void Deserialize(const SerializedComponent &data) = 0;
};

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
  virtual std::string TypeName() const = 0;
  virtual ~Component() = default;
  virtual SerializedComponent Serialize() const = 0;
  virtual void Deserialize(const SerializedComponent &data) = 0;
};

class Transform : public Component {
public:
  float x = 0;
  float y = 0;
  float rotation = 0;
  float scale_x = 0;
  float scale_y = 0;

  Transform() = default;
  Transform(float x, float y) : x(x), y(y) {}

  std::string TypeName() const override { return "Transform"; }

  SerializedComponent Serialize() const override {
    return {"Transform", {{"x", x}, {"y", y}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    x = std::get<float>(data.fields.at("x"));
    y = std::get<float>(data.fields.at("y"));
  }
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
  std::string texturePath;

  AnimatedSprite(const std::string &initialAnim,
                 const std::vector<Rectangle> &frames, float speed,
                 Texture2D tex)
      : texture(tex) {

    AddAnimation(initialAnim, frames, speed);
    SetAnimation(initialAnim);
  }
  AnimatedSprite() = default;

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
  std::string TypeName() const override { return "AnimatedSprite"; }

  SerializedComponent Serialize() const override {
    return {"AnimatedSprite",
            {
                {"texturePath", texturePath},
                {"currentAnim", currentAnim},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    texturePath = std::get<std::string>(data.fields.at("texturePath"));
    currentAnim = std::get<std::string>(data.fields.at("currentAnim"));
  }
};

class Collider : public Component {
public:
  float width = 20;
  float height = 20;
  bool isTrigger = false;

  std::string TypeName() const override { return "Collider"; }

  SerializedComponent Serialize() const override {
    return {"Collider",
            {
                {"width", width},
                {"height", height},
                {"isTrigger", isTrigger},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    width = std::get<float>(data.fields.at("width"));
    height = std::get<float>(data.fields.at("height"));
    isTrigger = std::get<bool>(data.fields.at("isTrigger"));
  }
};

class CameraComponent : public Component {

public:
  Camera2D cam;
  bool active = false;
  std::string TypeName() const override { return "CameraComponent"; }

  SerializedComponent Serialize() const override {
    return {"CameraComponent",
            {
                {"active", active},
                {"zoom", cam.zoom},
                {"offset_x", cam.offset.x},
                {"offset_y", cam.offset.y},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    active = std::get<bool>(data.fields.at("active"));
    cam.zoom = std::get<float>(data.fields.at("zoom"));
    cam.offset.x = std::get<float>(data.fields.at("offset_x"));
    cam.offset.y = std::get<float>(data.fields.at("offset_y"));
  }
};

class Controller : public Component {
public:
  float speed = 200.0f;
  Direction direction = UP;
  Controller(float speed) : speed(speed) {}
  Controller() = default;

  std::string TypeName() const override { return "Controller"; }

  SerializedComponent Serialize() const override {
    return {"Controller",
            {
                {"speed", speed},
                {"direction", static_cast<int>(direction)},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    speed = std::get<float>(data.fields.at("speed"));
    direction =
        static_cast<Direction>(std::get<int>(data.fields.at("direction")));
  }
};

class AIController : public Component {
public:
  float speed = 200.0f;
  Direction direction = UP;
  AIBrainState brainState = FRIENDLY;
  int entityTarget = -1;
  std::string TypeName() const override { return "AIController"; }

  SerializedComponent Serialize() const override {
    return {"AIController",
            {
                {"speed", speed},
                {"direction", static_cast<int>(direction)},
                {"brainState", static_cast<int>(brainState)},
                {"entityTarget", entityTarget},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    speed = std::get<float>(data.fields.at("speed"));
    direction =
        static_cast<Direction>(std::get<int>(data.fields.at("direction")));
    brainState =
        static_cast<AIBrainState>(std::get<int>(data.fields.at("brainState")));
    entityTarget = std::get<int>(data.fields.at("entityTarget"));
  }
};

class InteractableComponent : public Component {
public:
  std::string TypeName() const override { return "InteractableComponent"; }

  SerializedComponent Serialize() const override {
    return {"InteractableComponent", {}};
  }

  void Deserialize(const SerializedComponent &) override {}
};

class Name : public Component {
public:
  std::string name;
  Name(const std::string &name) : name(name) {}
  Name() = default;

  std::string TypeName() const override { return "Name"; }

  SerializedComponent Serialize() const override {
    return {"Name", {{"name", name}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    name = std::get<std::string>(data.fields.at("name"));
  }
};

class AnimationState : public Component {
public:
  AnimState current = AnimState::IDLE;
  AnimState previous = AnimState::IDLE;
  Direction facing = DOWN;
  std::string TypeName() const override { return "AnimationState"; }

  SerializedComponent Serialize() const override {
    return {"AnimationState",
            {
                {"current", static_cast<int>(current)},
                {"previous", static_cast<int>(previous)},
                {"facing", static_cast<int>(facing)},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    current = static_cast<AnimState>(std::get<int>(data.fields.at("current")));
    previous =
        static_cast<AnimState>(std::get<int>(data.fields.at("previous")));
    facing = static_cast<Direction>(std::get<int>(data.fields.at("facing")));
  }
};

} // namespace criogenio
