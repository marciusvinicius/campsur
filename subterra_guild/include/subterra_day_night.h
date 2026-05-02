#pragma once

#include "graphics_types.h"
#include <string>

namespace criogenio {
class Renderer;
}

namespace subterra {

struct SubterraSession;

/** Matches reference Subterra Guild: `day_time` 0=midnight, 0.5=noon, wraps at 1. */
struct SubterraDayNight {
  double dayTime = 0.35;
  /** Seconds for a full 24h cycle; 0 = pause automatic advance. */
  double dayDuration = 300.0;
  /** Width of each named phase bucket (dawn/noon/dusk/midnight); clamped to [0.05, 0.45]. */
  double dayPhaseSize = 0.25;
  int worldDay = 1;
  /** 0 = cave/interior (no sky tint), 1 = full open sky. Lerped from roof tiles when `outdoorFromRoof`. */
  float outdoorFactor = 1.f;
  /** When true, `SubterraUpdateOutdoorFactor` drives `outdoorFactor` from roof overlap (reference). */
  bool outdoorFromRoof = true;
  /** Monotonic seconds for subtle torch flicker. */
  float effectTime = 0.f;
};

/** ~1 at noon, ~0 at midnight (sin curve). */
float SubterraDayAmbientBrightness(double dayTime);

/** Full-screen tint from reference `day_night_overlay_color`. */
criogenio::Color SubterraDayNightOverlayColor(double dayTime);

/** Advance `dayTime` from `dt` when `dayDuration` > 0. */
void SubterraAdvanceDayNight(SubterraDayNight &dn, float dt);

/** Lerp `outdoorFactor` toward 0 under roof tiles, 1 in open air (reference `update` / `local_player_under_roof`). */
void SubterraUpdateOutdoorFactor(SubterraSession &session, float dt);

/** Clamp phase size to editor/debug range. */
void SubterraClampDayPhaseSize(SubterraDayNight &dn);

/** Phase label: midnight | dawn | noon | dusk (same buckets as reference `day_time_phase_index_for_size`). */
std::string SubterraDayPhaseName(double dayTime, double phaseSize);

/** 24h HUD clock "HH:MM" from normalized day time. */
std::string SubterraDayTimeClockLabel(double dayTime);

/**
 * Composites atmosphere after the world draw: multiply dim (cave + night), sky tint when outdoors,
 * subtle additive lantern halo at low sun (matches reference feel; not a full light-map RT).
 */
void SubterraRenderAtmosphere(SubterraSession &session, criogenio::Renderer &renderer);

} // namespace subterra
