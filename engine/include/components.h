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
inline ComponentTypeId GetUniqueComponentTypeId() {
  static ComponentTypeId lastId = 0;
  return lastId++;
}

template <typename T> inline ComponentTypeId GetComponentTypeId() {
  static ComponentTypeId typeId = GetUniqueComponentTypeId();
  return typeId;
}

class Component {
public:
  virtual ~Component() = default;

  virtual std::string TypeName() const = 0;
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
    if (auto it = data.fields.find("x"); it != data.fields.end())
      x = std::get<float>(it->second);

    if (auto it = data.fields.find("y"); it != data.fields.end())
      y = std::get<float>(it->second);

    if (auto it = data.fields.find("rotation"); it != data.fields.end())
      rotation = std::get<float>(it->second);

    if (auto it = data.fields.find("scale_x"); it != data.fields.end())
      scale_x = std::get<float>(it->second);

    if (auto it = data.fields.find("scale_y"); it != data.fields.end())
      scale_y = std::get<float>(it->second);
  }
};

struct Animation {
  std::vector<Rectangle> frames;
  float frameTime = 0.1f;
};

class Controller : public Component {
public:
  float speed = 200.0f;
  Direction direction = Direction::UP;
  Controller() = default;
  Controller(float speed) : speed(speed) {}

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
  Direction direction = Direction::UP;
  AIBrainState brainState = FRIENDLY;
  int entityTarget = -1;

  AIController() = default;

  AIController(float speed, int entityTarget)
      : speed(speed), entityTarget(entityTarget) {}
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

class Name : public Component {
public:
  std::string name;

  Name() = default;
  Name(const std::string &name) : name(name) {}

  std::string TypeName() const override { return "Name"; }
  SerializedComponent Serialize() const override {
    return {"Name", {{"name", name}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    name = std::get<std::string>(data.fields.at("name"));
  }
};

} // namespace criogenio
