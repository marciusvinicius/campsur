#pragma once

#include "engine.h"
#include "raylib.h"
#include "scene.h"
#include <optional>

using namespace criogenio;

class EditorApp : public Engine {
public:
  EditorApp(int width, int height);
  void Run();

private:
  // Panels sizes
  int leftPanelWidth = 200;
  int rightPanelWidth = 220;

  std::optional<int> selectedEntityId;

  Vector2 GetMouseWorld();

  // Internal loops
  void DrawUI();
  void DrawHierarchyPanel();
  void DrawInspectorPanel();
  void DrawSceneView();

  void HandleMouseSelection();
  void HandleEntityDrag();
  void HandleInput();

  bool IsMouseInSceneView();

  // helpers
  void DrawButton(int x, int y, int w, int h, const char *label,
                  std::function<void()> onClick);
  void DrawInput(int x, int y, int w, int h, const char* label);

  void OnGUI() override; //
                         //
};
