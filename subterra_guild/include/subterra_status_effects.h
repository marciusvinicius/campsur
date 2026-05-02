#pragma once

#include "subterra_player_vitals.h"
#include "subterra_status_types.h"
#include "subterra_world_rules.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace subterra {

struct SubterraSession;

/** Sentinel: amount is delta per second until cleared (reference `STATUS_EFFECT_LINE_RATE_MODE`). */
constexpr double kStatusEffectRateModeDuration = -1.0;

class SubterraStatusRegistry {
public:
  std::vector<StatusEffectDef> defs;
  /** `player_effect_flag` -> index in defs */
  std::unordered_map<std::string, int> flag_to_index;

  void clear();
  /** Loads root array JSON (buffers.json / debuffers.json style). */
  bool appendFromJsonArray(const std::string &jsonText, bool is_debuff, std::string &errOut);
};

bool SubterraLoadStatusEffectsFromPaths(const char *const *bufferPaths, size_t bufferCount,
                                        const char *const *debufferPaths, size_t debufferCount,
                                        SubterraStatusRegistry &out, std::string &errOut);

bool SubterraStatusApply(SubterraSession &session, PlayerVitals &vitals,
                         const std::string &player_effect_flag);
void SubterraStatusTickPlayer(SubterraSession &session, PlayerVitals &vitals, float dt);
void SubterraStatusRefreshStarving(SubterraSession &session, PlayerVitals &vitals);

bool SubterraStatusPlayerHasActiveIncreaseLine(const PlayerVitals &vitals,
                                               const SubterraStatusRegistry &reg, StatusStatTarget t);
bool SubterraStatusPreventRegenHealth(const PlayerVitals &vitals);
bool SubterraStatusPreventRegenStamina(const PlayerVitals &vitals);
bool SubterraStatusPreventRegenFood(const PlayerVitals &vitals);

void SubterraStatusRemoveAllDebuffs(SubterraSession &session, PlayerVitals &vitals);

} // namespace subterra
