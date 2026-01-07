#include "editor.h"
#include "animation_database.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "input.h"
#include "raymath.h"
#include "resources.h"
#include "terrain.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

#include <algorithm>
#include <filesystem>

#include "criogenio_io.h"

using namespace criogenio;

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  EditorAppReset();
}

void EditorApp::EditorAppReset() {
  RegisterCoreComponents();
  GetWorld().AddSystem<MovementSystem>(GetWorld());
  GetWorld().AddSystem<AnimationSystem>(GetWorld());
  GetWorld().AddSystem<RenderSystem>(GetWorld());
  GetWorld().AddSystem<AIMovementSystem>(GetWorld());
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

  Camera2D &cam = GetWorld().maincamera;
  cam.offset = {sceneRT.texture.width * 0.5f, sceneRT.texture.height * 0.5f};
  cam.target = {0.0f, 0.0f};
  cam.rotation = 0.0f;
  cam.zoom = 1.0f;

  InitImGUI();
  bool firstFrame = false;
  // TODO:(maraujo) clean this code dude
  while (!WindowShouldClose()) {
    if (firstFrame == false) {
      firstFrame = true;
      sceneRT = LoadRenderTexture(width, height);
    }

    float dt = GetFrameTime();

    HandleInput();
    HandleEntityDrag();
    HandleScenePicking();

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

  // Debug: draw grid and origin so you can see things are visible
  GetWorld().Render(GetRenderer()); // expose renderer getter

  EndScissorMode();
}

void EditorApp::DrawHierarchyPanel() {
  if (ImGui::Begin("Hierarchy")) {
    for (auto id : GetWorld().GetAllEntities()) {
      bool selected =
          (selectedEntityId.has_value() && selectedEntityId.value() == id);

      char label[64];
      std::string entity_name = "";
      if (auto *nameComp = GetWorld().GetComponent<criogenio::Name>(id)) {
        entity_name = nameComp->name;
      }
      if (entity_name.empty()) {
        entity_name = "Test";
      }

      snprintf(label, sizeof(label), "Entity %s", entity_name.c_str());

      if (ImGui::Selectable(label, selected)) {
        selectedEntityId = id;
      }

      // --- Right-click context menu ---
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete Entity")) {
          // Clean up animation references if entity has AnimatedSprite
          if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(id)) {
            if (sprite->animationId != INVALID_ASSET_ID) {
              AnimationDatabase::instance().removeReference(sprite->animationId);
            }
          }
          GetWorld().DeleteEntity(id);
          if (selectedEntityId.has_value() && selectedEntityId.value() == id) {
            selectedEntityId.reset();
          }
          ImGui::EndPopup();
          break;
        }
        ImGui::EndPopup();
      }
    }

    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Create Empty")) {
        // CreateEmptyEntity();
      }
      if (selectedEntityId.has_value()) {
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Entity")) {
          int entityToDelete = selectedEntityId.value();
          // Clean up animation references if entity has AnimatedSprite
          if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(entityToDelete)) {
            if (sprite->animationId != INVALID_ASSET_ID) {
              AnimationDatabase::instance().removeReference(sprite->animationId);
            }
          }
          GetWorld().DeleteEntity(entityToDelete);
          selectedEntityId.reset();
        }
      }

      ImGui::EndPopup();
      // DrawHierarchyNodes();
    }
  }
  ImGui::End();

  if (ImGui::Begin("Inspector")) {

    if (selectedEntityId.has_value()) {

      int entity = selectedEntityId.value();
      DrawEntityHeader(entity);
      ImGui::Separator();
      DrawComponentInspectors(entity);
      ImGui::Separator();
      DrawAddComponentMenu(entity);
    }
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
        CreateEmptyEntityAtMouse();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();
}

bool EditorApp::IsMouseInWorldView() {
  int mx = GetMouseX();
  int my = GetMouseY();
  return mx > leftPanelWidth && mx < GetScreenWidth() - rightPanelWidth;
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

    auto *transform =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
    if (transform) {
      transform->x += drag.x;
      transform->y += drag.y;
    }
  }
}

