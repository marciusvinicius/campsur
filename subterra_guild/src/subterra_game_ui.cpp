#include "subterra_game_ui.h"
#include "components.h"
#include "game_ui.h"
#include "inventory.h"
#include "item_catalog.h"
#include "spawn_service.h"
#include "subterra_loadout.h"
#include "subterra_player_vitals.h"
#include "subterra_session.h"
#include "world.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace subterra {

using criogenio::Rect;

static void TruncateLabel(std::string &s, size_t maxChars) {
  if (s.size() > maxChars)
    s.resize(maxChars);
}

void SubterraGameUiDrawHud(SubterraSession &session, criogenio::GameUi &ui) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *load = session.world->GetComponent<SubterraLoadout>(session.player);
  auto *inv = session.world->GetComponent<criogenio::Inventory>(session.player);
  if (!load || !inv)
    return;

  const Rect bar = ui.layoutBottomBar(118.f, 4.f);
  ui.screenPanel(bar, {32, 32, 42, 240}, {160, 160, 180, 255});

  float y = bar.y + 6.f;
  const float x0 = bar.x + 8.f;

  if (!session.interactHint.empty()) {
    const float hintH = 20.f;
    ui.screenPanel({x0, y, bar.width - 16.f, hintH}, {72, 68, 28, 230}, {140, 130, 50, 255});
    ui.text(x0 + 4.f, y + 5.f, session.interactHint.c_str(), criogenio::Colors::White);
    y += hintH + 6.f;
  }

  ui.text(x0, y, "E: use   Tab: inv   1-5 / U: bar", criogenio::Colors::White);
  y += 14.f;

  const char *eqTitles[] = {"R hand", "L hand", "Body"};
  float colX = x0;
  const float rowBtnY = y + 12.f;
  for (int e = 0; e < kEquipSlots; ++e) {
    ui.text(colX, y, eqTitles[e], criogenio::Colors::White);
    std::string &eq = load->equipment[static_cast<size_t>(e)];
    const Rect slotRect = {colX, rowBtnY, 62.f, 34.f};
    if (eq.empty()) {
      if (ui.screenButton(slotRect, "--"))
        (void)0;
    } else {
      std::string shortName = criogenio::ItemCatalog::DisplayName(eq);
      TruncateLabel(shortName, 7);
      if (ui.screenButton(slotRect, shortName.c_str()))
        (void)0;
      const Rect stripRect = {colX + 66.f, rowBtnY, 24.f, 34.f};
      if (ui.screenButton(stripRect, "X")) {
        std::string err;
        if (!UnequipToInventory(*load, *inv, e, err) && !err.empty())
          sessionLog(session, err);
      }
    }
    colX += 100.f;
  }

  colX += 8.f;
  ui.text(colX, y, "Actions", criogenio::Colors::White);
  colX += 56.f;
  for (int i = 0; i < kActionBarSlots; ++i) {
    const bool sel = (load->active_action_slot == i);
    std::string lab = "[" + std::to_string(i + 1) + "]";
    const std::string &bid = load->action_bar[static_cast<size_t>(i)];
    if (!bid.empty()) {
      lab = criogenio::ItemCatalog::DisplayName(bid);
      TruncateLabel(lab, 8);
    }
    const Rect br = {colX, rowBtnY, 76.f, 34.f};
    if (ui.screenButton(br, lab.c_str(), sel))
      load->active_action_slot = i;
    colX += 82.f;
  }
}

