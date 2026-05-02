#pragma once

#include "ecs_core.h"

#include <string>

namespace criogenio {
class World;
}

namespace subterra {

struct SubterraSession;

struct SubterraInputBinding {
  int keyboard_primary = -1;
  int keyboard_secondary = -1;
  int mouse_primary = -1;
};

struct SubterraInputRuntime {
  float left_stick_deadzone = 0.22f;
  float right_stick_deadzone = 0.22f;
  float gamepad_mouse_speed = 2200.f;
  float keyboard_mouse_speed = 1200.f;

  SubterraInputBinding respawn;
  SubterraInputBinding inventory_toggle;
  SubterraInputBinding cancel_ui;
  SubterraInputBinding nav_up;
  SubterraInputBinding nav_down;
  SubterraInputBinding use_item;
  SubterraInputBinding interact;
  SubterraInputBinding confirm;
  SubterraInputBinding menu_left;
  SubterraInputBinding menu_right;
  SubterraInputBinding melee_attack;
  SubterraInputBinding spell_cast;
  SubterraInputBinding move_left;
  SubterraInputBinding move_right;
  SubterraInputBinding move_up;
  SubterraInputBinding move_down;
  SubterraInputBinding run;
};

void SubterraInputApplyDefaults(SubterraInputRuntime &out);

/** Load JSON (same shape as reference `input_config.json`). Returns false if missing/invalid. */
bool SubterraTryLoadInputConfigFromPath(const std::string &path, SubterraInputRuntime &out);

constexpr float kSubterraInputConfigReloadIntervalSec = 0.25f;
void SubterraInputHotReloadTick(SubterraSession &session, float dt);

bool SubterraInputBindingDown(const SubterraInputRuntime &in, const SubterraInputBinding &b);
bool SubterraInputBindingPressed(const SubterraInputRuntime &in, const SubterraInputBinding &b);

bool SubterraInputActionDown(const SubterraSession &session, const char *action);
bool SubterraInputActionPressed(const SubterraSession &session, const char *action);

/** For `MovementSystem` override: returns true when entity has `PlayerTag` and axes were written. */
bool SubterraPollPlayerMovementAxes(SubterraSession &session, criogenio::World &world,
                                    criogenio::ecs::EntityId id, float *outDx, float *outDy);

} // namespace subterra
