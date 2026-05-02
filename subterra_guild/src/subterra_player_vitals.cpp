#include "subterra_player_vitals.h"
#include "subterra_world_rules.h"

namespace subterra {

namespace {

constexpr float kStarvingStaminaMaxFraction = 0.35f;

} // namespace

criogenio::SerializedComponent PlayerVitals::Serialize() const {
  return {"PlayerVitals",
          {{"health", health},
           {"health_max", health_max},
           {"stamina", stamina},
           {"stamina_max", stamina_max},
           {"food_satiety", food_satiety},
           {"food_satiety_max", food_satiety_max},
           {"sprint_locked", sprint_locked_until_full_stamina ? 1.f : 0.f},
           {"dead", dead ? 1.f : 0.f}}};
}

void PlayerVitals::Deserialize(const criogenio::SerializedComponent &data) {
  if (auto it = data.fields.find("health"); it != data.fields.end())
    health = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("health_max"); it != data.fields.end())
    health_max = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("stamina"); it != data.fields.end())
    stamina = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("stamina_max"); it != data.fields.end())
    stamina_max = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("food_satiety"); it != data.fields.end())
    food_satiety = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("food_satiety_max"); it != data.fields.end())
    food_satiety_max = criogenio::GetFloat(it->second);
  if (auto it = data.fields.find("sprint_locked"); it != data.fields.end())
    sprint_locked_until_full_stamina = criogenio::GetFloat(it->second) > 0.5f;
  if (auto it = data.fields.find("dead"); it != data.fields.end())
    dead = criogenio::GetFloat(it->second) > 0.5f;
}

void SubterraInitPlayerVitals(PlayerVitals &v, const SubterraWorldRules &rules) {
  v.health_max = rules.health_max;
  v.stamina_max = rules.stamina_max;
  v.food_satiety_max = rules.food_max;
  v.health = v.health_max;
  v.stamina = rules.stamina_max;
  v.food_satiety = v.food_satiety_max;
  v.sprint_locked_until_full_stamina = false;
  v.dead = false;
}

float SubterraStaminaMaxForFood(const SubterraWorldRules &rules, float food_satiety) {
  if (food_satiety <= 0.f)
    return rules.stamina_max * kStarvingStaminaMaxFraction;
  return rules.stamina_max;
}

} // namespace subterra
