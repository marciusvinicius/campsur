#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace subterra {

constexpr std::uint8_t kTargetMaskHealth = 1 << 0;
constexpr std::uint8_t kTargetMaskStamina = 1 << 1;
constexpr std::uint8_t kTargetMaskFood = 1 << 2;

enum class StatusStatTarget : std::uint8_t { None = 0, Health = 1, Stamina = 2, Food = 3 };

enum class StatusLineKind : std::uint8_t {
  None = 0,
  Increase = 1,
  Decrease = 2,
  PreventRegen = 3,
  RemoveAllDebuffs = 4,
};

struct StatusEffectLineDef {
  StatusLineKind kind = StatusLineKind::None;
  std::uint8_t target_mask = 0;
  float amount_total = 0.f;
  double duration_sec = 0.;
};

struct StatusEffectDef {
  std::string player_effect_flag;
  bool is_debuff = false;
  std::vector<StatusEffectLineDef> lines;
};

struct StatusInstanceLine {
  StatusLineKind kind = StatusLineKind::None;
  std::uint8_t target_mask = 0;
  float amount_total = 0.f;
  double duration_sec = 0.;
  float applied = 0.f;
  bool remove_debuffs_done = false;
};

struct ActiveStatusInstance {
  std::string flag;
  int def_index = -1;
  double time_remaining = 0.;
  bool time_is_inert = false;
  std::vector<StatusInstanceLine> lines;
  bool prevent_regen_health = false;
  bool prevent_regen_stamina = false;
  bool prevent_regen_food = false;
};

} // namespace subterra
