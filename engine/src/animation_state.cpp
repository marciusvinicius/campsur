#pragma once
#include "animation_state.h"
#include <string>

namespace criogenio {

// TODO:(maraujo) move this to a cpp file
std::string anim_state_to_string(AnimState state) {
  std::string result;
#pragma warning(push)
#pragma warning(default : 4061)
  switch (state) {
  case AnimState::IDLE:
    result = "IDLE";
    break;
  case AnimState::WALKING:
    result = "WALKING";
    break;
  case AnimState::RUNNING:
    result = "RUNNING";
    break;
  case AnimState::JUMPING:
    result = "JUMPING";
    break;
  case AnimState::ATTACKING:
    result = "ATTACKING";
    break;
  }
#pragma warning(pop)
  return result;
};

std::string direction_to_string(Direction direction) {
  std::string result;
#pragma warning(push)
#pragma warning(default : 4061)
  switch (direction) {
  case Direction::UP:
    result = "up";
    break;
  case Direction::DOWN:
    result = "down";
    break;
  case Direction::LEFT:
    result = "left";
    break;
  case Direction::RIGHT:
    result = "right";
    break;
  }
#pragma warning(pop)

  return result;
};

} // namespace criogenio