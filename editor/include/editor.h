// For scene save/load file browser

#pragma once

#include "engine.h"
#include "animation_database.h"
#include "graphics_types.h"
#include "world.h"
#include "i_editor_game_mode.h"
#include "project_context.h"
#include "terrain_edit_history.h"
#include <optional>
#include <unordered_set>

#include "imgui.h"

#include <memory>
#include <string>
#include <unordered_map>

enum class FileDialogMode { None, SaveScene, LoadScene };
enum class SceneMode { Scene2D, Scene3D };

/** Which resize handle is being dragged on a selected ECS zone. */
enum class ZoneHandle {
  None,
  TL, T, TR,
  L,      R,
  BL, B, BR
};
struct Hierarchy {
  int parent = -1;
  std::vector<int> children;
};

struct EditorState {
  int selectedEntityId;
};

class EditorApp : public criogenio::Engine {
public:
  EditorApp(int width, int height);
  std::optional<criogenio::Vec2> WorldToTile(const criogenio::Terrain2D &terrain,
                                             criogenio::Vec2 worldPos);
  void EditorAppReset();
  /** Re-register the editor's default system set (called from reset and from Stop). */
  void RegisterEditorSystems();
  //~EditorApp();
  void Run();

private:
  EditorState state;

  criogenio::TextureHandle sceneRT;
  // Panels sizes
  int leftPanelWidth = 200;
  int rightPanelWidth = 220;
  /// TODO: move this to UI
  ImVec2 viewportPos;
  ImVec2 viewportSize;
  bool viewportHovered = false;

  criogenio::Vec2 lastMousePos{0, 0};
  std::optional<int> selectedEntityId;
  bool imguiBackendsInitialized = false;

  /** When true, World::Update runs (simulation); when false, scene is static (edit mode). */
  bool isPlaying = false;

  /**
   * Active play-mode session (present only while isPlaying).
   * Concrete type is selected per project (e.g. subterra::EditorGameMode).
   */
  std::unique_ptr<IEditorGameMode> gameMode;
  /** When true (default), stopping play reverts the world to its pre-play snapshot. */
  bool revertOnStop = true;

  /** Currently loaded project (empty when no project is open). */
  std::optional<ProjectContext> project;
  /** Absolute path to the opened `project.campsur` (for Save Project). */
  std::optional<std::string> projectCampsurAbsPath;
  /** Absolute path of the level file last opened or saved (editor document). */
  std::optional<std::string> currentLevelPath;
  /** True when the in-memory level differs from disk (best-effort). */
  bool levelDirty = false;

  /** Refresh the asset browser on the next frame (set after open/save). */
  bool assetBrowserDirty = false;

  /** When true, draw BoxCollider outlines in the viewport. */
  bool showColliderDebug = true;
  /** When true, selected entities show BoxCollider resize handles in the viewport. */
  bool showColliderGizmo = true;
  /** Map event / interactable / spawn ECS zones (2D). */
  bool showMapAuthoringGizmos = true;

  /** Zone resize state: which handle is dragged, initial sizes, etc. */
  ZoneHandle activeZoneHandle = ZoneHandle::None;
  /** BoxCollider gizmo drag state (same handle enum as zones). */
  ZoneHandle activeColliderHandle = ZoneHandle::None;
  float zoneResizeInitX = 0.f, zoneResizeInitY = 0.f;
  float zoneResizeInitW = 0.f, zoneResizeInitH = 0.f;
  criogenio::Vec2 zoneResizeDragOrigin = {0.f, 0.f};
  float colliderResizeInitX = 0.f, colliderResizeInitY = 0.f;
  float colliderResizeInitW = 0.f, colliderResizeInitH = 0.f;
  criogenio::Vec2 colliderResizeDragOrigin = {0.f, 0.f};
  SceneMode sceneMode = SceneMode::Scene2D;

  char newLevelFilenameBuf[260] = "untitled.campsurlevel";
  int newLevelTemplateKind = 0;
  bool newLevelSetAsInit = false;