void EditorApp::HandleInput() {
  if (selectedEntityId.has_value())
    return;

  if (!viewportHovered)
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

  DrawDockSpace();
  DrawTerrainEditor();
  // DrawToolbar();
  DrawMainMenuBar();
  
  // File browser popup (must be rendered every frame when visible)
  DrawFileBrowserPopup();
  
  rlImGuiEnd(); // renders ImGui

  // Grid overlay is drawn inside RenderSceneToTexture() in world space
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
    if (auto *t = GetWorld().GetComponent<criogenio::Transform>(
            selectedEntityId.value())) {
      DrawRectangleLines(t->x, t->y, 64, 64, YELLOW);
    }
  }
  GetWorld().Render(GetRenderer());

  // Draw terrain grid overlay (editor-only, in world space)
  if (terrainEditMode) {
    DrawTerrainGridOverlay();
  }

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
        criogenio::LoadWorldFromFile(GetWorld(), "world.json");
        EditorAppReset();
      }
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        criogenio::SaveWorldToFile(GetWorld(), "world.json");
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

        auto path = "/home/marcius/Workspace/criogenio/camp_sur_cpp/editor/"
                    "assets/raw/world.png";
        auto texture = LoadTexture(path);
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

        // Build an animation definition in the central AnimationDatabase
        auto animId = AnimationDatabase::instance().createAnimation(path);

        auto makeClip = [](const std::string &name,
                           const std::vector<Rectangle> &rects, float speed) {
          criogenio::AnimationClip clip;
          clip.name = name;
          clip.frameSpeed = speed;
          clip.frames.reserve(rects.size());
          for (const auto &r : rects) {
            criogenio::AnimationFrame f;
            f.rect = r;
            clip.frames.push_back(f);
          }
          return clip;
        };

        AnimationDatabase::instance().addClip(
            animId, makeClip("idle_down", idleDown, 0.10f));
        AnimationDatabase::instance().addClip(
            animId, makeClip("idle_up", idleUp, 0.10f));
        AnimationDatabase::instance().addClip(
            animId, makeClip("idle_left", idleLeft, 0.10f));
        AnimationDatabase::instance().addClip(
            animId, makeClip("idle_right", idleRight, 0.10f));

        // Add walking animation, should start after 64 pixels in y axis
        std::vector<Rectangle> walkDown = {
            {0, 512, 64, 128},   {64, 512, 64, 128},  {128, 512, 64, 128},
            {192, 512, 64, 128}, {256, 512, 64, 128}, {320, 512, 64, 128},
            {384, 512, 64, 128}, {448, 512, 64, 128}, {512, 512, 64, 128},
        };

        AnimationDatabase::instance().addClip(
            animId, makeClip("walk_down", walkDown, 0.10f));
        std::vector<Rectangle> walkLeft = {
            {0, 640, 64, 128},   {64, 640, 64, 128},  {128, 640, 64, 128},
            {192, 640, 64, 128}, {256, 640, 64, 128}, {320, 640, 64, 128},
            {384, 640, 64, 128}, {448, 640, 64, 128}, {512, 640, 64, 128},
        };
        AnimationDatabase::instance().addClip(
            animId, makeClip("walk_left", walkLeft, 0.10f));
        std::vector<Rectangle> walkRight = {
            {0, 768, 64, 128},   {64, 768, 64, 128},  {128, 768, 64, 128},
            {192, 768, 64, 128}, {256, 768, 64, 128}, {320, 768, 64, 128},
            {384, 768, 64, 128}, {448, 768, 64, 128}, {512, 768, 64, 128},
        };
        AnimationDatabase::instance().addClip(
            animId, makeClip("walk_right", walkRight, 0.10f));
        std::vector<Rectangle> walkUp = {
            {0, 896, 64, 128},   {64, 896, 64, 128},  {128, 896, 64, 128},
            {192, 896, 64, 128}, {256, 896, 64, 128}, {320, 896, 64, 128},
            {384, 896, 64, 128}, {448, 896, 64, 128}, {512, 896, 64, 128},
        };
        AnimationDatabase::instance().addClip(
            animId, makeClip("walk_up", walkUp, 0.10f));

        // Create runtime AnimatedSprite referencing the animation asset
        auto &anim =
            GetWorld().AddComponent<criogenio::AnimatedSprite>(id, animId);
        anim.SetClip("idle_down");
        // track usage
        AnimationDatabase::instance().addReference(animId);

        GetWorld().AddComponent<criogenio::AnimationState>(id);
      }
      if (ImGui::MenuItem("Sprite")) {
        // CreateSpriteEntity();
      }
      if (ImGui::MenuItem("Terrain")) {
        auto &t = GetWorld().CreateTerrain2D(
            "Terrain",
            "editor/assets/mystic_woods_free_2.2/sprites/tilesets/plains.png");
        // CreateTerrain();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  // Terrain painting mode
  if (terrainEditMode) {
    Vector2 mouseScreen = GetMousePosition();
    
    // Check if mouse is within viewport
    if (mouseScreen.x >= viewportPos.x &&
        mouseScreen.x <= viewportPos.x + viewportSize.x &&
        mouseScreen.y >= viewportPos.y &&
        mouseScreen.y <= viewportPos.y + viewportSize.y) {

      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 worldPos = ScreenToWorldPosition(mouseScreen);
        
        auto *terrain = GetWorld().GetTerrain();
        if (terrain) {
          int tileSize = terrain->tileset.tileSize;
          int tx = static_cast<int>(floor(worldPos.x / (float)tileSize));
          int ty = static_cast<int>(floor(worldPos.y / (float)tileSize));
          terrain->SetTile(terrainSelectedLayer, tx, ty, terrainSelectedTile);
        }
      }
    }
  }
}

