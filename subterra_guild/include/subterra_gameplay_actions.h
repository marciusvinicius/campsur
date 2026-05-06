#pragma once

#include "delayed_command_queue.h"
#include "map_events.h"

#include "json.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace subterra {

struct SubterraSession;

/** Per-action execution context; `payload` set for map-driven events. */
struct SubterraGameplayContext {
  SubterraSession &session;
  const MapEventPayload *payload = nullptr;
  nlohmann::json actionParams = nlohmann::json::object();
};

/**
 * String-keyed actions for map events and scripted sequences.
 * Register defaults via RegisterDefaultSubterraGameplayActions once per session.
 */
class GameplayActionRegistry {
public:
  using ActionFn = std::function<void(SubterraGameplayContext &)>;

  void registerAction(std::string id, ActionFn fn);
  bool runAction(const std::string &id, SubterraGameplayContext &ctx);
  void dispatchFromMapEvent(SubterraSession &session, const MapEventPayload &p);

private:
  std::unordered_map<std::string, ActionFn> actions_;
  std::unordered_set<std::string> missingLogged_;
};

/** Built-in handlers (teleport, waves, camera, etc.). */
void RegisterDefaultSubterraGameplayActions(GameplayActionRegistry &reg);

/** Subscribe `session.mapEvents` so dispatch runs the registry (call once at startup). */
void RegisterSubterraGameplayMapEventHandler(SubterraSession &session);

/** Tick delayed gameplay commands (`session.gameplayCommandQueue`). */
void SubterraGameplayQueuesTick(SubterraSession &session, float dt);

/**
 * Enqueue a demo: lock input, camera shake, unlock. Uses cumulative delays from first step.
 * Pass `--gameplay-intro-demo` or call from console `intro` for verification.
 */
void SubterraEnqueueGameplayIntroDemo(SubterraSession &session);

void SubterraGameplayPushInputLock(SubterraSession &session);
void SubterraGameplayPopInputLock(SubterraSession &session);

} // namespace subterra
