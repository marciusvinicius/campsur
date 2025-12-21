#pragma once

#include "engine.h"
#include "raylib.h"
#include "world.h"
#include <optional>

#include "imgui.h"
struct Hierarchy {
  int parent = -1;
  std::vector<int> children;
};

class EditorApp : public criogenio::Engine {
public:
  EditorApp(int width, int height);
  //~EditorApp();
  void Run();

private:
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

  // Entity Inspector
  void DrawEntityHeader(int entity);
  void DrawComponentInspectors(int entity);

  void DrawTransformInspector(int entity);
  void DrawAnimatedSpriteInspector(int entity);
  void DrawControllerInspector(int entity);
  void DrawAIControllerInspector(int entity);
  void DrawAddComponentMenu(int entity);

  void PickEntityAt(Vector2 worldPos);

  bool IsSceneInputAllowed() const;

  void DrawHierarchyNodes();
  void DrawEntityNode(int entity);

  bool IsMouseInWorldView();

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char *label);
  void OnGUI() override;

  bool dockLayoutBuilt = false;
};
