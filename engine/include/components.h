#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

using ComponentTypeId = std::size_t;

namespace criogenio {

inline ComponentTypeId GetNewComponentTypeId() {
  static ComponentTypeId lastId = 0;
  return lastId++;
}

template <typename T> ComponentTypeId GetComponentTypeId() {
  static ComponentTypeId id = GetNewComponentTypeId();
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
};

class Sprite : public Component {
public:
  Texture2D texture;
  bool loaded = false;
  std::string path;
};

class AnimatedSprite : public Component {
public:
  std::vector<Texture2D> frames;
  int currentFrame = 0;
  float frameTime = 0.1f; // Time per frame in seconds
  float elapsedTime = 0.0f;
  bool loaded = false;
  std::string path; // Base path for frames (e.g., "assets/sprite_")
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

class Name : public Component {
public:
  std::string name;
  Name(const std::string &name) : name(name) {}
};

} // namespace criogenio
