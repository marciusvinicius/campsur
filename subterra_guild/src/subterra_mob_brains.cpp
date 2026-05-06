#include "subterra_mob_brains.h"

#include "components.h"
#include "subterra_components.h"
#include "subterra_session.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace subterra {
namespace {

using BrainFn = std::function<void(SubterraSession &, criogenio::ecs::EntityId, criogenio::AIController &,
                                   nlohmann::json &, float)>;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

float jsonSpeed(const nlohmann::json &state, float fallback) {
  if (!state.is_object())
    return fallback;
  auto it = state.find("speed");
  if (it == state.end())
    return fallback;
  if (it->is_number_float())
    return it->get<float>();
  if (it->is_number_integer())
    return static_cast<float>(it->get<int>());
  return fallback;
}

bool jsonBool(const nlohmann::json &state, const char *key, bool fallback) {
  if (!state.is_object() || !key)
    return fallback;
  auto it = state.find(key);
  if (it == state.end())
    return fallback;
  if (it->is_boolean())
    return it->get<bool>();
  if (it->is_number_integer())
    return it->get<int>() != 0;
  return fallback;
}

void applyHiddenState(SubterraSession &session, criogenio::ecs::EntityId id, nlohmann::json &state) {
  if (!session.world)
    return;
  auto *tr = session.world->GetComponent<criogenio::Transform>(id);
  if (!tr)
    return;
  const bool hidden = jsonBool(state, "hidden", false);
  if (!state.contains("base_scale_x"))
    state["base_scale_x"] = tr->scale_x;
  if (!state.contains("base_scale_y"))
    state["base_scale_y"] = tr->scale_y;
  const float baseX = state["base_scale_x"].is_number() ? state["base_scale_x"].get<float>() : 1.f;
  const float baseY = state["base_scale_y"].is_number() ? state["base_scale_y"].get<float>() : 1.f;
  tr->scale_x = hidden ? 0.0001f : baseX;
  tr->scale_y = hidden ? 0.0001f : baseY;
}

void brainSimple(SubterraSession &, criogenio::ecs::EntityId id, criogenio::AIController &ai,
                 nlohmann::json &state, float) {
  ai.brainState = criogenio::AIBrainState::ENEMY_PATROL;
  ai.entityTarget = static_cast<int>(id);
  const float speed = std::max(10.f, jsonSpeed(state, 60.f));
  ai.velocity = {speed, speed};
}

void brainSimpleChasePlayer(SubterraSession &session, criogenio::ecs::EntityId id,
                            criogenio::AIController &ai, nlohmann::json &state, float) {
  if (session.player == criogenio::ecs::NULL_ENTITY ||
      (session.world && !session.world->HasEntity(session.player))) {
    brainSimple(session, id, ai, state, 0.f);
    return;
  }
  ai.brainState = criogenio::AIBrainState::ENEMY_AGREESSIVE;
  ai.entityTarget = static_cast<int>(session.player);
  const float speed = std::max(20.f, jsonSpeed(state, 90.f));
  ai.velocity = {speed, speed};
}

const std::unordered_map<std::string, BrainFn> &brainRegistry() {
  static const std::unordered_map<std::string, BrainFn> kBrains = {
      {"simple", brainSimple},
      {"simple_chase_player", brainSimpleChasePlayer},
  };
  return kBrains;
}

} // namespace

void SubterraMobBrainsTick(SubterraSession &session, float dt) {
  if (!session.world)
    return;
  std::vector<criogenio::ecs::EntityId> mobIds =
      session.world->GetEntitiesWith<MobTag, criogenio::Transform, criogenio::AIController>();
  const auto &reg = brainRegistry();

  for (criogenio::ecs::EntityId id : mobIds) {
    nlohmann::json &state = session.mobEntityDataByEntity[id];
    if (!state.is_object())
      state = nlohmann::json::object();
    if (!state.contains("brain_type"))
      state["brain_type"] = "simple";
    applyHiddenState(session, id, state);
    auto *ai = session.world->GetComponent<criogenio::AIController>(id);
    if (!ai)
      continue;
    std::string brainType = lowerAscii(state["brain_type"].is_string()
                                           ? state["brain_type"].get<std::string>()
                                           : std::string("simple"));
    auto it = reg.find(brainType);
    if (it == reg.end())
      it = reg.find("simple");
    it->second(session, id, *ai, state, dt);
  }

  for (auto it = session.mobEntityDataByEntity.begin(); it != session.mobEntityDataByEntity.end();) {
    if (!session.world->HasEntity(it->first)) {
      session.mobPrefabByEntity.erase(it->first);
      it = session.mobEntityDataByEntity.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace subterra
