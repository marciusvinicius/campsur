#pragma once

#include "engine.h"
#include "raylib.h"
#include "world.h"
#include <optional>

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

  std::optional<int> selectedEntityId;

  Vector2 GetMouseWorld();

  void InitImGUI();
  void RenderSceneToTexture();
  // Internal loops
  void DrawUI();
  void DrawDockSpace();
  void DrawHierarchyPanel();
  void DrawInspectorPanel();
  void DrawWorldView();

  void HandleMouseSelection();
  void HandleEntityDrag();
  void HandleInput();

  bool IsMouseInWorldView();

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char *label);
  void OnGUI() override;

  bool dockLayoutBuilt = false;
};