void EditorApp::CreateEmptyEntityAtMouse() {
  Vector2 mouseScreen = GetMousePosition();
  Vector2 worldPos = GetScreenToWorld2D(mouseScreen, GetWorld().maincamera);

  int id = GetWorld().CreateEntity("New Entity");
  GetWorld().AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
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

void EditorApp::DrawTerrainEditor() {
  ImGui::Begin("Terrain Editor");

  auto *terrain = GetWorld().GetTerrain();
  if (!terrain) {
    if (ImGui::Button("Create Terrain (default)")) {
      GetWorld().CreateTerrain2D("MainTerrain", "editor/assets/terrain.jpg");
    }
    ImGui::End();
    return;
  }

  // Toggle edit mode
  ImGui::Checkbox("Terrain Edit Mode", &terrainEditMode);
  ImGui::SameLine();
  ImGui::Text("Layer:");
  ImGui::SameLine();
  ImGui::InputInt("##layer", &terrainSelectedLayer);

  // Show selected tile info
  ImGui::Separator();
  ImGui::Text("Selected Tile: %d", terrainSelectedTile);
  ImGui::Text("Tile Size: %d x %d", terrain->tileset.tileSize,
              terrain->tileset.tileSize);
  ImGui::Text("Instructions: Click tiles below to select, then paint on "
              "terrain in viewport.");

  // Show tileset as selectable grid
  auto tex = terrain->tileset.atlas;
  if (tex) {
    int tileSize = terrain->tileset.tileSize;
    int cols = terrain->tileset.columns;
    int rows = terrain->tileset.rows;
    int atlasW = tex->texture.width;
    int atlasH = tex->texture.height;

    ImGui::Text("Tileset: %s", terrain->tileset.tilesetPath.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Change Texture")) {
      namespace fs = std::filesystem;
      auto root = fs::path("editor") / "assets";
      fileBrowserEntries.clear();
      try {
        if (fs::exists(root)) {
          for (auto &p : fs::recursive_directory_iterator(root)) {
            if (p.is_regular_file()) {
              std::string ext = p.path().extension().string();
              // Filter for image files
              if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || 
                  ext == ".bmp" || ext == ".gif" || ext == ".tga") {
                fileBrowserEntries.push_back(p.path().string());
              }
            }
          }
          std::sort(fileBrowserEntries.begin(), fileBrowserEntries.end());
        }
      } catch (...) {
      }
      fileBrowserFilter.clear();
      fileBrowserForTerrain = true;
      fileBrowserTargetEntity = -1; // Not used for terrain
      // Initialize preview with current terrain texture
      fileBrowserPreviewPath = terrain->tileset.tilesetPath;
      if (!fileBrowserPreviewPath.empty()) {
        fileBrowserPreviewTex = AssetManager::instance().load<criogenio::TextureResource>(fileBrowserPreviewPath);
      } else {
        fileBrowserPreviewTex = nullptr;
      }
      fileBrowserVisible = true;
      ImGui::OpenPopup("File Browser");
    }
    ImGui::Separator();

    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < cols; ++x) {
        int idx = y * cols + x;
        float u0 = (x * tileSize) / (float)atlasW;
        float v0 = (y * tileSize) / (float)atlasH;
        float u1 = ((x + 1) * tileSize) / (float)atlasW;
        float v1 = ((y + 1) * tileSize) / (float)atlasH;

        ImGui::PushID(idx);
        ImVec2 size(32, 32);
        ImTextureID id = (ImTextureID)(intptr_t)tex->texture.id;
        // ImGui 1.89+ requires an explicit string ID first; we use an empty
        // label and rely on PushID/PopID for uniqueness.
        bool selected = (terrainSelectedTile == idx);
        if (selected) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
        }
        if (ImGui::ImageButton("", id, size, ImVec2(u0, v0), ImVec2(u1, v1))) {
          terrainSelectedTile = idx;
        }
        if (selected) {
          ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::PopID();
      }
      ImGui::NewLine();
    }
  } else {
    ImGui::TextDisabled("Tileset atlas not loaded");
  }

  ImGui::End();
}

