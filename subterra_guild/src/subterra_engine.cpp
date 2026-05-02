#include "subterra_engine.h"
#include "subterra_console_commands.h"
#include "subterra_components.h"
#include "subterra_day_night.h"
#include "subterra_imgui.h"
#include "subterra_input_config.h"
#include "subterra_session.h"
#include "imgui.h"
#include <cstdio>

namespace subterra {

SubterraEngine::SubterraEngine(SubterraSession &session, int width, int height,
                               const char *title)
    : criogenio::Engine(width, height, title), session_(&session) {}

SubterraEngine::~SubterraEngine() {
  SubterraUnregisterMovementHooks();
  SubterraImGuiShutdown();
}

void SubterraEngine::RegisterDebugCommands() {
  RegisterSubterraConsoleCommands(*this);
  SubterraImGuiInit(GetRenderer());
}

void SubterraEngine::OnFrame(float dt) {
  SubterraInputHotReloadTick(*session_, dt);
  SubterraUpdateOutdoorFactor(*session_, dt);
  SubterraAdvanceDayNight(session_->dayNight, dt);
  if (dt > 1e-6f) {
    const float instFps = 1.f / dt;
    session_->fpsSmooth = session_->fpsSmooth * 0.92f + instFps * 0.08f;
  }
}

void SubterraEngine::OnAfterWorldRender(criogenio::Renderer &renderer) {
  SubterraRenderAtmosphere(*session_, renderer);
  renderer.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Alpha);
}

bool SubterraEngine::OnPollEvent(const void *sdlEvent) {
  SubterraImGuiProcessSdlEvent(sdlEvent);
  return false;
}

void SubterraEngine::OnGUI() {
  criogenio::Renderer &r = GetRenderer();
  SubterraImGuiNewFrame();
  if (ImGui::IsKeyPressed(ImGuiKey_Tab))
    session_->showInventoryPanel = !session_->showInventoryPanel;
  else if (SubterraInputActionPressed(*session_, "inventory_toggle"))
    session_->showInventoryPanel = !session_->showInventoryPanel;
  SubterraImGuiDrawHud(*session_);
  SubterraImGuiDrawSession(*session_);
  if (session_->debugOverlay)
    SubterraImGuiDrawDebugConfig(*session_);
  if (session_->debugOverlay && session_->fpsSmooth > 0.1f) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "fps %.0f  debug", session_->fpsSmooth);
    r.DrawDebugText(8.f, 8.f, buf);
    char dayBuf[140];
    const auto &dn = session_->dayNight;
    std::snprintf(dayBuf, sizeof dayBuf, "day %d  %s  %s  outdoor %.2f",
                  dn.worldDay, SubterraDayTimeClockLabel(dn.dayTime).c_str(),
                  SubterraDayPhaseName(dn.dayTime, dn.dayPhaseSize).c_str(),
                  static_cast<double>(dn.outdoorFactor));
    r.DrawDebugText(8.f, 38.f, dayBuf);
  }
  if (session_->world && session_->player != criogenio::ecs::NULL_ENTITY) {
    if (auto *inv = session_->world->GetComponent<Inventory>(session_->player)) {
      std::string invLine = std::string("INV ") + inv->FormatLine();
      r.DrawDebugText(8.f, 22.f, invLine.c_str());
    }
  }
  SubterraImGuiRenderDrawData(r);
}

} // namespace subterra
