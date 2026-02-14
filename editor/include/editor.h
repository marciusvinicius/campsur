// For scene save/load file browser

#pragma once

#include "engine.h"
#include "animation_database.h"
#include "graphics_types.h"
#include "world.h"
#include <optional>

#include "imgui.h"

#include <memory>
#include <string>
#include <unordered_map>

enum class FileDialogMode { None, SaveScene, LoadScene };
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

  void InitImGUI();
  void RenderSceneToTexture();

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
  void DrawAddComponentMenu(int entity);
  void DrawGlobalComponentsPanel();
  void DrawGlobalInspector();

  void PickEntityAt(criogenio::Vec2 worldPos);
  void CreateEmptyEntityAtMouse();

  bool IsSceneInputAllowed() const;

  void DrawHierarchyNodes();
  void DrawEntityNode(int entity);

  bool IsMouseInWorldView();

  // Helper to convert mouse screen position to world coordinates
  criogenio::Vec2 ScreenToWorldPosition(criogenio::Vec2 mouseScreen);

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char *label);
  void OnGUI() override;

  bool dockLayoutBuilt = false;
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

  // Terrain editor state
  bool terrainEditMode = false;
  int terrainSelectedTile = 0;
  int terrainSelectedLayer = 0;
  // Draw terrain editor panel
  void DrawTerrainEditor();
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
