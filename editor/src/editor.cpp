#include "editor.h"

#include "input.h"

#include "raymath.h"
#include "terrain.h"

// #define RAYGUI_IMPLEMENTATION
//  #include "raygui.h"

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

// Simple input box (non-functional, just for display)
void EditorApp::DrawInput(int x, int y, int w, int h, const char *label) {
  Rectangle rect = {(float)x, (float)y, (float)w, (float)h};
  bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);

  // Draw input box
  Color color = hovered ? LIGHTGRAY : GRAY;
  DrawRectangleRec(rect, color);
  DrawText(label, x + 10, y + 6, 18, BLACK);
}

void EditorApp::DrawSceneView() {
  Rectangle sceneRect = {
      (float)leftPanelWidth, 0,
      (float)(GetScreenWidth() - leftPanelWidth - rightPanelWidth),
      (float)GetScreenHeight()};

  // Background for scene view
  DrawRectangleRec(sceneRect, BLACK);
  Camera2D cam = Camera2D();
  cam.offset = {sceneRect.x + sceneRect.width / 2.0f,
                sceneRect.y + sceneRect.height / 2.0f};
  cam.target = {0.0f, 0.0f};
  cam.rotation = 0.0f;
  cam.zoom = 1.0f;

  // Update camera offset to the center of the scene view (important!)
  GetScene().AttachCamera2D(cam);

  DrawRectangleRec(sceneRect, BLACK);

  // Let engine render inside
  BeginScissorMode(leftPanelWidth, 0, sceneRect.width, sceneRect.height);
  // Debug: draw grid and origin so you can see things are visible

  GetScene().Render(GetRenderer()); // expose renderer getter
  // highlight selected entity
  if (selectedEntityId.has_value()) {
    auto &transform =
        GetScene().GetComponent<criogenio::Transform>(selectedEntityId.value());
    // Draw Rectangle on the spritedanimatin considering the Screen position
    auto word_position = GetWorldToScreen2D(Vector2{transform.x, transform.y},
                                            GetScene().maincamera);
    DrawRectangleLines(word_position.x, word_position.y, 64, 64, YELLOW);
    // DrawRectangleLines(GetScreenToWorld2D(transform.x - 2,
    // GetScene().maincamera), GetScreenToWorld2D(transform.y - 2,
    // GetScene().maincamera), 36, 36, YELLOW);
  }
  EndScissorMode();
}

void EditorApp::DrawHierarchyPanel() {
  DrawRectangle(0, 0, leftPanelWidth, GetScreenHeight(), {40, 40, 40, 255});
  DrawText("Entities", 10, 10, 20, RAYWHITE);

  int y = 40;
  DrawButton(10, y, 180, 25, "Create Terrain", [&]() {
    auto &t =
        GetScene().CreateTerrain2D("Terrain", "editor/assets/terrain.jpg");
  });
  y += 40;

  DrawButton(10, y, 180, 25, "Create Animated Entity", [&]() {
    int id = GetScene().CreateEntity("New Entity");
    GetScene().AddComponent<criogenio::Transform>(id, 0.0f, 0.0f);
    auto texture = LoadTexture("editor/assets/Woman/64X128_Idle_Free.png");
    if (!texture.id) {
      printf("Failed to load texture for animated sprite\n");
      return;
    }
    /// Make this data-driven later
    std::vector<Rectangle> idleDown = {
        {0, 0, 64, 128},
        {64, 0, 64, 128},
        {128, 0, 64, 128},
    };

    std::vector<Rectangle> idleLeft = {
        {0, 128, 64, 128},
        {64, 128, 64, 128},
        {128, 128, 64, 128},
    };

    std::vector<Rectangle> idleRight = {
        {0, 256, 64, 128},
        {64, 256, 64, 128},
        {128, 256, 64, 128},
    };

    std::vector<Rectangle> idleUp = {
        {0, 384, 64, 128},
        {64, 384, 64, 128},
        {128, 384, 64, 128},

    };

    auto *anim = GetScene().AddComponent<criogenio::AnimatedSprite>(
        id,
        "idleDown", // initial animation
        idleDown,   // frames
        0.15f,      // speed
        texture);

    anim->AddAnimation("idleUp", idleUp, 0.15f);
    anim->AddAnimation("idleLeft", idleLeft, 0.15f);
    anim->AddAnimation("idleRight", idleRight, 0.15f);
  });
  y += 40;
  for (int entityId : GetScene().GetEntitiesWith<criogenio::Name>()) {
    auto &name = GetScene().GetComponent<Name>(entityId);
    bool isSelected =
        selectedEntityId.has_value() && selectedEntityId.value() == entityId;
    Color textColor = isSelected ? YELLOW : WHITE;
    DrawText(name.name.c_str(), 10, y, 18, textColor);
    Rectangle rect = {(float)10, (float)y, 180.0f, 20.0f};
    if (CheckCollisionPointRec(GetMousePosition(), rect)) {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        selectedEntityId = entityId;
      }
    }
    y += 30;
  }
}

