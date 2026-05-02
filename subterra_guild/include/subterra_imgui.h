#pragma once

namespace criogenio {
class Renderer;
}

namespace subterra {

struct SubterraSession;

void SubterraImGuiInit(criogenio::Renderer &renderer);
void SubterraImGuiShutdown();
/** Forward SDL events to ImGui (call from `Engine::OnPollEvent`). */
void SubterraImGuiProcessSdlEvent(const void *sdlEvent);
void SubterraImGuiNewFrame();
void SubterraImGuiRenderDrawData(criogenio::Renderer &renderer);
void SubterraImGuiDrawSession(SubterraSession &session);
/** When `session.debugOverlay`, edit runtime camera flags + reload configs (reference cfg UI). */
void SubterraImGuiDrawDebugConfig(SubterraSession &session);
/** Bottom equipment + action bar (always when ImGui is up). */
void SubterraImGuiDrawHud(SubterraSession &session);

} // namespace subterra