void EditorApp::DrawTerrainGridOverlay() {
  // Draw grid overlay directly in world space (called inside BeginMode2D)
  auto *terrain = GetWorld().GetTerrain();
  if (!terrain || terrain->layers.empty())
    return;

  int tileSize = terrain->tileset.tileSize;
  int cols = 10; // default
  int rows = 10;

  // Get the layer bounds
  int selectedLayer = terrainSelectedLayer;
  if (selectedLayer < 0 ||
      selectedLayer >= static_cast<int>(terrain->layers.size())) {
    selectedLayer = 0;
  }

  if (selectedLayer < static_cast<int>(terrain->layers.size())) {
    auto &layer = terrain->layers[selectedLayer];
    cols = layer.width;
    rows = layer.height;
  }

  // Draw vertical grid lines (world space, already inside BeginMode2D)
  // Draw lines at exact tile boundaries - use thin rectangles for precise alignment
  float lineWidth = 1.0f;
  Color gridColor = Color{100, 200, 255, 150};
  float extend = 10.0f; // Extend grid slightly beyond terrain for visibility
  
  // Draw vertical lines at tile boundaries (including edges)
  for (int x = 0; x <= cols; ++x) {
    float xPos = (float)x * tileSize;
    // Draw a thin vertical rectangle centered on the boundary
    DrawRectangleRec(
      {xPos - lineWidth * 0.5f, -extend, lineWidth, (float)rows * tileSize + extend * 2.0f},
      gridColor
    );
  }

  // Draw horizontal lines at tile boundaries (including edges)
  for (int y = 0; y <= rows; ++y) {
    float yPos = (float)y * tileSize;
    // Draw a thin horizontal rectangle centered on the boundary
    DrawRectangleRec(
      {-extend, yPos - lineWidth * 0.5f, (float)cols * tileSize + extend * 2.0f, lineWidth},
      gridColor
    );
  }

  // Draw current tile preview at mouse cursor
  Vector2 mouseScreen = GetMousePosition();
  Vector2 worldPos = ScreenToWorldPosition(mouseScreen);
  
  // Calculate tile coordinates (snap to grid)
  int tx = static_cast<int>(floor(worldPos.x / (float)tileSize));
  int ty = static_cast<int>(floor(worldPos.y / (float)tileSize));

  if (tx >= 0 && tx < cols && ty >= 0 && ty < rows) {
    // Calculate tile position in world space (aligned to grid)
    Vector2 tilePos = {(float)tx * tileSize, (float)ty * tileSize};

    // Highlight the tile under cursor with a green border (world space)
    DrawRectangleLinesEx(
        {tilePos.x, tilePos.y, (float)tileSize, (float)tileSize}, 2.0f,
        Color{0, 255, 0, 220});
  }
}

void EditorApp::DrawEntityNode(int entity) {

  auto *name = GetWorld().GetComponent<criogenio::Name>(entity);
  if (!name)
    return;

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;

  if (selectedEntityId == entity)
    flags |= ImGuiTreeNodeFlags_Selected;

  bool open = ImGui::TreeNodeEx((void *)(intptr_t)entity, flags, "%s",
                                name->name.c_str());

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

    auto *name = GetWorld().GetComponent<Name>(entity);
    if (!name) continue;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                               ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;

    if (selectedEntityId.has_value() && selectedEntityId.value() == entity)
      flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx((void *)(intptr_t)entity, flags, "%s",
name.name.c_str());

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

    auto *t = GetWorld().GetComponent<criogenio::Transform>(entity);
    if (!t)
      continue;

    Rectangle bounds = {
        t->x, t->y, 64, 128 // TODO: use sprite size
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

Vector2 EditorApp::ScreenToWorldPosition(Vector2 mouseScreen) {
  // Check if mouse is within viewport
  if (mouseScreen.x < viewportPos.x || mouseScreen.x > viewportPos.x + viewportSize.x ||
      mouseScreen.y < viewportPos.y || mouseScreen.y > viewportPos.y + viewportSize.y) {
    return {0, 0}; // Return invalid position if outside viewport
  }
  
  // Convert screen coordinates to viewport coordinates
  Vector2 viewportMouse = {
    mouseScreen.x - viewportPos.x,
    mouseScreen.y - viewportPos.y
  };
  
  // Convert viewport coordinates to normalized texture coordinates (0-1)
  Vector2 normalizedPos = {
    viewportMouse.x / viewportSize.x,
    viewportMouse.y / viewportSize.y
  };
  
  // Convert normalized coordinates to texture screen coordinates
  // Note: Texture Y is flipped (0 at top in ImGui, 0 at bottom in texture)
  Vector2 textureScreen = {
    normalizedPos.x * (float)sceneRT.texture.width,
    (1.0f - normalizedPos.y) * (float)sceneRT.texture.height
  };
  
  // Convert texture screen coordinates to world coordinates using camera
  return GetScreenToWorld2D(textureScreen, GetWorld().maincamera);
}

void EditorApp::DrawEntityHeader(int entity) {
  auto *name = GetWorld().GetComponent<criogenio::Name>(entity);
  if (!name)
    return;

  char buffer[256];
  strncpy(buffer, name->name.c_str(), sizeof(buffer));

  if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
    name->name = buffer;
  }

  // show entity ID
  ImGui::Text("Entity ID: %d", entity);

  // Delete button
  ImGui::SameLine();
  if (ImGui::Button("Delete Entity")) {
    // Clean up animation references if entity has AnimatedSprite
    if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity)) {
      if (sprite->animationId != INVALID_ASSET_ID) {
        AnimationDatabase::instance().removeReference(sprite->animationId);
      }
    }
    GetWorld().DeleteEntity(entity);
    selectedEntityId.reset();
  }

  if (ImGui::BeginPopupContextItem("EntityHeaderContext")) {
    if (ImGui::MenuItem("Delete Entity")) {
      // Clean up animation references if entity has AnimatedSprite
      if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity)) {
        if (sprite->animationId != INVALID_ASSET_ID) {
          AnimationDatabase::instance().removeReference(sprite->animationId);
        }
      }
      GetWorld().DeleteEntity(entity);
      selectedEntityId.reset();
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawComponentInspectors(int entity) {

  if (GetWorld().HasComponent<criogenio::Transform>(entity))
    DrawTransformInspector(entity);

  if (GetWorld().HasComponent<criogenio::AnimationState>(entity))
    DrawAnimationStateInspector(entity);

  if (GetWorld().HasComponent<criogenio::AnimatedSprite>(entity))
    DrawAnimatedSpriteInspector(entity);

  if (GetWorld().HasComponent<criogenio::Controller>(entity))
    DrawControllerInspector(entity);

  if (GetWorld().HasComponent<criogenio::AIController>(entity))
    DrawAIControllerInspector(entity);
}

