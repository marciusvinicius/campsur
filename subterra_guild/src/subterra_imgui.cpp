#include "subterra_imgui.h"
#include "subterra_camera.h"
#include "subterra_server_config.h"
#include "inventory.h"
#include "item_catalog.h"
#include "map_events.h"
#include "render.h"
#include "spawn_service.h"
#include "subterra_loadout.h"
#include "subterra_player_vitals.h"
#include "subterra_session.h"
#include "subterra_components.h"
#include "components.h"
#include "world.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace subterra {

namespace {

bool g_imguiBackends = false;

void placePlayerCenter(SubterraSession &session, float cx, float cy) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!tr)
    return;
  tr->x = cx - static_cast<float>(session.playerW) * 0.5f;
  tr->y = cy - static_cast<float>(session.playerH) * 0.5f;
}

} // namespace

void SubterraImGuiInit(criogenio::Renderer &renderer) {
  if (g_imguiBackends)
    return;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  auto *window = static_cast<SDL_Window *>(renderer.GetWindowHandle());
  auto *sdlRenderer = static_cast<SDL_Renderer *>(renderer.GetRendererHandle());
  if (!window || !sdlRenderer) {
    ImGui::DestroyContext();
    return;
  }
  ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
  ImGui_ImplSDLRenderer3_Init(sdlRenderer);
  g_imguiBackends = true;
}

void SubterraImGuiShutdown() {
  if (g_imguiBackends) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    g_imguiBackends = false;
  }
  if (ImGui::GetCurrentContext())
    ImGui::DestroyContext();
}

void SubterraImGuiProcessSdlEvent(const void *sdlEvent) {
  if (!g_imguiBackends || !sdlEvent)
    return;
  ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event *>(sdlEvent));
}

void SubterraImGuiNewFrame() {
  if (!g_imguiBackends)
    return;
  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui::NewFrame();
}

void SubterraImGuiRenderDrawData(criogenio::Renderer &renderer) {
  if (!g_imguiBackends)
    return;
  ImGui::Render();
  ImGui_ImplSDLRenderer3_RenderDrawData(
      ImGui::GetDrawData(),
      static_cast<SDL_Renderer *>(renderer.GetRendererHandle()));
}

