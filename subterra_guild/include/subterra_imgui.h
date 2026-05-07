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
/** Odin-style entity inspector (`egui` / `entityinspector` console). ImGui only. */
void SubterraImGuiDrawEntityInspector(SubterraSession &session);
/** When `session.debugOverlay`, edit runtime camera flags + reload configs (ImGui). */
void SubterraImGuiDrawDebugConfig(SubterraSession &session);
/** When `session.debugOverlay`, map list + teleport (ImGui). */
void SubterraImGuiDrawDebugMapTeleport(SubterraSession &session);

} // namespace subterra
