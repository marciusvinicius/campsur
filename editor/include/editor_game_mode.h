#pragma once

#include "i_editor_game_mode.h"
#include "subterra_session.h"
#include "graphics_types.h"
#include <string>

namespace subterra {

/**
 * Subterra Guild implementation of IEditorGameMode.
 *
 * Manages the full Subterra game session embedded in the editor's viewport.
 * Created when the designer clicks Play; destroyed on Stop.
 *
 * Lifecycle:
 *   auto gm = std::make_unique<EditorGameMode>();
 *   gm->Start(app, project);  // snapshot + bootstrap game session
 *   while(playing) {
 *       gm->Tick(dt);         // session internals (day-night, command queue…)
 *       world.Update(dt);     // all 12 Subterra systems run
 *       gm->DrawHUD();        // vitals / hint overlay via editor ImGui context
 *   }
 *   gm->Stop(app, revert);    // tear down; optionally restore pre-play state
 */
class EditorGameMode : public IEditorGameMode {
public:
    SubterraSession session;
    std::string worldSnapshot;         // pre-play world JSON (for revert)
    criogenio::Camera2D editorCamera;  // camera position saved before play

    /** project may be nullptr when no project is loaded (falls back to hardcoded search paths). */
    void Start(EditorApp &app, const ProjectContext *project) override;
    void Stop(EditorApp &app, bool revert) override;
    void Tick(float dt) override;
    void DrawHUD() override;
};

} // namespace subterra
