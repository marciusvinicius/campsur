#include "map_event_system.h"
#include "components.h"
#include "map_events.h"
#include "subterra_session.h"
#include <algorithm>

namespace subterra {

static bool aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                        float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static bool triggerLooksLikeTeleport(const MapEventTrigger &trg) {
  return TriggerStringIsTeleport(trg.event_trigger) || TriggerStringIsTeleport(trg.event_type) ||
         TriggerStringIsTeleport(trg.object_type);
}

void MapEventSystem::Update(float /*dt*/) {
  if (!session || !session->world || session->player == criogenio::ecs::NULL_ENTITY)
    return;
  criogenio::Terrain2D *ter = session->world->GetTerrain();
  if (!ter)
    return;
  auto *tr = session->world->GetComponent<criogenio::Transform>(session->player);
  if (!tr)
    return;

  const float px = tr->x;
  const float py = tr->y;
  const float pw = static_cast<float>(session->playerW);
  const float ph = static_cast<float>(session->playerH);

  std::unordered_set<std::string> insideNow;
  for (const auto &trg : session->triggers) {
    bool inside = aabbOverlap(px, py, pw, ph, trg.x, trg.y, trg.w, trg.h);
    if (!inside)
      continue;
    insideNow.insert(trg.storage_key);
    if (session->triggerInsidePrevFrame.count(trg.storage_key))
      continue;
    if (triggerLooksLikeTeleport(trg)) {
      const float tw = std::max(trg.w, 1.f);
      const float th = std::max(trg.h, 1.f);
      if (ClosedDoorOverlapsRect(*session, trg.x, trg.y, tw, th))
        continue;
    }

    MapEventPayload p = MakePayloadFromTrigger(trg, false);
    session->mapEvents.dispatch(p);
  }
  session->triggerInsidePrevFrame = std::move(insideNow);
}

void MapEventSystem::Render(criogenio::Renderer &renderer) {
  if (!session || !session->world || !session->debugOverlay)
    return;
  for (const auto &trg : session->triggers) {
    criogenio::Color c = {0, 255, 128, 180};
    renderer.DrawRectOutline(trg.x, trg.y, trg.w, trg.h, c);
  }
  for (const auto &it : session->tiledInteractables) {
    float ix = it.x, iy = it.y, iw = it.w, ih = it.h;
    if (it.is_point) {
      iw = 16.f;
      ih = 16.f;
      ix -= iw * 0.5f;
      iy -= ih * 0.5f;
    }
    criogenio::Color c = {255, 64, 255, 160};
    renderer.DrawRectOutline(ix, iy, iw, ih, c);
  }
}

} // namespace subterra