  void InitImGUI();
  void RenderSceneToTexture();
  void DrawColliderDebug(criogenio::Renderer& ren);
  void DrawMapAuthoringGizmos(criogenio::Renderer& ren);
  void DrawZoneResizeHandles(criogenio::Renderer& ren, int entity);
  void DrawColliderResizeHandles(criogenio::Renderer& ren, int entity);
  void HandleZoneResize(criogenio::Vec2 worldMouse);
  void HandleColliderResize(criogenio::Vec2 worldMouse);
  /** Returns the handle under worldPos (if entity has an ECS zone) or None. */
  ZoneHandle HitTestZoneHandle(int entity, criogenio::Vec2 worldPos);
  ZoneHandle HitTestColliderHandle(int entity, criogenio::Vec2 worldPos);
  /** Snap world position to terrain tile grid when Ctrl is held. */
  criogenio::Vec2 SnapWorldToGrid(criogenio::Vec2 world);
  /** Bake all ECS zone entities into terrain.tmxMeta (for Level-JSON export). */
  void BakeEcsZonesIntoTmxMeta();
  /** Draw name labels over map authoring gizmos (called after EndCamera2D). */
  void DrawMapAuthoringLabels(criogenio::Renderer &ren);
  /** Load each shipped TMX, save as .json alongside it, and print a report. */
  void BatchMigrateTmxMaps();

  void DrawUI();
  void DrawHierarchyPanel();
  void DrawInspectorPanel();
  void DrawDockSpace();
  void DrawWorldView();
  void DrawMainMenuBar();
  void HandleEntityDrag(criogenio::Vec2 mouseDelta);
  void HandleInput(float dt, criogenio::Vec2 mouseDelta);
  void HandleScenePicking();

  // Entity Inspector TODO:(maraujo) Move this to UI file
  void DrawEntityHeader(int entity);
  void DrawComponentInspectors(int entity);
  void DrawGravityInspector(int entity);
  void DrawTransformInspector(int entity);
  void DrawAnimationStateInspector(int entity);
  void DrawAnimatedSpriteInspector(int entity);
  void DrawSpriteInspector(int entity);
  void DrawControllerInspector(int entity);
  void DrawAIControllerInspector(int entity);
  void DrawRigidBodyInspector(int entity);
  void DrawBoxColliderInspector(int entity);
  void DrawTransform3DInspector(int entity);
  void DrawCamera3DInspector(int entity);
  void DrawModel3DInspector(int entity);
  void DrawBoxCollider3DInspector(int entity);
  void DrawBox3DInspector(int entity);
  void DrawPlayerController3DInspector(int entity);
  void DrawMapEventZone2DInspector(int entity);
  void DrawInteractableZone2DInspector(int entity);
  void DrawWorldSpawnPrefab2DInspector(int entity);
  void DrawAddComponentMenu(int entity);
  void DrawGlobalComponentsPanel();
  void DrawGlobalInspector();

  void PickEntityAt(criogenio::Vec2 worldPos);
  void CreateEmptyEntityAtMouse();
  void CreateMapEventZoneAtMouse();
  void CreateInteractableZoneAtMouse();
  void CreateSpawnPrefabAtMouse();

  bool IsSceneInputAllowed() const;

  void DrawHierarchyNodes();
  void DrawEntityNode(int entity);

  bool IsMouseInWorldView();

  // Mouse position in screen coordinates (matches ImGui/viewportPos). Use this for
  // viewport hit-testing; GetMousePosition() returns window-relative coords.
  criogenio::Vec2 GetViewportMousePos() const;

  /**
   * World position under the cursor for 2D editing: maps the Viewport Image + scene
   * render-target Y flip. Hides Engine::GetMouseWorld() (window coords) while the
   * scene is shown in ImGui.
   */
  criogenio::Vec2 GetMouseWorld() const;

