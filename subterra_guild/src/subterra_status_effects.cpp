#include "subterra_status_effects.h"
#include "map_events.h"
#include "subterra_json_strip.h"
#include "subterra_session.h"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>

namespace subterra {

namespace {

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::uint8_t statTargetsToMask(const std::vector<std::string> &targets) {
  std::uint8_t m = 0;
  for (const std::string &t : targets) {
    std::string ls = lowerAscii(t);
    if (ls == "health")
      m |= kTargetMaskHealth;
    else if (ls == "stamina")
      m |= kTargetMaskStamina;
    else if (ls == "food")
      m |= kTargetMaskFood;
  }
  return m;
}

bool maskHas(std::uint8_t m, std::uint8_t bit) { return (m & bit) != 0; }

StatusLineKind parseLineKind(const std::string &s) {
  std::string ls = lowerAscii(s);
  if (ls == "increase")
    return StatusLineKind::Increase;
  if (ls == "decrease")
    return StatusLineKind::Decrease;
  if (ls == "prevent_regen")
    return StatusLineKind::PreventRegen;
  if (ls == "remove_all_debuffs")
    return StatusLineKind::RemoveAllDebuffs;
  return StatusLineKind::None;
}

int findActiveStatus(PlayerVitals &v, const std::string &flag) {
  for (int i = 0; i < static_cast<int>(v.active_statuses.size()); ++i) {
    if (v.active_statuses[static_cast<size_t>(i)].flag == flag)
      return i;
  }
  return -1;
}

void removeActiveStatusByFlag(PlayerVitals &v, const std::string &flag) {
  int idx = findActiveStatus(v, flag);
  if (idx < 0)
    return;
  v.active_statuses.erase(v.active_statuses.begin() + idx);
}

bool buildInstanceFromDef(const SubterraStatusRegistry &reg, int def_index, ActiveStatusInstance &out) {
  if (def_index < 0 || def_index >= static_cast<int>(reg.defs.size()))
    return false;
  const StatusEffectDef &def = reg.defs[static_cast<size_t>(def_index)];
  double max_dur = 0.1;
  bool has_rate_line = false;
  for (const auto &ln : def.lines) {
    if ((ln.kind == StatusLineKind::Increase || ln.kind == StatusLineKind::Decrease) &&
        ln.duration_sec == kStatusEffectRateModeDuration) {
      has_rate_line = true;
      continue;
    }
    if (ln.kind == StatusLineKind::Increase || ln.kind == StatusLineKind::Decrease) {
      if (ln.duration_sec > max_dur)
        max_dur = ln.duration_sec;
    }
  }
  if (has_rate_line)
    max_dur = std::max(max_dur, 86400.0);
  if (max_dur < 0.05)
    max_dur = 0.1;
  out = ActiveStatusInstance{};
  out.def_index = def_index;
  out.flag = def.player_effect_flag;
  out.time_remaining = max_dur;
  out.time_is_inert = has_rate_line;
  for (const auto &ln : def.lines) {
    switch (ln.kind) {
    case StatusLineKind::Increase:
    case StatusLineKind::Decrease: {
      double dur = ln.duration_sec;
      if (dur != kStatusEffectRateModeDuration && dur <= 0)
        dur = max_dur;
      StatusInstanceLine sl;
      sl.kind = ln.kind;
      sl.target_mask = ln.target_mask;
      sl.amount_total = ln.amount_total;
      sl.duration_sec = dur;
      out.lines.push_back(sl);
      break;
    }
    case StatusLineKind::PreventRegen: {
      std::uint8_t mask = ln.target_mask;
      if (maskHas(mask, kTargetMaskHealth))
        out.prevent_regen_health = true;
      if (maskHas(mask, kTargetMaskStamina))
        out.prevent_regen_stamina = true;
      if (maskHas(mask, kTargetMaskFood))
        out.prevent_regen_food = true;
      break;
    }
    case StatusLineKind::RemoveAllDebuffs:
      out.lines.push_back(StatusInstanceLine{.kind = StatusLineKind::RemoveAllDebuffs});
      break;
    default:
      break;
    }
  }
  return true;
}

void applyStatusOnApplyHooks(SubterraSession &session, PlayerVitals &vitals, ActiveStatusInstance &inst) {
  for (auto &ln : inst.lines) {
    if (ln.kind == StatusLineKind::RemoveAllDebuffs && !ln.remove_debuffs_done) {
      SubterraStatusRemoveAllDebuffs(session, vitals);
      ln.remove_debuffs_done = true;
    }
  }
}

} // namespace

void SubterraStatusRegistry::clear() {
  defs.clear();
  flag_to_index.clear();
}

bool SubterraStatusRegistry::appendFromJsonArray(const std::string &jsonText, bool is_debuff,
                                                std::string &errOut) {
  errOut.clear();
  nlohmann::json arr;
  try {
    arr = nlohmann::json::parse(jsonText);
  } catch (const std::exception &e) {
    errOut = e.what();
    return false;
  }
  if (!arr.is_array()) {
    errOut = "root is not array";
    return false;
  }
  for (const auto &item : arr) {
    if (!item.is_object())
      continue;
    StatusEffectDef def;
    def.is_debuff = is_debuff;
    if (item.contains("player_effect_flag") && item["player_effect_flag"].is_string())
      def.player_effect_flag = item["player_effect_flag"].get<std::string>();
    def.player_effect_flag = lowerAscii(std::move(def.player_effect_flag));
    if (def.player_effect_flag.empty()) {
      errOut = "missing player_effect_flag";
      return false;
    }
    if (!item.contains("effects") || !item["effects"].is_array())
      continue;
    for (const auto &elv : item["effects"]) {
      if (!elv.is_object())
        continue;
      StatusEffectLineDef ln;
      if (elv.contains("type") && elv["type"].is_string())
        ln.kind = parseLineKind(elv["type"].get<std::string>());
      if (ln.kind == StatusLineKind::None)
        continue;
      float amt = 0.f;
      if (elv.contains("amount")) {
        const auto &av = elv["amount"];
        if (av.is_number_float())
          amt = av.get<float>();
        else if (av.is_number_integer())
          amt = static_cast<float>(av.get<int>());
      }
      ln.amount_total = amt;
      bool has_dur = false;
      double dur_ms = 0.;
      if (elv.contains("duration")) {
        const auto &dv = elv["duration"];
        if (dv.is_number_float()) {
          dur_ms = dv.get<double>();
          has_dur = true;
        } else if (dv.is_number_integer()) {
          dur_ms = static_cast<double>(dv.get<int>());
          has_dur = true;
        }
      }
      if (has_dur && dur_ms > 0)
        ln.duration_sec = dur_ms / 1000.0;
      else if (ln.kind == StatusLineKind::Increase || ln.kind == StatusLineKind::Decrease)
        ln.duration_sec = kStatusEffectRateModeDuration;
      if (elv.contains("target")) {
        const auto &tv = elv["target"];
        std::vector<std::string> tgts;
        if (tv.is_array()) {
          for (const auto &te : tv) {
            if (te.is_string())
              tgts.push_back(te.get<std::string>());
          }
        } else if (tv.is_string()) {
          tgts.push_back(tv.get<std::string>());
        }
        ln.target_mask = statTargetsToMask(tgts);
      }
      def.lines.push_back(std::move(ln));
    }
    int idx = static_cast<int>(defs.size());
    defs.push_back(std::move(def));
    flag_to_index[defs[static_cast<size_t>(idx)].player_effect_flag] = idx;
  }
  return true;
}

bool SubterraLoadStatusEffectsFromPaths(const char *const *bufferPaths, size_t bufferCount,
                                        const char *const *debufferPaths, size_t debufferCount,
                                        SubterraStatusRegistry &out, std::string &errOut) {
  out.clear();
  auto loadOne = [&](const char *path, bool debuff) -> bool {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      errOut = std::string("cannot open ") + path;
      return false;
    }
    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string localErr;
    if (!out.appendFromJsonArray(SubterraStripJsonLineComments(raw), debuff, localErr)) {
      errOut = std::string(path) + ": " + localErr;
      return false;
    }
    return true;
  };
  for (size_t i = 0; i < bufferCount; ++i) {
    if (!loadOne(bufferPaths[i], false))
      return false;
  }
  for (size_t i = 0; i < debufferCount; ++i) {
    if (!loadOne(debufferPaths[i], true))
      return false;
  }
  return true;
}