void SubterraImGuiDrawSession(SubterraSession &session) {
  if (!g_imguiBackends || !session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  if (!session.showInventoryPanel)
    return;
  ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
  ImGui::Begin("Inventory", &session.showInventoryPanel);
  auto *inv = session.world->GetComponent<criogenio::Inventory>(session.player);
  auto *load = session.world->GetComponent<SubterraLoadout>(session.player);
  if (!inv) {
    ImGui::TextUnformatted("No inventory component.");
    ImGui::End();
    return;
  }
  ImGui::TextUnformatted("E: pick up nearby   Tab: close   1-5 / U: see hotbar");
  if (auto *vit = session.world->GetComponent<PlayerVitals>(session.player)) {
    ImGui::Separator();
    ImGui::Text("HP  %.0f / %.0f", vit->health, vit->health_max);
    ImGui::Text("ST  %.0f / %.0f", vit->stamina, vit->stamina_max);
    ImGui::Text("Food %.0f / %.0f", vit->food_satiety, vit->food_satiety_max);
    if (!vit->active_statuses.empty()) {
      ImGui::Text("Statuses: %zu", vit->active_statuses.size());
      for (const auto &st : vit->active_statuses) {
        if (!st.flag.empty())
          ImGui::BulletText("%s", st.flag.c_str());
      }
    }
    if (vit->dead)
      ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "Dead — console: respawn");
  }
  ImGui::Separator();
  bool any = false;
  for (int si = 0; si < criogenio::Inventory::kNumSlots; ++si) {
    const criogenio::InventorySlot &slot = inv->Slots()[static_cast<size_t>(si)];
    if (slot.item_id.empty() || slot.count <= 0)
      continue;
    any = true;
    ImGui::PushID(si);
    std::string label =
        criogenio::ItemCatalog::DisplayName(slot.item_id) + " x" + std::to_string(slot.count);
    ImGui::BulletText("%s", label.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Drop 1")) {
      const std::string id = slot.item_id;
      int removed = inv->TryRemove(id, 1);
      if (removed > 0) {
        if (load)
          load->ClearRefsForItem(id);
        auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
        if (tr) {
          float cx = tr->x + static_cast<float>(session.playerW) * 0.5f;
          float cy = tr->y + static_cast<float>(session.playerH) * 0.5f;
          SpawnPickupAt(session, cx, cy, id, 1);
        }
      }
    }
    ImGui::SameLine();
    const criogenio::ItemWearSlot ek = criogenio::ItemCatalog::WearSlotFor(slot.item_id);
    if (load) {
      if (ek == criogenio::ItemWearSlot::MainHand) {
        if (ImGui::SmallButton("Equip R")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::MainHand, slot.item_id,
                                  err) && !err.empty())
            sessionLog(session, err);
        }
        ImGui::SameLine();
      }
      if (ek == criogenio::ItemWearSlot::OffHand) {
        if (ImGui::SmallButton("Equip L")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::OffHand, slot.item_id,
                                  err) && !err.empty())
            sessionLog(session, err);
        }
        ImGui::SameLine();
      }
      if (ek == criogenio::ItemWearSlot::Armor) {
        if (ImGui::SmallButton("Equip B")) {
          std::string err;
          if (!EquipFromInventory(*load, *inv, criogenio::ItemWearSlot::Armor, slot.item_id,
                                  err) && !err.empty())
            sessionLog(session, err);
        }
        ImGui::SameLine();
      }
      ImGui::TextUnformatted("Bar");
      ImGui::SameLine();
      for (int bi = 0; bi < kActionBarSlots; ++bi) {
        ImGui::PushID(bi + si * 32);
        char bn[8];
        std::snprintf(bn, sizeof bn, "%d", bi + 1);
        if (ImGui::SmallButton(bn))
          AssignActionBarSlot(*load, bi, slot.item_id);
        ImGui::PopID();
        if (bi + 1 < kActionBarSlots)
          ImGui::SameLine();
      }
    }
    ImGui::PopID();
  }
  if (!any)
    ImGui::TextUnformatted("(empty)");
  ImGui::End();
}

void SubterraImGuiDrawDebugConfig(SubterraSession &session) {
  if (!g_imguiBackends)
    return;
  ImGui::SetNextWindowPos(ImVec2(32, 120), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(440, 380), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Debug — server_configuration", nullptr, 0)) {
    ImGui::End();
    return;
  }
  ImGui::TextUnformatted("Paths (hot reload):");
  ImGui::BulletText("world: %s",
                    session.worldConfigPath.empty() ? "(none)" : session.worldConfigPath.c_str());
  ImGui::BulletText("input: %s",
                    session.inputConfigPath.empty() ? "(none)" : session.inputConfigPath.c_str());
  ImGui::Separator();
  auto &cfg = session.camera.cfg;
  ImGui::DragFloat("camera.zoom", &cfg.zoom, 0.01f, 0.2f, 4.f);
  ImGui::Checkbox("camera.zoom_active (run zoom)", &cfg.zoom_active);
  ImGui::Checkbox("camera.shake_active", &cfg.shake_active);
  ImGui::DragFloat("zoom on_run_start.mul", &cfg.zoom_on_run_start.multiplier, 0.01f, 0.5f, 2.5f);
  ImGui::DragFloat("zoom on_run_stop.mul", &cfg.zoom_on_run_stop.multiplier, 0.01f, 0.5f, 3.f);
  if (ImGui::Button("Fire map trigger: on_attack (shake test)"))
    SubterraCameraNotifyTrigger(session, "on_attack");
  ImGui::SameLine();
  if (ImGui::Button("on_run_start"))
    SubterraCameraNotifyTrigger(session, "on_run_start");
  ImGui::SameLine();
  if (ImGui::Button("on_run_stop"))
    SubterraCameraNotifyTrigger(session, "on_run_stop");
  ImGui::Separator();
  if (ImGui::Button("Save world_config to disk")) {
    std::string log;
    if (SubterraSaveServerConfiguration(session, &log))
      sessionLog(session, log.empty() ? "[Save] done." : log);
    else
      sessionLog(session, log.empty() ? "[Save] failed." : log);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload world_config + input from disk")) {
    std::string log;
    SubterraHotReloadServerConfiguration(session, false, &log);
    sessionLog(session, log.empty() ? "[Reload] done." : log);
  }
  ImGui::End();
}