  // Helper to convert mouse screen position to world coordinates
  criogenio::Vec2 ScreenToWorldPosition(criogenio::Vec2 mouseScreen);

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char *label);
  void OnGUI() override;

  bool dockLayoutBuilt = false;
  // Force a one-shot rebuild on the next DrawDockSpace call (e.g. from
  // View > Reset Layout, or after a layout-version migration).
  bool dockLayoutResetRequested = false;
  FileDialogMode fileDialogMode = FileDialogMode::None;
  std::string pendingScenePath;

  // Editor-side per-entity temporary inputs (e.g., texture path being edited)
  std::unordered_map<int, std::string> texturePathEdits;
  // whether to edit the animation asset in-place (affects all instances)
  std::unordered_map<int, bool> editInPlace;

  // Simple ImGui file browser state
  bool fileBrowserVisible = false;
  int fileBrowserTargetEntity = -1;
  bool fileBrowserForTerrain =
      false; // Flag to indicate if browser is for terrain
  std::vector<std::string> fileBrowserEntries;
  std::string fileBrowserFilter;

  // file browser preview
  std::string fileBrowserPreviewPath;
  std::shared_ptr<criogenio::TextureResource> fileBrowserPreviewTex;

  // Per-entity draft data for building animation clips in the editor
  struct AnimationClipDraft {
    std::string name = "idle";
    float frameSpeed = 0.1f;
    int tileSize = 32;
    // Linked animation state for this clip
    criogenio::AnimState state = criogenio::AnimState::IDLE;
    // Linked facing direction for this clip
    criogenio::Direction direction = criogenio::Direction::DOWN;
    std::vector<int> tileIndices; // indices into the sprite sheet grid
  };

  std::unordered_map<int, AnimationClipDraft> animationClipDrafts;
  // Name of the clip currently being edited for a given entity (empty = new)
  std::unordered_map<int, std::string> editingClipName;

  // ---------------------------------------------------------------
  // Terrain / map editor state
  // ---------------------------------------------------------------
  enum class TerrainTool {
    Paint,      // 'B'
    Rect,       // 'R' — outline
    FilledRect, // shift+R — filled
    Line,       // 'L'
    FloodFill,  // 'G'
    Eyedropper, // 'I'
    Select,     // 'S' — region selection
    Stamp,      // auto-engaged after Ctrl+V
  };

  bool terrainEditMode = false;
  int terrainSelectedTile = 0;
  int terrainSelectedLayer = 0;
  TerrainTool terrainTool = TerrainTool::Paint;
  int terrainBrushW = 1;
  int terrainBrushH = 1;

  /** True when the picked tile is a TMX global ID; otherwise it's a tile index. */
  bool terrainSelectedIsGid = false;
  /**
   * Per-tile flip state for the brush (Tiled GID flip bits). Painting OR's
   * these into the stored gid so the renderer can mirror the tile. Diagonal
   * (rotation) is intentionally not exposed yet; see TODO in
   * `RenderChunkedLayerGid` re: SDL3 flip-then-rotate ordering.
   */
  bool brushFlipH = false;
  bool brushFlipV = false;

  /** When true, scene authoring snaps entity placement to the terrain grid. */
  bool snapToGrid = true;

  // Drag/preview state for tools that span multiple frames.
  bool terrainDragging = false;
  int terrainDragAnchorTx = 0;
  int terrainDragAnchorTy = 0;
  int terrainDragCurTx = 0;
  int terrainDragCurTy = 0;

  // Region selection (Phase 5)
  struct TileRegion { int x0, y0, x1, y1; int layer; };  // inclusive
  std::optional<TileRegion> tileSelection;

  struct TilePattern { int w = 0, h = 0; std::vector<int> tiles; };
  std::optional<TilePattern> tileClipboard;

  TerrainEditHistory terrainHistory;

  /** Editor-only: layers in this set reject paints/edits. Not persisted. */
  std::unordered_set<int> lockedLayers;

  // ---- Object layers (Phase: object layer) -------------------------------
  // Object layers group entities (zones, spawns, sprites). Each entity may
  // carry a `LayerMembership` component; entities without it default to
  // `kDefaultObjectLayer`. The editor maintains the catalog of known layer
  // names plus per-layer visibility/lock so the user can author them in the
  // Layers panel just like tile layers.
  static constexpr const char *kDefaultObjectLayer = "Default";
  std::vector<std::string> objectLayers{kDefaultObjectLayer};
  std::string currentObjectLayer = kDefaultObjectLayer;
  std::unordered_set<std::string> hiddenObjectLayers;
  std::unordered_set<std::string> lockedObjectLayers;

  /**
   * Tag the entity with `LayerMembership` whose `layerName == currentObjectLayer`.
   * Called by every entity-spawning code path so newly created authoring
   * objects land on the active layer. No-op if the entity is invalid.
   */
  void AssignToCurrentObjectLayer(int entityId);
  /**
   * Walk all entities and (a) ensure every distinct LayerMembership name is
   * present in `objectLayers`, and (b) re-apply EditorHidden tags so the
   * runtime mirrors `hiddenObjectLayers`. Cheap; called once per frame.
   */
  void SyncObjectLayerState();
  /** Resolve the layer name for an entity (defaulting to the Default layer). */
  std::string GetEntityLayerName(int entityId) const;

  // Helper: paint one cell, recording into the active stroke if any.
  // Returns true if the cell changed.
  bool PaintCell(int layer, int tx, int ty, int gid);
  /**
   * Combine `terrainSelectedTile` with the current `brushFlipH/V` flags.
   * Erase (-1) and non-gid modes are passed through unchanged. In gid mode,
   * the base gid is masked out before re-applying flip bits so toggling
   * flips after eyedroppering works as expected.
   */
  int EffectivePaintTile() const;
  /** Per-frame terrain mouse dispatch. Implements the active TerrainTool. */
  void HandleTerrainEditInput();
  /** Keyboard shortcuts for terrain editing (undo/redo, tool hotkeys, ...). */
  void HandleTerrainShortcuts();
  /** Copy active tileSelection to tileClipboard (Phase 5). */
  void CopyTileSelection();
  /** Copy + clear (one undoable edit) (Phase 5). */
  void CutTileSelection();
  /** Clear active tileSelection (one undoable edit) (Phase 5). */
  void DeleteTileSelection();
  /** Draw tool/selection overlays in the viewport (rect, line, brush ghost). */
  void DrawTerrainToolOverlay(const criogenio::Terrain2D &terrain);

  /**
   * Spawn an entity at `worldPos` carrying a Sprite component pointing at
   * `texturePath`. Used by the Asset Browser drag-drop. Honors snapToGrid.
   */
  void SpawnSpriteEntityAt(criogenio::Vec2 worldPos, const std::string &texturePath);

  // Draw terrain editor panel
  void DrawTerrainEditor();
  void DrawTerrainLayersPanel();
  void DrawTerrainStatusBar();
  /** Subterra-style level metadata: spawn prefabs, interactables, simple event objects. */
  void DrawLevelMetadataPanel();
  /** Asset browser panel (shown when a project is open). */
  void DrawAssetBrowser();
  /** Slim info bar shown above the dockspace, displaying the active project. */
  void DrawProjectInfoBar();

  void MarkLevelDirty();
  void LoadLevelFromAbsolutePath(const std::string &absPath);
  bool SaveCurrentLevel();
  void SaveCurrentLevelAs();
  void SaveProjectDescriptor();
  void HandleEditorShortcuts();
  void DrawNewLevelModal();
  /** Template: 0 = 2D tilemap, 1 = 2D free-form, 2 = 3D (matches legacy New Scene). */
  void ApplyLevelTemplateReset(int kind);
  // Draw grid overlay on terrain in viewport
  void DrawTerrainGridOverlay(const criogenio::Terrain2D &terrain,
                              const criogenio::Camera2D &camera);
  // Draw file browser popup (called every frame)
  const char *DrawFileBrowserPopup();

  // Animation authoring helpers
  // Draw animation editor content for a given entity
  void DrawAnimationEditor(int entity);
  // Top-level Animation Editor window (docked like Inspector/Terrain)
  void DrawAnimationEditorWindow();
};
