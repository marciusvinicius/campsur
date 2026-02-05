#pragma once

#include "graphics_types.h"

namespace criogenio {
class Input {
public:
  // key: use criogenio::Key (scancode value for SDL backend)
  static bool IsKeyDown(int key);
  static bool IsKeyPressed(int key);
  static bool IsMouseDown(int button);
  static Vec2 GetMousePosition();
  // Call once per frame so IsKeyPressed can detect edge; optional if only using IsKeyDown.
  static void EndFrame();
};
} // namespace criogenio
