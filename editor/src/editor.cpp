#include "editor.h"

#include "input.h"

#include "raymath.h" // <-- add this
#include "terrain.h"

//#define RAYGUI_IMPLEMENTATION
// #include "raygui.h"

using namespace criogenio;

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  // rlImGuiSetup(true); // ImGui initialization only in editor
  //  auto t = new Terrain();
  // GetScene().SetTerrain(t);
}

// #TODO:(maraujo) Move this to Engine
Vector2 EditorApp::GetMouseWorld() {
  return GetScreenToWorld2D(GetMousePosition(), GetScene().maincamera);
}
void EditorApp::Run() {
  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    HandleMouseSelection();
    HandleEntityDrag();
    HandleInput();

    GetScene().Update(dt);

    BeginDrawing();
    ClearBackground(DARKGRAY);

    DrawSceneView();
    DrawHierarchyPanel();
    DrawInspectorPanel();

    EndDrawing();
  }
}

void EditorApp::DrawButton(int x, int y, int w, int h, const char *label,
                           std::function<void()> onClick) {
  Rectangle rect = {(float)x, (float)y, (float)w, (float)h};
  bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);

  Color color = hovered ? LIGHTGRAY : GRAY;
  DrawRectangleRec(rect, color);
  DrawText(label, x + 10, y + 6, 18, BLACK);

  if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    onClick();
}

void EditorApp::DrawSceneView() {
  Rectangle sceneRect = {
      (float)leftPanelWidth, 0,
      (float)(GetScreenWidth() - leftPanelWidth - rightPanelWidth),
      (float)GetScreenHeight()};

  // Background for scene view
  DrawRectangleRec(sceneRect, BLACK);

  // Update camera offset to the center of the scene view (important!)
  Camera2D &cam = GetScene().maincamera;
  cam.offset = {sceneRect.x + sceneRect.width / 2.0f,
                sceneRect.y + sceneRect.height / 2.0f};

  DrawRectangleRec(sceneRect, BLACK);

  // Let engine render inside
  BeginScissorMode(leftPanelWidth, 0, sceneRect.width, sceneRect.height);
  // Debug: draw grid and origin so you can see things are visible
  BeginMode2D(cam);
  DrawGrid(100, 32);
  DrawCircle(0, 0, 6, RED);

  GetScene().Render(GetRenderer()); // expose renderer getter
  // highlight selected entity
  if (selectedEntityId.has_value()) {
    auto &e = GetScene().GetEntityById(*selectedEntityId);
    DrawRectangleLines(e.transform.x - 2, e.transform.y - 2, 36, 36, YELLOW);
  }
  EndMode2D();
  EndScissorMode();
}

void EditorApp::DrawHierarchyPanel() {
  DrawRectangle(0, 0, leftPanelWidth, GetScreenHeight(), {40, 40, 40, 255});
  DrawText("Entities", 10, 10, 20, RAYWHITE);

  int y = 40;

  DrawButton(10, y, 180, 25, "Create Terrain", [&]() {
    auto &t = GetScene().CreateTerrain("Terrain", "editor/assets/terrain.jpg");
  });
  y += 40;

  DrawButton(10, y, 180, 25, "Create Entity",
             [&]() { auto &e = GetScene().CreateEntity("New Entity"); });
  y += 40;

  for (auto &e : GetScene().GetEntities()) { // add getter (shown below)
    Color col = (selectedEntityId == e->id) ? YELLOW : WHITE;
    DrawText(e->name.c_str(), 10, y, 18, col);

    if (CheckCollisionPointRec(GetMousePosition(),
                               {0, (float)y, (float)leftPanelWidth, 20}) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      selectedEntityId = e->id;
    }

    y += 22;
  }
}

void EditorApp::DrawInspectorPanel() {
  int x = GetScreenWidth() - rightPanelWidth;

  DrawRectangle(x, 0, rightPanelWidth, GetScreenHeight(), {40, 40, 40, 255});
  DrawText("Inspector", x + 10, 10, 20, RAYWHITE);

  if (!selectedEntityId.has_value())
    return;

  auto &e = GetScene().GetEntityById(*selectedEntityId);

  char buf[128];
  snprintf(buf, sizeof(buf), "Entity: %s", e.name.c_str());
  DrawText(buf, x + 10, 50, 18, WHITE);

  // Transform fields
  DrawText("Position:", x + 10, 100, 16, WHITE);

  DrawText(TextFormat("X: %.1f", e.transform.x), x + 20, 130, 16, WHITE);
  DrawText(TextFormat("Y: %.1f", e.transform.y), x + 20, 160, 16, WHITE);
}

