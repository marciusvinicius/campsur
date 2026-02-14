#include "editor.h"
#include "animation_database.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "criogenio_io.h"
#include "graphics_types.h"
#include "input.h"
#include "keys.h"
#include "resources.h"
#include "terrain.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

#ifdef __cplusplus
extern "C" {
#endif
#include "../thirdpart/tinyfiledialogs.h"
#ifdef __cplusplus
}
#endif

namespace fs = std::filesystem;

using namespace criogenio;

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  EditorAppReset();
}

void EditorApp::EditorAppReset() {
  RegisterCoreComponents();
  GetWorld().AddSystem<MovementSystem>(GetWorld());
  GetWorld().AddSystem<AnimationSystem>(GetWorld());
  GetWorld().AddSystem<SpriteSystem>(GetWorld());
  GetWorld().AddSystem<RenderSystem>(GetWorld());
  GetWorld().AddSystem<AIMovementSystem>(GetWorld());
}

void EditorApp::InitImGUI() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();

  SDL_Window* window = (SDL_Window*)GetRenderer().GetWindowHandle();
  SDL_Renderer* sdlRenderer = (SDL_Renderer*)GetRenderer().GetRendererHandle();
  if (window && sdlRenderer) {
    ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
    ImGui_ImplSDLRenderer3_Init(sdlRenderer);
    imguiBackendsInitialized = true;
  }

  // ---- Fonts (base font FIRST) ----
  const char *fontPath = "editor/assets/fonts/Poppins-Black.ttf";
  ImFont *baseFont = nullptr;
  if (fs::exists(fontPath)) {
    baseFont = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
  }
  if (!baseFont) {
    ImFontConfig cfg;
    cfg.SizePixels = 16.0f;
    baseFont = io.Fonts->AddFontDefault(&cfg);
  }

  io.Fonts->Build();
}

void EditorApp::Run() {
  criogenio::Renderer& ren = GetRenderer();
  if (!ren.IsValid()) {
    std::fprintf(stderr, "Renderer failed to initialize: %s\n", ren.GetInitError());
    return;
  }
  InitImGUI();

  int vpW = ren.GetViewportWidth();
  int vpH = ren.GetViewportHeight();
  if (vpW > 0 && vpH > 0) {
    GetWorld().GetActiveCamera()->offset = {vpW * 0.5f, vpH * 0.5f};
  }
  GetWorld().GetActiveCamera()->target = {0.0f, 0.0f};
  GetWorld().GetActiveCamera()->rotation = 0.0f;
  GetWorld().GetActiveCamera()->zoom = 1.0f;

  auto prevTime = std::chrono::steady_clock::now();
  bool firstFrame = true;

  while (!ren.WindowShouldClose()) {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - prevTime).count();
    prevTime = now;

    Vec2 mousePos = GetMousePosition();
    Vec2 mouseDelta = {mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y};
    lastMousePos = mousePos;

    std::function<void(const void*)> passEventToImGui = [](const void* ev) {
      ImGui_ImplSDL3_ProcessEvent((const SDL_Event*)ev);
    };
    ren.ProcessEvents(&passEventToImGui);

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();

    HandleInput(dt, mouseDelta);
    HandleEntityDrag(mouseDelta);
    HandleScenePicking();

    GetWorld().Update(dt);

    RenderSceneToTexture();

    ren.BeginFrame();
    ren.DrawRect(0, 0, (float)ren.GetViewportWidth(), (float)ren.GetViewportHeight(), criogenio::Colors::Blue);
    OnGUI();
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), (SDL_Renderer*)ren.GetRendererHandle());
    ren.EndFrame();
    Input::EndFrame();
  }

  if (sceneRT.valid())
    GetRenderer().DestroyRenderTarget(&sceneRT);
  if (imguiBackendsInitialized) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    imguiBackendsInitialized = false;
  }
  ImGui::DestroyContext();
}

void EditorApp::DrawButton(int x, int y, int w, int h, const char *label,
                           std::function<void()> onClick) {
  criogenio::Rect rect = {(float)x, (float)y, (float)w, (float)h};
  bool hovered = criogenio::PointInRect(GetMousePosition(), rect);

  criogenio::Color color = hovered ? criogenio::Colors::Gray : criogenio::Colors::DarkGray;
  GetRenderer().DrawRect((float)x, (float)y, (float)w, (float)h, color);
  GetRenderer().DrawTextString(label, x + 10, y + 6, 18, criogenio::Colors::Black);

  if (hovered && Input::IsMouseDown(0))
    onClick();
}

void EditorApp::DrawInput(int x, int y, int w, int h, const char *label) {
  criogenio::Rect rect = {(float)x, (float)y, (float)w, (float)h};
  bool hovered = criogenio::PointInRect(GetMousePosition(), rect);

  criogenio::Color color = hovered ? criogenio::Colors::Gray : criogenio::Colors::DarkGray;
  GetRenderer().DrawRect((float)x, (float)y, (float)w, (float)h, color);
  GetRenderer().DrawTextString(label, x + 10, y + 6, 18, criogenio::Colors::Black);
}

void EditorApp::DrawWorldView() {
  int vpW = GetRenderer().GetViewportWidth();
  int vpH = GetRenderer().GetViewportHeight();
  criogenio::Rect worldRect = {
      (float)leftPanelWidth, 0,
      (float)(vpW - leftPanelWidth - rightPanelWidth),
      (float)vpH};

  GetRenderer().DrawRect(worldRect.x, worldRect.y, worldRect.width, worldRect.height, criogenio::Colors::Black);

  if (sceneRT.valid()) {
    GetWorld().GetActiveCamera()->offset = {sceneRT.width * 0.5f, sceneRT.height * 0.5f};
  }
  criogenio::Camera2D cam;
  cam.target = {0.0f, 0.0f};
  cam.rotation = 0.0f;
  cam.zoom = 1.0f;
  GetWorld().AttachCamera2D(cam);

  GetWorld().Render(GetRenderer());
}

