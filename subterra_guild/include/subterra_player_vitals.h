#pragma once

#include "components.h"
#include "serialization.h"
#include "subterra_status_types.h"

#include <vector>

namespace subterra {

/** Health / stamina / food satiety (reference `PlayerState` vitals). */
class PlayerVitals : public criogenio::Component {
public:
  float health = 100.f;
  float health_max = 100.f;
  float stamina = 100.f;
  float stamina_max = 100.f;
  float food_satiety = 100.f;
  float food_satiety_max = 100.f;
  bool sprint_locked_until_full_stamina = false;
  bool dead = false;
  /** Runtime buff/debuff instances (not serialized; rebuilt from host in MP later). */
  std::vector<ActiveStatusInstance> active_statuses;

  std::string TypeName() const override { return "PlayerVitals"; }
  criogenio::SerializedComponent Serialize() const override;
  void Deserialize(const criogenio::SerializedComponent &data) override;
};

void SubterraInitPlayerVitals(PlayerVitals &v, const struct SubterraWorldRules &rules);

float SubterraStaminaMaxForFood(const SubterraWorldRules &rules, float food_satiety);

} // namespace subterra
