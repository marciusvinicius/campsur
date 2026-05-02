#pragma once
#include "animation_database.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "box3d/fp_camera.h"
#include "graphics_types.h"
#include "network/net_entity.h"
#include "json.hpp"
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
  float scale_x = 1.0f;
  float scale_y = 1.0f;

  Transform() = default;
  Transform(float x, float y) : x(x), y(y) {}

  std::string TypeName() const override { return "Transform"; }

  SerializedComponent Serialize() const override {
    return {"Transform",
            {{"x", x},
             {"y", y},
             {"rotation", rotation},
             {"scale_x", scale_x},
             {"scale_y", scale_y}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("x"); it != data.fields.end())
      x = GetFloat(it->second);
    if (auto it = data.fields.find("y"); it != data.fields.end())
      y = GetFloat(it->second);
    if (auto it = data.fields.find("rotation"); it != data.fields.end())
      rotation = GetFloat(it->second);
    if (auto it = data.fields.find("scale_x"); it != data.fields.end())
      scale_x = GetFloat(it->second);
    if (auto it = data.fields.find("scale_y"); it != data.fields.end())
      scale_y = GetFloat(it->second);
  }
};

class Transform3D : public Component {
public:
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float rotationX = 0.0f;
  float rotationY = 0.0f;
  float rotationZ = 0.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float scaleZ = 1.0f;

  std::string TypeName() const override { return "Transform3D"; }

  SerializedComponent Serialize() const override {
    return {"Transform3D",
            {{"x", x},
             {"y", y},
             {"z", z},
             {"rotationX", rotationX},
             {"rotationY", rotationY},
             {"rotationZ", rotationZ},
             {"scaleX", scaleX},
             {"scaleY", scaleY},
             {"scaleZ", scaleZ}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("x"); it != data.fields.end())
      x = GetFloat(it->second);
    if (auto it = data.fields.find("y"); it != data.fields.end())
      y = GetFloat(it->second);
    if (auto it = data.fields.find("z"); it != data.fields.end())
      z = GetFloat(it->second);
    if (auto it = data.fields.find("rotationX"); it != data.fields.end())
      rotationX = GetFloat(it->second);
    if (auto it = data.fields.find("rotationY"); it != data.fields.end())
      rotationY = GetFloat(it->second);
    if (auto it = data.fields.find("rotationZ"); it != data.fields.end())
      rotationZ = GetFloat(it->second);
    if (auto it = data.fields.find("scaleX"); it != data.fields.end())
      scaleX = GetFloat(it->second);
    if (auto it = data.fields.find("scaleY"); it != data.fields.end())
      scaleY = GetFloat(it->second);
    if (auto it = data.fields.find("scaleZ"); it != data.fields.end())
      scaleZ = GetFloat(it->second);
  }
};

class Model3D : public Component {
public:
  std::string modelPath;
  float tintR = 1.0f;
  float tintG = 1.0f;
  float tintB = 1.0f;
  bool visible = true;

  std::string TypeName() const override { return "Model3D"; }

  SerializedComponent Serialize() const override {
    return {"Model3D",
            {{"modelPath", modelPath},
             {"tintR", tintR},
             {"tintG", tintG},
             {"tintB", tintB},
             {"visible", visible}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("modelPath"); it != data.fields.end())
      modelPath = GetString(it->second);
    if (auto it = data.fields.find("tintR"); it != data.fields.end())
      tintR = GetFloat(it->second);
    if (auto it = data.fields.find("tintG"); it != data.fields.end())
      tintG = GetFloat(it->second);
    if (auto it = data.fields.find("tintB"); it != data.fields.end())
      tintB = GetFloat(it->second);
    if (auto it = data.fields.find("visible"); it != data.fields.end())
      visible = std::get<bool>(it->second);
  }
};

class BoxCollider3D : public Component {
public:
  float sizeX = 1.0f;
  float sizeY = 1.0f;
  float sizeZ = 1.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float offsetZ = 0.0f;
  bool isTrigger = false;

  std::string TypeName() const override { return "BoxCollider3D"; }

