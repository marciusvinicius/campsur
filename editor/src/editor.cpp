#include "editor.h"
#include "input.h"
#include "raymath.h"
#include "terrain.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <cstdio>

using namespace criogenio;

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  GetWorld().AddSystem<MovementSystem>(GetWorld());
  GetWorld().AddSystem<AnimationSystem>(GetWorld());
  GetWorld().AddSystem<RenderSystem>(GetWorld());
  GetWorld().AddSystem<AIMovementSystem>(GetWorld());
}

// EditorApp::~EditorApp() { rlImGuiShutdown(); }

// #TODO:(maraujo) Move this to Engine
Vector2 EditorApp::GetMouseWorld() {
  return GetScreenToWorld2D(GetMousePosition(), GetWorld().maincamera);
}

void EditorApp::InitImGUI() {
  rlImGuiSetup(true); // creates context + renderer + backend

  IMGUI_CHECKVERSION();
  ImGuiIO &io = ImGui::GetIO();

  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();

  // ---- Fonts (base font FIRST) ----
  ImFont *baseFont = io.Fonts->AddFontFromFileTTF(
      "editor/assets/fonts/Poppins-Black.ttf", 16.0f);

  IM_ASSERT(baseFont != nullptr);

  io.Fonts->Build();
}

void EditorApp::Run() {

  InitImGUI();
  bool firstFrame = false;
  // TODO:(maraujo) clean this code dude
  while (!WindowShouldClose()) {
    if (firstFrame == false) {
      firstFrame = true;
      sceneRT = LoadRenderTexture(width, height);
    }

    float dt = GetFrameTime();
    // HandleMouseSelection();
    HandleEntityDrag();
    HandleScenePicking();
    HandleInput();

    Camera2D &cam = GetWorld().maincamera;
    cam.offset = {sceneRT.texture.width * 0.5f, sceneRT.texture.height * 0.5f};
    cam.target = {0.0f, 0.0f};
    cam.rotation = 0.0f;
    cam.zoom = 1.0f;
    GetWorld().Update(dt);

    // TODO:(maraujo) I need this?
    //  === DRAW ===
    RenderSceneToTexture();
    BeginDrawing();
    ClearBackground(BLUE);
    OnGUI();
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

  GetWorld().maincamera.offset = {sceneRT.texture.width * 0.5f,
                                  sceneRT.texture.height * 0.5f};
  cam.target = {0.0f, 0.0f};
  cam.rotation = 0.0f;
  cam.zoom = 1.0f;

  // Update camera offset to the center of the World view (important!)
  GetWorld().AttachCamera2D(cam);

  // Let engine render inside
  BeginScissorMode(leftPanelWidth, 0, WorldRect.width, WorldRect.height);

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

  // Debug: draw grid and origin so you can see things are visible
  GetWorld().Render(GetRenderer()); // expose renderer getter

  EndScissorMode();
}

void EditorApp::DrawHierarchyPanel() {
  if (ImGui::Begin("Hierarchy")) {
    ImGui::Text("Scene entities here");

    if (ImGui::BeginPopupContextWindow()) {

      if (ImGui::MenuItem("Create Empty")) {
        // CreateEmptyEntity();
      }

      if (selectedEntityId.has_value()) {
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
          // DeleteEntity(selectedEntityId.value());
        }
      }

      ImGui::EndPopup();

      DrawHierarchyNodes();
    }
  }
  ImGui::End();

  if (ImGui::Begin("Inspector")) {
  }
  ImGui::End();
  if (ImGui::Begin("Viewport")) {

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Resize render texture if needed
    static ImVec2 lastSize = {0, 0};
    if ((int)avail.x != (int)lastSize.x || (int)avail.y != (int)lastSize.y) {
      if (sceneRT.id != 0)
        UnloadRenderTexture(sceneRT);

      if (avail.x > 0 && avail.y > 0)
        sceneRT = LoadRenderTexture((int)avail.x, (int)avail.y);

      lastSize = avail;
    }

    // ---- Compute viewport rect ----
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    ImVec2 wPos = ImGui::GetWindowPos();

    viewportPos = {wPos.x + vMin.x, wPos.y + vMin.y};
    viewportSize = {vMax.x - vMin.x, vMax.y - vMin.y};
    viewportHovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // ---- Draw texture ONCE ----
    if (sceneRT.id != 0 && viewportSize.x > 0 && viewportSize.y > 0) {
      ImGui::Image((ImTextureID)(intptr_t)sceneRT.texture.id, viewportSize,
                   ImVec2(0, 1), ImVec2(1, 0));
    }

    // Context menu
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Create Empty")) {
        // CreateEmptyEntityAtMouse();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();

  /*
  DrawWorldViewawRectangle(0, 0, leftPanelWidth, GetScreenHeight(), {40, 40, 40,
  255}); DrawText("Entities", 10, 10, 20, RAYWHITE);

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
  */
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

  rlImGuiBegin(); // creates ImGui frame
  DrawToolbar();
  DrawMainMenuBar();
  DrawDockSpace();
  // DrawHierarchyPanel();
  rlImGuiEnd(); // renders ImGui
}

void EditorApp::DrawDockSpace() {
  ImGuiIO &io = ImGui::GetIO();
  if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable))
    return;

  ImGuiViewport *viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("EditorDockSpace", nullptr, flags);

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

  ImGui::DockSpace(dockspace_id, ImVec2(0, 0));

  // 🔒 BUILD LAYOUT ONCE
  if (!dockLayoutBuilt) {
    dockLayoutBuilt = true;
    printf("Build dock");

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID dock_main = dockspace_id;
    ImGuiID dock_left, dock_right;

    dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f,
                                            nullptr, &dock_main);

    dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f,
                                             nullptr, &dock_main);

    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Viewport", dock_main);
    ImGui::DockBuilderDockWindow("##Toolbar", dock_main);
    if (ImGui::Button("Click Me")) {
      // Action to take when clicked
    }
    ImGui::DockBuilderFinish(dockspace_id);
  }
  DrawHierarchyPanel();
  ImGui::End();
}

