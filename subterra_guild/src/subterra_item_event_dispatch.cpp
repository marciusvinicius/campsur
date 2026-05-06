#include "subterra_item_event_dispatch.h"

#include "components.h"
#include "map_events.h"
#include "subterra_item_light.h"
#include "subterra_loadout.h"
#include "subterra_session.h"
#include "subterra_components.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace subterra {
namespace {

struct ActiveLightEmitter {
  std::string prefab;
  int source_entity_id = 0;
  float x = 0.f;
  float y = 0.f;
  ItemLightEmission light;
};

using DataProviderFn = std::function<void(const ItemEventBuildContext &, nlohmann::json &)>;

float dist2(float ax, float ay, float bx, float by) {
  const float dx = ax - bx;
  const float dy = ay - by;
  return dx * dx + dy * dy;
}

std::string pairKey(const ActiveLightEmitter &em, const std::string &event,
                    const std::string &targetType, int targetId) {
  return std::to_string(em.source_entity_id) + "|" + em.prefab + "|" + event + "|" + targetType +
         "|" + std::to_string(targetId);
}

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

void defaultLightPayload(const ItemEventBuildContext &ctx, nlohmann::json &out) {
  out = nlohmann::json::object();
  out["light_radius"] = ctx.light_radius;
  out["light_intensity"] = ctx.light_intensity;
  out["light_color"] = nlohmann::json::array({ctx.light_r, ctx.light_g, ctx.light_b});
  out["source_item_prefab"] = ctx.source_item_prefab;
  out["source_entity_id"] = ctx.source_entity_id;
  out["target_type"] = ctx.target_type;
  out["target_id"] = ctx.target_id;
}

const std::unordered_map<std::string, DataProviderFn> &providerRegistry() {
  static const std::unordered_map<std::string, DataProviderFn> kReg = {
      {"energy_torch_activated", defaultLightPayload},
      {"energy_torch_property_changed", defaultLightPayload},
      {"energy_torch_light_payload", defaultLightPayload},
  };
  return kReg;
}

void collectActiveEmitters(SubterraSession &session, std::vector<ActiveLightEmitter> &out) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *ptr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!ptr)
    return;
  const float pcx = ptr->x + static_cast<float>(session.playerW) * 0.5f;
  const float pcy = ptr->y + static_cast<float>(session.playerH) * 0.5f;

  std::unordered_set<std::string> seenCarried;
  auto appendEmitter = [&](const std::string &prefab, int sourceId, float ex, float ey) {
    if (prefab.empty())
      return;
    ItemLightEmission em;
    if (!SubterraItemLight::Lookup(prefab, em))
      return;
    ActiveLightEmitter a;
    a.prefab = lowerAscii(prefab);
    a.source_entity_id = sourceId;
    a.x = ex;
    a.y = ey;
    a.light = em;
    out.push_back(std::move(a));
  };

  if (auto *load = session.world->GetComponent<SubterraLoadout>(session.player)) {
    int abi = load->active_action_slot;
    if (abi < 0 || abi >= kActionBarSlots)
      abi = 0;
    const std::string activeItem = load->action_bar[static_cast<size_t>(abi)];
    if (!activeItem.empty() && seenCarried.insert(lowerAscii(activeItem)).second)
      appendEmitter(activeItem, static_cast<int>(session.player), pcx, pcy);
    for (const std::string &eq : load->equipment) {
      const std::string key = lowerAscii(eq);
      if (eq.empty() || !seenCarried.insert(key).second)
        continue;
      appendEmitter(eq, static_cast<int>(session.player), pcx, pcy);
    }
  }

  auto pickups = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : pickups) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr || pk->item_id.empty())
      continue;
    appendEmitter(pk->item_id, static_cast<int>(id), tr->x + pk->width * 0.5f,
                  tr->y + pk->height * 0.5f);
  }
}

} // namespace

bool SubterraItemEventBuildData(std::string_view provider, const ItemEventBuildContext &ctx,
                                nlohmann::json &out) {
  const std::string key = lowerAscii(std::string(provider));
  const auto &reg = providerRegistry();
  auto it = reg.find(key);
  if (it == reg.end()) {
    defaultLightPayload(ctx, out);
    return false;
  }
  it->second(ctx, out);
  return true;
}