void SubterraGameUiDrawInventory(SubterraSession &session, criogenio::GameUi &ui) {
  if (!session.showInventoryPanel || !session.world ||
      session.player == criogenio::ecs::NULL_ENTITY)
    return;

  auto *inv = session.world->GetComponent<criogenio::Inventory>(session.player);
  auto *load = session.world->GetComponent<SubterraLoadout>(session.player);
  if (!inv)
    return;

  const float panelW = std::min(560.f, static_cast<float>(ui.viewportW()) - 24.f);
  const float panelH = std::min(420.f, static_cast<float>(ui.viewportH()) - 24.f);
  const float px = (static_cast<float>(ui.viewportW()) - panelW) * 0.5f;
  const float py = (static_cast<float>(ui.viewportH()) - panelH) * 0.5f;
  const Rect panel = {px, py, panelW, panelH};
  ui.screenPanel(panel, {24, 24, 32, 245}, {200, 200, 220, 255});

  float y = py + 10.f;
  const float x = px + 10.f;
  ui.text(x, y, "Inventory  (Tab to close)", criogenio::Colors::White);
  y += 16.f;

  if (auto *vit = session.world->GetComponent<PlayerVitals>(session.player)) {
    char line[160];
    std::snprintf(line, sizeof line, "HP %.0f/%.0f  ST %.0f/%.0f  Food %.0f/%.0f", vit->health,
                  vit->health_max, vit->stamina, vit->stamina_max, vit->food_satiety,
                  vit->food_satiety_max);
    ui.text(x, y, line, criogenio::Colors::White);
    y += 14.f;
    if (!vit->active_statuses.empty()) {
      ui.text(x, y, "Statuses:", criogenio::Colors::White);
      y += 14.f;
      for (const auto &st : vit->active_statuses) {
        if (!st.flag.empty()) {
          ui.text(x + 8.f, y, st.flag.c_str(), criogenio::Colors::White);
          y += 14.f;
        }
      }
    }
    if (vit->dead) {
      ui.text(x, y, "Dead — console: respawn", criogenio::Colors::Red);
      y += 14.f;
    }
  }

  y += 6.f;
  ui.text(x, y, "Items (click buttons)", criogenio::Colors::White);
  y += 16.f;

  bool any = false;
  for (int si = 0; si < criogenio::Inventory::kNumSlots; ++si) {
    const criogenio::InventorySlot &slot = inv->Slots()[static_cast<size_t>(si)];
    if (slot.item_id.empty() || slot.count <= 0)
      continue;
    any = true;
    std::string label =
        criogenio::ItemCatalog::DisplayName(slot.item_id) + " x" + std::to_string(slot.count);
    TruncateLabel(label, 22);
    constexpr float kLabelColW = 200.f;
    ui.text(x, y, label.c_str(), criogenio::Colors::White);
    float cx = x + kLabelColW;
    if (load) {
      const criogenio::ItemWearSlot ek = criogenio::ItemCatalog::WearSlotFor(slot.item_id);
      if (ek == criogenio::ItemWearSlot::MainHand) {
        if (ui.screenButton({cx, y - 2.f, 36.f, 22.f}, "R")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::MainHand, slot.item_id,
                                  err) &&
              !err.empty())
            sessionLog(session, err);
        }
        cx += 40.f;
      } else if (ek == criogenio::ItemWearSlot::OffHand) {
        if (ui.screenButton({cx, y - 2.f, 36.f, 22.f}, "L")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::OffHand, slot.item_id,
                                  err) &&
              !err.empty())
            sessionLog(session, err);
        }
        cx += 40.f;
      } else if (ek == criogenio::ItemWearSlot::Armor) {
        if (ui.screenButton({cx, y - 2.f, 36.f, 22.f}, "B")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::Armor, slot.item_id,
                                  err) &&
              !err.empty())
            sessionLog(session, err);
        }
        cx += 40.f;
      }
      for (int bi = 0; bi < kActionBarSlots; ++bi) {
        char bn[4];
        std::snprintf(bn, sizeof bn, "%d", bi + 1);
        if (ui.screenButton({cx, y - 2.f, 26.f, 22.f}, bn))
          AssignActionBarSlot(*load, bi, slot.item_id);
        cx += 30.f;
      }
    }
    if (ui.screenButton({cx, y - 2.f, 52.f, 22.f}, "Drop")) {
      const std::string id = slot.item_id;
      int removed = inv->TryRemoveFolded(id, 1);
      if (removed > 0) {
        if (load)
          load->ClearRefsForItem(id);
        auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
        if (tr) {
          float pcx = tr->x + static_cast<float>(session.playerW) * 0.5f;
          float pcy = tr->y + static_cast<float>(session.playerH) * 0.5f;
          SpawnPickupAt(session, pcx, pcy, id, 1);
        }
      }
    }
    y += 26.f;
    if (y > py + panelH - 28.f)
      break;
  }
  if (!any)
    ui.text(x, y, "(empty)", criogenio::Colors::Gray);
}

} // namespace subterra
