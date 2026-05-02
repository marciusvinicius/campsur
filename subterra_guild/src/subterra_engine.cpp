#include "subterra_engine.h"
#include "subterra_console_commands.h"
#include "subterra_components.h"
#include "subterra_session.h"
#include <cstdio>

namespace subterra {

SubterraEngine::SubterraEngine(SubterraSession &session, int width, int height,
                               const char *title)
    : criogenio::Engine(width, height, title), session_(&session) {}

void SubterraEngine::RegisterDebugCommands() {
  RegisterSubterraConsoleCommands(*this);
}

void SubterraEngine::OnFrame(float dt) {
  if (dt > 1e-6f) {
    const float instFps = 1.f / dt;
    session_->fpsSmooth = session_->fpsSmooth * 0.92f + instFps * 0.08f;
  }
}

void SubterraEngine::OnGUI() {
  criogenio::Renderer &r = GetRenderer();
  if (session_->debugOverlay && session_->fpsSmooth > 0.1f) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "fps %.0f  debug", session_->fpsSmooth);
    r.DrawDebugText(8.f, 8.f, buf);
  }
  if (session_->world && session_->player != criogenio::ecs::NULL_ENTITY) {
    if (auto *inv = session_->world->GetComponent<Inventory>(session_->player)) {
      std::string invLine = std::string("INV ") + inv->FormatLine();
      r.DrawDebugText(8.f, 22.f, invLine.c_str());
    }
  }
}

} // namespace subterra
