#include "subterra_vitals_tick.h"
#include "animated_component.h"
#include "animation_state.h"
#include "components.h"
#include "map_events.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_player_vitals.h"
#include "subterra_session.h"
#include "subterra_status_effects.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace subterra {

namespace {

constexpr float kStaminaDrainMove = 7.f;
constexpr float kStaminaDrainRun = 36.f;
constexpr float kCampfireWarmthRadius = 96.f;
constexpr float kCampfireHealthRegen = 6.f;

static std::string normType(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    ++i;
  if (i > 0)
    s.erase(0, i);
  return s;
}

static void interactableCenter(const criogenio::TiledInteractable &it, float &cx, float &cy) {
  if (it.is_point) {
    cx = it.x;
    cy = it.y;
  } else {
    cx = it.x + it.w * 0.5f;
    cy = it.y + it.h * 0.5f;
  }
}

static void restEmissionModifiers(SubterraSession &session, float px, float py, float &outStam,
                                float &outHp, float &outFood) {
  outStam = outHp = outFood = 0.f;
  for (const criogenio::TiledInteractable &it : session.tiledInteractables) {
    const std::string kind = normType(it.interactable_type);
    SubterraInteractableRestDef rd{};
    if (!SubterraInteractableTryGetRest(kind, rd))
      continue;
    float icx = 0.f, icy = 0.f;
    interactableCenter(it, icx, icy);
    const float dx = px - icx;
    const float dy = py - icy;
    const float d2 = dx * dx + dy * dy;
    if (rd.rest_radius > 0.0001f) {
      const float r = rd.rest_radius;
      if (d2 > r * r)
        continue;
    } else {
      continue;
    }
    if (rd.stamina_per_sec > outStam)
      outStam = rd.stamina_per_sec;
    if (rd.health_per_sec > outHp)
      outHp = rd.health_per_sec;
    if (kind != "campfire" && rd.food_per_sec > outFood)
      outFood = rd.food_per_sec;
  }
}

} // namespace

