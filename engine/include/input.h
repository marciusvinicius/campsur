#pragma once
#include "raylib.h"

namespace criogenio {
class Input {
public:
  static bool IsKeyDown(int key);
  static bool IsKeyPressed(int key);
  static bool IsMouseDown(int button);
  static Vector2 GetMousePosition();
};
} // namespace criogenio
