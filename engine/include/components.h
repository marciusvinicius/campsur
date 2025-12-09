#pragma once
#include "raylib.h"
#include <string>
#include <vector>

namespace criogenio {

struct Transform {
  float x = 0;
  float y = 0;
};

struct Sprite {
  Texture2D texture;
  bool loaded = false;
  std::string path;
};

struct Collider {
  float width = 20;
  float height = 20;
  bool isTrigger = false; // If true, no physics resolution
};

struct CameraComponent {
  Camera2D cam;
  bool active = false;
};

} // namespace criogenio
