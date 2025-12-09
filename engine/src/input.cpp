#include "input.h"

namespace criogenio {
bool Input::IsKeyDown(int key) { return ::IsKeyDown(key); }

bool Input::IsKeyPressed(int key) { return ::IsKeyPressed(key); }

bool Input::IsMouseDown(int button) { return ::IsMouseButtonDown(button); }

Vector2 Input::GetMousePosition() { return ::GetMousePosition(); }
} // namespace criogenio
