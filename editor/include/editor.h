#pragma once

#include "engine.h"
#include "raylib.h"
#include "world.h"
#include <optional>

#include "imgui.h"

#include <memory>
#include <string>
#include <unordered_map>

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
  void EditorAppReset();
  //~EditorApp();
  void Run();

private:
  EditorState state;

  RenderTexture2D sceneRT;
  // Panels sizes
  int leftPanelWidth = 200;
  int rightPanelWidth = 220;
  /// TODO: move this to UI
  ImVec2 viewportPos;
  ImVec2 viewportSize;
  bool viewportHovered = false;

  std::optional<int> selectedEntityId;

  void InitImGUI();
  void RenderSceneToTexture();

  void DrawUI();
  void DrawDockSpace();
  void DrawHierarchyPanel();
  void DrawWorldView();
  void DrawMainMenuBar();
  void DrawToolbar();
  void HandleEntityDrag();
  void HandleInput();
  void HandleScenePicking();

  // Entity Inspector TODO:(maraujo) Move this to UI file
  void DrawEntityHeader(int entity);
  void DrawComponentInspectors(int entity);
  void DrawTransformInspector(int entity);
  void DrawAnimationStateInspector(int entity);
  void DrawAnimatedSpriteInspector(int entity);
  void DrawControllerInspector(int entity);
  void DrawAIControllerInspector(int entity);
  void DrawAddComponentMenu(int entity);

  void PickEntityAt(Vector2 worldPos);
  void CreateEmptyEntityAtMouse();

  bool IsSceneInputAllowed() const;

  void DrawHierarchyNodes();
  void DrawEntityNode(int entity);

  bool IsMouseInWorldView();
  
  // Helper to convert mouse screen position to world coordinates
  Vector2 ScreenToWorldPosition(Vector2 mouseScreen);

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char *label);
  void OnGUI() override;

  bool dockLayoutBuilt = false;

  // Editor-side per-entity temporary inputs (e.g., texture path being edited)
  std::unordered_map<int, std::string> texturePathEdits;
  // whether to edit the animation asset in-place (affects all instances)
  std::unordered_map<int, bool> editInPlace;

  // Simple ImGui file browser state
  bool fileBrowserVisible = false;
  int fileBrowserTargetEntity = -1;
  bool fileBrowserForTerrain = false; // Flag to indicate if browser is for terrain
  std::vector<std::string> fileBrowserEntries;
  std::string fileBrowserFilter;

  // file browser preview
  std::string fileBrowserPreviewPath;
  std::shared_ptr<criogenio::TextureResource> fileBrowserPreviewTex;

  // Terrain editor state
  bool terrainEditMode = false;
  int terrainSelectedTile = 0;
  int terrainSelectedLayer = 0;
  // Draw terrain editor panel
  void DrawTerrainEditor();
  // Draw grid overlay on terrain in viewport
  void DrawTerrainGridOverlay();
  // Draw file browser popup (called every frame)
  void DrawFileBrowserPopup();
};
