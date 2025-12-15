#include "editor.h"

#include "input.h"

#include "raymath.h"
#include "terrain.h"
#include <cstddef>
#include <optional>

// #define RAYGUI_IMPLEMENTATION
//  #include "raygui.h"

using namespace criogenio;

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  // rlImGuiSetup(true); // ImGui initialization only in editor
  //  auto t = new Terrain();
  // GetWorld().SetTerrain(t);
  GetWorld().AddSystem<MovementSystem>(GetWorld());
  GetWorld().AddSystem<AnimationSystem>(GetWorld());
  GetWorld().AddSystem<RenderSystem>(GetWorld());
  GetWorld().AddSystem<AIMovementSystem>(GetWorld());
}

// #TODO:(maraujo) Move this to Engine
Vector2 EditorApp::GetMouseWorld() {
  return GetScreenToWorld2D(GetMousePosition(), GetWorld().maincamera);
}

void EditorApp::Run() {
  ToggleFullscreen();
  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    HandleMouseSelection();
    HandleEntityDrag();
    HandleInput();
    GetWorld().Update(dt);
    BeginDrawing();
    ClearBackground(DARKGRAY);
    DrawWorldView();
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

void EditorApp::DrawWorldView() {
  Rectangle WorldRect = {
      (float)leftPanelWidth, 0,
      (float)(GetScreenWidth() - leftPanelWidth - rightPanelWidth),
      (float)GetScreenHeight()};

  // Background for World view
  DrawRectangleRec(WorldRect, BLACK);
  Camera2D cam = Camera2D();
  cam.offset = {WorldRect.x + WorldRect.width / 2.0f,
                WorldRect.y + WorldRect.height / 2.0f};
  cam.target = {0.0f, 0.0f};
  cam.rotation = 0.0f;
  cam.zoom = 1.0f;

  // Update camera offset to the center of the World view (important!)
  GetWorld().AttachCamera2D(cam);

  // DrawRectangleRec(WorldRect, BLACK);

  // Let engine render inside
  BeginScissorMode(leftPanelWidth, 0, WorldRect.width, WorldRect.height);
  // Debug: draw grid and origin so you can see things are visible
  GetWorld().Render(GetRenderer()); // expose renderer getter
  // highlight selected entity
  if (selectedEntityId.has_value()) {
    auto &transform =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
    // Draw Rectangle on the spritedanimatin considering the Screen position
    auto word_position = GetWorldToScreen2D(Vector2{transform.x, transform.y},
                                            GetWorld().maincamera);
    auto &AnimatedSprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(
        selectedEntityId.value());
    int height = 32;
    int widht = 32;

    if (AnimatedSprite.animations.size() > 0) {
      auto frame = AnimatedSprite.GetFrame();
      widht = frame.width;
      height = frame.height;
    }

    DrawRectangleLines(word_position.x, word_position.y, widht, height, YELLOW);
  }
  EndScissorMode();
}

void EditorApp::DrawHierarchyPanel() {
  DrawRectangle(0, 0, leftPanelWidth, GetScreenHeight(), {40, 40, 40, 255});
  DrawText("Entities", 10, 10, 20, RAYWHITE);

  int y = 40;
  DrawButton(10, y, 180, 25, "Create Terrain", [&]() {
    auto &t =
        GetWorld().CreateTerrain2D("Terrain", "editor/assets/terrain.jpg");
  });
  y += 40;

  DrawButton(10, y, 180, 25, "Create Animated Entity", [&]() {
    int id = GetWorld().CreateEntity("New Entity");
    GetWorld().AddComponent<criogenio::Transform>(id, 0.0f, 0.0f);
    auto texture = LoadTexture("editor/assets/Woman/woman.png");
    if (!texture.id) {
      printf("Failed to load texture for animated sprite\n");
      return;
    }

    /// Make this data-driven later
    std::vector<Rectangle> idleDown = {
        {0, 0, 64, 128},   {64, 0, 64, 128},  {128, 0, 64, 128},
        {192, 0, 64, 128}, {256, 0, 64, 128}, {320, 0, 64, 128},
        {384, 0, 64, 128}, {448, 0, 64, 128}, {512, 0, 64, 128},
    };

    std::vector<Rectangle> idleLeft = {
        {0, 128, 64, 128},   {64, 128, 64, 128},  {128, 128, 64, 128},
        {192, 128, 64, 128}, {256, 128, 64, 128}, {320, 128, 64, 128},
        {384, 128, 64, 128}, {448, 128, 64, 128}, {512, 128, 64, 128},
    };

    std::vector<Rectangle> idleRight = {
        {0, 256, 64, 128},   {64, 256, 64, 128},  {128, 256, 64, 128},
        {192, 256, 64, 128}, {256, 256, 64, 128}, {320, 256, 64, 128},
        {384, 256, 64, 128}, {448, 256, 64, 128}, {512, 256, 64, 128},
    };

    std::vector<Rectangle> idleUp = {
        {0, 384, 64, 128},   {64, 384, 64, 128},  {128, 384, 64, 128},
        {192, 384, 64, 128}, {256, 384, 64, 128}, {320, 384, 64, 128},
        {384, 384, 64, 128}, {448, 384, 64, 128}, {512, 384, 64, 128},

    };
    auto *anim = GetWorld().AddComponent<criogenio::AnimatedSprite>(
        id,
        "idle_down", // initial animation
        idleDown,    // frames
        0.10f,       // speed
        texture);

    anim->AddAnimation("idle_up", idleUp, 0.10f);
    anim->AddAnimation("idle_left", idleLeft, 0.10f);
    anim->AddAnimation("idle_right", idleRight, 0.10f);

    // Add walking animation, should start after 64 pixels in y axis
    std::vector<Rectangle> walkDown = {
        {0, 512, 64, 128},   {64, 512, 64, 128},  {128, 512, 64, 128},
        {192, 512, 64, 128}, {256, 512, 64, 128}, {320, 512, 64, 128},
        {384, 512, 64, 128}, {448, 512, 64, 128}, {512, 512, 64, 128},
    };
    anim->AddAnimation("walk_down", walkDown, 0.1f);
    std::vector<Rectangle> walkLeft = {
        {0, 640, 64, 128},   {64, 640, 64, 128},  {128, 640, 64, 128},
        {192, 640, 64, 128}, {256, 640, 64, 128}, {320, 640, 64, 128},
        {384, 640, 64, 128}, {448, 640, 64, 128}, {512, 640, 64, 128},
    };
    anim->AddAnimation("walk_left", walkLeft, 0.1f);
    std::vector<Rectangle> walkRight = {
        {0, 768, 64, 128},   {64, 768, 64, 128},  {128, 768, 64, 128},
        {192, 768, 64, 128}, {256, 768, 64, 128}, {320, 768, 64, 128},
        {384, 768, 64, 128}, {448, 768, 64, 128}, {512, 768, 64, 128},
    };
    anim->AddAnimation("walk_right", walkRight, 0.1f);
    std::vector<Rectangle> walkUp = {
        {0, 896, 64, 128},   {64, 896, 64, 128},  {128, 896, 64, 128},
        {192, 896, 64, 128}, {256, 896, 64, 128}, {320, 896, 64, 128},
        {384, 896, 64, 128}, {448, 896, 64, 128}, {512, 896, 64, 128},
    };
    anim->AddAnimation("walk_up", walkUp, 0.1f);

    // TODO:inter dependent of animatin sprited component
    GetWorld().AddComponent<AnimationState>(id);
  });
  y += 40;
  for (int entityId : GetWorld().GetEntitiesWith<criogenio::Name>()) {
    auto &name = GetWorld().GetComponent<Name>(entityId);
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
    auto ids = GetWorld().GetEntitiesWith<AIController>();
    if (selectedEntityId.has_value()) {
      for (auto id : ids) {
        auto &ctrl = GetWorld().GetComponent<AIController>(id);
        ctrl.entityTarget = selectedEntityId.value();
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

  auto &name = GetWorld().GetComponent<Name>(selectedEntityId.value());

  char buf[128];
  snprintf(buf, sizeof(buf), "Entity: %s", name.name.c_str());
  // DrawText(buf, x + 10, 50, 18, WHITE);

  // Change the entity name by clicking in text
  DrawInput(x + 20, 50, 18, 18, buf);

  // Transform fields
  DrawText("Position:", x + 10, 100, 16, WHITE);

  auto &transform =
      GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
  DrawText(TextFormat("X: %.1f", transform.x), x + 20, 130, 16, WHITE);
  DrawText(TextFormat("Y: %.1f", transform.y), x + 20, 160, 16, WHITE);

  // More components can be added here later
  auto &animSprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(
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

  // Draw button to switch Player tag and add controller component
  DrawButton(x + 20, y + 20, rightPanelWidth - 40, 25, "Add Player Controller",
             [&]() {
               // Check if entity already has controller
               try {
                 GetWorld().GetComponent<criogenio::Controller>(
                     selectedEntityId.value());
                 // If found, do nothing
                 return;
               } catch (const std::runtime_error &e) {
                 // Not found, add component
                 GetWorld().AddComponent<criogenio::Controller>(
                     selectedEntityId.value(), 200.0f);
               }
             });
  // I need to start to integrate some kind of interface
  //  BUG removing player controller
  DrawButton(x + 20, y + 70, rightPanelWidth - 40, 25,
             "Remove Player Controller", [&]() {
               // Check if entity has controller
               try {
                 GetWorld().RemoveComponent<criogenio::Controller>(
                     selectedEntityId.value());
               } catch (const std::runtime_error &e) {
                 // Not found, do nothing
                 return;
               }
             });

  // Add Button to add AIController
  DrawButton(x + 20, y + 140, rightPanelWidth - 40, 25, "Add AI Controller",
             [&]() {
               // Check if entity has aicontroller
               try {

                 GetWorld().GetComponent<criogenio::AIController>(
                     selectedEntityId.value());
                 // If found, do nothing
                 return;

               } catch (const std::runtime_error &e) {
                 // Not found add
                 auto ai = GetWorld().AddComponent<criogenio::AIController>(
                     selectedEntityId.value());
               }
             });

  DrawButton(x + 20, y + 220, rightPanelWidth - 40, 25, "Remove AI Controller",
             [&]() {
               // Check if entity has controller
               try {
                 GetWorld().RemoveComponent<criogenio::AIController>(
                     selectedEntityId.value());
               } catch (const std::runtime_error &e) {
                 // Not found, do nothing
                 return;
               }
             });

  // Sow information about controller if present
  try {
    auto &controller = GetWorld().GetComponent<criogenio::Controller>(
        selectedEntityId.value());
    DrawText("Controller Component:", x + 10, y + 120, 16, WHITE);
    DrawText(TextFormat("Speed: %.1f", controller.speed), x + 20, y + 150, 16,
             WHITE);
  } catch (const std::runtime_error &e) {
    // Not found, do nothing
  }
}

bool EditorApp::IsMouseInWorldView() {
  int mx = GetMouseX();
  int my = GetMouseY();
  return mx > leftPanelWidth && mx < GetScreenWidth() - rightPanelWidth;
}

void EditorApp::HandleMouseSelection() {
  if (!IsMouseInWorldView())
    return;

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    // Convert mouse position to world coordinates using the World camera
    Vector2 mouseScreen = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouseScreen, GetWorld().maincamera);
    selectedEntityId.reset();

    for (int entityId : GetWorld().GetEntitiesWith<criogenio::Transform>()) {
      auto &transform = GetWorld().GetComponent<criogenio::Transform>(entityId);
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
  if (!IsMouseInWorldView())
    return;

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    // Convert previous and current mouse positions to world coordinates,
    // so drag works correctly under zoom and pan.
    Vector2 mouseScreen = GetMousePosition();
    Vector2 prevMouseScreen = {mouseScreen.x - GetMouseDelta().x,
                               mouseScreen.y - GetMouseDelta().y};

    // TODO:(maraujo) Do I need a editor camera here?
    Vector2 prevWorld =
        GetScreenToWorld2D(prevMouseScreen, GetWorld().maincamera);
    Vector2 currWorld = GetScreenToWorld2D(mouseScreen, GetWorld().maincamera);

    Vector2 drag = Vector2Subtract(currWorld, prevWorld);

    auto &transform =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
    transform.x += drag.x;
    transform.y += drag.y;
  }
}

void EditorApp::HandleInput() {
  if (selectedEntityId.has_value()) {
    return;
  }

  // World VIEW CAMERA ONLY IF MOUSE IS INSIDE World VIEW
  if (!IsMouseInWorldView())
    return;

  float dt = GetFrameTime();

  // ------------------------------------------
  // ZOOM (scroll wheel)
  // ------------------------------------------
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    float zoomSpeed = 0.1f;
    GetWorld().maincamera.zoom += wheel * zoomSpeed;
    if (GetWorld().maincamera.zoom < 0.1f)
      GetWorld().maincamera.zoom = 0.1f;
    if (GetWorld().maincamera.zoom > 8.0f)
      GetWorld().maincamera.zoom = 8.0f;
  }

  // ------------------------------------------
  // PAN WITH MIDDLE MOUSE BUTTON
  // ------------------------------------------
  if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    Vector2 delta = GetMouseDelta();
    GetWorld().maincamera.target.x -= delta.x / GetWorld().maincamera.zoom;
    GetWorld().maincamera.target.y -= delta.y / GetWorld().maincamera.zoom;
  }

  // ------------------------------------------
  // RIGHT MOUSE DRAG ALSO PANS
  // ------------------------------------------
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 delta = GetMouseDelta();
    GetWorld().maincamera.target.x -= delta.x / GetWorld().maincamera.zoom;
    GetWorld().maincamera.target.y -= delta.y / GetWorld().maincamera.zoom;
  }

  // ------------------------------------------
  // WASD MOVEMENT
  // ------------------------------------------
  float speed = 500.0f * dt;
  if (IsKeyDown(KEY_W))
    GetWorld().maincamera.target.y -= speed;
  if (IsKeyDown(KEY_S))
    GetWorld().maincamera.target.y += speed;
  if (IsKeyDown(KEY_A))
    GetWorld().maincamera.target.x -= speed;
  if (IsKeyDown(KEY_D))
    GetWorld().maincamera.target.x += speed;
}

void EditorApp::OnGUI() {
  if (selectedEntityId.has_value()) {
    auto &nameComp =
        GetWorld().GetComponent<criogenio::Name>(selectedEntityId.value());
    DrawText(nameComp.name.c_str(), 10, 10, 20, WHITE);
  }
}
