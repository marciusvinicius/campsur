#pragma once
namespace criogenio {
enum class AnimState { IDLE, WALKING, RUNNING, JUMPING, ATTACKING };

enum AIBrainState {
  FRIENDLY,
  FRIENDLY_PATROL,
  ENEMY,
  ENEMY_AGREESSIVE,
  ENEMY_PATROL,
};
} // namespace criogenio