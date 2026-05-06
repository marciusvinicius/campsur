#include "subterra_gameplay_actions.h"

#include "components.h"
#include "gameplay_tags.h"
#include "input.h"
#include "spawn_service.h"
#include "subterra_camera.h"
#include "subterra_player_vitals.h"
#include "subterra_session.h"

#include <cstdio>
#include <filesystem>
#include <algorithm>

namespace subterra {
namespace fs = std::filesystem;

void GameplayActionRegistry::registerAction(std::string id, ActionFn fn) {
  if (!id.empty() && fn)
    actions_[std::move(id)] = std::move(fn);
}

bool GameplayActionRegistry::runAction(const std::string &id, SubterraGameplayContext &ctx) {
  auto it = actions_.find(id);
  if (it == actions_.end()) {
    if (missingLogged_.insert(id).second)
      std::fprintf(stderr, "[GameplayAction] unknown action id: %s\n", id.c_str());
    return false;
  }
  it->second(ctx);
  return true;
}

static void runGameplayActionsJsonArray(SubterraSession &session, const MapEventPayload &p,
                                       const nlohmann::json &arr,
                                       GameplayActionRegistry &reg) {
  if (!arr.is_array())
    return;
  for (const auto &el : arr) {
    SubterraGameplayContext ctx{session, &p, nlohmann::json::object()};
    std::string id;
    if (el.is_string()) {
      id = el.get<std::string>();
    } else if (el.is_object()) {
      ctx.actionParams = el;
      if (el.contains("id") && el["id"].is_string())
        id = el["id"].get<std::string>();
    }
    if (!id.empty())
      reg.runAction(id, ctx);
  }
}

void GameplayActionRegistry::dispatchFromMapEvent(SubterraSession &session,
                                                 const MapEventPayload &p) {
  if (!p.gameplay_actions.empty()) {
    try {
      auto j = nlohmann::json::parse(p.gameplay_actions);
      if (j.is_array()) {
        runGameplayActionsJsonArray(session, p, j, *this);
        return;
      }
      if (j.is_object() && j.contains("actions") && j["actions"].is_array()) {
        runGameplayActionsJsonArray(session, p, j["actions"], *this);
        return;
      }
    } catch (...) {
      std::fprintf(stderr, "[GameplayAction] invalid gameplay_actions JSON on event %s\n",
                   p.event_id.c_str());
    }
  }

  SubterraGameplayContext ctx{session, &p, {}};
  if (!p.event_trigger.empty())
    runAction("notify_camera", ctx);
  if (TriggerStringIsMonsterWave(p.event_trigger)) {
    runAction("spawn_monster_wave", ctx);
    return;
  }
  if (!p.teleport_to.empty()) {
    runAction("teleport", ctx);
    return;
  }
  if (!p.event_trigger.empty() || !p.event_id.empty())
    runAction("log_unhandled_map_event", ctx);
}

void SubterraGameplayPushInputLock(SubterraSession &session) {
  session.gameplayInputLockDepth++;
  if (session.gameplayInputLockDepth != 1)
    return;
  criogenio::Input::SetSuppressGameplayInput(true);
  if (session.world && session.player != criogenio::ecs::NULL_ENTITY) {
    if (auto *c = session.world->GetComponent<criogenio::Controller>(session.player))
      c->movement_frozen = true;
  }
}

void SubterraGameplayPopInputLock(SubterraSession &session) {
  if (session.gameplayInputLockDepth <= 0)
    return;
  session.gameplayInputLockDepth--;
  if (session.gameplayInputLockDepth != 0)
    return;
  criogenio::Input::SetSuppressGameplayInput(false);
  if (session.world && session.player != criogenio::ecs::NULL_ENTITY) {
    if (auto *c = session.world->GetComponent<criogenio::Controller>(session.player))
      c->movement_frozen = false;
  }
}

void SubterraGameplayQueuesTick(SubterraSession &session, float dt) {
  session.gameplayCommandQueue.update(dt);
}

void RegisterSubterraGameplayMapEventHandler(SubterraSession &session) {
  session.mapEvents.addListener([&session](const MapEventPayload &p) {
    session.gameplay.dispatchFromMapEvent(session, p);
  });
}

static void actionNotifyCamera(SubterraGameplayContext &ctx) {
  if (!ctx.payload || ctx.payload->event_trigger.empty())
    return;
  SubterraCameraNotifyTrigger(ctx.session, ctx.payload->event_trigger);
}

static void actionSpawnMonsterWave(SubterraGameplayContext &ctx) {
  float cx = 0.f, cy = 0.f;
  int n = 5;
  if (ctx.payload) {
    const MapEventPayload &p = *ctx.payload;
    cx = p.world_x + p.world_w * 0.5f;
    cy = p.world_y + p.world_h * 0.5f;
    if (p.is_point)
      cx = p.world_x, cy = p.world_y;
    n = p.spawn_count > 0 ? p.spawn_count : 5;
  }
  if (ctx.actionParams.contains("world_x") && ctx.actionParams["world_x"].is_number())
    cx = ctx.actionParams["world_x"].get<float>();
  if (ctx.actionParams.contains("world_y") && ctx.actionParams["world_y"].is_number())
    cy = ctx.actionParams["world_y"].get<float>();
  if (ctx.actionParams.contains("count") && ctx.actionParams["count"].is_number_integer())
    n = ctx.actionParams["count"].get<int>();
  if (n < 1)
    n = 1;
  SpawnMobsAround(ctx.session, cx, cy, n);
  char buf[96];
  std::snprintf(buf, sizeof buf, "Monster wave: spawned %d at (%.0f,%.0f)", n, cx, cy);
  sessionLog(ctx.session, buf);
}

static void actionTeleport(SubterraGameplayContext &ctx) {
  std::string destRel;
  std::string spawn;
  if (ctx.payload) {
    const MapEventPayload &p = *ctx.payload;
    if (TriggerStringIsMonsterWave(p.event_trigger))
      return;
    destRel = p.teleport_to;
    spawn = p.spawn_point;
  }
  if (ctx.actionParams.contains("map") && ctx.actionParams["map"].is_string())
    destRel = ctx.actionParams["map"].get<std::string>();
  if (ctx.actionParams.contains("spawn_point") && ctx.actionParams["spawn_point"].is_string())
    spawn = ctx.actionParams["spawn_point"].get<std::string>();
  if (destRel.empty())
    return;
  fs::path base = fs::path(ctx.session.mapPath).parent_path();
  fs::path dest = (base / destRel).lexically_normal();
  std::string err;
  if (ctx.session.loadMap(dest.string(), err)) {
    ctx.session.placePlayerAtSpawn(spawn);
    sessionLog(ctx.session, "Teleported to " + dest.string());
  } else {
    std::fprintf(stderr, "[MapEvent] teleport failed: %s\n", err.c_str());
    sessionLog(ctx.session, std::string("Teleport failed: ") + err);
  }
}

static void actionLogUnhandled(SubterraGameplayContext &ctx) {
  if (!ctx.payload)
    return;
  const MapEventPayload &p = *ctx.payload;
  std::fprintf(stderr, "[MapEvent] id=%s trigger=%s type=%s manual=%d\n", p.event_id.c_str(),
               p.event_trigger.c_str(), p.event_type.c_str(), p.manual ? 1 : 0);
}

static void actionCameraShake(SubterraGameplayContext &ctx) {
  SubterraCameraBundle &cam = ctx.session.camera;
  float intensity = cam.cfg.shake_on_attack.intensity;
  float duration = cam.cfg.shake_on_attack.duration;
  float frequency = cam.cfg.shake_on_attack.frequency;
  float decay = cam.cfg.shake_on_attack.decay;
  if (ctx.actionParams.contains("intensity"))
    intensity = ctx.actionParams["intensity"].get<float>();
  if (ctx.actionParams.contains("duration"))
    duration = ctx.actionParams["duration"].get<float>();
  if (ctx.actionParams.contains("frequency"))
    frequency = ctx.actionParams["frequency"].get<float>();
  if (ctx.actionParams.contains("decay"))
    decay = ctx.actionParams["decay"].get<float>();
  if (duration <= 0.f || intensity <= 0.f)
    return;
  cam.shake.active = true;
  cam.shake.elapsed = 0.f;
  cam.shake.intensity = intensity;
  cam.shake.duration = duration;
  cam.shake.frequency = frequency > 0.f ? frequency : 35.f;
  cam.shake.decay = decay > 0.f ? decay : 1.f;
  cam.shake.dir_x = 0.f;
  cam.shake.dir_y = 0.f;
}

static void actionLockPlayerInput(SubterraGameplayContext &ctx) {
  SubterraGameplayPushInputLock(ctx.session);
}

static void actionUnlockPlayerInput(SubterraGameplayContext &ctx) {
  SubterraGameplayPopInputLock(ctx.session);
}

static void actionDamagePlayer(SubterraGameplayContext &ctx) {
  if (!ctx.session.world || ctx.session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *v = ctx.session.world->GetComponent<PlayerVitals>(ctx.session.player);
  if (!v)
    return;
  float amount = 10.f;
  if (ctx.actionParams.contains("amount") && ctx.actionParams["amount"].is_number())
    amount = ctx.actionParams["amount"].get<float>();
  v->health = std::max(0.f, v->health - amount);
  sessionLog(ctx.session, "Player took damage (gameplay action).");
}

void RegisterDefaultSubterraGameplayActions(GameplayActionRegistry &reg) {
  reg.registerAction("notify_camera", actionNotifyCamera);
  reg.registerAction("spawn_monster_wave", actionSpawnMonsterWave);
  reg.registerAction("spawn_wave", actionSpawnMonsterWave);
  reg.registerAction("teleport", actionTeleport);
  reg.registerAction("log_unhandled_map_event", actionLogUnhandled);
  reg.registerAction("camera_shake", actionCameraShake);
  reg.registerAction("lock_player_input", actionLockPlayerInput);
  reg.registerAction("unlock_player_input", actionUnlockPlayerInput);
  reg.registerAction("damage_player", actionDamagePlayer);
}

void SubterraEnqueueGameplayIntroDemo(SubterraSession &session) {
  session.gameplayCommandQueue.clear();
  const float kShakeDelay = 0.05f;
  const float kShakeDur = 0.35f;
  session.gameplayCommandQueue.push(0.f, [&session]() {
    SubterraGameplayContext ctx{session, nullptr, {}};
    session.gameplay.runAction("lock_player_input", ctx);
  });
  session.gameplayCommandQueue.push(kShakeDelay, [&session, kShakeDur]() {
    SubterraGameplayContext ctx{session, nullptr,
                                 nlohmann::json{{"intensity", 6.f}, {"duration", kShakeDur}}};
    session.gameplay.runAction("camera_shake", ctx);
  });
  session.gameplayCommandQueue.push(kShakeDelay + kShakeDur + 0.1f, [&session]() {
    SubterraGameplayContext ctx{session, nullptr, nlohmann::json{{"amount", 5.f}}};
    session.gameplay.runAction("damage_player", ctx);
  });
  session.gameplayCommandQueue.push(kShakeDelay + kShakeDur + 0.35f, [&session]() {
    SubterraGameplayContext ctx{session, nullptr, {}};
    session.gameplay.runAction("unlock_player_input", ctx);
    sessionLog(session, "Gameplay intro demo finished.");
  });
}

} // namespace subterra