void EditorApp::RenderSceneToTexture() {
  BeginTextureMode(sceneRT);
  ClearBackground(BLACK);
  BeginMode2D(GetWorld().maincamera);
  if (selectedEntityId.has_value()) {

    auto &t =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());

    DrawRectangleLines(t.x, t.y, 64, 64, YELLOW);
  }
  GetWorld().Render(GetRenderer());
  EndMode2D();
  EndTextureMode();
}

void EditorApp::DrawMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene")) {
        // NewScene();
      }
      if (ImGui::MenuItem("Open Scene")) {
        // OpenScene();
      }
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        // SaveScene();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject")) {
      if (ImGui::MenuItem("Create Empty")) {
        // CreateEmptyEntity();
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
      }
      if (ImGui::MenuItem("Sprite")) {
        // CreateSpriteEntity();
      }
      if (ImGui::MenuItem("Terrain")) {
        auto &t =
            GetWorld().CreateTerrain2D("Terrain", "editor/assets/terrain.jpg");
        // CreateTerrain();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
void EditorApp::DrawToolbar() {
  ImGui::Begin("##Toolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);

  if (ImGui::Button("+ Entity")) {
    //  CreateEmptyEntity();
  }

  ImGui::SameLine();

  if (ImGui::Button("▶ Play")) {
    //    EnterPlayMode();
  }

  ImGui::SameLine();

  if (ImGui::Button("■ Stop")) {
    // ExitPlayMode();
  }

  ImGui::End();
}

void EditorApp::DrawEntityNode(int entity) {

  auto &name = GetWorld().GetComponent<criogenio::Name>(entity);
  // auto *hierarchy = GetWorld().TryGetComponent<Hierarchy>(entity);

  // bool hasChildren = hierarchy && !hierarchy->children.empty();

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;

  // if (!hasChildren)
  //   flags |= ImGuiTreeNodeFlags_Leaf;

  if (selectedEntityId == entity)
    flags |= ImGuiTreeNodeFlags_Selected;

  bool open = ImGui::TreeNodeEx((void *)(intptr_t)entity, flags, "%s",
                                name.name.c_str());

  if (ImGui::IsItemClicked()) {
    selectedEntityId = entity;
  }

  // Context menu
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Create Child")) {
      // CreateChildEntity(entity);
    }
    if (ImGui::MenuItem("Delete")) {
      // DeleteEntity(entity);
      ImGui::EndPopup();
      return;
    }
    ImGui::EndPopup();
  }

  // ImGui::EndPopup();

  // if (open && hasChildren) {
  //  for (int child : hierarchy->children) {
  //    DrawEntityNode(child);
  //  }
  ImGui::TreePop();
  //}
}

void EditorApp::DrawHierarchyNodes() {
  // for (int entity : GetWorld().GetEntitiesWith<Hierarchy>()) {
  //   auto &h = GetWorld().GetComponent<Hierarchy>(entity);
  //   if (h.parent == -1) {
  //
  for (int entity : GetWorld().GetEntitiesWith<criogenio::Name>()) {
    DrawEntityNode(entity);
  }
}
/*
void EditorApp::DrawHierarchyNodes() {

  for (int entity : GetWorld().GetEntitiesWith<Name>()) {

    auto &name = GetWorld().GetComponent<Name>(entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                               ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;

    if (selectedEntityId.has_value() && selectedEntityId.value() == entity)
      flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx((void *)(intptr_t)entity, flags, "%s", name.name.c_str());

    // Selection
    if (ImGui::IsItemClicked()) {
      selectedEntityId = entity;
    }

    // Context menu (right-click)
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Delete")) {
        DeleteEntity(entity);
        ImGui::EndPopup();
        return; // entity list changed
      }
      ImGui::EndPopup();
    }
  }
}*/

void EditorApp::HandleScenePicking() {

  if (!viewportHovered)
    return;

  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    return;

  Vector2 mouse = GetMousePosition();

  float vx = viewportPos.x;
  float vy = viewportPos.y;
  float vw = viewportSize.x;
  float vh = viewportSize.y;

  if (mouse.x < vx || mouse.y < vy || mouse.x > vx + vw || mouse.y > vy + vh)
    return;

  // Convert to viewport-local
  Vector2 local;
  local.x = mouse.x - vx;
  local.y = mouse.y - vy;

  // Normalize to render texture space
  Vector2 tex;
  tex.x = local.x * ((float)sceneRT.texture.width / vw);
  tex.y = local.y * ((float)sceneRT.texture.height / vh);

  // Convert to world
  Vector2 world = GetScreenToWorld2D(tex, GetWorld().maincamera);
  PickEntityAt(world);
}

void EditorApp::PickEntityAt(Vector2 worldPos) {

  selectedEntityId.reset();

  for (int entity : GetWorld().GetEntitiesWith<criogenio::Transform>()) {

    auto &t = GetWorld().GetComponent<criogenio::Transform>(entity);

    Rectangle bounds = {
        t.x, t.y, 64, 128 // TODO: use sprite size
    };

    if (CheckCollisionPointRec(worldPos, bounds)) {
      selectedEntityId = entity;
      return;
    }
  }
}
bool EditorApp::IsSceneInputAllowed() const {
  return viewportHovered && !ImGui::IsAnyItemActive() &&
         !ImGui::IsAnyItemHovered();
}