bool EditorApp::IsMouseInSceneView() {
  int mx = GetMouseX();
  int my = GetMouseY();
  return mx > leftPanelWidth && mx < GetScreenWidth() - rightPanelWidth;
}

void EditorApp::HandleMouseSelection() {
  if (!IsMouseInSceneView())
    return;

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    // Convert mouse position to world coordinates using the scene camera
    Vector2 mouseScreen = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouseScreen, GetScene().maincamera);

    for (auto &e : GetScene().GetEntities()) {
      Rectangle r = {e->transform.x, e->transform.y, 32.0f,
                     32.0f}; // world-space rect
      if (CheckCollisionPointRec(worldMouse, r)) {
        selectedEntityId = e->id;
        return;
      }
    }
    selectedEntityId.reset();
  }
}

void EditorApp::HandleEntityDrag() {
  if (!selectedEntityId.has_value())
    return;
  if (!IsMouseInSceneView())
    return;

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    auto &e = GetScene().GetEntityById(*selectedEntityId);
    // Convert previous and current mouse positions to world coordinates,
    // so drag works correctly under zoom and pan.
    Vector2 mouseScreen = GetMousePosition();
    Vector2 prevMouseScreen = {mouseScreen.x - GetMouseDelta().x,
                               mouseScreen.y - GetMouseDelta().y};

    // TODO:(maraujo) Do I need a editor camera here?
    Vector2 prevWorld =
        GetScreenToWorld2D(prevMouseScreen, GetScene().maincamera);
    Vector2 currWorld = GetScreenToWorld2D(mouseScreen, GetScene().maincamera);

    Vector2 drag = Vector2Subtract(currWorld, prevWorld);

    e.transform.x += drag.x;
    e.transform.y += drag.y;
  }
}

void EditorApp::HandleInput() {
  if (selectedEntityId.has_value()) {
    return;
  }

  // SCENE VIEW CAMERA ONLY IF MOUSE IS INSIDE SCENE VIEW
  if (!IsMouseInSceneView())
    return;

  float dt = GetFrameTime();

  // ------------------------------------------
  // ZOOM (scroll wheel)
  // ------------------------------------------
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    float zoomSpeed = 0.1f;
    GetScene().maincamera.zoom += wheel * zoomSpeed;

    if (GetScene().maincamera.zoom < 0.1f)
      GetScene().maincamera.zoom = 0.1f;
    if (GetScene().maincamera.zoom > 8.0f)
      GetScene().maincamera.zoom = 8.0f;
  }

  // ------------------------------------------
  // PAN WITH MIDDLE MOUSE BUTTON
  // ------------------------------------------
  if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    Vector2 delta = GetMouseDelta();
    GetScene().maincamera.target.x -= delta.x / GetScene().maincamera.zoom;
    GetScene().maincamera.target.y -= delta.y / GetScene().maincamera.zoom;
  }

  // ------------------------------------------
  // RIGHT MOUSE DRAG ALSO PANS
  // ------------------------------------------
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 delta = GetMouseDelta();
    GetScene().maincamera.target.x -= delta.x / GetScene().maincamera.zoom;
    GetScene().maincamera.target.y -= delta.y / GetScene().maincamera.zoom;
  }

  // ------------------------------------------
  // WASD MOVEMENT
  // ------------------------------------------
  float speed = 500.0f * dt;
  if (IsKeyDown(KEY_W))
    GetScene().maincamera.target.y -= speed;
  if (IsKeyDown(KEY_S))
    GetScene().maincamera.target.y += speed;
  if (IsKeyDown(KEY_A))
    GetScene().maincamera.target.x -= speed;
  if (IsKeyDown(KEY_D))
    GetScene().maincamera.target.x += speed;
}

void EditorApp::OnGUI() {
  // ImGui::Begin("Hierarchy");
  for (auto &e : GetScene().GetEntities()) {
    // ImGui::Text("%s", e->name.c_str());
  }
  // ImGui::End();

  // ImGui::Begin("Inspector");
  if (selectedEntityId.has_value()) {
    auto &ent = GetScene().GetEntityById(*selectedEntityId);
    // ImGui::InputFloat("X", &ent.transform.x);
    // ImGui::InputFloat("Y", &ent.transform.y);
  }
  // ImGui::End();
}