void EditorApp::DrawAnimationStateInspector(int entity) {

  ImGui::PushID("AnimationState");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Animation State");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::AnimationState>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  
  if (!headerOpen)
    return;

  auto *animState = GetWorld().GetComponent<criogenio::AnimationState>(entity);
  if (!animState)
    return;

  ImGui::Text("Current State: %s",
              criogenio::anim_state_to_string(animState->current).c_str());

  ImGui::Separator();
  ImGui::Text("Previous State:");
  ImGui::Text("%s",
              criogenio::anim_state_to_string(animState->previous).c_str());

  ImGui::Separator();
  ImGui::Text("Current State:");
  for (int i = 0; i < static_cast<int>(criogenio::AnimState::Count); ++i) {
    criogenio::AnimState state = static_cast<criogenio::AnimState>(i);
    bool selected = (animState->current == state);
    if (ImGui::Selectable(criogenio::anim_state_to_string(state).c_str(),
                          selected)) {
      animState->SetState(state);
    }
    if (selected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  ImGui::Text("Current Direction: %s",
              criogenio::direction_to_string(animState->facing).c_str());

  ImGui::Separator();
  ImGui::Text("States:");
  for (int i = 0; i < static_cast<int>(criogenio::Direction::Count); ++i) {
    criogenio::Direction direction = static_cast<criogenio::Direction>(i);
    bool selected = (animState->facing == direction);
    if (ImGui::Selectable(criogenio::direction_to_string(direction).c_str(),
                          selected)) {
      animState->SetDirection(direction);
    }
    if (selected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  if (ImGui::BeginPopupContextItem("AnimStateContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::AnimationState>(entity);
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawTransformInspector(int entity) {

  ImGui::PushID("Transform");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Transform>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  
  if (!headerOpen)
    return;

  auto *t = GetWorld().GetComponent<criogenio::Transform>(entity);
  if (!t)
    return;

  ImGui::DragFloat2("Position", &t->x, 1.0f);
  ImGui::DragFloat("Rotation", &t->rotation, 0.5f);
  ImGui::DragFloat2("Scale", &t->scale_x, 0.01f, 0.01f, 100.0f);

  // Context menu
  if (ImGui::BeginPopupContextItem("TransformContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::Transform>(entity);
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawAnimatedSpriteInspector(int entity) {

  ImGui::PushID("AnimatedSprite");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Animated Sprite");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    // Clean up animation references
    auto *sp = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity);
    if (sp && sp->animationId != INVALID_ASSET_ID) {
      AnimationDatabase::instance().removeReference(sp->animationId);
    }
    GetWorld().RemoveComponent<criogenio::AnimatedSprite>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  
  if (!headerOpen)
    return;

  auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity);
  if (!sprite)
    return;

  ImGui::Text("Current Animation: %s", sprite->currentClipName.empty()
                                           ? "(none)"
                                           : sprite->currentClipName.c_str());

  ImGui::Separator();
  ImGui::Text("Animations:");

  const auto *def =
      AnimationDatabase::instance().getAnimation(sprite->animationId);
  if (!def) {
    ImGui::TextDisabled("No animation data");
  } else {
    for (const auto &clip : def->clips) {
      bool selected = (sprite->currentClipName == clip.name);
      if (ImGui::Selectable(clip.name.c_str(), selected)) {
        sprite->SetClip(clip.name);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Frames: %d, Speed: %.2f", (int)clip.frames.size(),
                          clip.frameSpeed);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
  }

  if (ImGui::BeginPopupContextItem("AnimSpriteContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      // decrement usage if this component owned an animation asset
      auto *sp = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity);
      if (sp && sp->animationId != INVALID_ASSET_ID) {
        AnimationDatabase::instance().removeReference(sp->animationId);
      }
      GetWorld().RemoveComponent<criogenio::AnimatedSprite>(entity);
    }
    ImGui::EndPopup();
  }

  // Texture path editor + reload/apply controls
  const auto *def2 =
      AnimationDatabase::instance().getAnimation(sprite->animationId);
  if (def2) {
    ImGui::Separator();
    ImGui::Text("Texture:");
    char buf[512] = {0};
    auto &tmp = texturePathEdits[entity];
    if (tmp.empty())
      tmp = def2->texturePath;
    strncpy(buf, tmp.c_str(), sizeof(buf) - 1);
    if (ImGui::InputText("Texture Path", buf, sizeof(buf))) {
      tmp = std::string(buf);
    }

    // edit-in-place toggle
    bool inplaceDefault = false;
    if (editInPlace.find(entity) == editInPlace.end())
      editInPlace[entity] = false;
    ImGui::Checkbox("Edit In-Place (affect all instances)",
                    &editInPlace[entity]);

    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
      bool inplace = false;
      auto itEdit = editInPlace.find(entity);
      if (itEdit != editInPlace.end())
        inplace = itEdit->second;

      AssetId oldId = sprite->animationId;
      int refs = AnimationDatabase::instance().getRefCount(oldId);

      if (inplace || refs <= 1) {
        // safe to edit in-place
        std::string oldPath =
            AnimationDatabase::instance().setTexturePath(oldId, tmp);
        if (!oldPath.empty())
          AssetManager::instance().unload(oldPath);
        AssetManager::instance().load<criogenio::TextureResource>(tmp);
      } else {
        // clone and assign to this entity
        AssetId newId = AnimationDatabase::instance().cloneAnimation(oldId);
        if (newId == INVALID_ASSET_ID) {
          newId = AnimationDatabase::instance().createAnimation(tmp);
        } else {
          AnimationDatabase::instance().setTexturePath(newId, tmp);
        }
        // update refcounts
        AnimationDatabase::instance().removeReference(oldId);
        AnimationDatabase::instance().addReference(newId);
        sprite->animationId = newId;
        AssetManager::instance().load<criogenio::TextureResource>(tmp);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
      namespace fs = std::filesystem;
      auto root = fs::path("editor") / "assets";
      fileBrowserEntries.clear();
      try {
        if (fs::exists(root)) {
          for (auto &p : fs::recursive_directory_iterator(root)) {
            if (p.is_regular_file())
              fileBrowserEntries.push_back(p.path().string());
          }
          std::sort(fileBrowserEntries.begin(), fileBrowserEntries.end());
        }
      } catch (...) {
      }
      fileBrowserFilter.clear();
      fileBrowserTargetEntity = entity;
      fileBrowserForTerrain = false;
      fileBrowserVisible = true;
      ImGui::OpenPopup("File Browser");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      if (!def2->texturePath.empty()) {
        AssetManager::instance().unload(def2->texturePath);
        auto res = AssetManager::instance().load<criogenio::TextureResource>(
            def2->texturePath);
        if (!res)
          TraceLog(LOG_WARNING, "Editor: failed to reload texture %s",
                   def2->texturePath.c_str());
      }
    }
  } else {
    if (ImGui::Button("Reload Texture")) {
      // nothing to do
    }
  }

  // File browser popup is now in DrawFileBrowserPopup() - removed from here
}

void EditorApp::DrawFileBrowserPopup() {
  // Simple file browser popup - must be called every frame when visible
  // OpenPopup is called when button is clicked, BeginPopupModal shows it
  if (fileBrowserVisible && ImGui::BeginPopupModal("File Browser", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text(fileBrowserForTerrain ? 
                  "Select terrain texture file (root: editor/assets)" :
                  "Select texture file (root: editor/assets)");
      char filterBuf[256] = {0};
      strncpy(filterBuf, fileBrowserFilter.c_str(), sizeof(filterBuf) - 1);
      if (ImGui::InputText("Filter", filterBuf, sizeof(filterBuf))) {
        fileBrowserFilter = std::string(filterBuf);
      }
      ImGui::Separator();
      ImGui::BeginChild("file_list", ImVec2(700, 300), true);
      for (const auto &entry : fileBrowserEntries) {
        if (!fileBrowserFilter.empty()) {
          if (entry.find(fileBrowserFilter) == std::string::npos)
            continue;
        }
        if (ImGui::Selectable(entry.c_str())) {
          if (fileBrowserForTerrain) {
            fileBrowserPreviewPath = entry; // Store selected path for terrain
          } else {
            texturePathEdits[fileBrowserTargetEntity] = entry;
          }
        }
        if (ImGui::IsItemHovered()) {
          // update preview when hovered
          if (fileBrowserPreviewPath != entry) {
            fileBrowserPreviewPath = entry;
            fileBrowserPreviewTex =
                AssetManager::instance().load<criogenio::TextureResource>(
                    entry);
          }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          if (fileBrowserForTerrain) {
            // Apply terrain texture
            auto *terrain = GetWorld().GetTerrain();
            if (terrain) {
              // Unload old texture
              if (!terrain->tileset.tilesetPath.empty()) {
                AssetManager::instance().unload(terrain->tileset.tilesetPath);
              }
              // Update tileset with new texture
              terrain->tileset.tilesetPath = entry;
              terrain->tileset.atlas = AssetManager::instance().load<criogenio::TextureResource>(entry);
              if (!terrain->tileset.atlas) {
                TraceLog(LOG_WARNING, "Failed to load terrain texture: %s", entry.c_str());
              }
            }
            fileBrowserVisible = false;
            fileBrowserForTerrain = false;
            ImGui::CloseCurrentPopup();
            break;
          } else {
            texturePathEdits[fileBrowserTargetEntity] = entry;
            // apply selection by cloning or editing in-place depending on user's
            // choice
            auto *spr = GetWorld().GetComponent<criogenio::AnimatedSprite>(
                fileBrowserTargetEntity);
            bool inplace = false;
            auto itEdit = editInPlace.find(fileBrowserTargetEntity);
            if (itEdit != editInPlace.end())
              inplace = itEdit->second;

            AssetId oldId = spr->animationId;
            int refs = AnimationDatabase::instance().getRefCount(oldId);

            if (inplace || refs <= 1) {
              std::string oldPath =
                  AnimationDatabase::instance().setTexturePath(oldId, entry);
              if (!oldPath.empty())
                AssetManager::instance().unload(oldPath);
              AssetManager::instance().load<criogenio::TextureResource>(entry);
            } else {
              AssetId newId = AnimationDatabase::instance().cloneAnimation(oldId);
              if (newId == INVALID_ASSET_ID) {
                newId = AnimationDatabase::instance().createAnimation(entry);
              } else {
                AnimationDatabase::instance().setTexturePath(newId, entry);
              }
              AnimationDatabase::instance().removeReference(oldId);
              AnimationDatabase::instance().addReference(newId);
              spr->animationId = newId;
              AssetManager::instance().load<criogenio::TextureResource>(entry);
            }

            fileBrowserVisible = false;
            ImGui::CloseCurrentPopup();
            break;
          }
        }
      }
      ImGui::EndChild();

      // preview pane
      ImGui::SameLine();
      ImGui::BeginChild("preview", ImVec2(260, 300), true);
      ImGui::Text("Preview:");
      if (!fileBrowserPreviewPath.empty() && fileBrowserPreviewTex) {
        // scale to fit into preview area while keeping aspect
        int tw = fileBrowserPreviewTex->texture.width;
        int th = fileBrowserPreviewTex->texture.height;
        float maxW = 240.0f, maxH = 240.0f;
        float scale = std::min(maxW / tw, maxH / th);
        ImGui::Image((ImTextureID)(intptr_t)fileBrowserPreviewTex->texture.id,
                     ImVec2(tw * scale, th * scale));
      } else {
        ImGui::TextDisabled("No preview");
      }
      ImGui::EndChild();

      if (ImGui::Button("OK")) {
        if (fileBrowserForTerrain) {
          // Apply terrain texture
          auto *terrain = GetWorld().GetTerrain();
          if (terrain && !fileBrowserPreviewPath.empty()) {
            // Unload old texture
            if (!terrain->tileset.tilesetPath.empty()) {
              AssetManager::instance().unload(terrain->tileset.tilesetPath);
            }
            // Update tileset with new texture
            terrain->tileset.tilesetPath = fileBrowserPreviewPath;
            terrain->tileset.atlas = AssetManager::instance().load<criogenio::TextureResource>(fileBrowserPreviewPath);
            if (!terrain->tileset.atlas) {
              TraceLog(LOG_WARNING, "Failed to load terrain texture: %s", fileBrowserPreviewPath.c_str());
            }
          }
          fileBrowserForTerrain = false;
        } else {
          auto sel = texturePathEdits[fileBrowserTargetEntity];
          auto *spr = GetWorld().GetComponent<criogenio::AnimatedSprite>(
              fileBrowserTargetEntity);
          bool inplace = false;
          auto itEdit = editInPlace.find(fileBrowserTargetEntity);
          if (itEdit != editInPlace.end())
            inplace = itEdit->second;

          AssetId oldId = spr->animationId;
          int refs = AnimationDatabase::instance().getRefCount(oldId);

          if (inplace || refs <= 1) {
            std::string oldPath =
                AnimationDatabase::instance().setTexturePath(oldId, sel);
            if (!oldPath.empty())
              AssetManager::instance().unload(oldPath);
            AssetManager::instance().load<criogenio::TextureResource>(sel);
          } else {
            AssetId newId = AnimationDatabase::instance().cloneAnimation(oldId);
            if (newId == INVALID_ASSET_ID) {
              newId = AnimationDatabase::instance().createAnimation(sel);
            } else {
              AnimationDatabase::instance().setTexturePath(newId, sel);
            }
            AnimationDatabase::instance().removeReference(oldId);
            AnimationDatabase::instance().addReference(newId);
            spr->animationId = newId;
            AssetManager::instance().load<criogenio::TextureResource>(sel);
          }
        }

        fileBrowserVisible = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        fileBrowserVisible = false;
        fileBrowserForTerrain = false;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
}

void EditorApp::DrawControllerInspector(int entity) {

  ImGui::PushID("Controller");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Player Controller");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Controller>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  
  if (!headerOpen)
    return;

  auto *ctrl = GetWorld().GetComponent<criogenio::Controller>(entity);
  if (!ctrl)
    return;

  ImGui::DragFloat("Speed", &ctrl->speed, 1.0f, 0.0f, 1000.0f);

  if (ImGui::BeginPopupContextItem("ControllerContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::Controller>(entity);
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawAIControllerInspector(int entity) {

  ImGui::PushID("AIController");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Player AI Controller");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::AIController>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  
  if (!headerOpen)
    return;

  auto *ctrl = GetWorld().GetComponent<criogenio::AIController>(entity);
  if (!ctrl)
    return;

  ImGui::DragFloat("Speed", &ctrl->speed, 1.0f, 0.0f, 1000.0f);

  // Add list of entities to choose from
  ImGui::Text("(-1 means no target)");
  // Gather entity names
  std::vector<std::string> entityNames;
  std::vector<int> entityIds;
  entityNames.push_back("None (-1)");
  entityIds.push_back(-1);
  for (auto id : GetWorld().GetAllEntities()) {
    if (id == entity)
      continue; // skip self
    auto *nameComp = GetWorld().GetComponent<criogenio::Name>(id);
    std::string disp = nameComp ? nameComp->name : "";
    entityNames.push_back(disp + " (" + std::to_string(id) + ")");
    entityIds.push_back(id);
  }
  // Find current target index
  int currentIndex = 0;
  for (size_t i = 0; i < entityIds.size(); ++i)
    if (entityIds[i] == ctrl->entityTarget)
      currentIndex = (int)i;
  // Create combo box
  if (ImGui::Combo(
          "Target Entity", &currentIndex,
          [](void *data, int idx, const char **out_text) {
            auto &names = *(std::vector<std::string> *)data;
            *out_text = names[idx].c_str();
            return true;
          },
          (void *)&entityNames, (int)entityNames.size(), 8)) {
    ctrl->entityTarget = entityIds[currentIndex];
  }

  if (ImGui::BeginPopupContextItem("AIControllerContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::AIController>(entity);
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawAddComponentMenu(int entity) {

  if (ImGui::Button("Add Component")) {
    ImGui::OpenPopup("AddComponentPopup");
  }

  if (ImGui::BeginPopup("AddComponentPopup")) {

    if (!GetWorld().HasComponent<criogenio::Transform>(entity)) {
      if (ImGui::MenuItem("Transform")) {
        GetWorld().AddComponent<criogenio::Transform>(entity, 0, 0);
      }
    }

    if (!GetWorld().HasComponent<criogenio::AnimatedSprite>(entity)) {
      if (ImGui::MenuItem("Animated Sprite")) {
        GetWorld().AddComponent<criogenio::AnimatedSprite>(entity);
      }
    }

    if (!GetWorld().HasComponent<criogenio::Controller>(entity)) {
      if (ImGui::MenuItem("Player Controller")) {
        GetWorld().AddComponent<criogenio::Controller>(entity, 200.0f);
      }
    }

    if (!GetWorld().HasComponent<criogenio::AIController>(entity)) {
      if (ImGui::MenuItem("AI Controller")) {
        GetWorld().AddComponent<criogenio::AIController>(entity, 200.0f,
                                                         entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::AnimationState>(entity)) {
      if (ImGui::MenuItem("Animation State")) {
        GetWorld().AddComponent<criogenio::AnimationState>(entity);
      }
    }
    ImGui::EndPopup();
  }
}