void SubterraStatusRemoveAllDebuffs(SubterraSession &session, PlayerVitals &vitals) {
  if (session.statusRegistry.defs.empty())
    return;
  size_t w = 0;
  for (size_t i = 0; i < vitals.active_statuses.size(); ++i) {
    ActiveStatusInstance &a = vitals.active_statuses[i];
    int di = a.def_index;
    bool debuff =
        di >= 0 && di < static_cast<int>(session.statusRegistry.defs.size()) &&
        session.statusRegistry.defs[static_cast<size_t>(di)].is_debuff;
    if (debuff)
      continue;
    if (w != i)
      vitals.active_statuses[w] = std::move(a);
    ++w;
  }
  vitals.active_statuses.resize(w);
}

bool SubterraStatusApply(SubterraSession &session, PlayerVitals &vitals,
                         const std::string &player_effect_flag) {
  const std::string key = lowerAscii(player_effect_flag);
  auto it = session.statusRegistry.flag_to_index.find(key);
  if (it == session.statusRegistry.flag_to_index.end())
    return false;
  removeActiveStatusByFlag(vitals, key);
  ActiveStatusInstance inst;
  if (!buildInstanceFromDef(session.statusRegistry, it->second, inst))
    return false;
  applyStatusOnApplyHooks(session, vitals, inst);
  vitals.active_statuses.push_back(std::move(inst));
  return true;
}

