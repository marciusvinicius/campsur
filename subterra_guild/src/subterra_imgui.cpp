#include "subterra_imgui.h"
#include "subterra_camera.h"
#include "subterra_server_config.h"
#include "inventory.h"
#include "item_catalog.h"
#include "render.h"
#include "spawn_service.h"
#include "subterra_loadout.h"
#include "subterra_player_vitals.h"
#include "subterra_session.h"
#include "components.h"
#include "world.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <string>

namespace subterra {

namespace {

bool g_imguiBackends = false;

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
  if (ImGui::Button("Reload world_config + input from disk")) {
    std::string log;
    SubterraHotReloadServerConfiguration(session, false, &log);
    sessionLog(session, log.empty() ? "[Reload] done." : log);
  }
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
