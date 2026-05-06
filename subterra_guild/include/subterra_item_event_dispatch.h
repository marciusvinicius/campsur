#pragma once

#include "json.hpp"

#include <string>
#include <string_view>

namespace subterra {

struct SubterraSession;

struct ItemEventBuildContext {
  std::string source_item_prefab;
  int source_entity_id = 0;
  float source_x = 0.f;
  float source_y = 0.f;
  float light_radius = 0.f;
  float light_intensity = 0.f;
  unsigned char light_r = 255;
  unsigned char light_g = 180;
  unsigned char light_b = 80;
  std::string target_type;
  int target_id = 0;
  nlohmann::json dispatch_params = nlohmann::json::object();
};

bool SubterraItemEventBuildData(std::string_view provider, const ItemEventBuildContext &ctx,
                                nlohmann::json &out);
void SubterraItemEventDispatchTick(SubterraSession &session, float dt);

} // namespace subterra
