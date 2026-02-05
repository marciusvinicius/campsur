#pragma once

namespace criogenio {

// Key identifiers for Input::IsKeyDown / IsKeyPressed.
// Values match SDL scancodes so the engine's SDL backend can use them directly.
enum class Key : int {
  Up = 82,
  Down = 81,
  Left = 80,
  Right = 79,
  W = 26,
  A = 4,
  S = 22,
  D = 7,
  Space = 44,
  Escape = 41,
  Enter = 40,
  LeftCtrl = 224,
  RightCtrl = 228,
  LeftShift = 225,
  RightShift = 229,
  LeftAlt = 226,
  RightAlt = 230,
};

} // namespace criogenio