  SerializedComponent Serialize() const override {
    return {"BoxCollider3D",
            {{"sizeX", sizeX},
             {"sizeY", sizeY},
             {"sizeZ", sizeZ},
             {"offsetX", offsetX},
             {"offsetY", offsetY},
             {"offsetZ", offsetZ},
             {"isTrigger", isTrigger}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("sizeX"); it != data.fields.end())
      sizeX = GetFloat(it->second);
    if (auto it = data.fields.find("sizeY"); it != data.fields.end())
      sizeY = GetFloat(it->second);
    if (auto it = data.fields.find("sizeZ"); it != data.fields.end())
      sizeZ = GetFloat(it->second);
    if (auto it = data.fields.find("offsetX"); it != data.fields.end())
      offsetX = GetFloat(it->second);
    if (auto it = data.fields.find("offsetY"); it != data.fields.end())
      offsetY = GetFloat(it->second);
    if (auto it = data.fields.find("offsetZ"); it != data.fields.end())
      offsetZ = GetFloat(it->second);
    if (auto it = data.fields.find("isTrigger"); it != data.fields.end())
      isTrigger = std::get<bool>(it->second);
  }
};

struct Animation {
  std::vector<Rect> frames;
  float frameTime = 0.1f;
};

class Controller : public Component {
public:
  Vec2 velocity = {0, 0};
  /** When true, movement input is ignored (e.g. player death in Subterra Guild). */
  bool movement_frozen = false;
  /**
   * Top-down TMX collision (Subterra-style): when both extents > 0 and terrain has a
   * collision mask, `MovementSystem` resolves against solid tiles using offsets from `Transform`.
   */
  float tile_collision_w = 0.f;
  float tile_collision_h = 0.f;
  float tile_collision_offset_x = 0.f;
  float tile_collision_offset_y = 0.f;

  Direction direction = Direction::UP;
  Controller() = default;
  Controller(Vec2 velocity) : velocity(velocity) {}

  std::string TypeName() const override { return "Controller"; }

