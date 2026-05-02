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
  /** SDL_SCANCODE_E — pick up / interact in Subterra Guild. */
  E = 8,
  /** SDL_SCANCODE_1 … SDL_SCANCODE_5 — action bar selection. */
  Num1 = 30,
  Num2 = 31,
  Num3 = 32,
  Num4 = 33,
  Num5 = 34,
  /** SDL_SCANCODE_U — use consumable from active action bar (Subterra). */
  U = 24,
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
