#pragma once

class EditorApp;
struct ProjectContext;

/**
 * Pluggable in-editor game mode.
 *
 * The editor owns one instance while play mode is active. Different projects
 * (Subterra Guild, FPS, platformer, ...) provide their own implementation of
 * this interface; the editor only knows about the abstract operations.
 *
 * Lifecycle:
 *   - Start  : called once when entering play. Snapshot world, load configs,
 *              swap systems, spawn the player, etc.
 *   - Tick   : called every frame while playing (before World::Update).
 *   - DrawHUD: called inside the editor's existing ImGui frame to draw any
 *              gameplay overlays (vitals, interact hint, etc.).
 *   - Stop   : called once when leaving play. Restore systems, optionally
 *              revert the world snapshot.
 */
class IEditorGameMode {
public:
    virtual ~IEditorGameMode() = default;

    /** project may be nullptr when no project is loaded. */
    virtual void Start(EditorApp &app, const ProjectContext *project) = 0;
    virtual void Stop(EditorApp &app, bool revert) = 0;
    virtual void Tick(float dt) = 0;
    virtual void DrawHUD() = 0;
};