void EditorApp::DrawInspectorPanel() {
  int x = GetScreenWidth() - rightPanelWidth;

  DrawRectangle(x, 0, rightPanelWidth, GetScreenHeight(), {40, 40, 40, 255});
  DrawText("Inspector", x + 10, 10, 20, RAYWHITE);

  if (!selectedEntityId.has_value())
    return;

  auto &name = GetScene().GetComponent<Name>(selectedEntityId.value());

  char buf[128];
  snprintf(buf, sizeof(buf), "Entity: %s", name.name.c_str());
  // DrawText(buf, x + 10, 50, 18, WHITE);

  // Change the entity name by clicking in text
  DrawInput(x + 20, 50, 18, 18, buf);

  // Transform fields
  DrawText("Position:", x + 10, 100, 16, WHITE);

  auto &transform =
      GetScene().GetComponent<criogenio::Transform>(selectedEntityId.value());
  DrawText(TextFormat("X: %.1f", transform.x), x + 20, 130, 16, WHITE);
  DrawText(TextFormat("Y: %.1f", transform.y), x + 20, 160, 16, WHITE);

  // More components can be added here later
  auto &animSprite = GetScene().GetComponent<criogenio::AnimatedSprite>(
      selectedEntityId.value());
  DrawText("Animated Sprite:", x + 10, 200, 16, WHITE);
  DrawText(TextFormat("Current Anim: %s", animSprite.currentAnim.c_str()),
           x + 20, 230, 16, WHITE);
  DrawText(TextFormat("Frame Index: %d", animSprite.frameIndex), x + 20, 260,
           16, WHITE);
  DrawText(TextFormat("Timer: %.2f", animSprite.timer), x + 20, 290, 16, WHITE);
  DrawText(TextFormat("Total Anims: %d", (int)animSprite.animations.size()),
           x + 20, 320, 16, WHITE);
  // Select animation
  int y = 350;
  Color color = WHITE;
  for (const auto &pair : animSprite.animations) {
    const std::string &animName = pair.first;
    // Make button colored with animnation is selected
    if (animSprite.currentAnim == animName) {
      color = YELLOW;
    } else {
      color = WHITE;
    }
    DrawButton(x + 20, y, rightPanelWidth - 40, 25, animName.c_str(),
               [&]() { animSprite.SetAnimation(animName); });
    DrawRectangleLines(x + 20, y, rightPanelWidth - 40, 30, color);
    y += 35;
  }
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

    for (int entityId : GetScene().GetEntitiesWith<criogenio::Transform>()) {
      auto &transform = GetScene().GetComponent<criogenio::Transform>(entityId);
      // Assuming each entity has a 32x32 size for selection purposes
      Rectangle r = {transform.x, transform.y, 64.0f,
                     64.0f}; // world-space rect
      if (CheckCollisionPointRec(worldMouse, r)) {
        selectedEntityId = entityId;
        return;
      }
    }
  }
}

void EditorApp::HandleEntityDrag() {
  if (!selectedEntityId.has_value())
    return;
  if (!IsMouseInSceneView())
    return;

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
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

    auto &transform =
        GetScene().GetComponent<criogenio::Transform>(selectedEntityId.value());
    transform.x += drag.x;
    transform.y += drag.y;
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
  if (selectedEntityId.has_value()) {
    auto &nameComp =
        GetScene().GetComponent<criogenio::Name>(selectedEntityId.value());
    DrawText(nameComp.name.c_str(), 10, 10, 20, WHITE);
  }
}
