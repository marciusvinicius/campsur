#include "subterra_item_light_runtime.h"

#include "components.h"
#include "gameplay_tags.h"
#include "subterra_item_light.h"
#include "subterra_loadout.h"
#include "subterra_session.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>
#include <vector>

namespace subterra {
namespace {

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

void setState(criogenio::World &world, criogenio::ecs::EntityId id, const char *sourceKind,
              std::vector<ItemLightEmitterEntry> &&emitters) {
  ItemLightEmitterState *state = world.GetComponent<ItemLightEmitterState>(id);
  if (!state)
    state = &world.AddComponent<ItemLightEmitterState>(id);
  state->enabled = true;
  state->source_kind = sourceKind ? sourceKind : "";
  state->emitters = std::move(emitters);
}

} // namespace

criogenio::SerializedComponent ItemLightEmitterState::Serialize() const {
  return {"ItemLightEmitterState",
          {{"enabled", enabled ? 1.f : 0.f},
           {"source_kind", source_kind},
           {"emitter_count", static_cast<float>(emitters.size())}}};
}

void ItemLightEmitterState::Deserialize(const criogenio::SerializedComponent &data) {
  if (auto it = data.fields.find("enabled"); it != data.fields.end())
    enabled = criogenio::GetFloat(it->second) > 0.5f;
  if (auto it = data.fields.find("source_kind"); it != data.fields.end())
    source_kind = criogenio::GetString(it->second);
  emitters.clear();
}

void SubterraItemLightSyncTick(SubterraSession &session, float /*dt*/) {
  if (!session.world)
    return;
  auto &world = *session.world;
  std::unordered_set<criogenio::ecs::EntityId> activeEmitterOwners;

  auto holderIds =
      world.GetEntitiesWith<criogenio::Inventory, SubterraLoadout, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : holderIds) {
    auto *load = world.GetComponent<SubterraLoadout>(id);
    if (!load)
      continue;

    std::unordered_set<std::string> seen;
    std::vector<ItemLightEmitterEntry> emitters;
    auto maybeAppend = [&](const std::string &rawId) {
      if (rawId.empty())
        return;
      std::string key = lowerAscii(rawId);
      if (!seen.insert(key).second)
        return;
      ItemLightEmission em{};
      if (!SubterraItemLight::Lookup(key, em) || !em.valid)
        return;
      ItemLightEmitterEntry entry;
      entry.source_item_prefab = key;
      entry.radius = em.radius;
      entry.intensity = em.intensity;
      entry.r = em.r;
      entry.g = em.g;
      entry.b = em.b;
      emitters.push_back(entry);
    };

    int abi = load->active_action_slot;
    if (abi < 0 || abi >= kActionBarSlots)
      abi = 0;
    maybeAppend(load->action_bar[static_cast<size_t>(abi)]);
    for (const std::string &eq : load->equipment)
      maybeAppend(eq);

    if (!emitters.empty()) {
      setState(world, id, "carried", std::move(emitters));
      activeEmitterOwners.insert(id);
    }
  }

  auto pickupIds = world.GetEntitiesWith<criogenio::WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : pickupIds) {
    auto *pk = world.GetComponent<criogenio::WorldPickup>(id);
    if (!pk || pk->item_id.empty() || pk->count <= 0)
      continue;

    ItemLightEmission em{};
    if (!SubterraItemLight::Lookup(pk->item_id, em) || !em.valid)
      continue;
    std::vector<ItemLightEmitterEntry> emitters;
    ItemLightEmitterEntry entry;
    entry.source_item_prefab = lowerAscii(pk->item_id);
    entry.radius = em.radius;
    entry.intensity = em.intensity;
    entry.r = em.r;
    entry.g = em.g;
    entry.b = em.b;
    emitters.push_back(entry);
    setState(world, id, "world_pickup", std::move(emitters));
    activeEmitterOwners.insert(id);
  }

  auto emitterIds = world.GetEntitiesWith<ItemLightEmitterState>();
  for (criogenio::ecs::EntityId id : emitterIds) {
    if (activeEmitterOwners.count(id))
      continue;
    world.RemoveComponent<ItemLightEmitterState>(id);
  }
}

} // namespace subterra