void EditorApp::DrawGlobalComponentsPanel() {
  ImGui::Text("Global Components");
  ImGui::Separator();

  // Gravity
  bool hasGravity = GetWorld().HasGlobalComponent<criogenio::Gravity>();
  if (hasGravity) {
    auto *gravity = GetWorld().GetGlobalComponent<criogenio::Gravity>();
    if (ImGui::CollapsingHeader("Gravity", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat("Gravity Strength", &gravity->strength, 0.1f, 0.0f,
                       100.0f);
      ImGui::SameLine();
      if (ImGui::SmallButton("Remove##GlobalGravity")) {
        GetWorld().RemoveGlobalComponent<criogenio::Gravity>();
      }
    }
  } else {
    if (ImGui::Button("Add Gravity")) {
      GetWorld().AddGlobalComponent<criogenio::Gravity>();
    }
  }
}

// Draws the global inspector window for global components
void EditorApp::DrawGlobalInspector() {
  if (ImGui::Begin("Global Inspector")) {
    DrawGlobalComponentsPanel();
  }
  ImGui::End();
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
          if (auto *sprite =
                  GetWorld().GetComponent<criogenio::AnimatedSprite>(id)) {
            if (sprite->animationId != INVALID_ASSET_ID) {
              AnimationDatabase::instance().removeReference(
                  sprite->animationId);
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
          if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(
                  entityToDelete)) {
            if (sprite->animationId != INVALID_ASSET_ID) {
              AnimationDatabase::instance().removeReference(
                  sprite->animationId);
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

  // Inspector window (components only)
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

  // --- Place this at the end of the file, after all other EditorApp member
  // functions ---

  if (ImGui::Begin("Viewport")) {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    static ImVec2 lastSize = {0, 0};
    if ((int)avail.x != (int)lastSize.x || (int)avail.y != (int)lastSize.y) {
      if (sceneRT.valid())
        GetRenderer().DestroyRenderTarget(&sceneRT);
      if (avail.x > 0 && avail.y > 0)
        sceneRT = GetRenderer().CreateRenderTarget((int)avail.x, (int)avail.y);
      lastSize = avail;
    }

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    ImVec2 wPos = ImGui::GetWindowPos();

    viewportPos = {wPos.x + vMin.x, wPos.y + vMin.y};
    viewportSize = {vMax.x - vMin.x, vMax.y - vMin.y};
    viewportHovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    if (sceneRT.valid() && viewportSize.x > 0 && viewportSize.y > 0) {
      ImGui::Image((ImTextureID)(intptr_t)sceneRT.opaque, viewportSize,
                   ImVec2(0, 0), ImVec2(1, 1));
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
  Vec2 mouse = GetMousePosition();
  criogenio::Rect viewRect = {(float)viewportPos.x, (float)viewportPos.y, (float)viewportSize.x, (float)viewportSize.y};
  return criogenio::PointInRect(mouse, viewRect);
}

void EditorApp::HandleEntityDrag(Vec2 mouseDelta) {
  if (!selectedEntityId.has_value())
    return;
  if (!IsMouseInWorldView())
    return;

  if (Input::IsMouseDown(0)) {
    Vec2 mouseScreen = GetMousePosition();
    Vec2 prevMouseScreen = {mouseScreen.x - mouseDelta.x, mouseScreen.y - mouseDelta.y};

    float vpW = (float)GetRenderer().GetViewportWidth();
    float vpH = (float)GetRenderer().GetViewportHeight();
    Vec2 prevWorld = ScreenToWorld2D(prevMouseScreen, *GetWorld().GetActiveCamera(), vpW, vpH);
    Vec2 currWorld = ScreenToWorld2D(mouseScreen, *GetWorld().GetActiveCamera(), vpW, vpH);

    Vec2 drag = {currWorld.x - prevWorld.x, currWorld.y - prevWorld.y};

    auto* transform =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
    if (transform) {
      transform->x += drag.x;
      transform->y += drag.y;
    }
  }
}

void EditorApp::HandleInput(float dt, Vec2 mouseDelta) {
  if (selectedEntityId.has_value())
    return;
  if (!viewportHovered)
    return;

  float wheel = ImGui::GetIO().MouseWheel;
  if (wheel != 0) {
    float zoomSpeed = 0.1f;
    GetWorld().GetActiveCamera()->zoom += wheel * zoomSpeed;
    if (GetWorld().GetActiveCamera()->zoom < 0.1f)
      GetWorld().GetActiveCamera()->zoom = 0.1f;
    if (GetWorld().GetActiveCamera()->zoom > 8.0f)
      GetWorld().GetActiveCamera()->zoom = 8.0f;
  }

  // Pan only on middle mouse drag (button 2). Right-click stays for context menu; left for selection/terrain.
  if (Input::IsMouseDown(2)) {
    GetWorld().GetActiveCamera()->target.x -= mouseDelta.x / GetWorld().GetActiveCamera()->zoom;
    GetWorld().GetActiveCamera()->target.y -= mouseDelta.y / GetWorld().GetActiveCamera()->zoom;
  }

  float speed = 500.0f * dt;
  using namespace criogenio;
  if (Input::IsKeyDown(static_cast<int>(Key::W)))
    GetWorld().GetActiveCamera()->target.y -= speed;
  if (Input::IsKeyDown(static_cast<int>(Key::S)))
    GetWorld().GetActiveCamera()->target.y += speed;
  if (Input::IsKeyDown(static_cast<int>(Key::A)))
    GetWorld().GetActiveCamera()->target.x -= speed;
  if (Input::IsKeyDown(static_cast<int>(Key::D)))
    GetWorld().GetActiveCamera()->target.x += speed;
}

void EditorApp::OnGUI() {
  DrawDockSpace();
  DrawTerrainEditor();
  DrawAnimationEditorWindow();
  DrawMainMenuBar();
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
    ImGui::DockBuilderDockWindow("Terrain Editor", dock_right);
    ImGui::DockBuilderDockWindow("Animation Editor", dock_right);
    ImGui::DockBuilderDockWindow("Global Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Viewport", dock_main);
    ImGui::DockBuilderDockWindow("##Toolbar", dock_main);
    if (ImGui::Button("Click Me")) {
      // Action to take when clicked
    }
    ImGui::DockBuilderFinish(dockspace_id);
  }
  DrawHierarchyPanel();
  DrawGlobalInspector();
  ImGui::End();
}

static criogenio::Vec2 TerrainWorldToTile(criogenio::Vec2 worldPos,
                                         const criogenio::Terrain2D &terrain) {
  float ts = static_cast<float>(terrain.tileset.tileSize);
  float tx = std::floor((worldPos.x - terrain.origin.x) / ts);
  float ty = std::floor((worldPos.y - terrain.origin.y) / ts);
  return {tx, ty};
}

void DrawTileHighlight(criogenio::Renderer& renderer, const criogenio::Terrain2D &terrain,
                      criogenio::Vec2 tileWorld, const criogenio::Camera2D &camera) {
  if (terrain.layers.empty())
    return;
  int ts = terrain.tileset.tileSize;
  float x = terrain.origin.x + tileWorld.x * ts;
  float y = terrain.origin.y + tileWorld.y * ts;
  renderer.DrawRectOutline(x, y, (float)ts, (float)ts, criogenio::Colors::Yellow);
}

void EditorApp::RenderSceneToTexture() {
  if (!sceneRT.valid())
    return;
  criogenio::Renderer& ren = GetRenderer();
  ren.SetRenderTarget(sceneRT);
  ren.DrawRect(0, 0, (float)sceneRT.width, (float)sceneRT.height, criogenio::Colors::Black);
  ren.BeginCamera2D(*GetWorld().GetActiveCamera());

  if (selectedEntityId.has_value()) {
    if (auto* t = GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value()))
      ren.DrawRectOutline(t->x, t->y, 64, 64, criogenio::Colors::Yellow);
  }

  GetWorld().Render(ren);

  if (terrainEditMode && GetWorld().GetTerrain()) {
    criogenio::Vec2 worldPos = ScreenToWorldPosition(GetMousePosition());
    criogenio::Vec2 tile = TerrainWorldToTile(worldPos, *GetWorld().GetTerrain());
    DrawTerrainGridOverlay(*GetWorld().GetTerrain(), *GetWorld().GetActiveCamera());
    DrawTileHighlight(ren, *GetWorld().GetTerrain(), tile, *GetWorld().GetActiveCamera());
  }

  ren.EndCamera2D();
  ren.UnsetRenderTarget();
}

void EditorApp::DrawMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene")) {
        // Delete all entities and reset editor state
        auto &world = GetWorld();
        for (auto id : world.GetAllEntities()) {
          world.DeleteEntity(id);
        }
        EditorAppReset();
      }
      if (ImGui::MenuItem("Open Scene...")) {
        const char *filters[] = {"*.json"};
        const char *path = tinyfd_openFileDialog("Open Scene", "world.json", 1,
                                                 filters, "Scene Files", 0);
        if (path && strlen(path) > 0) {
          printf("[Editor] Selected file: '%s'\n", path);
          FILE *fp = fopen(path, "r");
          if (fp) {
            fclose(fp);
            criogenio::LoadWorldFromFile(GetWorld(), path);
            EditorAppReset();
          } else {
            printf("[Editor] ERROR: File does not exist or cannot be opened: "
                   "'%s'\n",
                   path);
          }
        }
      }
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        criogenio::SaveWorldToFile(GetWorld(), "world.json");
      }
      if (ImGui::MenuItem("Save Scene As...")) {
        const char *filters[] = {"*.json"};
        const char *path = tinyfd_saveFileDialog("Save Scene As", "world.json",
                                                 1, filters, "Scene Files");
        if (path && strlen(path) > 0) {
          criogenio::SaveWorldToFile(GetWorld(), path);
        }
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
        // TODO: move this to a  help function?
        int id = GetWorld().CreateEntity("New Entity");
        GetWorld().AddComponent<criogenio::Name>(id, "New Entity " +
                                                         std::to_string(id));

        GetWorld().AddComponent<criogenio::Transform>(id, 0.0f, 0.0f);
      }
      if (ImGui::MenuItem("Sprite")) {
        // CreateSpriteEntity();
      }
      if (ImGui::MenuItem("Terrain")) {
        const char *path = DrawFileBrowserPopup();
        if (path && path[0] != '\0') {
          GetWorld().CreateTerrain2D("Terrain", path);
        }
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  if (terrainEditMode) {
    Vec2 mouseScreen = GetMousePosition();
    criogenio::Rect viewRect = {(float)viewportPos.x, (float)viewportPos.y, (float)viewportSize.x, (float)viewportSize.y};
    if (criogenio::PointInRect(mouseScreen, viewRect)) {
      Vec2 worldPos = ScreenToWorldPosition(mouseScreen);
      auto* terrain = GetWorld().GetTerrain();
      if (terrain) {
        float ts = static_cast<float>(terrain->tileset.tileSize);
        int tx = static_cast<int>(std::floor((worldPos.x - terrain->origin.x) / ts));
        int ty = static_cast<int>(std::floor((worldPos.y - terrain->origin.y) / ts));
        if (Input::IsMouseDown(0)) {
          terrain->SetTile(terrainSelectedLayer, tx, ty, terrainSelectedTile);
        } else if (Input::IsMouseDown(1)) {
          terrain->SetTile(terrainSelectedLayer, tx, ty, -1);
        }
      }
    }
  }
}

void EditorApp::CreateEmptyEntityAtMouse() {
  Vec2 worldPos = GetMouseWorld();
  int id = GetWorld().CreateEntity("New Entity");
  GetWorld().AddComponent<criogenio::Name>(id,
                                           "New Entity " + std::to_string(id));
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
    ImGui::Text("No terrain. Create one to start painting.");
    if (ImGui::Button("Create Terrain...")) {
      const char *path = DrawFileBrowserPopup();
      if (path && path[0] != '\0') {
        GetWorld().CreateTerrain2D("MainTerrain", path);
      }
    }
    ImGui::End();
    return;
  }

  if (terrain->layers.empty()) {
    if (ImGui::Button("Add Layer")) {
      terrain->AddLayer();
      terrainSelectedLayer = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Terrain")) {
      GetWorld().DeleteTerrain();
    }
    ImGui::End();
    return;
  }

  if (terrainSelectedLayer < 0 ||
      terrainSelectedLayer >= static_cast<int>(terrain->layers.size())) {
    terrainSelectedLayer = static_cast<int>(terrain->layers.size()) - 1;
  }

  ImGui::Checkbox("Terrain Edit Mode", &terrainEditMode);
  ImGui::SameLine();
  ImGui::Text("Layer:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::InputInt("##layer", &terrainSelectedLayer, 1, 1);
  terrainSelectedLayer = std::max(
      0, std::min(terrainSelectedLayer,
                  static_cast<int>(terrain->layers.size()) - 1));
  ImGui::SameLine();
  if (ImGui::Button("Add Layer")) {
    terrain->AddLayer();
    terrainSelectedLayer = static_cast<int>(terrain->layers.size()) - 1;
  }

  ImGui::Separator();
  ImGui::Text("Chunk size: %d x %d (infinite terrain)", terrain->GetChunkSize(),
              terrain->GetChunkSize());
  ImGui::Text("Selected Tile: %d", terrainSelectedTile);
  ImGui::Text("Tile Size: %d x %d", terrain->tileset.tileSize,
              terrain->tileset.tileSize);
  ImGui::Text("Left-click: paint | Right-click: erase");

  // Erase tool
  bool eraseSelected = (terrainSelectedTile == -1);
  if (eraseSelected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
  }
  if (ImGui::Button("Erase (empty tile)")) {
    terrainSelectedTile = -1;
  }
  if (eraseSelected) {
    ImGui::PopStyleColor();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Select to paint empty tiles with left-click.\nRight-click always erases regardless of selection.");
  }

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
      const char *path = DrawFileBrowserPopup();
      GetWorld().GetTerrain()->SetAtlas(0, path);
    }
    ImGui::DragInt("Tileset size", &terrain->tileset.tileSize, 1, 0, 100);
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
        ImTextureID id = (ImTextureID)(intptr_t)tex->texture.opaque;
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

  ImGui::Separator();
  if (ImGui::Button("Delete Terrain", ImVec2(-1, 0))) {
    GetWorld().DeleteTerrain();
  }

  ImGui::End();
}

std::optional<criogenio::Vec2>
EditorApp::WorldToTile(const criogenio::Terrain2D &terrain, criogenio::Vec2 worldPos) {
  criogenio::Vec2 local = {worldPos.x - terrain.origin.x, worldPos.y - terrain.origin.y};
  float ts = static_cast<float>(terrain.tileset.tileSize);
  int tx = static_cast<int>(std::floor(local.x / ts));
  int ty = static_cast<int>(std::floor(local.y / ts));
  return criogenio::Vec2{static_cast<float>(tx), static_cast<float>(ty)};
}

void EditorApp::DrawTerrainGridOverlay(const criogenio::Terrain2D &terrain,
                                       const criogenio::Camera2D &cam) {
  if (terrain.layers.empty())
    return;
  float vpW = sceneRT.valid() ? (float)sceneRT.width : viewportSize.x;
  float vpH = sceneRT.valid() ? (float)sceneRT.height : viewportSize.y;

  const int ts = terrain.tileset.tileSize;
  int minTx, minTy, maxTx, maxTy;
  terrain.GetVisibleTileRange(cam, vpW, vpH, minTx, minTy, maxTx, maxTy);

  float startX = terrain.origin.x + minTx * ts;
  float startY = terrain.origin.y + minTy * ts;
  float endX = terrain.origin.x + (maxTx + 1) * ts;
  float endY = terrain.origin.y + (maxTy + 1) * ts;

  int numX = maxTx - minTx + 2;
  int numY = maxTy - minTy + 2;
  criogenio::Color gridColor{128, 128, 128, 102};

  criogenio::Renderer& ren = GetRenderer();
  for (int x = 0; x <= numX; ++x) {
    float wx = startX + x * ts;
    ren.DrawLine(wx, startY, wx, endY, gridColor);
  }
  for (int y = 0; y <= numY; ++y) {
    float wy = startY + y * ts;
    ren.DrawLine(startX, wy, endX, wy, gridColor);
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
  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    return;

  Vec2 mouse = GetMousePosition();
  float vx = viewportPos.x, vy = viewportPos.y, vw = viewportSize.x, vh = viewportSize.y;
  if (mouse.x < vx || mouse.y < vy || mouse.x > vx + vw || mouse.y > vy + vh)
    return;
  if (!sceneRT.valid())
    return;

  Vec2 local = {mouse.x - vx, mouse.y - vy};
  Vec2 tex = {local.x * ((float)sceneRT.width / vw), local.y * ((float)sceneRT.height / vh)};

  Vec2 world = criogenio::ScreenToWorld2D(tex, *GetWorld().GetActiveCamera(), (float)sceneRT.width, (float)sceneRT.height);
  PickEntityAt(world);
}

void EditorApp::PickEntityAt(criogenio::Vec2 worldPos) {
  selectedEntityId.reset();
  criogenio::Rect bounds;
  bounds.width = 64;
  bounds.height = 128;
  for (int entity : GetWorld().GetEntitiesWith<criogenio::Transform>()) {
    auto* t = GetWorld().GetComponent<criogenio::Transform>(entity);
    if (!t)
      continue;
    bounds.x = t->x;
    bounds.y = t->y;
    if (criogenio::PointInRect(worldPos, bounds)) {
      selectedEntityId = entity;
      return;
    }
  }
}
bool EditorApp::IsSceneInputAllowed() const {
  return viewportHovered && !ImGui::IsAnyItemActive() &&
         !ImGui::IsAnyItemHovered();
}

criogenio::Vec2 EditorApp::ScreenToWorldPosition(criogenio::Vec2 mouseScreen) {
  if (mouseScreen.x < viewportPos.x || mouseScreen.x > viewportPos.x + viewportSize.x ||
      mouseScreen.y < viewportPos.y || mouseScreen.y > viewportPos.y + viewportSize.y)
    return {0, 0};
  if (!sceneRT.valid())
    return {0, 0};

  Vec2 viewportMouse = {mouseScreen.x - viewportPos.x, mouseScreen.y - viewportPos.y};
  Vec2 normalizedPos = {viewportMouse.x / viewportSize.x, viewportMouse.y / viewportSize.y};
  Vec2 textureScreen = {normalizedPos.x * (float)sceneRT.width,
                        normalizedPos.y * (float)sceneRT.height};

  float vpW = (float)sceneRT.width;
  float vpH = (float)sceneRT.height;
  return criogenio::ScreenToWorld2D(textureScreen, *GetWorld().GetActiveCamera(), vpW, vpH);
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
    if (auto *sprite =
            GetWorld().GetComponent<criogenio::AnimatedSprite>(entity)) {
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
      if (auto *sprite =
              GetWorld().GetComponent<criogenio::AnimatedSprite>(entity)) {
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
  if (GetWorld().HasComponent<criogenio::Sprite>(entity))
    DrawSpriteInspector(entity);
}

void EditorApp::DrawGravityInspector(int entity) {

  ImGui::PushID("Gravity");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Gravity");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Gravity>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *gravity = GetWorld().GetComponent<criogenio::Gravity>(entity);
  if (!gravity)
    return;

  ImGui::DragFloat("Gravity Strength", &gravity->strength, 0.1f, 0.0f, 100.0f);

  if (ImGui::BeginPopupContextItem("GravityContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::Gravity>(entity);
    }
    ImGui::EndPopup();
  }
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
  bool headerOpen =
      ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
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

// TODO:(maraujo) move these kind of thing to UI
const char *EditorApp::DrawFileBrowserPopup() {
  const char *filters[] = {"*.png", "*.jpeg", "*.jpg"};
  const char *path =
      tinyfd_openFileDialog("Test", "", 1, filters, "Select File", 0);

  if (path && strlen(path) > 0) {
    printf("[Editor] Selected file: '%s'\n", path);
    FILE *fp = fopen(path, "r");
    if (fp) {
      fclose(fp);
      // GetWorld().GetTerrain().tileset.atlas.texture = fileBrowserPreviewTex;
      return path;
    } else {
      printf("[Editor] ERROR: File does not exist or cannot be opened: "
             "'%s'\n",
             path);
    }
  }
  return nullptr;
  /*

      ImGui::BeginPopupModal("File Browser", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(fileBrowserForTerrain
                    ? "Select terrain texture file (root: editor/assets)"
                    : "Select texture file (root: editor/assets)");
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
        if (this->fileDialogMode == FileDialogMode::SaveScene ||
            this->fileDialogMode == FileDialogMode::LoadScene) {
          this->pendingScenePath = entry;
        } else if (fileBrowserForTerrain) {
          fileBrowserPreviewPath = entry; // Store selected path for
  terrain } else { texturePathEdits[fileBrowserTargetEntity] = entry;
        }
      }
      if (ImGui::IsItemHovered()) {
        // update preview when hovered
        if (fileBrowserPreviewPath != entry) {
          fileBrowserPreviewPath = entry;
          fileBrowserPreviewTex =
              AssetManager::instance().load<criogenio::TextureResource>(entry);
        }
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (this->fileDialogMode == FileDialogMode::SaveScene ||
            this->fileDialogMode == FileDialogMode::LoadScene) {
          this->pendingScenePath = entry;
          fileBrowserVisible = false;
          ImGui::CloseCurrentPopup();
          break;
        } else if (fileBrowserForTerrain) {
          // ...existing code for terrain...
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
            ImGui::Image(
                (ImTextureID)(intptr_t)fileBrowserPreviewTex->texture.opaque,
                ImVec2(tw * scale, th * scale));
          } else {
            ImGui::TextDisabled("No preview");
          }
          ImGui::EndChild();

          if (ImGui::Button("OK")) {
            if (this->fileDialogMode == FileDialogMode::SaveScene &&
                !this->pendingScenePath.empty()) {
              criogenio::SaveWorldToFile(GetWorld(),
  this->pendingScenePath); this->fileDialogMode = FileDialogMode::None;
              this->pendingScenePath.clear();
              fileBrowserVisible = false;
              ImGui::CloseCurrentPopup();
              return;
            } else if (this->fileDialogMode == FileDialogMode::LoadScene
  && !this->pendingScenePath.empty()) {
              criogenio::LoadWorldFromFile(GetWorld(),
  this->pendingScenePath); EditorAppReset(); this->fileDialogMode =
  FileDialogMode::None; this->pendingScenePath.clear(); fileBrowserVisible
  = false; ImGui::CloseCurrentPopup(); return; } else if
  (fileBrowserForTerrain) {
              // ...existing code for terrain...
              ImGui::SameLine();
              if (ImGui::Button("Cancel")) {
                fileBrowserVisible = false;
                fileBrowserForTerrain = false;
                ImGui::CloseCurrentPopup();
              }

              ImGui::EndPopup();
            }
          }
        }
      }
    }
  }*/
}

void EditorApp::DrawAnimationEditorWindow() {
  if (!ImGui::Begin("Animation Editor")) {
    ImGui::End();
    return;
  }

  if (!selectedEntityId.has_value()) {
    ImGui::TextDisabled("No entity selected.");
    ImGui::End();
    return;
  }

  int entity = selectedEntityId.value();
  DrawAnimationEditor(entity);

  ImGui::End();
}

void EditorApp::DrawAnimationEditor(int entity) {

  auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity);

  if (!sprite) {
    ImGui::TextDisabled("Entity has no AnimatedSprite component.");
    if (ImGui::Button("Add AnimatedSprite Component")) {
      GetWorld().AddComponent<criogenio::AnimatedSprite>(entity);
      if (!GetWorld().HasComponent<criogenio::AnimationState>(entity)) {
        GetWorld().AddComponent<criogenio::AnimationState>(entity);
      }
    }
    return;
  }

  // Ensure we have a draft for this entity
  auto &draft = animationClipDrafts[entity];

  // Texture / spritesheet selection
  auto *animDef =
      criogenio::AnimationDatabase::instance().getAnimation(sprite->animationId);

  auto &texPathEdit = texturePathEdits[entity];
  if (texPathEdit.empty() && animDef) {
    texPathEdit = animDef->texturePath;
  }

  char texBuf[512] = {0};
  strncpy(texBuf, texPathEdit.c_str(), sizeof(texBuf) - 1);
  if (ImGui::InputText("Texture Path", texBuf, sizeof(texBuf))) {
    texPathEdit = std::string(texBuf);
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse##AnimTexture")) {
    const char *path = DrawFileBrowserPopup();
    if (path && std::strlen(path) > 0) {
      texPathEdit = path;
    }
  }

  ImGui::Separator();

  // Editing vs creating new clip state
  auto &currentEditingName = editingClipName[entity];
  if (!currentEditingName.empty()) {
    ImGui::Text("Editing clip: %s", currentEditingName.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Done")) {
      // Finish editing and reset draft to defaults so the next Save
      // creates a brand new clip instead of overwriting.
      currentEditingName.clear();
      draft = AnimationClipDraft{};
      draft.tileIndices.clear();
    }
  } else {
    ImGui::Text("Creating new clip");
  }

  ImGui::Separator();

  // Export / import this animation definition as standalone JSON
  ImGui::Text("Animation Asset:");
  ImGui::SameLine();
  if (ImGui::Button("Export...")) {
    if (sprite->animationId != criogenio::INVALID_ASSET_ID) {
      const char *filters[] = {"*.json"};
      const char *path = tinyfd_saveFileDialog(
          "Export Animation", "animation.json", 1, filters, "Animation Files");
      if (path && std::strlen(path) > 0) {
        criogenio::SaveAnimationToFile(sprite->animationId, path);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Import...")) {
    const char *filters[] = {"*.json"};
    const char *path = tinyfd_openFileDialog("Import Animation", "", 1, filters,
                                             "Animation Files", 0);
    if (path && std::strlen(path) > 0) {
      criogenio::AssetId oldId = sprite->animationId;
      criogenio::AssetId newId = criogenio::LoadAnimationFromFile(path);
      if (newId != criogenio::INVALID_ASSET_ID) {
        if (oldId != criogenio::INVALID_ASSET_ID) {
          criogenio::AnimationDatabase::instance().removeReference(oldId);
        }
        sprite->animationId = newId;
        criogenio::AnimationDatabase::instance().addReference(newId);
        sprite->currentClipName.clear();
      }
    }
  }

  ImGui::Separator();

  ImGui::InputInt("Tile Size", &draft.tileSize);
  if (draft.tileSize < 1)
    draft.tileSize = 1;

  ImGui::DragFloat("Frame Speed (s)", &draft.frameSpeed, 0.01f, 0.01f, 10.0f);

  ImGui::Separator();

  // Link this clip to an AnimState
  int currentStateIndex = static_cast<int>(draft.state);
  const char *currentStateLabel =
      criogenio::anim_state_to_string(draft.state).c_str();
  if (ImGui::BeginCombo("Anim State", currentStateLabel)) {
    for (int i = 0; i < static_cast<int>(criogenio::AnimState::Count); ++i) {
      auto state = static_cast<criogenio::AnimState>(i);
      bool selected = (i == currentStateIndex);
      const std::string label = criogenio::anim_state_to_string(state);
      if (ImGui::Selectable(label.c_str(), selected)) {
        draft.state = state;
        currentStateIndex = i;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Link this clip to a Direction
  int currentDirIndex = static_cast<int>(draft.direction);
  const std::string currentDirLabel =
      criogenio::direction_to_string(draft.direction);
  if (ImGui::BeginCombo("Direction", currentDirLabel.c_str())) {
    for (int i = 0; i < static_cast<int>(criogenio::Direction::Count); ++i) {
      auto dir = static_cast<criogenio::Direction>(i);
      bool selected = (i == currentDirIndex);
      const std::string label = criogenio::direction_to_string(dir);
      if (ImGui::Selectable(label.c_str(), selected)) {
        draft.direction = dir;
        currentDirIndex = i;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Show computed clip name (state + direction)
  std::string baseName;
  switch (draft.state) {
  case criogenio::AnimState::IDLE:
    baseName = "idle";
    break;
  case criogenio::AnimState::WALKING:
    baseName = "walk";
    break;
  case criogenio::AnimState::ATTACKING:
    baseName = "attack";
    break;
  case criogenio::AnimState::RUNNING:
    baseName = "run";
    break;
  case criogenio::AnimState::JUMPING:
    baseName = "jump";
    break;
  default:
    baseName = "idle";
    break;
  }
  std::string dirSuffix = criogenio::direction_to_string(draft.direction);
  draft.name = baseName + "_" + dirSuffix;
  ImGui::Text("Clip Name: %s", draft.name.c_str());

  ImGui::Separator();

  std::shared_ptr<criogenio::TextureResource> tex = nullptr;
  int cols = 0;
  int rows = 0;

  if (!texPathEdit.empty()) {
    tex =
        criogenio::AssetManager::instance().load<criogenio::TextureResource>(
            texPathEdit);
    if (tex && draft.tileSize > 0) {
      cols = tex->texture.width / draft.tileSize;
      rows = tex->texture.height / draft.tileSize;
    }
  }

  if (!tex || cols <= 0 || rows <= 0) {
    ImGui::TextDisabled(
        "Select a valid texture and tile size to pick frames.");
  } else {
    ImGui::Text("Click tiles to toggle them as frames (order = click order).");

    ImTextureID id = (ImTextureID)(intptr_t)tex->texture.opaque;
    ImVec2 buttonSize(32, 32);

    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < cols; ++x) {
        int idx = y * cols + x;
        float u0 =
            (x * draft.tileSize) / static_cast<float>(tex->texture.width);
        float v0 =
            (y * draft.tileSize) / static_cast<float>(tex->texture.height);
        float u1 = ((x + 1) * draft.tileSize) /
                   static_cast<float>(tex->texture.width);
        float v1 = ((y + 1) * draft.tileSize) /
                   static_cast<float>(tex->texture.height);

        bool selected = std::find(draft.tileIndices.begin(),
                                  draft.tileIndices.end(),
                                  idx) != draft.tileIndices.end();

        ImGui::PushID(idx);
        if (selected) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
        }

        if (ImGui::ImageButton("", id, buttonSize, ImVec2(u0, v0),
                               ImVec2(u1, v1))) {
          auto it = std::find(draft.tileIndices.begin(),
                              draft.tileIndices.end(), idx);
          if (it == draft.tileIndices.end()) {
            draft.tileIndices.push_back(idx);
          } else {
            draft.tileIndices.erase(it);
          }
        }

        if (selected) {
          ImGui::PopStyleColor();
        }
        ImGui::PopID();
        ImGui::SameLine();
      }
      ImGui::NewLine();
    }
  }

  ImGui::Separator();
  ImGui::Text("Frames in clip: %d", (int)draft.tileIndices.size());
  if (ImGui::Button("Clear Frames")) {
    draft.tileIndices.clear();
  }

  ImGui::SameLine();
  bool canSave = !texPathEdit.empty() && draft.tileSize > 0 &&
                 !draft.name.empty() && !draft.tileIndices.empty();
  if (!canSave) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Save Clip to Animation")) {
    // Ensure animation asset exists
    criogenio::AssetId animId = sprite->animationId;
    if (animId == criogenio::INVALID_ASSET_ID) {
      animId =
          criogenio::AnimationDatabase::instance().createAnimation(texPathEdit);
      sprite->animationId = animId;
      criogenio::AnimationDatabase::instance().addReference(animId);
    } else {
      // Ensure texture path matches the edited one
      criogenio::AnimationDatabase::instance().setTexturePath(animId,
                                                               texPathEdit);
    }

    criogenio::AnimationClip clip;
    clip.name = draft.name;
    clip.frameSpeed = draft.frameSpeed;
    clip.state = draft.state;
    clip.direction = draft.direction;

    // Build frames from selected tile indices in the order they were clicked
    if (tex && draft.tileSize > 0) {
      int sheetCols = tex->texture.width / draft.tileSize;
      for (int idx : draft.tileIndices) {
        int x = idx % sheetCols;
        int y = idx / sheetCols;
        criogenio::AnimationFrame frame;
        frame.rect.x = static_cast<float>(x * draft.tileSize);
        frame.rect.y = static_cast<float>(y * draft.tileSize);
        frame.rect.width = static_cast<float>(draft.tileSize);
        frame.rect.height = static_cast<float>(draft.tileSize);
        clip.frames.push_back(frame);
      }
    }

    // If we are editing an existing clip, remove the old version first
    auto &editingName = editingClipName[entity];
    if (!editingName.empty()) {
      criogenio::AnimationDatabase::instance().removeClip(animId, editingName);
    }

    criogenio::AnimationDatabase::instance().addClip(animId, clip);
    sprite->SetClip(clip.name);
    // Ensure we have an AnimationState component for this animated entity
    if (!GetWorld().HasComponent<criogenio::AnimationState>(entity)) {
      GetWorld().AddComponent<criogenio::AnimationState>(entity);
    }
    // Track this clip as the one currently being edited
    editingName = clip.name;
  }
  if (!canSave) {
    ImGui::EndDisabled();
  }

  ImGui::Separator();
  ImGui::Text("Existing Clips on this Animation:");
  const auto *def =
      criogenio::AnimationDatabase::instance().getAnimation(sprite->animationId);
  if (!def || def->clips.empty()) {
    ImGui::TextDisabled("No clips defined yet.");
  } else {
    for (const auto &clip : def->clips) {
      bool selected = (sprite->currentClipName == clip.name);

      // Draw clip name as text and allow clicking it to select the clip
      if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.2f, 1.0f));
      }
      ImGui::TextUnformatted(clip.name.c_str());
      if (ImGui::IsItemClicked()) {
        sprite->SetClip(clip.name);
      }
      if (selected) {
        ImGui::PopStyleColor();
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("State: %s, Dir: %s\nFrames: %d, Speed: %.2f",
                          criogenio::anim_state_to_string(clip.state).c_str(),
                          criogenio::direction_to_string(clip.direction).c_str(),
                          (int)clip.frames.size(), clip.frameSpeed);
      }
      ImGui::SameLine();
      std::string editLabel = "Edit##" + clip.name;
      if (ImGui::SmallButton(editLabel.c_str())) {
        // Populate draft from this clip for editing
        auto &draft = animationClipDrafts[entity];
        draft.state = clip.state;
        draft.direction = clip.direction;
        draft.frameSpeed = clip.frameSpeed;
        draft.tileIndices.clear();

        // Infer tile size from first frame, if any
        if (!clip.frames.empty()) {
          draft.tileSize = static_cast<int>(clip.frames[0].rect.width);
        }

        // Reconstruct tile indices from frame rectangles
        if (def && !def->texturePath.empty() && draft.tileSize > 0) {
          auto tex =
              criogenio::AssetManager::instance().load<criogenio::TextureResource>(
                  def->texturePath);
          if (tex) {
            int sheetCols = tex->texture.width / draft.tileSize;
            for (const auto &f : clip.frames) {
              int x = static_cast<int>(f.rect.x) / draft.tileSize;
              int y = static_cast<int>(f.rect.y) / draft.tileSize;
              int idx = y * sheetCols + x;
              draft.tileIndices.push_back(idx);
            }
          }
        }

        // Track which clip we are editing so Save overwrites it
        editingClipName[entity] = clip.name;
      }
      ImGui::SameLine();
      std::string deleteLabel = "Delete##" + clip.name;
      if (ImGui::SmallButton(deleteLabel.c_str())) {
        criogenio::AnimationDatabase::instance().removeClip(
            sprite->animationId, clip.name);
        if (sprite->currentClipName == clip.name) {
          sprite->currentClipName.clear();
        }
        if (editingClipName[entity] == clip.name) {
          editingClipName[entity].clear();
        }
        break; // clips vector changed, break out of loop
      }
    }
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
      const char *path = DrawFileBrowserPopup();
      tmp = path;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      if (!def2->texturePath.empty()) {
        AssetManager::instance().unload(def2->texturePath);
        auto res = AssetManager::instance().load<criogenio::TextureResource>(
            def2->texturePath);
        if (!res)
          std::fprintf(stderr, "Editor: failed to reload texture %s\n",
                       def2->texturePath.c_str());
      }
    }
  } else {
    if (ImGui::Button("Reload Texture")) {
      // nothing to do
    }
  }
}

void EditorApp::DrawSpriteInspector(int entity) {
  ImGui::PushID("Sprite");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Sprite");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);

  if (ImGui::SmallButton("X")) {
    auto *sp = GetWorld().GetComponent<criogenio::Sprite>(entity);
    GetWorld().RemoveComponent<criogenio::Sprite>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }

  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *sprite = GetWorld().GetComponent<criogenio::Sprite>(entity);
  if (!sprite)
    return;

  ImGui::Separator();
  char buf[512] = {0};
  auto &tmp = sprite->atlasPath;
  strncpy(buf, tmp.c_str(), sizeof(buf) - 1);
  if (ImGui::InputText("Texture Path", buf, sizeof(buf))) {
    tmp = std::string(buf);
  }
  if (ImGui::Button("Browse")) {
    const char *path = DrawFileBrowserPopup();
    sprite->SetTexture(path);
  }

  ImGui::Separator();

  ImGui::InputInt("##sprite size", &sprite->spriteSize);

  ImGui::InputInt("##sprite X", &sprite->spriteX);

  ImGui::InputInt("##sprite Y", &sprite->spriteY);

  if (ImGui::BeginPopupContextItem("SpriteContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      // decrement usage if this component owned an animation asset
      auto *sp = GetWorld().GetComponent<criogenio::Sprite>(entity);
      GetWorld().RemoveComponent<criogenio::AnimatedSprite>(entity);
    }
    ImGui::Separator();
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

  ImGui::DragFloat("Speed", &ctrl->velocity.x, 1.0f, 0.0f, 1000.0f);

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

  ImGui::DragFloat("Speed", &ctrl->velocity.x, 1.0f, 0.0f, 1000.0f);

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
        // Auto-create AnimationState when adding an AnimatedSprite
        if (!GetWorld().HasComponent<criogenio::AnimationState>(entity)) {
          GetWorld().AddComponent<criogenio::AnimationState>(entity);
        }
      }
    }

    if (!GetWorld().HasComponent<criogenio::Controller>(entity)) {
      if (ImGui::MenuItem("Player Controller")) {
        GetWorld().AddComponent<criogenio::Controller>(entity,
                                                       criogenio::Vec2{200.0f, 200.0f});
      }
    }

    if (!GetWorld().HasComponent<criogenio::AIController>(entity)) {
      if (ImGui::MenuItem("AI Controller")) {
        GetWorld().AddComponent<criogenio::AIController>(
            entity, criogenio::Vec2{200.0f, 200.0f}, entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::AnimationState>(entity)) {
      if (ImGui::MenuItem("Animation State")) {
        GetWorld().AddComponent<criogenio::AnimationState>(entity);
      }
    }

    if (!GetWorld().HasComponent<criogenio::Sprite>(entity)) {
      if (ImGui::MenuItem("Sprite")) {
        GetWorld().AddComponent<criogenio::Sprite>(entity);
      }
    }
    ImGui::EndPopup();
  }
}