void SubterraImGuiDrawEntityInspector(SubterraSession &session) {
  if (!g_imguiBackends || !session.showEntityInspector)
    return;

  ImGui::SetNextWindowSize(ImVec2(700, 520), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Entity Inspector", &session.showEntityInspector)) {
    ImGui::End();
    return;
  }

  static int tab = 0;
  static int prevTab = -1;
  static int selIdx = 0;

  std::vector<criogenio::ecs::EntityId> mobIds;
  std::vector<criogenio::ecs::EntityId> pickupIds;
  std::vector<criogenio::ecs::EntityId> playerIds;
  if (session.world) {
    mobIds = session.world->GetEntitiesWith<MobTag, criogenio::Transform>();
    pickupIds = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
    playerIds = session.world->GetEntitiesWith<PlayerTag, criogenio::Transform>();
  }

  std::vector<std::pair<std::string, std::uint8_t>> stateRows;
  stateRows.reserve(session.interactableStateFlags.size());
  for (const auto &kv : session.interactableStateFlags)
    stateRows.push_back(kv);
  std::sort(stateRows.begin(), stateRows.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  char tabMobs[48], tabItems[48], tabInter[48], tabPl[48], tabSt[48];
  std::snprintf(tabMobs, sizeof tabMobs, "Mobs (%zu)", mobIds.size());
  std::snprintf(tabItems, sizeof tabItems, "Items (%zu)", pickupIds.size());
  std::snprintf(tabInter, sizeof tabInter, "Interactables (%zu)",
                session.tiledInteractables.size());
  std::snprintf(tabPl, sizeof tabPl, "Players (%zu)", playerIds.size());
  std::snprintf(tabSt, sizeof tabSt, "States (%zu)", stateRows.size());

  if (ImGui::BeginTabBar("entity_inspector_tabs")) {
    if (ImGui::BeginTabItem(tabMobs)) {
      tab = 0;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabItems)) {
      tab = 1;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabInter)) {
      tab = 2;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabPl)) {
      tab = 3;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabSt)) {
      tab = 4;
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (tab != prevTab) {
    selIdx = 0;
    prevTab = tab;
  }

  int count = 0;
  switch (tab) {
  case 0:
    count = static_cast<int>(mobIds.size());
    break;
  case 1:
    count = static_cast<int>(pickupIds.size());
    break;
  case 2:
    count = static_cast<int>(session.tiledInteractables.size());
    break;
  case 3:
    count = static_cast<int>(playerIds.size());
    break;
  case 4:
    count = static_cast<int>(stateRows.size());
    break;
  default:
    break;
  }

  const int maxSel = count > 0 ? count - 1 : 0;
  selIdx = std::clamp(selIdx, 0, maxSel);

  if (!session.world) {
    ImGui::TextUnformatted("No world loaded.");
    ImGui::End();
    return;
  }

  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput) {
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && count > 0)
      selIdx = std::min(selIdx + 1, count - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && count > 0)
      selIdx = std::max(selIdx - 1, 0);
  }

  ImGui::BeginChild("inspector_list", ImVec2(340, 400), ImGuiChildFlags_Borders);
  for (int i = 0; i < count; ++i) {
    char label[320];
    label[0] = '\0';
    switch (tab) {
    case 0: {
      criogenio::ecs::EntityId id = mobIds[static_cast<size_t>(i)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *nm = session.world->GetComponent<criogenio::Name>(id);
      std::snprintf(label, sizeof label, "#%d %s (%.0f,%.0f)", static_cast<int>(id),
                    nm ? nm->name.c_str() : "mob", tr ? tr->x : 0.f, tr ? tr->y : 0.f);
      break;
    }
    case 1: {
      criogenio::ecs::EntityId id = pickupIds[static_cast<size_t>(i)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *pk = session.world->GetComponent<WorldPickup>(id);
      std::snprintf(label, sizeof label, "#%d %s x%d (%.0f,%.0f)", static_cast<int>(id),
                    pk ? pk->item_id.c_str() : "?", pk ? pk->count : 0, tr ? tr->x : 0.f,
                    tr ? tr->y : 0.f);
      break;
    }
    case 2: {
      const auto &it = session.tiledInteractables[static_cast<size_t>(i)];
      std::snprintf(label, sizeof label, "#%d %s (%.0f,%.0f)", it.tiled_object_id,
                    it.interactable_type.c_str(), it.x, it.y);
      break;
    }
    case 3: {
      criogenio::ecs::EntityId id = playerIds[static_cast<size_t>(i)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *nm = session.world->GetComponent<criogenio::Name>(id);
      const char *tag = (id == session.player) ? " [local]" : "";
      std::snprintf(label, sizeof label, "#%d %s%s (%.0f,%.0f)", static_cast<int>(id),
                    nm ? nm->name.c_str() : "player", tag, tr ? tr->x : 0.f, tr ? tr->y : 0.f);
      break;
    }
    case 4: {
      const auto &row = stateRows[static_cast<size_t>(i)];
      std::string k = row.first;
      if (k.size() > 40)
        k = "..." + k.substr(k.size() - 39);
      std::snprintf(label, sizeof label, "[flag] %s  0x%02x", k.c_str(),
                    static_cast<unsigned>(row.second));
      break;
    }
    default:
      break;
    }
    if (ImGui::Selectable(label, selIdx == i))
      selIdx = i;
  }
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("inspector_detail", ImVec2(0, 400), ImGuiChildFlags_Borders);
  if (count == 0) {
    ImGui::TextUnformatted("(none)");
  } else {
    switch (tab) {
    case 0: {
      criogenio::ecs::EntityId id = mobIds[static_cast<size_t>(selIdx)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *nm = session.world->GetComponent<criogenio::Name>(id);
      if (tr) {
        ImGui::Text("entity #%d", static_cast<int>(id));
        ImGui::Text("name: %s", nm ? nm->name.c_str() : "(none)");
        ImGui::Text("position: (%.1f, %.1f)", tr->x, tr->y);
        ImGui::Text("scale: %.2f %.2f", tr->scale_x, tr->scale_y);
        float mcx = tr->x + static_cast<float>(session.playerW) * tr->scale_x * 0.5f;
        float mcy = tr->y + static_cast<float>(session.playerH) * tr->scale_y * 0.5f;
        if (session.player != criogenio::ecs::NULL_ENTITY && ImGui::Button("TP player to mob"))
          placePlayerCenter(session, mcx + 24.f, mcy);
        ImGui::SameLine();
        if (id != session.player && ImGui::Button("Delete mob")) {
          session.world->DeleteEntity(id);
          selIdx = std::max(0, selIdx - 1);
        }
      }
      break;
    }
    case 1: {
      criogenio::ecs::EntityId id = pickupIds[static_cast<size_t>(selIdx)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *pk = session.world->GetComponent<WorldPickup>(id);
      if (pk && tr) {
        ImGui::Text("prefab: %s", pk->item_id.c_str());
        ImGui::Text("display: %s", criogenio::ItemCatalog::DisplayName(pk->item_id).c_str());
        ImGui::Text("count: %d", pk->count);
        ImGui::Text("size: %.0f x %.0f", pk->width, pk->height);
        ImGui::Text("position: (%.1f, %.1f)", tr->x, tr->y);
        float cx = tr->x + pk->width * 0.5f;
        float cy = tr->y + pk->height * 0.5f;
        if (session.player != criogenio::ecs::NULL_ENTITY && ImGui::Button("TP to item"))
          placePlayerCenter(session, cx, cy + 32.f);
        ImGui::SameLine();
        if (ImGui::Button("Delete pickup")) {
          if (session.nearestPickupEntity == id)
            session.nearestPickupEntity = criogenio::ecs::NULL_ENTITY;
          session.world->DeleteEntity(id);
          selIdx = std::max(0, selIdx - 1);
        }
      }
      break;
    }
    case 2: {
      const auto &it = session.tiledInteractables[static_cast<size_t>(selIdx)];
      std::string key = InteractableStateKey(session.mapPath, it);
      std::uint8_t fl = InteractableFlagsEffective(session, it);
      float cx = it.is_point ? it.x : it.x + it.w * 0.5f;
      float cy = it.is_point ? it.y : it.y + it.h * 0.5f;
      ImGui::Text("type: %s", it.interactable_type.c_str());
      ImGui::Text("object id: %d", it.tiled_object_id);
      ImGui::Text("pos: (%.1f, %.1f)  size: %.0f x %.0f", cx, cy, it.w, it.h);
      ImGui::Text("flags: 0x%02x", static_cast<unsigned>(fl));
      ImGui::TextWrapped("key: %s", key.c_str());
      bool op = (fl & InteractableState::Open) != 0;
      bool br = (fl & InteractableState::Burning) != 0;
      ImGui::Text("open: %s", op ? "yes" : "no");
      ImGui::Text("burning: %s", br ? "yes" : "no");
      if (session.player != criogenio::ecs::NULL_ENTITY && ImGui::Button("TP to object"))
        placePlayerCenter(session, cx, cy + 32.f);
      break;
    }
    case 3: {
      criogenio::ecs::EntityId id = playerIds[static_cast<size_t>(selIdx)];
      auto *tr = session.world->GetComponent<criogenio::Transform>(id);
      auto *nm = session.world->GetComponent<criogenio::Name>(id);
      auto *vit = session.world->GetComponent<PlayerVitals>(id);
      auto *load = session.world->GetComponent<SubterraLoadout>(id);
      if (tr) {
        ImGui::Text("entity #%d %s", static_cast<int>(id), nm ? nm->name.c_str() : "");
        ImGui::Text("local: %s", id == session.player ? "yes" : "no");
        ImGui::Text("position: (%.1f, %.1f)", tr->x, tr->y);
        if (vit) {
          ImGui::Separator();
          ImGui::Text("HP %.0f / %.0f", vit->health, vit->health_max);
          ImGui::Text("ST %.0f / %.0f", vit->stamina, vit->stamina_max);
          ImGui::Text("Food %.0f / %.0f", vit->food_satiety, vit->food_satiety_max);
          ImGui::Text("dead: %s", vit->dead ? "yes" : "no");
        }
        if (load) {
          ImGui::Separator();
          ImGui::TextUnformatted("equipment:");
          const char *elabs[] = {"R hand", "L hand", "Body"};
          for (int e = 0; e < kEquipSlots; ++e) {
            const std::string &eq = load->equipment[static_cast<size_t>(e)];
            ImGui::BulletText("%s: %s", elabs[e],
                              eq.empty() ? "(empty)"
                                         : criogenio::ItemCatalog::DisplayName(eq).c_str());
          }
        }
        if (id != session.player && session.player != criogenio::ecs::NULL_ENTITY &&
            ImGui::Button("TP local player here")) {
          float pcx = tr->x + static_cast<float>(session.playerW) * 0.5f;
          float pcy = tr->y + static_cast<float>(session.playerH) * 0.5f;
          placePlayerCenter(session, pcx + 32.f, pcy);
        }
      }
      break;
    }
    case 4: {
      const std::string &k = stateRows[static_cast<size_t>(selIdx)].first;
      std::uint8_t fl = stateRows[static_cast<size_t>(selIdx)].second;
      ImGui::TextWrapped("key:\n%s", k.c_str());
      ImGui::Text("value: 0x%02x", static_cast<unsigned>(fl));
      bool hasOpen = (fl & InteractableState::Open) != 0;
      bool hasBurn = (fl & InteractableState::Burning) != 0;
      ImGui::Text("OPEN: %s", hasOpen ? "yes" : "no");
      ImGui::Text("BURNING: %s", hasBurn ? "yes" : "no");
      if (ImGui::Button(hasOpen ? "Clear OPEN" : "Set OPEN")) {
        std::uint8_t &f = session.interactableStateFlags[k];
        f ^= InteractableState::Open;
      }
      ImGui::SameLine();
      if (ImGui::Button(hasBurn ? "Clear BURN" : "Set BURN")) {
        std::uint8_t &f = session.interactableStateFlags[k];
        f ^= InteractableState::Burning;
      }
      break;
    }
    default:
      break;
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::TextDisabled("Up/Down to navigate list, click row to select.");
  ImGui::End();
}

void SubterraImGuiDrawHud(SubterraSession &session) {
  if (!g_imguiBackends || !session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *load = session.world->GetComponent<SubterraLoadout>(session.player);
  auto *inv = session.world->GetComponent<criogenio::Inventory>(session.player);
  if (!load || !inv)
    return;

  ImGuiViewport *vp = ImGui::GetMainViewport();
  const float barH = 104.f;
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - barH),
                           ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, barH), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
  ImGui::Begin(
      "##subterra_hud", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

  if (!session.interactHint.empty())
    ImGui::TextColored(ImVec4(1.f, 0.95f, 0.5f, 1.f), "%s", session.interactHint.c_str());
  else
    ImGui::Dummy(ImVec2(0, 2));

  ImGui::Separator();

  const char *eqLabels[] = {"R hand", "L hand", "Body"};
  for (int e = 0; e < kEquipSlots; ++e) {
    ImGui::BeginGroup();
    ImGui::TextUnformatted(eqLabels[e]);
    std::string &eq = load->equipment[static_cast<size_t>(e)];
    if (eq.empty()) {
      ImGui::Button("—", ImVec2(72, 40));
    } else {
      std::string shortName = criogenio::ItemCatalog::DisplayName(eq);
      if (shortName.size() > 8)
        shortName.resize(8);
      ImGui::Button(shortName.c_str(), ImVec2(56, 40));
      ImGui::SameLine();
      ImGui::PushID(e + 200);
      if (ImGui::SmallButton("X")) {
        std::string err;
        if (!UnequipToInventory(*load, *inv, e, err) && !err.empty())
          sessionLog(session, err);
      }
      ImGui::PopID();
    }
    ImGui::EndGroup();
    ImGui::SameLine();
  }

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(16, 1));
  ImGui::SameLine();

  ImGui::BeginGroup();
  ImGui::TextUnformatted("Action bar (1-5, U=use consumable)");
  ImGui::BeginGroup();
  for (int i = 0; i < kActionBarSlots; ++i) {
    if (i > 0)
      ImGui::SameLine();
    const bool sel = (load->active_action_slot == i);
    if (sel)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.7f, 1.f));
    std::string lab = "[" + std::to_string(i + 1) + "]";
    const std::string &bid = load->action_bar[static_cast<size_t>(i)];
    if (!bid.empty())
      lab = criogenio::ItemCatalog::DisplayName(bid);
    if (lab.size() > 8)
      lab.resize(8);
    ImGui::PushID(i + 400);
    if (ImGui::Button(lab.c_str(), ImVec2(76, 40)))
      load->active_action_slot = i;
    ImGui::PopID();
    if (sel)
      ImGui::PopStyleColor();
  }
  ImGui::EndGroup();
  ImGui::EndGroup();

  ImGui::End();
  ImGui::PopStyleVar();
}

} // namespace subterra