void SubterraTickVitalsAndStatus(SubterraSession &session, criogenio::World &world, float dt) {
  if (dt <= 0.f || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *vit = world.GetComponent<PlayerVitals>(session.player);
  auto *tr = world.GetComponent<criogenio::Transform>(session.player);
  auto *anim = world.GetComponent<criogenio::AnimationState>(session.player);
  auto *ctrl = world.GetComponent<criogenio::Controller>(session.player);
  if (!vit || !tr)
    return;

  SubterraStatusTickPlayer(session, *vit, dt);

  if (vit->dead) {
    if (ctrl)
      ctrl->movement_frozen = true;
    return;
  }

  const float pw = static_cast<float>(session.playerW);
  const float ph = static_cast<float>(session.playerH);
  const float pcx = tr->x + pw * 0.5f;
  const float pcy = tr->y + ph * 0.5f;

  const SubterraWorldRules &wr = session.worldRules;
  float restStam = 0.f, restHp = 0.f, restFood = 0.f;
  restEmissionModifiers(session, pcx, pcy, restStam, restHp, restFood);

  if (wr.food_max > 1e-5f)
    vit->food_satiety_max = wr.food_max;
  vit->food_satiety -= wr.food_consumption_rate * dt;
  if (!SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Food))
    vit->food_satiety += restFood * dt;
  vit->food_satiety = std::clamp(vit->food_satiety, 0.f, vit->food_satiety_max);

  if (!session.statusRegistry.defs.empty())
    SubterraStatusRefreshStarving(session, *vit);

  vit->stamina_max = SubterraStaminaMaxForFood(wr, vit->food_satiety);
  if (vit->stamina > vit->stamina_max)
    vit->stamina = vit->stamina_max;

  float sat_norm = 1.f;
  if (vit->food_satiety_max > 1e-5f)
    sat_norm = vit->food_satiety / vit->food_satiety_max;
  const float stam_mult = std::max(0.32f, sat_norm);
  const float health_regen_mult = std::clamp((sat_norm - 0.15f) / 0.85f, 0.f, 1.f);

  if (vit->food_satiety <= 0.f &&
      session.statusRegistry.flag_to_index.find("starving") ==
          session.statusRegistry.flag_to_index.end()) {
    vit->health -= wr.health_consumption_rate * dt;
  }

  bool did_sprint_stamina_tick = false;
  if (anim) {
    if (anim->current == criogenio::AnimState::RUNNING) {
      did_sprint_stamina_tick = true;
      vit->stamina -= wr.stamina_consumption_rate * dt;
    } else if (anim->current == criogenio::AnimState::WALKING) {
      vit->stamina -= kStaminaDrainMove * dt;
    }
  }

  float stam_regen = wr.stamina_regen_rate * stam_mult * dt + restStam * dt;
  if (!SubterraStatusPreventRegenStamina(*vit) &&
      !SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Stamina))
    vit->stamina += stam_regen;
  vit->stamina = std::clamp(vit->stamina, 0.f, vit->stamina_max);

  if (did_sprint_stamina_tick && vit->stamina <= 1e-4f)
    vit->sprint_locked_until_full_stamina = true;
  if (vit->stamina + 1e-3f >= vit->stamina_max)
    vit->sprint_locked_until_full_stamina = false;
  if (vit->sprint_locked_until_full_stamina && anim &&
      anim->current == criogenio::AnimState::RUNNING)
    anim->current = criogenio::AnimState::WALKING;

  vit->health = std::clamp(vit->health, 0.f, vit->health_max);
  if (!SubterraStatusPreventRegenHealth(*vit) &&
      !SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Health) &&
      vit->health < vit->health_max && vit->food_satiety > 0.f) {
    vit->health += wr.health_regen_rate * health_regen_mult * dt;
    vit->health = std::clamp(vit->health, 0.f, vit->health_max);
  }
  if (restHp > 0.f && vit->health < vit->health_max &&
      !SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Health)) {
    vit->health += restHp * dt;
    vit->health = std::clamp(vit->health, 0.f, vit->health_max);
  }

  bool near_burning_campfire = false;
  float campfire_food_per_sec = 0.f;
  for (const criogenio::TiledInteractable &it : session.tiledInteractables) {
    if (normType(it.interactable_type) != "campfire")
      continue;
    const std::uint8_t fl = InteractableFlagsEffective(session, it);
    if ((fl & InteractableState::Burning) == 0)
      continue;
    float icx = 0.f, icy = 0.f;
    interactableCenter(it, icx, icy);
    const float dx = pcx - icx;
    const float dy = pcy - icy;
    if (dx * dx + dy * dy > kCampfireWarmthRadius * kCampfireWarmthRadius)
      continue;
    near_burning_campfire = true;
    float per = wr.food_regen_rate;
    SubterraInteractableRestDef rd{};
    if (SubterraInteractableTryGetRest("campfire", rd))
      per += rd.food_per_sec;
    if (per > campfire_food_per_sec)
      campfire_food_per_sec = per;
  }
  if (near_burning_campfire) {
    if (!SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Health))
      vit->health += kCampfireHealthRegen * dt;
    vit->health = std::clamp(vit->health, 0.f, vit->health_max);
    if (campfire_food_per_sec > 0.f &&
        !SubterraStatusPlayerHasActiveIncreaseLine(*vit, session.statusRegistry, StatusStatTarget::Food)) {
      vit->food_satiety += campfire_food_per_sec * dt;
      vit->food_satiety = std::clamp(vit->food_satiety, 0.f, vit->food_satiety_max);
    }
  }

  if (vit->health <= 0.f && !vit->dead) {
    vit->dead = true;
    vit->health = 0.f;
    if (ctrl)
      ctrl->movement_frozen = true;
    sessionLog(session, "You succumbed. Use console: respawn");
  } else if (vit->health > 0.f && ctrl) {
    ctrl->movement_frozen = false;
  }
}

} // namespace subterra
