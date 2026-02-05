#include "input.h"
#include <SDL3/SDL.h>

namespace criogenio {

namespace {
constexpr int kMaxScancodes = 512;
static bool s_prevKeys[kMaxScancodes] = {};
} // namespace

bool Input::IsKeyDown(int key) {
  int numkeys = 0;
  const bool* state = SDL_GetKeyboardState(&numkeys);
  if (!state || key < 0 || key >= numkeys)
    return false;
  return state[key];
}

bool Input::IsKeyPressed(int key) {
  int numkeys = 0;
  const bool* state = SDL_GetKeyboardState(&numkeys);
  if (!state || key < 0 || key >= numkeys)
    return false;
  return state[key] && !s_prevKeys[key];
}

bool Input::IsMouseDown(int button) {
  float x = 0, y = 0;
  SDL_MouseButtonFlags state = SDL_GetMouseState(&x, &y);
  if (button == 0)
    return (state & SDL_BUTTON_LEFT) != 0;
  if (button == 1)
    return (state & SDL_BUTTON_RIGHT) != 0;
  if (button == 2)
    return (state & SDL_BUTTON_MIDDLE) != 0;
  return false;
}

Vec2 Input::GetMousePosition() {
  float x = 0, y = 0;
  SDL_GetMouseState(&x, &y);
  return Vec2{x, y};
}

void Input::EndFrame() {
  int numkeys = 0;
  const bool* state = SDL_GetKeyboardState(&numkeys);
  if (state) {
    for (int i = 0; i < numkeys && i < kMaxScancodes; ++i)
      s_prevKeys[i] = state[i];
  }
}

} // namespace criogenio
