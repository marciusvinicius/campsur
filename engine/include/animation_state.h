#pragma once
#include <string>

namespace criogenio {

// REFACTORŸ:(maraujo) implement state machine for animation states and make
// this names better
/*

enum class Color { Red, Green, Blue };
// C++26 Reflection usage (Conceptual based on P2996)
constexpr auto info = ^Color; // Reflect on the type
constexpr auto members = std::meta::enumerators_of(info); // Get all enumerators
// You can now iterate 'members' to get names and values at compile-time

*/
enum class AnimState { IDLE, WALKING, RUNNING, JUMPING, ATTACKING, Count };

enum class Direction { UP, DOWN, LEFT, RIGHT, Count };

std::string anim_state_to_string(AnimState state);
std::string direction_to_string(Direction direction);

enum AIBrainState {
  FRIENDLY,
  FRIENDLY_PATROL,
  ENEMY,
  ENEMY_AGREESSIVE,
  ENEMY_PATROL,
};
} // namespace criogenio