void SubterraStatusTickPlayer(SubterraSession &session, PlayerVitals &vitals, float dt) {
  (void)session;
  if (dt <= 0.f || vitals.active_statuses.empty())
    return;
  const float dt32 = dt;
  size_t i = 0;
  while (i < vitals.active_statuses.size()) {
    ActiveStatusInstance &inst = vitals.active_statuses[i];
    if (!inst.time_is_inert)
      inst.time_remaining -= static_cast<double>(dt);
    for (auto &ln : inst.lines) {
      switch (ln.kind) {
      case StatusLineKind::Increase:
      case StatusLineKind::Decrease:
        if (ln.amount_total == 0.f)
          break;
        if (ln.duration_sec == kStatusEffectRateModeDuration) {
          float chunk = ln.amount_total * dt32;
          if (ln.kind == StatusLineKind::Decrease)
            chunk = -chunk;
          if (maskHas(ln.target_mask, kTargetMaskHealth)) {
            vitals.health += chunk;
            vitals.health = std::clamp(vitals.health, 0.f, vitals.health_max);
          }
          if (maskHas(ln.target_mask, kTargetMaskStamina)) {
            vitals.stamina += chunk;
            vitals.stamina = std::clamp(vitals.stamina, 0.f, vitals.stamina_max);
          }
          if (maskHas(ln.target_mask, kTargetMaskFood)) {
            vitals.food_satiety += chunk;
            vitals.food_satiety = std::clamp(vitals.food_satiety, 0.f, vitals.food_satiety_max);
          }
          break;
        }
        if (ln.duration_sec <= 1e-8)
          break;
        {
          float rate = (ln.kind == StatusLineKind::Increase ? 1.f : -1.f) *
                       (ln.amount_total / static_cast<float>(ln.duration_sec));
          float chunk = rate * dt32;
          float next = ln.applied + chunk;
          if (ln.kind == StatusLineKind::Increase) {
            if (next > ln.amount_total)
              chunk = ln.amount_total - ln.applied;
          } else {
            if (next < -ln.amount_total)
              chunk = -ln.amount_total - ln.applied;
          }
          ln.applied += chunk;
          if (maskHas(ln.target_mask, kTargetMaskHealth)) {
            vitals.health += chunk;
            vitals.health = std::clamp(vitals.health, 0.f, vitals.health_max);
          }
          if (maskHas(ln.target_mask, kTargetMaskStamina)) {
            vitals.stamina += chunk;
            vitals.stamina = std::clamp(vitals.stamina, 0.f, vitals.stamina_max);
          }
          if (maskHas(ln.target_mask, kTargetMaskFood)) {
            vitals.food_satiety += chunk;
            vitals.food_satiety = std::clamp(vitals.food_satiety, 0.f, vitals.food_satiety_max);
          }
        }
        break;
      default:
        break;
      }
    }
    if (inst.time_remaining <= 0) {
      vitals.active_statuses.erase(vitals.active_statuses.begin() +
                                   static_cast<std::ptrdiff_t>(i));
      continue;
    }
    ++i;
  }
}

void SubterraStatusRefreshStarving(SubterraSession &session, PlayerVitals &vitals) {
  if (session.statusRegistry.flag_to_index.find("starving") == session.statusRegistry.flag_to_index.end())
    return;
  if (vitals.food_satiety > 0.f) {
    removeActiveStatusByFlag(vitals, "starving");
    return;
  }
  if (findActiveStatus(vitals, "starving") < 0)
    SubterraStatusApply(session, vitals, "starving");
}

bool SubterraStatusPlayerHasActiveIncreaseLine(const PlayerVitals &vitals,
                                               const SubterraStatusRegistry &reg, StatusStatTarget t) {
  std::uint8_t tmask = 0;
  switch (t) {
  case StatusStatTarget::Health:
    tmask = kTargetMaskHealth;
    break;
  case StatusStatTarget::Stamina:
    tmask = kTargetMaskStamina;
    break;
  case StatusStatTarget::Food:
    tmask = kTargetMaskFood;
    break;
  default:
    return false;
  }
  for (const auto &inst : vitals.active_statuses) {
    for (const auto &ln : inst.lines) {
      if (ln.kind != StatusLineKind::Increase || ln.amount_total == 0.f)
        continue;
      if (maskHas(ln.target_mask, tmask))
        return true;
    }
  }
  (void)reg;
  return false;
}

bool SubterraStatusPreventRegenHealth(const PlayerVitals &vitals) {
  for (const auto &a : vitals.active_statuses) {
    if (a.prevent_regen_health)
      return true;
  }
  return false;
}

bool SubterraStatusPreventRegenStamina(const PlayerVitals &vitals) {
  for (const auto &a : vitals.active_statuses) {
    if (a.prevent_regen_stamina)
      return true;
  }
  return false;
}

bool SubterraStatusPreventRegenFood(const PlayerVitals &vitals) {
  for (const auto &a : vitals.active_statuses) {
    if (a.prevent_regen_food)
      return true;
  }
  return false;
}

} // namespace subterra
