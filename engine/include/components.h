#pragma once
#include "animation_database.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "json.hpp"
#include "raylib.h"
#include "resources.h"
#include "serialization.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Base class for global (scene-wide) components

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

class GlobalComponent {
public:
  virtual ~GlobalComponent() = default;
  virtual std::string TypeName() const = 0;
  virtual SerializedComponent Serialize() const = 0;
  virtual void Deserialize(const SerializedComponent &data) = 0;
};

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
  Vector2 velocity = {0, 0};

  Direction direction = Direction::UP;
  Controller() = default;
  Controller(Vector2 velocity) : velocity(velocity) {}

  std::string TypeName() const override { return "Controller"; }

  SerializedComponent Serialize() const override {
    return {"Controller",
            {
                {"velocity_x", velocity.x},
                {"velocity_y", velocity.y},
                {"direction", static_cast<int>(direction)},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    velocity.x = std::get<float>(data.fields.at("velocity_x"));
    velocity.y = std::get<float>(data.fields.at("velocity_y"));
    direction =
        static_cast<Direction>(std::get<int>(data.fields.at("direction")));
  }
};

class AIController : public Component {
public:
  Vector2 velocity = {0, 0};
  Direction direction = Direction::UP;
  AIBrainState brainState = FRIENDLY;
  int entityTarget = -1;

  AIController() = default;
  AIController(Vector2 velocity, int entityTarget)
      : velocity(velocity), entityTarget(entityTarget) {}

  std::string TypeName() const override { return "AIController"; }

  SerializedComponent Serialize() const override {
    return {"AIController",
            {
                {"velocity_x", velocity.x},
                {"velocity_y", velocity.y},
                {"direction", static_cast<int>(direction)},
                {"brainState", static_cast<int>(brainState)},
                {"entityTarget", entityTarget},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    velocity.x = std::get<float>(data.fields.at("velocity_x"));
    velocity.y = std::get<float>(data.fields.at("velocity_y"));
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

class Gravity : public GlobalComponent {
public:
  Gravity() = default;
  std::string TypeName() const override { return "Gravity"; }
  float strength = 9.81f;
  SerializedComponent Serialize() const override { return {"Gravity", {}}; }
  void Deserialize(const SerializedComponent &data) override {}
};

class RigidBody : public Component {
public:
  RigidBody() = default;
  std::string TypeName() const override { return "RigidBody"; }
  float mass = 1.0f;
  SerializedComponent Serialize() const override {
    return {"RigidBody", {{"mass", mass}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("mass"); it != data.fields.end())
      mass = std::get<float>(it->second);
  }
};

// TODO:(maraujo) Normalize the way I'm serializing
class BoxCollider : public Component {
public:
  BoxCollider() = default;
  std::string TypeName() const override { return "BoxCollider"; }
  float width = 1.0f;
  float height = 1.0f;
  SerializedComponent Serialize() const override {
    return {"BoxCollider", {{"width", width}, {"height", height}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("width"); it != data.fields.end())

      width = std::get<float>(it->second);
    if (auto it = data.fields.find("height"); it != data.fields.end())
      height = std::get<float>(it->second);
  }
};

class Sprite : public Component {
public:
  Sprite() = default;
  explicit Sprite(AssetId id) : textureId(id) {}
  AssetId textureId = INVALID_ASSET_ID;
  std::string TypeName() const override { return "Sprite"; }
  int spriteX = 32;
  int spriteY = 32;
  int spriteSize = 32;

  std::shared_ptr<TextureResource> atlas;
  std::string atlasPath;

  SerializedComponent Serialize() const override {
    SerializedComponent out;
    out.type = TypeName();
    out.fields["textureId"] = static_cast<int>(textureId);
    out.fields["spriteX"] = static_cast<int>(spriteX);
    out.fields["spriteY"] = static_cast<int>(spriteY);
    out.fields["spriteSize"] = static_cast<int>(spriteSize);
    return out;
  }

  void Deserialize(const SerializedComponent &data) override {
    textureId =
        static_cast<AssetId>(std::get<int>(data.fields.at("textureId")));
    spriteSize =
        static_cast<AssetId>(std::get<int>(data.fields.at("spriteSize")));
  }

  void SetTexture(const char *path) {
    atlas = criogenio::AssetManager::instance().load<TextureResource>(path);
    atlasPath = path;
  }
};

} // namespace criogenio