  SerializedComponent Serialize() const override {
    return {"Controller",
            {
                {"velocity_x", velocity.x},
                {"velocity_y", velocity.y},
                {"direction", static_cast<int>(direction)},
                {"movement_frozen", movement_frozen ? 1.f : 0.f},
                {"tile_collision_w", tile_collision_w},
                {"tile_collision_h", tile_collision_h},
                {"tile_collision_offset_x", tile_collision_offset_x},
                {"tile_collision_offset_y", tile_collision_offset_y},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    velocity.x = GetFloat(data.fields.at("velocity_x"));
    velocity.y = GetFloat(data.fields.at("velocity_y"));
    direction = static_cast<Direction>(GetInt(data.fields.at("direction")));
    if (auto it = data.fields.find("movement_frozen"); it != data.fields.end())
      movement_frozen = GetFloat(it->second) > 0.5f;
    if (auto it = data.fields.find("tile_collision_w"); it != data.fields.end())
      tile_collision_w = GetFloat(it->second);
    if (auto it = data.fields.find("tile_collision_h"); it != data.fields.end())
      tile_collision_h = GetFloat(it->second);
    if (auto it = data.fields.find("tile_collision_offset_x"); it != data.fields.end())
      tile_collision_offset_x = GetFloat(it->second);
    if (auto it = data.fields.find("tile_collision_offset_y"); it != data.fields.end())
      tile_collision_offset_y = GetFloat(it->second);
  }
};

class PlayerController3D : public Component {
public:
  float moveSpeed = 4.5f;
  float verticalSpeed = 3.0f;

  std::string TypeName() const override { return "PlayerController3D"; }

  SerializedComponent Serialize() const override {
    return {"PlayerController3D",
            {{"moveSpeed", moveSpeed}, {"verticalSpeed", verticalSpeed}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("moveSpeed"); it != data.fields.end())
      moveSpeed = GetFloat(it->second);
    if (auto it = data.fields.find("verticalSpeed"); it != data.fields.end())
      verticalSpeed = GetFloat(it->second);
  }
};

class AIController : public Component {
public:
  Vec2 velocity = {0, 0};
  Direction direction = Direction::UP;
  AIBrainState brainState = FRIENDLY;
  int entityTarget = -1;

  AIController() = default;
  AIController(Vec2 velocity, int entityTarget)
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
    velocity.x = GetFloat(data.fields.at("velocity_x"));
    velocity.y = GetFloat(data.fields.at("velocity_y"));
    direction = static_cast<Direction>(GetInt(data.fields.at("direction")));
    brainState = static_cast<AIBrainState>(GetInt(data.fields.at("brainState")));
    entityTarget = GetInt(data.fields.at("entityTarget"));
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

/** Optional draw order for `RenderSystem` / `SpriteSystem`. Higher draws later (on top). */
class SpriteRenderLayer : public Component {
public:
  int layer = 0;

  SpriteRenderLayer() = default;
  explicit SpriteRenderLayer(int l) : layer(l) {}

  std::string TypeName() const override { return "SpriteRenderLayer"; }
  SerializedComponent Serialize() const override {
    return {"SpriteRenderLayer", {{"layer", layer}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("layer"); it != data.fields.end())
      layer = GetInt(it->second);
  }
};

class Gravity : public GlobalComponent {
public:
  Gravity() = default;
  std::string TypeName() const override { return "Gravity"; }
  /** Downward acceleration in pixels/s². Use ~980 for typical platformer feel. */
  float strength = 980.0f;
  SerializedComponent Serialize() const override {
    return {"Gravity", {{"strength", strength}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("strength"); it != data.fields.end())
      strength = GetFloat(it->second);
  }
};

/** Set by CollisionSystem when entity lands on a platform. Cleared each frame; used for jump. */
class Grounded : public Component {
public:
  bool value = false;
  std::string TypeName() const override { return "Grounded"; }
  SerializedComponent Serialize() const override { return {"Grounded", {{"value", value}}}; }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("value"); it != data.fields.end())
      value = std::get<bool>(it->second);
  }
};

class RigidBody : public Component {
public:
  RigidBody() = default;
  std::string TypeName() const override { return "RigidBody"; }
  float mass = 1.0f;
  /** Physics velocity (pixels/s). GravitySystem modifies .y; movement systems set .x (and .y for jump). */
  Vec2 velocity = {0, 0};
  SerializedComponent Serialize() const override {
    return {"RigidBody",
            {{"mass", mass}, {"velocity_x", velocity.x}, {"velocity_y", velocity.y}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("mass"); it != data.fields.end())
      mass = GetFloat(it->second);
    if (auto it = data.fields.find("velocity_x"); it != data.fields.end())
      velocity.x = GetFloat(it->second);
    if (auto it = data.fields.find("velocity_y"); it != data.fields.end())
      velocity.y = GetFloat(it->second);
  }
};

// TODO:(maraujo) Normalize the way I'm serializing
class BoxCollider : public Component {
public:
  BoxCollider() = default;
  std::string TypeName() const override { return "BoxCollider"; }
  float width = 1.0f;
  float height = 1.0f;
  /** Offset from entity Transform position (collider rect = transform + offset, size). */
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  /** When true, only collides when entity falls onto it from above (one-way platform). */
  bool isPlatform = false;
  SerializedComponent Serialize() const override {
    return {"BoxCollider",
            {{"width", width}, {"height", height}, {"offsetX", offsetX}, {"offsetY", offsetY}, {"isPlatform", isPlatform}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("width"); it != data.fields.end())
      width = GetFloat(it->second);
    if (auto it = data.fields.find("height"); it != data.fields.end())
      height = GetFloat(it->second);
    if (auto it = data.fields.find("offsetX"); it != data.fields.end())
      offsetX = GetFloat(it->second);
    if (auto it = data.fields.find("offsetY"); it != data.fields.end())
      offsetY = GetFloat(it->second);
    if (auto it = data.fields.find("isPlatform"); it != data.fields.end())
      isPlatform = std::get<bool>(it->second);
  }
};

class Box3D : public Component {
public:
  float halfX = 0.5f;
  float halfY = 0.5f;
  float halfZ = 0.5f;
  float colorR = 0.75f;
  float colorG = 0.78f;
  float colorB = 0.88f;
  bool wireframe = true;

  std::string TypeName() const override { return "Box3D"; }

  SerializedComponent Serialize() const override {
    return {"Box3D",
            {{"halfX", halfX},
             {"halfY", halfY},
             {"halfZ", halfZ},
             {"colorR", colorR},
             {"colorG", colorG},
             {"colorB", colorB},
             {"wireframe", wireframe}}};
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("halfX"); it != data.fields.end())
      halfX = GetFloat(it->second);
    if (auto it = data.fields.find("halfY"); it != data.fields.end())
      halfY = GetFloat(it->second);
    if (auto it = data.fields.find("halfZ"); it != data.fields.end())
      halfZ = GetFloat(it->second);
    if (auto it = data.fields.find("colorR"); it != data.fields.end())
      colorR = GetFloat(it->second);
    if (auto it = data.fields.find("colorG"); it != data.fields.end())
      colorG = GetFloat(it->second);
    if (auto it = data.fields.find("colorB"); it != data.fields.end())
      colorB = GetFloat(it->second);
    if (auto it = data.fields.find("wireframe"); it != data.fields.end())
      wireframe = std::get<bool>(it->second);
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
    if (auto it = data.fields.find("textureId"); it != data.fields.end())
      textureId = static_cast<AssetId>(GetInt(it->second));
    if (auto it = data.fields.find("spriteX"); it != data.fields.end())
      spriteX = GetInt(it->second);
    if (auto it = data.fields.find("spriteY"); it != data.fields.end())
      spriteY = GetInt(it->second);
    if (auto it = data.fields.find("spriteSize"); it != data.fields.end())
      spriteSize = GetInt(it->second);
  }

  void SetTexture(const char *path) {
    atlas = criogenio::AssetManager::instance().load<TextureResource>(path);
    atlasPath = path;
  }
};

/** Tag component: entities with this are replicated over the network. */
class NetReplicated : public Component {
public:
  std::string TypeName() const override { return "NetReplicated"; }
  SerializedComponent Serialize() const override {
    return {"NetReplicated", {}};
  }
  void Deserialize(const SerializedComponent &) override {}
};

/** Client-only: net entity id for this replicated entity (e.g. for connection-order color). */
struct ReplicatedNetId : public Component {
  NetEntityId id = INVALID_NET_ENTITY;
  ReplicatedNetId() = default;
  explicit ReplicatedNetId(NetEntityId i) : id(i) {}
  std::string TypeName() const override { return "ReplicatedNetId"; }
  SerializedComponent Serialize() const override { return {"ReplicatedNetId", {}}; }
  void Deserialize(const SerializedComponent &) override {}
};

/** 2D camera as an entity component. When present on an entity, it can be used as the main camera. */
class Camera2DComponent : public Component {
public:
  Camera2D data;

  Camera2DComponent() = default;
  explicit Camera2DComponent(const Camera2D& cam) : data(cam) {}

  std::string TypeName() const override { return "Camera2D"; }

  SerializedComponent Serialize() const override {
    return {"Camera2D",
            {{"offset_x", data.offset.x},
             {"offset_y", data.offset.y},
             {"target_x", data.target.x},
             {"target_y", data.target.y},
             {"rotation", data.rotation},
             {"zoom", data.zoom}}};
  }

  void Deserialize(const SerializedComponent& data_in) override {
    if (auto it = data_in.fields.find("offset_x"); it != data_in.fields.end())
      data.offset.x = GetFloat(it->second);
    if (auto it = data_in.fields.find("offset_y"); it != data_in.fields.end())
      data.offset.y = GetFloat(it->second);
    if (auto it = data_in.fields.find("target_x"); it != data_in.fields.end())
      data.target.x = GetFloat(it->second);
    if (auto it = data_in.fields.find("target_y"); it != data_in.fields.end())
      data.target.y = GetFloat(it->second);
    if (auto it = data_in.fields.find("rotation"); it != data_in.fields.end())
      data.rotation = GetFloat(it->second);
    if (auto it = data_in.fields.find("zoom"); it != data_in.fields.end())
      data.zoom = GetFloat(it->second);
  }
};

// Backward-compatible alias used by existing game/editor code.
using Camera = Camera2DComponent;

class Camera3D : public Component {
public:
  float positionX = 0.0f;
  float positionY = 1.65f;
  float positionZ = 2.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  float fovDeg = 70.0f;
  float nearPlane = 0.05f;
  float farPlane = 120.0f;

  Camera3D() = default;
  explicit Camera3D(const box3d::FPCamera& cam) { FromFPCamera(cam); }

  std::string TypeName() const override { return "Camera3D"; }

  SerializedComponent Serialize() const override {
    return {"Camera3D",
            {{"positionX", positionX},
             {"positionY", positionY},
             {"positionZ", positionZ},
             {"yaw", yaw},
             {"pitch", pitch},
             {"fovDeg", fovDeg},
             {"nearPlane", nearPlane},
             {"farPlane", farPlane}}};
  }

  void Deserialize(const SerializedComponent& data_in) override {
    if (auto it = data_in.fields.find("positionX"); it != data_in.fields.end())
      positionX = GetFloat(it->second);
    if (auto it = data_in.fields.find("positionY"); it != data_in.fields.end())
      positionY = GetFloat(it->second);
    if (auto it = data_in.fields.find("positionZ"); it != data_in.fields.end())
      positionZ = GetFloat(it->second);
    if (auto it = data_in.fields.find("yaw"); it != data_in.fields.end())
      yaw = GetFloat(it->second);
    if (auto it = data_in.fields.find("pitch"); it != data_in.fields.end())
      pitch = GetFloat(it->second);
    if (auto it = data_in.fields.find("fovDeg"); it != data_in.fields.end())
      fovDeg = GetFloat(it->second);
    if (auto it = data_in.fields.find("nearPlane"); it != data_in.fields.end())
      nearPlane = GetFloat(it->second);
    if (auto it = data_in.fields.find("farPlane"); it != data_in.fields.end())
      farPlane = GetFloat(it->second);
  }

  box3d::FPCamera ToFPCamera() const {
    box3d::FPCamera cam;
    cam.position = {positionX, positionY, positionZ};
    cam.yaw = yaw;
    cam.pitch = pitch;
    cam.fovDeg = fovDeg;
    cam.nearPlane = nearPlane;
    cam.farPlane = farPlane;
    return cam;
  }

  void FromFPCamera(const box3d::FPCamera& cam) {
    positionX = cam.position.x;
    positionY = cam.position.y;
    positionZ = cam.position.z;
    yaw = cam.yaw;
    pitch = cam.pitch;
    fovDeg = cam.fovDeg;
    nearPlane = cam.nearPlane;
    farPlane = cam.farPlane;
  }
};

}  // namespace criogenio