void SubterraItemEventDispatchTick(SubterraSession &session, float dt) {
  if (!session.world)
    return;
  session.itemEventDispatchClockSec += std::max(0.f, dt);

  std::vector<ActiveLightEmitter> emitters;
  collectActiveEmitters(session, emitters);
  std::unordered_set<std::string> insideNow;

  auto dispatchForTarget = [&](const ActiveLightEmitter &em, const std::string &targetType, int targetId,
                               const std::vector<ItemEventDispatchDef> &defs,
                               const nlohmann::json &dispatchParams = nlohmann::json::object()) {
    for (const ItemEventDispatchDef &def : defs) {
      if (def.event.empty())
        continue;
      const std::string pk = pairKey(em, def.event, targetType, targetId);
      insideNow.insert(pk);
      const bool wasInside = session.itemEventPairsInside.count(pk) != 0;
      const std::string when = def.event_trigger_when.empty() ? "on_collision_enter" : def.event_trigger_when;
      bool shouldDispatch = false;
      if (when == "on_collision_enter" || when == "on_overlap_enter")
        shouldDispatch = !wasInside;
      else if (when == "on_collision_stay" || when == "on_overlap_stay")
        shouldDispatch = true;
      if (!shouldDispatch)
        continue;
      const float now = session.itemEventDispatchClockSec;
      const int cooldownMs = def.cooldown_ms > 0 ? def.cooldown_ms : 250;
      const float cooldownSec = static_cast<float>(cooldownMs) / 1000.f;
      auto cd = session.itemEventCooldownUntilSec.find(pk);
      if (cd != session.itemEventCooldownUntilSec.end() && cd->second > now)
        continue;
      session.itemEventCooldownUntilSec[pk] = now + cooldownSec;

      ItemEventBuildContext ctx;
      ctx.source_item_prefab = em.prefab;
      ctx.source_entity_id = em.source_entity_id;
      ctx.source_x = em.x;
      ctx.source_y = em.y;
      ctx.light_radius = em.light.radius;
      ctx.light_intensity = em.light.intensity;
      ctx.light_r = em.light.r;
      ctx.light_g = em.light.g;
      ctx.light_b = em.light.b;
      ctx.target_type = targetType;
      ctx.target_id = targetId;
      ctx.dispatch_params = def.params.is_object() ? def.params : dispatchParams;

      MapEventPayload p;
      p.event_trigger = def.event;
      p.event_type = "item_dispatch";
      p.event_id = def.event;
      p.manual = false;
      SubterraItemEventBuildData(def.event_action_get_data, ctx, p.event_data);
      session.mapEvents.dispatch(p);
    }
  };

  for (const ActiveLightEmitter &em : emitters) {
    const std::vector<ItemEventDispatchDef> *defs = nullptr;
    if (!SubterraItemLight::TryGetEventDispatchDefs(em.prefab, defs) || !defs || defs->empty())
      continue;

    const float rr2 = em.light.radius * em.light.radius;

    for (const auto &it : session.tiledInteractables) {
      float tx = it.is_point ? it.x : it.x + it.w * 0.5f;
      float ty = it.is_point ? it.y : it.y + it.h * 0.5f;
      if (dist2(em.x, em.y, tx, ty) > rr2)
        continue;
      dispatchForTarget(em, "interactable", it.tiled_object_id, *defs);
    }

    auto mobIds = session.world->GetEntitiesWith<MobTag, criogenio::Transform>();
    for (criogenio::ecs::EntityId mobId : mobIds) {
      auto *tr = session.world->GetComponent<criogenio::Transform>(mobId);
      if (!tr)
        continue;
      const float mx = tr->x + static_cast<float>(session.playerW) * tr->scale_x * 0.5f;
      const float my = tr->y + static_cast<float>(session.playerH) * tr->scale_y * 0.5f;
      if (dist2(em.x, em.y, mx, my) > rr2)
        continue;
      dispatchForTarget(em, "mob", static_cast<int>(mobId), *defs);
    }
  }

  session.itemEventPairsInside = std::move(insideNow);
  for (auto it = session.itemEventCooldownUntilSec.begin();
       it != session.itemEventCooldownUntilSec.end();) {
    if (it->second < session.itemEventDispatchClockSec - 1.0f)
      it = session.itemEventCooldownUntilSec.erase(it);
    else
      ++it;
  }
}

} // namespace subterra
