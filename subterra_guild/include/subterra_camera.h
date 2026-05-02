#pragma once

#include "graphics_types.h"
#include <string>

namespace subterra {

struct SubterraSession;

struct SubterraCameraShakeEventCfg {
  float intensity = 0.f;
  float duration = 0.f;
  float frequency = 0.f;
  float decay = 1.f;
  float dir_x = 0.f;
  float dir_y = 0.f;
};

struct SubterraCameraZoomEventCfg {
  float multiplier = 1.f;
  float speed = 4.f;
};

/** Parsed `world_config.json` → `camera` (reference `ServerConfigCamera`). */
struct SubterraCameraConfig {
  float zoom = 1.15f;
  bool follow_player = true;
  criogenio::Vec2 follow_offset{0.f, 0.f};
  bool zoom_active = false;
  SubterraCameraZoomEventCfg zoom_on_run_start;
  SubterraCameraZoomEventCfg zoom_on_run_stop;
  SubterraCameraZoomEventCfg zoom_on_run_alias;
  bool shake_active = false;
  SubterraCameraShakeEventCfg shake_on_attack;
  SubterraCameraShakeEventCfg shake_player_attack;
  SubterraCameraShakeEventCfg shake_player_hit;
};

struct SubterraCameraShakeRuntime {
  bool active = false;
  float elapsed = 0.f;
  float intensity = 0.f;
  float duration = 0.f;
  float frequency = 0.f;
  float decay = 1.f;
  float dir_x = 0.f;
  float dir_y = 0.f;
};

struct SubterraCameraRunZoomRuntime {
  float current_mul = 1.f;
  float target_mul = 1.f;
  float target_speed = 4.f;
};

struct SubterraCameraBundle {
  SubterraCameraConfig cfg;
  SubterraCameraShakeRuntime shake;
  SubterraCameraRunZoomRuntime runZoom;
  /** Applied to `Camera2D::target` each frame (shake), world pixels. */
  float shake_tx = 0.f;
  float shake_ty = 0.f;
};

void SubterraCameraResetRuntimeFromConfig(SubterraCameraBundle &cam);
void SubterraCameraApplyConfigDefaults(SubterraCameraBundle &cam);

/** Lowercased trigger: `on_attack`, `on_run_start`, `on_run_stop` (map events + manual). */
void SubterraCameraNotifyTrigger(SubterraSession &session, const std::string &triggerRaw);

void SubterraCameraTick(SubterraSession &session, float dt, bool runHeld);

/**
 * Sets camera target to player center + follow offset, base `zoom` from config × run zoom mul,
 * then applies shake offset to `target` (reference applies shake to target).
 */
void SubterraCameraApplyToView(SubterraSession &session, criogenio::Camera2D *cam, float playerCx,
                               float playerCy);

} // namespace subterra
