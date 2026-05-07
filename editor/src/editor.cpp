#include "editor.h"
#include "editor_game_mode.h"
#include "free_form_2d_editor_game_mode.h"
#include "animation_database.h"
#include "animated_component.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "animated_component.h"
#include "criogenio_io.h"
#include "subterra_player_json_io.h"
#include "graphics_types.h"
#include "input.h"
#include "keys.h"
#include "resources.h"
#include "terrain.h"
#include "tmx_metadata.h"
#include "gameplay_tags.h"
#include "map_authoring_components.h"
#include "object_layer.h"
#include "component_factory.h"
#include "json.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

namespace {
struct EditorVec3 {
  float x;
  float y;
  float z;
};

/** Picks Subterra vs free-form play mode from `project.campsur` `game_mode`. */
static std::unique_ptr<IEditorGameMode> MakeEditorGameModeForProject(
    const std::optional<ProjectContext> &proj) {
  if (proj.has_value()) {
    const std::string &id = proj->gameModeId;
    if (id == "free_form_2d")
      return std::make_unique<FreeForm2DEditorGameMode>();
    if (!id.empty() && id != "subterra_guild")
      std::printf(
          "[Editor] Unknown game_mode \"%s\" — using Subterra play mode.\n",
          id.c_str());
  }
  return std::make_unique<subterra::EditorGameMode>();
}

static ImVec2 ProjectIso(const EditorVec3& p, float centerX, float centerY, float scale) {
  const float sx = centerX + (p.x - p.z) * scale;
  const float sy = centerY + (p.x + p.z) * scale * 0.5f - p.y * scale;
  return ImVec2(sx, sy);
}

static uint8_t ClampColor(float v) {
  if (v < 0.0f)
    v = 0.0f;
  if (v > 1.0f)
    v = 1.0f;
  return static_cast<uint8_t>(v * 255.0f);
}

static std::string TmxPropGetString(const std::vector<TmxProperty> &props, const char *key) {
  const char *v = TmxGetPropertyString(props, key, nullptr);
  return (v && v[0]) ? std::string(v) : std::string();
}

static void TmxPropSetString(std::vector<TmxProperty> &props, const char *key,
                             const std::string &val) {
  for (auto &pr : props) {
    if (pr.name == key) {
      pr.value = val;
      pr.type = "string";
      return;
    }
  }
  props.push_back({std::string(key), val, "string"});
}

static int TmxPropGetInt(const std::vector<TmxProperty> &props, const char *key, int def) {
  return TmxGetPropertyInt(props, key, def);
}

static void TmxPropSetInt(std::vector<TmxProperty> &props, const char *key, int v) {
  const std::string s = std::to_string(v);
  for (auto &pr : props) {
    if (pr.name == key) {
      pr.value = s;
      pr.type = "int";
      return;
    }
  }
  props.push_back({std::string(key), s, "int"});
}

static bool TmxPropGetBool(const std::vector<TmxProperty> &props, const char *key, bool def) {
  return TmxGetPropertyBool(props, key, def);
}

static void TmxPropSetBool(std::vector<TmxProperty> &props, const char *key, bool v) {
  const std::string s = v ? "true" : "false";
  for (auto &pr : props) {
    if (pr.name == key) {
      pr.value = s;
      pr.type = "bool";
      return;
    }
  }
  props.push_back({std::string(key), s, "bool"});
}

// kHandleRadius: half-size of each resize square in world units.
static constexpr float kHandleRadius = 6.f;

/** Compute the 8 handle world positions for a zone rect. Index matches ZoneHandle (1..8=TL..BR). */
static void ComputeHandlePositions(float zx, float zy, float zw, float zh,
                                   float hx[9], float hy[9]) {
  const float cx = zx + zw * 0.5f;
  const float cy = zy + zh * 0.5f;
  hx[(int)ZoneHandle::TL] = zx;   hy[(int)ZoneHandle::TL] = zy;
  hx[(int)ZoneHandle::T]  = cx;   hy[(int)ZoneHandle::T]  = zy;
  hx[(int)ZoneHandle::TR] = zx+zw;hy[(int)ZoneHandle::TR] = zy;
  hx[(int)ZoneHandle::L]  = zx;   hy[(int)ZoneHandle::L]  = cy;
  hx[(int)ZoneHandle::R]  = zx+zw;hy[(int)ZoneHandle::R]  = cy;
  hx[(int)ZoneHandle::BL] = zx;   hy[(int)ZoneHandle::BL] = zy+zh;
  hx[(int)ZoneHandle::B]  = cx;   hy[(int)ZoneHandle::B]  = zy+zh;
  hx[(int)ZoneHandle::BR] = zx+zw;hy[(int)ZoneHandle::BR] = zy+zh;
}

/** Pick rect: map-authoring zones use component size; else BoxCollider; else default sprite AABB. */
static bool ComputeEntityPickBounds(World &world, int entity, float &x, float &y, float &w,
                                    float &h) {
  auto *t = world.GetComponent<Transform>(entity);
  if (!t)
    return false;
  if (auto *z = world.GetComponent<MapEventZone2D>(entity)) {
    const bool point = z->point || (z->width < 0.5f && z->height < 0.5f);
    if (point) {
      x = t->x - 8.f;
      y = t->y - 8.f;
      w = 16.f;
      h = 16.f;
    } else {
      x = t->x;
      y = t->y;
      w = std::max(z->width, 1.f);
      h = std::max(z->height, 1.f);
    }
    return true;
  }
  if (auto *z = world.GetComponent<InteractableZone2D>(entity)) {
    const bool point = z->point || (z->width < 0.5f && z->height < 0.5f);
    if (point) {
      x = t->x - 8.f;
      y = t->y - 8.f;
      w = 16.f;
      h = 16.f;
    } else {
      x = t->x;
      y = t->y;
      w = std::max(z->width, 1.f);
      h = std::max(z->height, 1.f);
    }
    return true;
  }
  if (auto *z = world.GetComponent<WorldSpawnPrefab2D>(entity)) {
    if (z->width < 0.5f && z->height < 0.5f) {
      x = t->x - 8.f;
      y = t->y - 8.f;
      w = 16.f;
      h = 16.f;
    } else {
      x = t->x;
      y = t->y;
      w = std::max(z->width, 1.f);
      h = std::max(z->height, 1.f);
    }
    return true;
  }
  if (auto *col = world.GetComponent<BoxCollider>(entity)) {
    x = t->x + col->offsetX;
    y = t->y + col->offsetY;
    w = col->width;
    h = col->height;
    return true;
  }
  x = t->x;
  y = t->y;
  w = 64.f;
  h = 128.f;
  return true;
}

static void DrawIsoLine(Renderer& ren, const EditorVec3& a, const EditorVec3& b, float centerX,
                        float centerY, float scale, Color color) {
  ImVec2 pa = ProjectIso(a, centerX, centerY, scale);
  ImVec2 pb = ProjectIso(b, centerX, centerY, scale);
  ren.DrawLine(pa.x, pa.y, pb.x, pb.y, color);
}

static void DrawIsoWireCube(Renderer& ren, const EditorVec3& c, const EditorVec3& h, float centerX,
                            float centerY, float scale, Color color) {
  EditorVec3 p[8] = {
      {c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z},
      {c.x + h.x, c.y - h.y, c.z + h.z}, {c.x - h.x, c.y - h.y, c.z + h.z},
      {c.x - h.x, c.y + h.y, c.z - h.z}, {c.x + h.x, c.y + h.y, c.z - h.z},
      {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z},
  };
  const int e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (int i = 0; i < 12; ++i) {
    DrawIsoLine(ren, p[e[i][0]], p[e[i][1]], centerX, centerY, scale, color);
  }
}

static EditorVec3 WorldToCamera(const EditorVec3& p, const box3d::FPCamera& cam) {
  float x = p.x - cam.position.x;
  float y = p.y - cam.position.y;
  float z = p.z - cam.position.z;

  float cy = std::cos(-cam.yaw);
  float sy = std::sin(-cam.yaw);
  float x1 = x * cy - z * sy;
  float z1 = x * sy + z * cy;

  float cp = std::cos(-cam.pitch);
  float sp = std::sin(-cam.pitch);
  float y2 = y * cp - z1 * sp;
  float z2 = y * sp + z1 * cp;
  return {x1, y2, z2};
}

static bool ProjectPerspective(const EditorVec3& p, const box3d::FPCamera& cam, float viewportW,
                               float viewportH, ImVec2& out) {
  EditorVec3 v = WorldToCamera(p, cam);
  if (v.z <= 0.01f)
    return false;

  float focal = (viewportH * 0.5f) / std::tan((cam.fovDeg * 3.14159265f / 180.0f) * 0.5f);
  out.x = viewportW * 0.5f + (v.x / v.z) * focal;
  out.y = viewportH * 0.5f - (v.y / v.z) * focal;
  return true;
}

static void DrawPerspectiveLine(Renderer& ren, const EditorVec3& a, const EditorVec3& b,
                                const box3d::FPCamera& cam, float viewportW, float viewportH,
                                Color color) {
  ImVec2 pa, pb;
  if (!ProjectPerspective(a, cam, viewportW, viewportH, pa))
    return;
  if (!ProjectPerspective(b, cam, viewportW, viewportH, pb))
    return;
  ren.DrawLine(pa.x, pa.y, pb.x, pb.y, color);
}

static void DrawPerspectiveWireCube(Renderer& ren, const EditorVec3& c, const EditorVec3& h,
                                    const box3d::FPCamera& cam, float viewportW, float viewportH,
                                    Color color) {
  EditorVec3 p[8] = {
      {c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z},
      {c.x + h.x, c.y - h.y, c.z + h.z}, {c.x - h.x, c.y - h.y, c.z + h.z},
      {c.x - h.x, c.y + h.y, c.z - h.z}, {c.x + h.x, c.y + h.y, c.z - h.z},
      {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z},
  };
  const int e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (int i = 0; i < 12; ++i)
    DrawPerspectiveLine(ren, p[e[i][0]], p[e[i][1]], cam, viewportW, viewportH, color);
}
} // namespace

EditorApp::EditorApp(int width, int height) : Engine(width, height, "Editor") {
  EditorAppReset();
}

void EditorApp::RegisterEditorSystems() {
  GetWorld().AddSystem<GravitySystem>(GetWorld());
  GetWorld().AddSystem<CollisionSystem>(GetWorld());
  GetWorld().AddSystem<MovementSystem>(GetWorld());
  GetWorld().AddSystem<MovementSystem3D>(GetWorld());
  GetWorld().AddSystem<AnimationSystem>(GetWorld());
  GetWorld().AddSystem<SpriteSystem>(GetWorld());
  GetWorld().AddSystem<RenderSystem>(GetWorld());
  GetWorld().AddSystem<AIMovementSystem>(GetWorld());
}

void EditorApp::EditorAppReset() {
  RegisterCoreComponents();
  RegisterEditorSystems();
}

void EditorApp::MarkLevelDirty() { levelDirty = true; }

void EditorApp::LoadLevelFromAbsolutePath(const std::string &absPath) {
  if (!criogenio::LoadWorldFromFile(GetWorld(), absPath)) {
    std::fprintf(stderr, "[Editor] Failed to load level: %s\n", absPath.c_str());
    return;
  }
  EditorAppReset();
  currentLevelPath = fs::absolute(absPath).string();
  levelDirty = false;
}

bool EditorApp::SaveCurrentLevel() {
  if (!currentLevelPath.has_value()) {
    SaveCurrentLevelAs();
    return currentLevelPath.has_value();
  }
  if (!criogenio::SaveWorldToFile(GetWorld(), *currentLevelPath)) {
    std::fprintf(stderr, "[Editor] Save failed: %s\n", currentLevelPath->c_str());
    return false;
  }
  levelDirty = false;
  return true;
}

void EditorApp::SaveCurrentLevelAs() {
  std::string defName = "world.campsurlevel";
  if (project.has_value() && !project->levelsDir.empty())
    defName = (fs::path(project->levelsDir) / "world.campsurlevel").string();
  const char *filters[] = {"*.campsurlevel", "*.json"};
  const char *path = tinyfd_saveFileDialog("Save Level", defName.c_str(), 2, filters,
                                          "Level (*.campsurlevel, *.json)");
  if (path && path[0]) {
    if (criogenio::SaveWorldToFile(GetWorld(), path)) {
      currentLevelPath = fs::absolute(path).string();
      levelDirty = false;
    }
  }
}

void EditorApp::SaveProjectDescriptor() {
  if (!project.has_value() || !projectCampsurAbsPath.has_value()) {
    std::fprintf(stderr, "[Editor] No project file path to save.\n");
    return;
  }
  if (!ProjectContext::SaveToFile(*projectCampsurAbsPath, *project))
    std::fprintf(stderr, "[Editor] Save project failed: %s\n",
                 projectCampsurAbsPath->c_str());
  else
    std::printf("[Editor] Saved project: %s\n", projectCampsurAbsPath->c_str());
}

namespace {

static std::string FindFirstImageUnderDir(const std::string &dir) {
  if (dir.empty())
    return {};
  std::error_code ec;
  fs::directory_options opts = fs::directory_options::skip_permission_denied;
  for (fs::recursive_directory_iterator it(dir, opts, ec), end;
       it != end; it.increment(ec)) {
    if (ec)
      break;
    if (!it->is_regular_file())
      continue;
    std::string ext = it->path().extension().string();
    for (char &c : ext)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
      return it->path().string();
  }
  return {};
}

static bool FileExistsPath(const std::string &p) {
  std::error_code ec;
  return !p.empty() && fs::is_regular_file(p, ec);
}

} // namespace

bool EditorApp::SaveCurrentLevelWithBake() {
  BakeEcsZonesIntoTmxMeta();
  return SaveCurrentLevel();
}

void EditorApp::UpsertPlayerStartInLevel(criogenio::Terrain2D &terrain, float worldCx,
                                         float worldCy) {
  for (auto &g : terrain.tmxMeta.objectGroups) {
    g.objects.erase(
        std::remove_if(g.objects.begin(), g.objects.end(),
                       [](const criogenio::TiledMapObject &o) {
                         return o.name == "player_start_position";
                       }),
        g.objects.end());
  }
  criogenio::TiledObjectGroup *spawns = nullptr;
  for (auto &g : terrain.tmxMeta.objectGroups) {
    if (g.name == "spawns") {
      spawns = &g;
      break;
    }
  }
  if (!spawns) {
    terrain.tmxMeta.objectGroups.push_back(criogenio::TiledObjectGroup{"spawns", -1, {}});
    spawns = &terrain.tmxMeta.objectGroups.back();
  }
  criogenio::TiledMapObject o;
  o.id = 91001;
  o.name = "player_start_position";
  o.x = worldCx;
  o.y = worldCy;
  o.point = true;
  spawns->objects.push_back(std::move(o));
}

void EditorApp::EnsureDefaultTilemapAfterTemplateReset() {
  std::string tsPath;
  if (project.has_value())
    tsPath = FindFirstImageUnderDir(project->spritesDir);
  static const char *kFallbackRels[] = {
      "assets/sprites/tileset.png",
      "assets/sprites/tiles.png",
      "subterra_guild/assets/sprites/tileset.png",
  };
  if (tsPath.empty() && project.has_value()) {
    for (const char *rel : kFallbackRels) {
      std::string p = project->Resolve(rel);
      if (FileExistsPath(p)) {
        tsPath = std::move(p);
        break;
      }
    }
  }
  if (tsPath.empty())
    std::fprintf(
        stderr,
        "[Editor] New 2D tilemap: no tileset image found under sprites dir or common paths; "
        "using empty path (assign a tileset later).\n");

  auto &w = GetWorld();
  w.CreateTerrain2D("Terrain", tsPath);
  criogenio::Terrain2D *t = w.GetTerrain();
  if (!t)
    return;

  t->tmxMeta.layerInfo.clear();
  criogenio::TiledLayerMeta lm;
  lm.name = "ground";
  lm.mapLayerIndex = 0;
  t->tmxMeta.layerInfo.push_back(std::move(lm));

  t->InferTmxContentBoundsFromTiles();

  const auto &m = t->tmxMeta;
  const float sx = static_cast<float>(t->GridStepX());
  const float sy = static_cast<float>(t->GridStepY());
  float cx = (static_cast<float>(m.boundsMinTx) + static_cast<float>(m.boundsMaxTx)) * 0.5f * sx;
  float cy = (static_cast<float>(m.boundsMinTy) + static_cast<float>(m.boundsMaxTy)) * 0.5f * sy;

  UpsertPlayerStartInLevel(*t, cx, cy);
  terrainEditMode = true;
  if (criogenio::Camera2D *cam = w.GetActiveCamera()) {
    cam->target.x = cx;
    cam->target.y = cy;
  }
  MarkLevelDirty();
}

void EditorApp::HandleEditorShortcuts() {
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantTextInput)
    return;
  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    SaveCurrentLevelWithBake();
    return;
  }
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    SaveCurrentLevel();
}

void EditorApp::ApplyLevelTemplateReset(int kind) {
  auto &world = GetWorld();
  for (auto id : world.GetAllEntities())
    world.DeleteEntity(id);
  world.DeleteTerrain();
  terrainHistory.Clear();
  selectedEntityId.reset();
  tileSelection.reset();
  if (kind == 2) {
    sceneMode = SceneMode::Scene3D;
    terrainEditMode = false;
  } else {
    sceneMode = SceneMode::Scene2D;
    if (kind == 0) {
      snapToGrid = true;
      terrainEditMode = false;
    } else {
      snapToGrid = false;
      terrainEditMode = false;
    }
  }
  EditorAppReset();
  if (kind == 0)
    EnsureDefaultTilemapAfterTemplateReset();
}

void EditorApp::DrawNewLevelModal() {
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("NewLevelModal", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
    ImGui::InputText("Filename", newLevelFilenameBuf, sizeof newLevelFilenameBuf);
    ImGui::RadioButton("2D tilemap", &newLevelTemplateKind, 0);
    ImGui::RadioButton("2D free-form", &newLevelTemplateKind, 1);
    ImGui::RadioButton("3D", &newLevelTemplateKind, 2);
    if (project.has_value())
      ImGui::Checkbox("Set as project init level", &newLevelSetAsInit);
    else
      newLevelSetAsInit = false;

    if (ImGui::Button("Create", ImVec2(120, 0))) {
      std::string name = newLevelFilenameBuf;
      if (name.empty())
        name = "untitled.campsurlevel";
      if (name.find('.') == std::string::npos)
        name += ".campsurlevel";

      std::string fullPath;
      if (project.has_value() && !project->levelsDir.empty()) {
        fullPath = (fs::path(project->levelsDir) / name).string();
      } else {
        const char *filters[] = {"*.campsurlevel", "*.json"};
        const char *picked =
            tinyfd_saveFileDialog("Save New Level", name.c_str(), 2, filters, "Level");
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        if (picked && picked[0]) {
          ApplyLevelTemplateReset(newLevelTemplateKind);
          std::string fp = fs::absolute(picked).string();
          if (criogenio::SaveWorldToFile(GetWorld(), fp)) {
            currentLevelPath = fp;
            levelDirty = false;
            if (newLevelSetAsInit && project.has_value()) {
              project->initLevel = fp;
              if (projectCampsurAbsPath.has_value())
                ProjectContext::SaveToFile(*projectCampsurAbsPath, *project);
            }
          }
        }
        return;
      }

      ApplyLevelTemplateReset(newLevelTemplateKind);
      if (criogenio::SaveWorldToFile(GetWorld(), fullPath)) {
        currentLevelPath = fs::absolute(fullPath).string();
        levelDirty = false;
        if (newLevelSetAsInit && project.has_value()) {
          project->initLevel = *currentLevelPath;
          if (projectCampsurAbsPath.has_value())
            ProjectContext::SaveToFile(*projectCampsurAbsPath, *project);
        }
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0)))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
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

    std::function<bool(const void*)> passEventToImGui = [](const void* ev) {
      ImGui_ImplSDL3_ProcessEvent((const SDL_Event*)ev);
      return false;
    };
    ren.ProcessEvents(&passEventToImGui);

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();

    HandleEditorShortcuts();

    HandleInput(dt, mouseDelta);
    if (!isPlaying) {
      Vec2 worldMouse = ScreenToWorldPosition(GetViewportMousePos());
      const bool click0 = ImGui::IsMouseClicked(0) && viewportHovered;

      if (activeColliderHandle == ZoneHandle::None && activeZoneHandle == ZoneHandle::None &&
          click0 && selectedEntityId.has_value()) {
        const int se = selectedEntityId.value();
        ZoneHandle zh = HitTestZoneHandle(se, worldMouse);
        if (zh != ZoneHandle::None) {
          activeZoneHandle = zh;
          activeColliderHandle = ZoneHandle::None;
          float zx, zy, zw, zh0;
          ComputeEntityPickBounds(GetWorld(), se, zx, zy, zw, zh0);
          zoneResizeDragOrigin = worldMouse;
          zoneResizeInitX = zx;
          zoneResizeInitY = zy;
          zoneResizeInitW = zw;
          zoneResizeInitH = zh0;
        } else if (showColliderGizmo && GetWorld().GetComponent<BoxCollider>(se)) {
          ZoneHandle ch = HitTestColliderHandle(se, worldMouse);
          if (ch != ZoneHandle::None) {
            activeColliderHandle = ch;
            activeZoneHandle = ZoneHandle::None;
            auto *tr = GetWorld().GetComponent<Transform>(se);
            auto *col = GetWorld().GetComponent<BoxCollider>(se);
            if (tr && col) {
              colliderResizeDragOrigin = worldMouse;
              colliderResizeInitX = tr->x + col->offsetX;
              colliderResizeInitY = tr->y + col->offsetY;
              colliderResizeInitW = col->width;
              colliderResizeInitH = col->height;
            }
          }
        }
      }

      if (activeColliderHandle != ZoneHandle::None) {
        HandleColliderResize(worldMouse);
      } else if (activeZoneHandle != ZoneHandle::None) {
        HandleZoneResize(worldMouse);
      } else {
        HandleEntityDrag(mouseDelta);
        HandleScenePicking();
      }
    }

    if (isPlaying) {
      if (gameMode) gameMode->Tick(dt);
      GetWorld().Update(dt);
    }

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

  ImGui::Checkbox("Show Collider Bounds", &showColliderDebug);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Draw BoxCollider outlines in the viewport (green = solid, cyan = one-way platform).");
  }
  ImGui::Checkbox("Collider resize gizmo", &showColliderGizmo);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When an entity with BoxCollider is selected, show green handles to resize in the viewport.");
  }
  ImGui::Checkbox("Show Map Authoring Zones", &showMapAuthoringGizmos);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Map event (cyan), interactable (magenta), spawn prefab (orange). "
        "Picking prefers the smallest hit box.");
  }
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
  namespace ecs = criogenio::ecs;
  criogenio::World &w = GetWorld();

  std::unordered_map<ecs::EntityId, std::vector<ecs::EntityId>> childrenByParent;
  std::vector<ecs::EntityId> roots;
  roots.reserve(w.GetAllEntities().size());

  for (ecs::EntityId id : w.GetAllEntities()) {
    if (auto *p = w.GetComponent<criogenio::Parent>(id)) {
      if (p->parent != ecs::NULL_ENTITY && w.HasEntity(p->parent))
        childrenByParent[p->parent].push_back(id);
      else
        roots.push_back(id);
    } else {
      roots.push_back(id);
    }
  }
  for (auto &kv : childrenByParent)
    std::sort(kv.second.begin(), kv.second.end());
  std::sort(roots.begin(), roots.end());

  std::vector<ecs::EntityId> pendingDelete;

  if (ImGui::Begin("Hierarchy")) {
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *payload =
              ImGui::AcceptDragDropPayload("CAMPSUR_ENTITY_ID")) {
        IM_ASSERT(payload->DataSize == sizeof(ecs::EntityId));
        ecs::EntityId dragged = *(const ecs::EntityId *)payload->Data;
        SetEntityParent(dragged, ecs::NULL_ENTITY);
      }
      ImGui::EndDragDropTarget();
    }

    for (ecs::EntityId root : roots)
      DrawHierarchyEntityNode(root, childrenByParent, pendingDelete);

    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Create Empty")) {
        ecs::EntityId e = w.CreateEntity("GameObject");
        w.AddComponent<criogenio::Transform>(e);
        if (criogenio::Camera2D *cam = w.GetActiveCamera()) {
          if (auto *t = w.GetComponent<criogenio::Transform>(e)) {
            t->x = cam->target.x;
            t->y = cam->target.y;
          }
        }
        w.AddComponent<criogenio::Name>(e, "GameObject");
        selectedEntityId = static_cast<int>(e);
        MarkLevelDirty();
      }
      if (selectedEntityId.has_value()) {
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Entity"))
          DestroyEntityInEditor(
              static_cast<ecs::EntityId>(selectedEntityId.value()));
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();

  for (ecs::EntityId id : pendingDelete)
    DestroyEntityInEditor(id);

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
    if (isPlaying) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
      if (ImGui::Button("■ Stop")) {
        if (gameMode) {
          gameMode->Stop(*this, revertOnStop);
          gameMode.reset();
        }
        isPlaying = false;
      }
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::Checkbox("Revert", &revertOnStop);
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "▶ Playing");
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
      if (ImGui::Button("▶ Play")) {
        // Strip authoring-only EditorHidden tags so play mode renders the
        // actual game state, not whatever the user toggled in the Layers
        // panel. SyncObjectLayerState will reapply them after Stop.
        auto &world = GetWorld();
        for (int eid : world.GetAllEntities()) {
          if (world.GetComponent<criogenio::EditorHidden>(eid))
            world.RemoveComponent<criogenio::EditorHidden>(eid);
        }
        gameMode = MakeEditorGameModeForProject(project);
        gameMode->Start(*this, project.has_value() ? &(*project) : nullptr);
        isPlaying = true;
      }
      ImGui::PopStyleColor();
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();

    static ImVec2 lastSize = {0, 0};
    if ((int)avail.x != (int)lastSize.x || (int)avail.y != (int)lastSize.y) {
      if (sceneRT.valid())
        GetRenderer().DestroyRenderTarget(&sceneRT);
      if (avail.x > 0 && avail.y > 0)
        sceneRT = GetRenderer().CreateRenderTarget((int)avail.x, (int)avail.y);
      lastSize = avail;
    }

    viewportHovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // Use cursor position and avail size so viewport matches where the image is
    // actually drawn (below the Play/Stop header). Using vMin/vMax was wrong
    // because the image is drawn at the cursor, not at the content region top.
    viewportPos = ImGui::GetCursorScreenPos();
    viewportSize = avail;

    if (sceneRT.valid() && viewportSize.x > 0 && viewportSize.y > 0) {
      ImGui::Image((ImTextureID)(intptr_t)sceneRT.opaque, viewportSize,
                   ImVec2(0, 0), ImVec2(1, 1));
      // Drop target: dragged sprites from the Asset Browser become entities
      // with Transform + Sprite at the cursor world position.
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("CAMPSUR_SPRITE_PATH")) {
          const char *path = static_cast<const char *>(payload->Data);
          if (path && path[0]) SpawnSpriteEntityAt(GetMouseWorld(), path);
        }
        ImGui::EndDragDropTarget();
      }
    }

    // Context menu
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Create Empty")) {
        CreateEmptyEntityAtMouse();
      }
      if (sceneMode == SceneMode::Scene2D) {
        if (ImGui::BeginMenu("Add Zone Here")) {
          if (ImGui::MenuItem("Map Event Zone"))
            CreateMapEventZoneAtMouse();
          if (ImGui::MenuItem("Interactable Zone"))
            CreateInteractableZoneAtMouse();
          if (ImGui::MenuItem("World Spawn Prefab"))
            CreateSpawnPrefabAtMouse();
          ImGui::EndMenu();
        }
      }
      if (selectedEntityId.has_value()) {
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate Entity")) {
          const int src = selectedEntityId.value();
          const SerializedWorld sw = GetWorld().Serialize(".");
          for (const auto &se : sw.entities) {
            if (se.id != src)
              continue;
            const criogenio::ecs::EntityId neo = GetWorld().CreateEntity("copy");
            for (const auto &sc : se.components) {
              criogenio::Component *c = criogenio::ComponentFactory::Create(sc.type, GetWorld(), neo);
              if (c)
                c->Deserialize(sc);
            }
            // Offset duplicate so it's visible
            if (auto *t = GetWorld().GetComponent<criogenio::Transform>(neo)) {
              t->x += 32.f; t->y += 32.f;
            }
            selectedEntityId = static_cast<int>(neo);
            MarkLevelDirty();
            break;
          }
        }
        if (ImGui::MenuItem("Delete Entity")) {
          GetWorld().DeleteEntity(selectedEntityId.value());
          selectedEntityId.reset();
          activeZoneHandle = ZoneHandle::None;
          activeColliderHandle = ZoneHandle::None;
          MarkLevelDirty();
        }
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();
}

bool EditorApp::IsMouseInWorldView() {
  Vec2 mouse = GetViewportMousePos();
  criogenio::Rect viewRect = {(float)viewportPos.x, (float)viewportPos.y, (float)viewportSize.x, (float)viewportSize.y};
  return criogenio::PointInRect(mouse, viewRect);
}

namespace {

/** True if `ancestor` is on the parent chain above `descendant`. */
static bool EditorHierarchyIsUnder(criogenio::World &w, criogenio::ecs::EntityId ancestor,
                                   criogenio::ecs::EntityId descendant) {
  namespace ecs = criogenio::ecs;
  for (ecs::EntityId x = descendant; x != ecs::NULL_ENTITY;) {
    auto *p = w.GetComponent<criogenio::Parent>(x);
    if (!p)
      break;
    if (p->parent == ancestor)
      return true;
    x = p->parent;
  }
  return false;
}

static void ApplyTransformDeltaToHierarchyChildren(criogenio::World &w,
                                                   criogenio::ecs::EntityId root,
                                                   float dx, float dy) {
  namespace ecs = criogenio::ecs;
  if ((dx == 0.f && dy == 0.f) || !w.HasEntity(root))
    return;
  for (ecs::EntityId id : w.GetAllEntities()) {
    if (id == root)
      continue;
    if (!EditorHierarchyIsUnder(w, root, id))
      continue;
    if (auto *t = w.GetComponent<criogenio::Transform>(id)) {
      t->x += dx;
      t->y += dy;
    }
  }
}

} // namespace

void EditorApp::HandleEntityDrag(Vec2 mouseDelta) {
  if (sceneMode == SceneMode::Scene3D)
    return;
  if (!selectedEntityId.has_value())
    return;
  if (!IsMouseInWorldView())
    return;

  if (Input::IsMouseDown(0)) {
    Vec2 mouseScreen = GetViewportMousePos();
    Vec2 prevMouseScreen = {mouseScreen.x - mouseDelta.x, mouseScreen.y - mouseDelta.y};

    Vec2 prevWorld = ScreenToWorldPosition(prevMouseScreen);
    Vec2 currWorld = ScreenToWorldPosition(mouseScreen);

    Vec2 drag = {currWorld.x - prevWorld.x, currWorld.y - prevWorld.y};

    auto* transform =
        GetWorld().GetComponent<criogenio::Transform>(selectedEntityId.value());
    if (transform) {
      const float ox = transform->x;
      const float oy = transform->y;
      transform->x += drag.x;
      transform->y += drag.y;
      Vec2 snapped = SnapWorldToGrid({transform->x, transform->y});
      const float dx = snapped.x - ox;
      const float dy = snapped.y - oy;
      transform->x = snapped.x;
      transform->y = snapped.y;
      if (dx != 0.f || dy != 0.f) {
        ApplyTransformDeltaToHierarchyChildren(
            GetWorld(),
            static_cast<criogenio::ecs::EntityId>(selectedEntityId.value()), dx, dy);
        MarkLevelDirty();
      }
    }
  }
}

void EditorApp::HandleInput(float dt, Vec2 mouseDelta) {
  if (sceneMode == SceneMode::Scene3D)
    return;
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

  // WASD camera movement only in edit mode; when playing, input goes to the game
  if (!isPlaying) {
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
}

// Height (in px) of the slim project info bar that sits between the main menu
// and the dockspace. The dockspace uses this constant to offset itself so the
// bar always remains visible above the viewport.
static constexpr float kProjectInfoBarHeight = 30.0f;

void EditorApp::OnGUI() {
  // Reconcile object-layer state up front: discover newly-loaded layer names,
  // re-apply Hidden tags so the runtime mirrors `hiddenObjectLayers`. Skipped
  // in play mode so play-mode rendering isn't subject to authoring filters.
  if (!isPlaying)
    SyncObjectLayerState();

  DrawDockSpace();
  DrawProjectInfoBar();
  DrawNewLevelModal();
  DrawTerrainEditor();
  DrawTerrainLayersPanel();
  DrawLevelMetadataPanel();
  DrawAssetBrowser();
  if (isPlaying && gameMode)
    gameMode->DrawHUD();
  DrawAnimationEditorWindow();
  DrawTerrainStatusBar();
  DrawMainMenuBar();
}

void EditorApp::DrawDockSpace() {
  ImGuiIO &io = ImGui::GetIO();
  if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable))
    return;

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  // Leave room above the dockspace for the main menu bar + project info bar.
  const float topPadding = 8.0f + kProjectInfoBarHeight;
  ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + topPadding));
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - topPadding));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("EditorDockSpace", nullptr, flags);

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

  ImGui::DockSpace(dockspace_id, ImVec2(0, 0));

  // Build the canonical layout when:
  //   - no saved layout exists yet (first launch or fresh imgui.ini), OR
  //   - the user explicitly requested a reset via View > Reset Layout.
  // Otherwise we trust the layout that was deserialized from imgui.ini, so
  // user customizations persist across runs. Existing users upgrading to a
  // build that introduces new panels should run View > Reset Layout once to
  // pick up the new docking targets (otherwise the new panels float).
  const bool noSavedLayout =
      (ImGui::DockBuilderGetNode(dockspace_id) == nullptr);
  if (!dockLayoutBuilt && (noSavedLayout || dockLayoutResetRequested)) {
    dockLayoutBuilt = true;
    dockLayoutResetRequested = false;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID dock_main = dockspace_id;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(
        dock_main, ImGuiDir_Left, 0.20f, nullptr, &dock_main);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(
        dock_main, ImGuiDir_Right, 0.25f, nullptr, &dock_main);

    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Layers", dock_left);
    ImGui::DockBuilderDockWindow("Asset Browser", dock_left);
    ImGui::DockBuilderDockWindow("Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Terrain Editor", dock_right);
    ImGui::DockBuilderDockWindow("Animation Editor", dock_right);
    ImGui::DockBuilderDockWindow("Global Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Viewport", dock_main);
    ImGui::DockBuilderFinish(dockspace_id);
  } else {
    dockLayoutBuilt = true;
  }
  DrawHierarchyPanel();
  DrawGlobalInspector();
  ImGui::End();
}

void EditorApp::DrawProjectInfoBar() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();

  // Sit just below the main menu bar.
  const float menuBarH = ImGui::GetFrameHeight();
  ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarH));
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, kProjectInfoBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##ProjectInfoBar", nullptr, flags);

  if (project.has_value()) {
    ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.f), "%s", "\xE2\x96\xB8");
    ImGui::SameLine();
    ImGui::TextUnformatted(project->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", project->projectRoot.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (currentLevelPath.has_value()) {
      std::error_code ec;
      fs::path rel =
          fs::relative(fs::path(*currentLevelPath), fs::path(project->projectRoot), ec);
      const std::string show =
          !ec ? rel.generic_string()
              : fs::path(*currentLevelPath).filename().string();
      ImGui::Text("Level: %s%s", show.c_str(), levelDirty ? " *" : "");
    } else {
      ImGui::TextDisabled("Level: %s", levelDirty ? "(unsaved) *" : "(no file)");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Save##levelbar"))
      SaveCurrentLevel();

    const float btnW = 60.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - btnW - 12.0f);
    if (ImGui::SmallButton("Close")) {
      project.reset();
      projectCampsurAbsPath.reset();
      currentLevelPath.reset();
      levelDirty = false;
      assetBrowserDirty = true;
    }
  } else {
    ImGui::TextDisabled("No project loaded — use File > Open Project...");
    ImGui::SameLine();
    if (currentLevelPath.has_value()) {
      ImGui::Text("Level: %s%s", fs::path(*currentLevelPath).filename().c_str(),
                  levelDirty ? " *" : "");
      ImGui::SameLine();
      if (ImGui::SmallButton("Save##levelbar2"))
        SaveCurrentLevel();
    }
  }

  ImGui::End();
  ImGui::PopStyleVar(2);
}

void EditorApp::DrawTerrainStatusBar() {
  if (!terrainEditMode || isPlaying)
    return;
  Terrain2D *terrain = GetWorld().GetTerrain();
  if (!terrain) return;

  ImGuiViewport *vp = ImGui::GetMainViewport();
  const float h = 22.0f;
  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - h));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x, h));
  ImGui::SetNextWindowViewport(vp->ID);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##TerrainStatusBar", nullptr, flags);

  const char *toolName = "Paint";
  switch (terrainTool) {
    case TerrainTool::Paint:      toolName = "Paint";      break;
    case TerrainTool::Rect:       toolName = "Rect";       break;
    case TerrainTool::FilledRect: toolName = "FilledRect"; break;
    case TerrainTool::Line:       toolName = "Line";       break;
    case TerrainTool::FloodFill:  toolName = "Flood Fill"; break;
    case TerrainTool::Eyedropper: toolName = "Eyedropper"; break;
    case TerrainTool::Select:     toolName = "Select";     break;
    case TerrainTool::Stamp:      toolName = "Stamp";      break;
  }

  ImGui::Text("tile (%d, %d)", terrainDragCurTx, terrainDragCurTy);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("layer %d", terrainSelectedLayer);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("tool: %s", toolName);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("brush %dx%d", terrainBrushW, terrainBrushH);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  if (terrainSelectedIsGid)
    ImGui::Text("gid: %u", criogenio::TileGid(terrainSelectedTile));
  else
    ImGui::Text("tile: %d", terrainSelectedTile);
  if (terrainSelectedIsGid && (brushFlipH || brushFlipV)) {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s%s",
                       brushFlipH ? "H" : "", brushFlipV ? "V" : "");
  }

  ImGui::SameLine();
  const float right = ImGui::GetWindowWidth() - 220.0f;
  if (right > 0) ImGui::SameLine(right);
  ImGui::TextDisabled("undo: %zu  redo: %zu", terrainHistory.UndoSize(), terrainHistory.RedoSize());

  ImGui::End();
  ImGui::PopStyleVar(2);
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

void EditorApp::DrawColliderDebug(criogenio::Renderer& ren) {
  auto ids = GetWorld().GetEntitiesWith<criogenio::Transform, criogenio::BoxCollider>();
  for (criogenio::ecs::EntityId id : ids) {
    auto* tr = GetWorld().GetComponent<criogenio::Transform>(id);
    auto* col = GetWorld().GetComponent<criogenio::BoxCollider>(id);
    if (!tr || !col)
      continue;
    bool selected = selectedEntityId.has_value() && selectedEntityId.value() == id;
    criogenio::Color color = col->isPlatform
                                 ? criogenio::Color{0, 200, 255, 255}
                                 : criogenio::Color{0, 255, 0, 255};
    if (selected)
      color = criogenio::Colors::Yellow;
    ren.DrawRectOutline(tr->x + col->offsetX, tr->y + col->offsetY, col->width, col->height, color);
  }
}

void EditorApp::DrawMapAuthoringGizmos(criogenio::Renderer& ren) {
  auto outline = [&](int entity, const criogenio::Color &c) {
    float x, y, w, h;
    if (!ComputeEntityPickBounds(GetWorld(), entity, x, y, w, h))
      return;
    ren.DrawRectOutline(x, y, w, h, c);
  };

  for (int e : GetWorld().GetEntitiesWith<criogenio::MapEventZone2D, criogenio::Transform>())
    outline(e, criogenio::Color{80, 200, 255, 255});
  for (int e : GetWorld().GetEntitiesWith<criogenio::InteractableZone2D, criogenio::Transform>())
    outline(e, criogenio::Color{255, 120, 220, 255});
  for (int e : GetWorld().GetEntitiesWith<criogenio::WorldSpawnPrefab2D, criogenio::Transform>())
    outline(e, criogenio::Color{255, 180, 60, 255});

  if (selectedEntityId.has_value()) {
    DrawZoneResizeHandles(ren, selectedEntityId.value());
    if (showColliderGizmo && GetWorld().GetComponent<BoxCollider>(selectedEntityId.value()))
      DrawColliderResizeHandles(ren, selectedEntityId.value());
  }
}

void EditorApp::DrawMapAuthoringLabels(criogenio::Renderer &ren) {
  if (!showMapAuthoringGizmos)
    return;
  const criogenio::Camera2D *cam = GetWorld().GetActiveCamera();
  if (!cam)
    return;

  auto drawLabel = [&](int entity, const std::string &label,
                       const criogenio::Color &col) {
    float x, y, w, h;
    if (!ComputeEntityPickBounds(GetWorld(), entity, x, y, w, h))
      return;
    criogenio::Vec2 worldCenter{x + w * 0.5f, y + h * 0.5f};
    criogenio::Vec2 sc =
        WorldToScreen2D(worldCenter, *cam, (float)sceneRT.width, (float)sceneRT.height);
    ren.DrawTextString(label, (int)sc.x + 2, (int)sc.y - 8, 11, col);
  };

  for (int e : GetWorld().GetEntitiesWith<criogenio::MapEventZone2D, criogenio::Transform>()) {
    auto *z = GetWorld().GetComponent<criogenio::MapEventZone2D>(e);
    std::string lbl = (z && !z->storage_key.empty()) ? z->storage_key : "event";
    drawLabel(e, lbl, criogenio::Color{80, 200, 255, 220});
  }
  for (int e :
       GetWorld().GetEntitiesWith<criogenio::InteractableZone2D, criogenio::Transform>()) {
    auto *z = GetWorld().GetComponent<criogenio::InteractableZone2D>(e);
    std::string lbl =
        (z && !z->interactable_type.empty()) ? z->interactable_type : "interactable";
    drawLabel(e, lbl, criogenio::Color{255, 120, 220, 220});
  }
  for (int e :
       GetWorld().GetEntitiesWith<criogenio::WorldSpawnPrefab2D, criogenio::Transform>()) {
    auto *z = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(e);
    std::string lbl = (z && !z->prefab_name.empty()) ? z->prefab_name : "spawn";
    drawLabel(e, lbl, criogenio::Color{255, 180, 60, 220});
  }
}

void EditorApp::DrawZoneResizeHandles(criogenio::Renderer& ren, int entity) {
  float zx, zy, zw, zh;
  if (!ComputeEntityPickBounds(GetWorld(), entity, zx, zy, zw, zh))
    return;
  auto *mz = GetWorld().GetComponent<criogenio::MapEventZone2D>(entity);
  auto *iz = GetWorld().GetComponent<criogenio::InteractableZone2D>(entity);
  auto *sz = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(entity);
  if (!mz && !iz && !sz)
    return;

  float hx[9], hy[9];
  ComputeHandlePositions(zx, zy, zw, zh, hx, hy);
  const float r = kHandleRadius;
  const criogenio::Color hcol{255, 255, 255, 220};
  for (int i = (int)ZoneHandle::TL; i <= (int)ZoneHandle::BR; ++i) {
    ren.DrawRectOutline(hx[i] - r, hy[i] - r, r * 2.f, r * 2.f, hcol);
    ren.DrawRect(hx[i] - r + 1.f, hy[i] - r + 1.f, r * 2.f - 2.f, r * 2.f - 2.f,
                 criogenio::Color{80, 80, 80, 200});
  }
}

ZoneHandle EditorApp::HitTestZoneHandle(int entity, criogenio::Vec2 worldPos) {
  float zx, zy, zw, zh;
  ComputeEntityPickBounds(GetWorld(), entity, zx, zy, zw, zh);
  auto *mz = GetWorld().GetComponent<criogenio::MapEventZone2D>(entity);
  auto *iz = GetWorld().GetComponent<criogenio::InteractableZone2D>(entity);
  auto *sz = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(entity);
  if (!mz && !iz && !sz)
    return ZoneHandle::None;
  float hx[9], hy[9];
  ComputeHandlePositions(zx, zy, zw, zh, hx, hy);
  const float r = kHandleRadius * 1.5f;
  for (int i = (int)ZoneHandle::TL; i <= (int)ZoneHandle::BR; ++i) {
    if (worldPos.x >= hx[i] - r && worldPos.x <= hx[i] + r &&
        worldPos.y >= hy[i] - r && worldPos.y <= hy[i] + r)
      return static_cast<ZoneHandle>(i);
  }
  return ZoneHandle::None;
}

void EditorApp::DrawColliderResizeHandles(criogenio::Renderer& ren, int entity) {
  auto *col = GetWorld().GetComponent<BoxCollider>(entity);
  auto *tr = GetWorld().GetComponent<Transform>(entity);
  if (!col || !tr)
    return;
  float zx = tr->x + col->offsetX;
  float zy = tr->y + col->offsetY;
  float zw = col->width;
  float zh = col->height;
  float hx[9], hy[9];
  ComputeHandlePositions(zx, zy, zw, zh, hx, hy);
  const float r = kHandleRadius;
  const criogenio::Color hcol{120, 255, 160, 220};
  for (int i = (int)ZoneHandle::TL; i <= (int)ZoneHandle::BR; ++i) {
    ren.DrawRectOutline(hx[i] - r, hy[i] - r, r * 2.f, r * 2.f, hcol);
    ren.DrawRect(hx[i] - r + 1.f, hy[i] - r + 1.f, r * 2.f - 2.f, r * 2.f - 2.f,
                 criogenio::Color{40, 90, 55, 200});
  }
}

ZoneHandle EditorApp::HitTestColliderHandle(int entity, criogenio::Vec2 worldPos) {
  auto *col = GetWorld().GetComponent<BoxCollider>(entity);
  auto *tr = GetWorld().GetComponent<Transform>(entity);
  if (!col || !tr)
    return ZoneHandle::None;
  float zx = tr->x + col->offsetX;
  float zy = tr->y + col->offsetY;
  float zw = col->width;
  float zh = col->height;
  float hx[9], hy[9];
  ComputeHandlePositions(zx, zy, zw, zh, hx, hy);
  const float r = kHandleRadius * 1.5f;
  for (int i = (int)ZoneHandle::TL; i <= (int)ZoneHandle::BR; ++i) {
    if (worldPos.x >= hx[i] - r && worldPos.x <= hx[i] + r &&
        worldPos.y >= hy[i] - r && worldPos.y <= hy[i] + r)
      return static_cast<ZoneHandle>(i);
  }
  return ZoneHandle::None;
}

void EditorApp::HandleColliderResize(criogenio::Vec2 worldMouse) {
  if (!selectedEntityId.has_value())
    return;
  int entity = selectedEntityId.value();
  if (snapToGrid) {
    const criogenio::Terrain2D *terr = GetWorld().GetTerrain();
    if (terr) {
      const float gx = static_cast<float>(terr->GridStepX());
      const float gy = static_cast<float>(terr->GridStepY());
      if (gx > 0 && gy > 0) {
        worldMouse.x =
            std::floor((worldMouse.x - terr->origin.x) / gx) * gx + terr->origin.x;
        worldMouse.y =
            std::floor((worldMouse.y - terr->origin.y) / gy) * gy + terr->origin.y;
      }
    }
  }
  if (!ImGui::IsMouseDown(0)) {
    if (activeColliderHandle != ZoneHandle::None)
      MarkLevelDirty();
    activeColliderHandle = ZoneHandle::None;
    return;
  }
  if (activeColliderHandle == ZoneHandle::None)
    return;
  auto *tr = GetWorld().GetComponent<Transform>(entity);
  auto *col = GetWorld().GetComponent<BoxCollider>(entity);
  if (!tr || !col)
    return;
  const float dx = worldMouse.x - colliderResizeDragOrigin.x;
  const float dy = worldMouse.y - colliderResizeDragOrigin.y;
  float nx = colliderResizeInitX;
  float ny = colliderResizeInitY;
  float nw = colliderResizeInitW;
  float nh = colliderResizeInitH;
  switch (activeColliderHandle) {
    case ZoneHandle::TL: nx += dx; ny += dy; nw -= dx; nh -= dy; break;
    case ZoneHandle::T: ny += dy; nh -= dy; break;
    case ZoneHandle::TR: ny += dy; nw += dx; nh -= dy; break;
    case ZoneHandle::L: nx += dx; nw -= dx; break;
    case ZoneHandle::R: nw += dx; break;
    case ZoneHandle::BL: nx += dx; nw -= dx; nh += dy; break;
    case ZoneHandle::B: nh += dy; break;
    case ZoneHandle::BR: nw += dx; nh += dy; break;
    default: break;
  }
  nw = std::max(nw, 1.f);
  nh = std::max(nh, 1.f);
  col->offsetX = nx - tr->x;
  col->offsetY = ny - tr->y;
  col->width = nw;
  col->height = nh;
}

criogenio::Vec2 EditorApp::SnapWorldToGrid(criogenio::Vec2 world) {
  if (!Input::IsKeyDown(static_cast<int>(criogenio::Key::LeftCtrl)) &&
      !Input::IsKeyDown(static_cast<int>(criogenio::Key::RightCtrl)))
    return world;
  const criogenio::Terrain2D *t = GetWorld().GetTerrain();
  const float grid = t ? static_cast<float>(t->tileset.tileSize) : 32.f;
  world.x = std::round(world.x / grid) * grid;
  world.y = std::round(world.y / grid) * grid;
  return world;
}

void EditorApp::HandleZoneResize(criogenio::Vec2 worldMouse) {
  if (!selectedEntityId.has_value())
    return;

  int entity = selectedEntityId.value();
  worldMouse = SnapWorldToGrid(worldMouse);

  if (!ImGui::IsMouseDown(0)) {
    if (activeZoneHandle != ZoneHandle::None)
      MarkLevelDirty();
    activeZoneHandle = ZoneHandle::None;
    return;
  }

  if (activeZoneHandle == ZoneHandle::None)
    return;

  auto *tr = GetWorld().GetComponent<criogenio::Transform>(entity);
  if (!tr)
    return;

  auto applyResize = [&](float &compW, float &compH) {
    const float dx = worldMouse.x - zoneResizeDragOrigin.x;
    const float dy = worldMouse.y - zoneResizeDragOrigin.y;
    float nx = zoneResizeInitX, ny = zoneResizeInitY;
    float nw = zoneResizeInitW, nh = zoneResizeInitH;
    switch (activeZoneHandle) {
      case ZoneHandle::TL: nx += dx; ny += dy; nw -= dx; nh -= dy; break;
      case ZoneHandle::T:               ny += dy; nh -= dy; break;
      case ZoneHandle::TR:              ny += dy; nw += dx; nh -= dy; break;
      case ZoneHandle::L:  nx += dx;            nw -= dx;            break;
      case ZoneHandle::R:                        nw += dx;            break;
      case ZoneHandle::BL: nx += dx;            nw -= dx; nh += dy;  break;
      case ZoneHandle::B:                                  nh += dy;  break;
      case ZoneHandle::BR:                       nw += dx; nh += dy;  break;
      default: break;
    }
    nw = std::max(nw, 1.f);
    nh = std::max(nh, 1.f);
    tr->x = nx;
    tr->y = ny;
    compW = nw;
    compH = nh;
  };

  if (auto *z = GetWorld().GetComponent<criogenio::MapEventZone2D>(entity))
    applyResize(z->width, z->height);
  else if (auto *z = GetWorld().GetComponent<criogenio::InteractableZone2D>(entity))
    applyResize(z->width, z->height);
  else if (auto *z = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(entity))
    applyResize(z->width, z->height);
}

void EditorApp::BakeEcsZonesIntoTmxMeta() {
  criogenio::Terrain2D *terrain = GetWorld().GetTerrain();
  if (!terrain)
    return;
  criogenio::TmxMapMetadata &m = terrain->tmxMeta;

  // --- Map event zones → object group "ecs_event_zones" ---
  criogenio::TiledObjectGroup *ecsGroup = nullptr;
  for (auto &g : m.objectGroups) {
    if (g.name == "ecs_event_zones") { ecsGroup = &g; break; }
  }
  if (!ecsGroup) {
    m.objectGroups.push_back({"ecs_event_zones", -1, {}});
    ecsGroup = &m.objectGroups.back();
  }
  ecsGroup->objects.clear();

  int objIdSeq = 90000;
  for (int e : GetWorld().GetEntitiesWith<criogenio::MapEventZone2D, criogenio::Transform>()) {
    auto *z  = GetWorld().GetComponent<criogenio::MapEventZone2D>(e);
    auto *tr = GetWorld().GetComponent<criogenio::Transform>(e);
    if (!z || !tr || !z->activated) continue;
    criogenio::TiledMapObject o;
    o.id = objIdSeq++;
    o.name = z->storage_key;
    o.objectType = z->object_type.empty() ? "event" : z->object_type;
    o.x = tr->x;
    o.y = tr->y;
    o.width  = z->point ? 0.f : z->width;
    o.height = z->point ? 0.f : z->height;
    o.point  = z->point;
    auto setProp = [&](const char *k, const std::string &v) {
      if (!v.empty()) o.properties.push_back({k, v, "string"});
    };
    auto setBool = [&](const char *k, bool v) {
      o.properties.push_back({k, v ? "true" : "false", "bool"});
    };
    auto setInt = [&](const char *k, int v) {
      if (v) o.properties.push_back({k, std::to_string(v), "int"});
    };
    setProp("event_id",         z->event_id);
    setProp("event_trigger",    z->event_trigger);
    setProp("event_type",       z->event_type);
    setProp("teleport_to",      z->teleport_to.empty() ? z->target_level : z->teleport_to);
    setProp("spawn_point",      z->spawn_point);
    setProp("gameplay_actions", z->gameplay_actions);
    setInt ("spawn_count",      z->spawn_count);
    setInt ("monster_count",    z->monster_count);
    setBool("activated",        z->activated);
    ecsGroup->objects.push_back(std::move(o));
  }

  // --- Interactable zones ---
  std::unordered_set<int> bakedInterIds;
  for (int e : GetWorld().GetEntitiesWith<criogenio::InteractableZone2D, criogenio::Transform>()) {
    auto *z  = GetWorld().GetComponent<criogenio::InteractableZone2D>(e);
    auto *tr = GetWorld().GetComponent<criogenio::Transform>(e);
    if (!z || !tr || z->interactable_type.empty()) continue;
    criogenio::TiledInteractable ti;
    ti.x = tr->x; ti.y = tr->y;
    ti.w = z->point ? 0.f : z->width;
    ti.h = z->point ? 0.f : z->height;
    ti.is_point = z->point || (z->width < 0.5f && z->height < 0.5f);
    ti.tiled_object_id = z->tiled_object_id != 0 ? z->tiled_object_id : objIdSeq++;
    ti.interactable_type = z->interactable_type;
    if (!z->properties_json.empty()) {
      try {
        nlohmann::json jp = nlohmann::json::parse(z->properties_json);
        for (auto it = jp.begin(); it != jp.end(); ++it) {
          criogenio::TmxProperty p;
          p.name = it.key();
          p.value = it->is_string() ? it->get<std::string>() : it->dump();
          p.type  = "string";
          ti.properties.push_back(std::move(p));
        }
      } catch (...) {}
    }
    if (ti.tiled_object_id != 0)
      bakedInterIds.insert(ti.tiled_object_id);
    m.interactables.erase(
        std::remove_if(m.interactables.begin(), m.interactables.end(),
                       [&](const criogenio::TiledInteractable &x) {
                         return x.tiled_object_id == ti.tiled_object_id;
                       }),
        m.interactables.end());
    m.interactables.push_back(std::move(ti));
  }

  // --- Spawn prefab zones ---
  for (int e : GetWorld().GetEntitiesWith<criogenio::WorldSpawnPrefab2D, criogenio::Transform>()) {
    auto *z  = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(e);
    auto *tr = GetWorld().GetComponent<criogenio::Transform>(e);
    if (!z || !tr || z->prefab_name.empty()) continue;
    const float ecx = tr->x + z->width * 0.5f;
    const float ecy = tr->y + z->height * 0.5f;
    m.spawnPrefabs.erase(
        std::remove_if(m.spawnPrefabs.begin(), m.spawnPrefabs.end(),
                       [&](const criogenio::TiledSpawnPrefab &sp) {
                         if (sp.prefabName != z->prefab_name) return false;
                         const float scx = sp.x + sp.width * 0.5f;
                         const float scy = sp.y + sp.height * 0.5f;
                         return std::fabs(ecx - scx) < 24.f && std::fabs(ecy - scy) < 24.f;
                       }),
        m.spawnPrefabs.end());
    criogenio::TiledSpawnPrefab sp;
    sp.x = tr->x; sp.y = tr->y;
    sp.width = z->width; sp.height = z->height;
    sp.prefabName = z->prefab_name;
    sp.quantity = z->quantity > 0 ? z->quantity : 1;
    m.spawnPrefabs.push_back(std::move(sp));
  }
}

void EditorApp::RenderSceneToTexture() {
  if (!sceneRT.valid())
    return;
  criogenio::Renderer& ren = GetRenderer();
  ren.SetRenderTarget(sceneRT);
  if (sceneMode == SceneMode::Scene3D) {
    ren.DrawRect(0, 0, (float)sceneRT.width, (float)sceneRT.height, criogenio::Colors::Black);
    box3d::FPCamera cam = *GetWorld().GetActiveCamera3D();
    if (isPlaying) {
      int camEntity = GetWorld().GetMainCamera3DEntity();
      if (camEntity != criogenio::ecs::NULL_ENTITY) {
        auto* camComp = GetWorld().GetComponent<criogenio::Camera3D>(camEntity);
        auto* tr3d = GetWorld().GetComponent<criogenio::Transform3D>(camEntity);
        if (camComp && tr3d) {
          camComp->positionX = tr3d->x;
          camComp->positionY = tr3d->y;
          camComp->positionZ = tr3d->z;
          camComp->yaw = tr3d->rotationY;
          camComp->pitch = tr3d->rotationX;
          cam = camComp->ToFPCamera();
        }
      }
    }

    // Ground grid on XZ plane
    for (int i = -10; i <= 10; ++i) {
      Color gridColor = (i == 0) ? Color{95, 95, 95, 255} : Color{52, 52, 52, 255};
      DrawPerspectiveLine(ren, EditorVec3{-10.0f, 0.0f, static_cast<float>(i)},
                          EditorVec3{10.0f, 0.0f, static_cast<float>(i)}, cam, sceneRT.width,
                          sceneRT.height, gridColor);
      DrawPerspectiveLine(ren, EditorVec3{static_cast<float>(i), 0.0f, -10.0f},
                          EditorVec3{static_cast<float>(i), 0.0f, 10.0f}, cam, sceneRT.width,
                          sceneRT.height, gridColor);
    }

    // Axis gizmo
    DrawPerspectiveLine(ren, EditorVec3{0.0f, 0.0f, 0.0f}, EditorVec3{2.5f, 0.0f, 0.0f}, cam,
                        sceneRT.width, sceneRT.height, Color{240, 90, 90, 255});
    DrawPerspectiveLine(ren, EditorVec3{0.0f, 0.0f, 0.0f}, EditorVec3{0.0f, 2.5f, 0.0f}, cam,
                        sceneRT.width, sceneRT.height, Color{90, 240, 90, 255});
    DrawPerspectiveLine(ren, EditorVec3{0.0f, 0.0f, 0.0f}, EditorVec3{0.0f, 0.0f, 2.5f}, cam,
                        sceneRT.width, sceneRT.height, Color{90, 140, 255, 255});
    ImVec2 px, py, pz;
    if (ProjectPerspective(EditorVec3{2.8f, 0.0f, 0.0f}, cam, sceneRT.width, sceneRT.height, px))
      ren.DrawTextString("X", static_cast<int>(px.x), static_cast<int>(px.y), 14,
                         Color{240, 90, 90, 255});
    if (ProjectPerspective(EditorVec3{0.0f, 2.8f, 0.0f}, cam, sceneRT.width, sceneRT.height, py))
      ren.DrawTextString("Y", static_cast<int>(py.x), static_cast<int>(py.y), 14,
                         Color{90, 240, 90, 255});
    if (ProjectPerspective(EditorVec3{0.0f, 0.0f, 2.8f}, cam, sceneRT.width, sceneRT.height, pz))
      ren.DrawTextString("Z", static_cast<int>(pz.x), static_cast<int>(pz.y), 14,
                         Color{90, 140, 255, 255});

    // Draw 3D boxes from components
    for (int entity : GetWorld().GetAllEntities()) {
      auto* t = GetWorld().GetComponent<criogenio::Transform3D>(entity);
      auto* box = GetWorld().GetComponent<criogenio::Box3D>(entity);
      if (!t || !box)
        continue;

      Color boxColor{ClampColor(box->colorR), ClampColor(box->colorG), ClampColor(box->colorB),
                     255};
      EditorVec3 center{t->x, t->y, t->z};
      EditorVec3 half{box->halfX * t->scaleX, box->halfY * t->scaleY, box->halfZ * t->scaleZ};
      DrawPerspectiveWireCube(ren, center, half, cam, sceneRT.width, sceneRT.height, boxColor);
      if (!box->wireframe) {
        DrawPerspectiveLine(ren,
                            EditorVec3{center.x - half.x, center.y + half.y, center.z - half.z},
                            EditorVec3{center.x + half.x, center.y + half.y, center.z + half.z},
                            cam, sceneRT.width, sceneRT.height, boxColor);
        DrawPerspectiveLine(ren,
                            EditorVec3{center.x + half.x, center.y + half.y, center.z - half.z},
                            EditorVec3{center.x - half.x, center.y + half.y, center.z + half.z},
                            cam, sceneRT.width, sceneRT.height, boxColor);
      }

      if (selectedEntityId.has_value() && selectedEntityId.value() == entity) {
        EditorVec3 selHalf{half.x + 0.06f, half.y + 0.06f, half.z + 0.06f};
        DrawPerspectiveWireCube(ren, center, selHalf, cam, sceneRT.width, sceneRT.height,
                                criogenio::Colors::Yellow);
      }
    }

    ren.DrawTextString("3D Scene", 16, 12, 18, criogenio::Colors::White);
    ren.DrawTextString("Using active main Camera3D while playing.", 16, 34, 14,
                       criogenio::Colors::Gray);
    ren.UnsetRenderTarget();
    return;
  }

  // Camera offset must be (0,0) so screen center (halfW, halfH) maps to world target.
  // The formula uses offset + halfW, so offset=0 puts target at viewport center.
  GetWorld().GetActiveCamera()->offset = {0.0f, 0.0f};
  ren.DrawRect(0, 0, (float)sceneRT.width, (float)sceneRT.height, criogenio::Colors::Black);
  ren.BeginCamera2D(*GetWorld().GetActiveCamera());

  if (selectedEntityId.has_value()) {
    float sx, sy, sw, sh;
    if (ComputeEntityPickBounds(GetWorld(), selectedEntityId.value(), sx, sy, sw, sh))
      ren.DrawRectOutline(sx, sy, sw, sh, criogenio::Colors::Yellow);
  }

  GetWorld().Render(ren);

  if (showColliderDebug)
    DrawColliderDebug(ren);
  if (showMapAuthoringGizmos)
    DrawMapAuthoringGizmos(ren);

  if (terrainEditMode && GetWorld().GetTerrain()) {
    criogenio::Vec2 worldPos = ScreenToWorldPosition(GetViewportMousePos());
    criogenio::Vec2 tile = TerrainWorldToTile(worldPos, *GetWorld().GetTerrain());
    DrawTerrainGridOverlay(*GetWorld().GetTerrain(), *GetWorld().GetActiveCamera());
    DrawTileHighlight(ren, *GetWorld().GetTerrain(), tile, *GetWorld().GetActiveCamera());
    DrawTerrainToolOverlay(*GetWorld().GetTerrain());
  }

  ren.EndCamera2D();

  DrawMapAuthoringLabels(ren);

  ren.UnsetRenderTarget();
}

void EditorApp::BatchMigrateTmxMaps() {
  struct MapEntry { const char *tmx; const char *json; };
  static constexpr MapEntry kMaps[] = {
    {"subterra_guild/assets/levels/Cave.tmx",
     "subterra_guild/assets/levels/Cave.campsurlevel"},
    {"subterra_guild/assets/levels/City.tmx",
     "subterra_guild/assets/levels/City.campsurlevel"},
    {"subterra_guild/assets/levels/Corredor_Lab.tmx",
     "subterra_guild/assets/levels/Corredor_Lab.campsurlevel"},
    {"subterra_guild/assets/levels/SmallDunger/Dungeon.tmx",
     "subterra_guild/assets/levels/SmallDunger/Dungeon.campsurlevel"},
  };
  int ok = 0, fail = 0;
  for (const auto &m : kMaps) {
    criogenio::World tmp;
    if (!criogenio::LoadTerrainFromTmxFile(tmp, m.tmx)) {
      printf("[Editor] Migrate FAIL (load): %s\n", m.tmx);
      ++fail;
      continue;
    }
    if (!criogenio::SaveWorldToFile(tmp, m.json)) {
      printf("[Editor] Migrate FAIL (save): %s\n", m.json);
      ++fail;
      continue;
    }
    printf("[Editor] Migrated: %s -> %s\n", m.tmx, m.json);
    ++ok;
  }
  printf("[Editor] TMX migration done: %d ok, %d failed.\n", ok, fail);
}

void EditorApp::DrawMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Project...")) {
        const char *filters[] = {"*.campsur"};
        const char *p = tinyfd_openFileDialog("Open Project", "", 1,
                                              filters, "Campsur Project (*.campsur)", 0);
        if (p && std::strlen(p) > 0) {
          ProjectContext ctx;
          if (ProjectContext::LoadFromFile(p, ctx)) {
            project = std::move(ctx);
            projectCampsurAbsPath = fs::absolute(std::string(p)).string();
            std::filesystem::current_path(project->projectRoot);
            if (!project->initLevel.empty()) {
              auto &w = GetWorld();
              for (auto id : w.GetAllEntities()) w.DeleteEntity(id);
              w.DeleteTerrain();
              criogenio::LoadWorldFromFile(w, project->initLevel);
              EditorAppReset();
              currentLevelPath = fs::absolute(project->initLevel).string();
              levelDirty = false;
            } else {
              currentLevelPath.reset();
              levelDirty = false;
            }
            assetBrowserDirty = true;
          }
        }
      }
      if (ImGui::BeginMenu("Recent Projects")) {
        const auto &recent = ProjectContext::RecentPaths();
        if (recent.empty()) {
          ImGui::MenuItem("(none)", nullptr, false, false);
        } else {
          for (const auto &rp : recent) {
            if (ImGui::MenuItem(rp.c_str())) {
              ProjectContext ctx;
              if (ProjectContext::LoadFromFile(rp, ctx)) {
                project = std::move(ctx);
                projectCampsurAbsPath = fs::absolute(rp).string();
                std::filesystem::current_path(project->projectRoot);
                if (!project->initLevel.empty()) {
                  auto &w = GetWorld();
                  for (auto id : w.GetAllEntities()) w.DeleteEntity(id);
                  w.DeleteTerrain();
                  criogenio::LoadWorldFromFile(w, project->initLevel);
                  EditorAppReset();
                  currentLevelPath = fs::absolute(project->initLevel).string();
                  levelDirty = false;
                } else {
                  currentLevelPath.reset();
                  levelDirty = false;
                }
                assetBrowserDirty = true;
              }
            }
          }
        }
        ImGui::EndMenu();
      }
      if (project.has_value()) {
        const bool canSaveProj =
            projectCampsurAbsPath.has_value() && !projectCampsurAbsPath->empty();
        if (ImGui::MenuItem("Save Project", nullptr, false, canSaveProj)) {
          SaveProjectDescriptor();
        }
        if (ImGui::MenuItem("Close Project")) {
          project.reset();
          projectCampsurAbsPath.reset();
          currentLevelPath.reset();
          levelDirty = false;
          assetBrowserDirty = true;
        }
      }
      ImGui::Separator();
      if (ImGui::MenuItem("New Level...")) {
        ImGui::OpenPopup("NewLevelModal");
      }
      if (ImGui::MenuItem("Open Level...")) {
        const char *filters[] = {"*.campsurlevel", "*.json"};
        std::string defLev = "world.campsurlevel";
        if (project.has_value() && !project->levelsDir.empty())
          defLev = (fs::path(project->levelsDir) / "world.campsurlevel").string();
        const char *path = tinyfd_openFileDialog("Open Level", defLev.c_str(), 2,
                                                 filters, "Level (*.campsurlevel, *.json)", 0);
        if (path && strlen(path) > 0) {
          FILE *fp = fopen(path, "r");
          if (fp) {
            fclose(fp);
            LoadLevelFromAbsolutePath(fs::absolute(path).string());
          } else {
            printf("[Editor] ERROR: File does not exist or cannot be opened: '%s'\n", path);
          }
        }
      }
      if (ImGui::MenuItem("Save Level", "Ctrl+S")) {
        SaveCurrentLevel();
      }
      if (ImGui::MenuItem("Save Level As...")) {
        SaveCurrentLevelAs();
      }
      if (ImGui::MenuItem("Save Level (Bake zones first)...", "Ctrl+Shift+S")) {
        SaveCurrentLevelWithBake();
      }
      if (ImGui::BeginMenu("Import")) {
        if (ImGui::MenuItem("Tiled Map (.tmx)...")) {
          const char *tmxFilters[] = {"*.tmx"};
          const char *tmxPath = tinyfd_openFileDialog(
              "Import Tiled Map", "", 1, tmxFilters, "Tiled Map (.tmx)", 0);
          if (tmxPath && strlen(tmxPath) > 0) {
            if (criogenio::LoadTerrainFromTmxFile(GetWorld(), std::string(tmxPath))) {
              printf("[Editor] Loaded TMX terrain: %s\n", tmxPath);
              MarkLevelDirty();
            } else {
              printf("[Editor] ERROR: Failed to load TMX (see console): %s\n", tmxPath);
            }
          }
        }
        if (ImGui::MenuItem("Terrain from level file...")) {
          const char *filters[] = {"*.campsurlevel", "*.json"};
          const char *p =
              tinyfd_openFileDialog("Import terrain from level file", "world.campsurlevel", 2,
                                    filters, "Level Files (*.campsurlevel, *.json)", 0);
          if (p && strlen(p) > 0) {
            std::string err;
            if (criogenio::LoadWorldTerrainAndLevelFromFile(GetWorld(), std::string(p), err)) {
              printf("[Editor] Loaded terrain+level from: %s\n", p);
              MarkLevelDirty();
            } else {
              printf("[Editor] ERROR: %s\n", err.c_str());
            }
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Clear Scene")) {
        if (ImGui::MenuItem("2D (tilemap)")) {
          ApplyLevelTemplateReset(0);
          currentLevelPath.reset();
          MarkLevelDirty();
        }
        if (ImGui::MenuItem("2D (free-form)")) {
          ApplyLevelTemplateReset(1);
          currentLevelPath.reset();
          MarkLevelDirty();
        }
        if (ImGui::MenuItem("3D")) {
          ApplyLevelTemplateReset(2);
          currentLevelPath.reset();
          MarkLevelDirty();
        }
        ImGui::EndMenu();
      }
      ImGui::MenuItem("Snap to grid", nullptr, &snapToGrid);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
      bool is2D = sceneMode == SceneMode::Scene2D;
      bool is3D = sceneMode == SceneMode::Scene3D;
      if (ImGui::MenuItem("2D Scene Mode", nullptr, is2D)) {
        sceneMode = SceneMode::Scene2D;
      }
      if (ImGui::MenuItem("3D Scene Mode", nullptr, is3D)) {
        sceneMode = SceneMode::Scene3D;
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

        if (sceneMode == SceneMode::Scene3D) {
          GetWorld().AddComponent<criogenio::Transform3D>(id);
          GetWorld().AddComponent<criogenio::Box3D>(id);
        } else {
          GetWorld().AddComponent<criogenio::Transform>(id, 0.0f, 0.0f);
        }
        MarkLevelDirty();
      }
      if (ImGui::MenuItem("Sprite")) {
        // CreateSpriteEntity();
      }
      if (ImGui::MenuItem("Terrain")) {
        const char *path = DrawFileBrowserPopup();
        if (path && path[0] != '\0') {
          GetWorld().CreateTerrain2D("Terrain", path);
          MarkLevelDirty();
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
      if (ImGui::MenuItem("Migrate TMX Maps → JSON")) {
        BatchMigrateTmxMaps();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "For each shipped TMX in subterra_guild/assets/levels/, load it and\n"
            "save a .json scene file alongside it. Run once after the first checkout.\n"
            "Then update world_config.json 'init_map' to point to the .json file.");
      }
      if (ImGui::MenuItem("Import Subterra animation (.campsuranims)...")) {
        if (!selectedEntityId.has_value()) {
          printf("[Editor] Select an entity first (AnimatedSprite will be created if missing).\n");
        } else {
          const char *jsonF[] = {"*.campsuranims", "*.json"};
          const char *pJson = tinyfd_openFileDialog(
              "Import Subterra animation", "subterra_guild/data/animations/player.campsuranims", 2,
              jsonF, "Animation Files (*.campsuranims, *.json)", 0);
          if (pJson && std::strlen(pJson) > 0) {
            std::string err;
            criogenio::AssetId nid = criogenio::LoadSubterraGuildAnimationJson(
                std::string(pJson), nullptr, nullptr, &err);
            if (nid == criogenio::INVALID_ASSET_ID)
              printf("[Editor] Import failed: %s\n", err.c_str());
            else {
              auto &w = GetWorld();
              const int e = selectedEntityId.value();
              if (!w.HasComponent<criogenio::AnimatedSprite>(e))
                w.AddComponent<criogenio::AnimatedSprite>(e, nid);
              else {
                auto *sp = w.GetComponent<criogenio::AnimatedSprite>(e);
                if (sp->animationId != criogenio::INVALID_ASSET_ID)
                  criogenio::AnimationDatabase::instance().removeReference(sp->animationId);
                sp->animationId = nid;
                criogenio::AnimationDatabase::instance().addReference(nid);
              }
              if (!w.HasComponent<criogenio::AnimationState>(e))
                w.AddComponent<criogenio::AnimationState>(e);
              if (const auto *def = criogenio::AnimationDatabase::instance().getAnimation(nid)) {
                texturePathEdits[e] = def->texturePath;
                if (!def->clips.empty())
                  w.GetComponent<criogenio::AnimatedSprite>(e)->SetClip(def->clips[0].name);
              }
              printf("[Editor] Imported Subterra animation -> id %u (%s)\n",
                     static_cast<unsigned>(nid), pJson);
            }
          }
        }
      }
      if (ImGui::MenuItem("Export Subterra animation (.campsuranims)...")) {
        if (!selectedEntityId.has_value())
          printf("[Editor] Select an entity with AnimatedSprite.\n");
        else {
          auto *sp =
              GetWorld().GetComponent<criogenio::AnimatedSprite>(selectedEntityId.value());
          if (!sp || sp->animationId == criogenio::INVALID_ASSET_ID)
            printf("[Editor] No AnimatedSprite or no animation assigned.\n");
          else {
            const char *jsonF[] = {"*.campsuranims", "*.json"};
            const char *pOut = tinyfd_saveFileDialog("Export Subterra animation", "player.campsuranims",
                                                     2, jsonF, "Animation Files");
            if (pOut && std::strlen(pOut) > 0) {
              std::string err;
              if (!criogenio::SaveSubterraPlayerAnimationJson(sp->animationId, std::string(pOut),
                                                              &err))
                printf("[Editor] Export failed: %s\n", err.c_str());
              else
                printf("[Editor] Exported Subterra animation: %s\n", pOut);
            }
          }
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Reset Layout")) {
        dockLayoutBuilt = false;
        dockLayoutResetRequested = true;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Restore the default panel layout. Useful after panels have been\n"
            "added in a new build, or to recover from a misplaced docking.");
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  HandleTerrainEditInput();

  HandleTerrainShortcuts();
}

bool EditorApp::PaintCell(int layer, int tx, int ty, int gid) {
  auto *terrain = GetWorld().GetTerrain();
  if (!terrain) return false;
  if (layer < 0 || layer >= static_cast<int>(terrain->layers.size())) return false;

  if (lockedLayers.count(layer)) return false;

  const int prev = terrain->GetTile(layer, tx, ty);
  if (prev == gid)
    return false;
  terrain->SetTile(layer, tx, ty, gid);
  if (terrainHistory.IsRecording())
    terrainHistory.Record(tx, ty, prev, gid);
  return true;
}

int EditorApp::EffectivePaintTile() const {
  // Erase (-1) and tile-index mode pass through unchanged. Tile-index mode
  // (the legacy non-TMX path) doesn't carry flip bits.
  if (terrainSelectedTile < 0 || !terrainSelectedIsGid)
    return terrainSelectedTile;
  // The selected tile may already carry flip bits (from eyedroppering a
  // flipped tile). Normalize to the base gid before re-applying.
  const uint32_t base = criogenio::TileGid(terrainSelectedTile);
  if (base == 0)
    return 0;
  return criogenio::MakeFlippedTile(base, brushFlipH, brushFlipV, false);
}

// ---------------------------------------------------------------------------
// Terrain edit input dispatch — tool-aware.
//
// Strokes:
//   - Paint / Erase use a continuous stroke (Begin on press, End on release),
//     painting brush footprint on every cell entered.
//   - Rect / FilledRect / Line use a deferred stroke: anchor on press, preview
//     while dragging, commit on release as a single undoable edit.
//   - FloodFill commits one stroke on press.
//   - Eyedropper picks the gid under the cursor and switches to Paint.
// ---------------------------------------------------------------------------
void EditorApp::HandleTerrainEditInput() {
  if (!terrainEditMode || isPlaying) {
    if (terrainHistory.IsRecording()) terrainHistory.End();
    terrainDragging = false;
    return;
  }
  Vec2 mouseScreen = GetViewportMousePos();
  criogenio::Rect viewRect = {(float)viewportPos.x, (float)viewportPos.y,
                              (float)viewportSize.x, (float)viewportSize.y};
  const bool inViewport = criogenio::PointInRect(mouseScreen, viewRect);

  Terrain2D *terrain = GetWorld().GetTerrain();
  if (!terrain) return;

  Vec2 worldPos = ScreenToWorldPosition(mouseScreen);
  const float tsX = static_cast<float>(terrain->GridStepX());
  const float tsY = static_cast<float>(terrain->GridStepY());
  const int tx = static_cast<int>(std::floor((worldPos.x - terrain->origin.x) / tsX));
  const int ty = static_cast<int>(std::floor((worldPos.y - terrain->origin.y) / tsY));

  terrainDragCurTx = tx;
  terrainDragCurTy = ty;

  // Skip when mouse is over an ImGui panel that wants the click.
  if (ImGui::GetIO().WantCaptureMouse) {
    if (terrainHistory.IsRecording()) terrainHistory.End();
    terrainDragging = false;
    return;
  }

  const bool lmbPressed  = ImGui::IsMouseClicked(ImGuiMouseButton_Left)  && inViewport;
  const bool rmbPressed  = ImGui::IsMouseClicked(ImGuiMouseButton_Right) && inViewport;
  const bool lmbDown     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool rmbDown     = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool lmbReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  const bool rmbReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Right);

  auto stampBrushAt = [&](int cx, int cy, int gid) {
    const int hw = std::max(1, terrainBrushW);
    const int hh = std::max(1, terrainBrushH);
    const int x0 = cx - hw / 2;
    const int y0 = cy - hh / 2;
    for (int yy = 0; yy < hh; ++yy)
      for (int xx = 0; xx < hw; ++xx)
        PaintCell(terrainSelectedLayer, x0 + xx, y0 + yy, gid);
  };

  // Bresenham line on tile coords used by Paint drag interpolation and the
  // Line tool.
  auto stampLine = [&](int ax, int ay, int bx, int by, int gid) {
    int x0 = ax, y0 = ay, x1 = bx, y1 = by;
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      stampBrushAt(x0, y0, gid);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  };

  auto stampRectOutline = [&](int x0, int y0, int x1, int y1, int gid) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    for (int x = x0; x <= x1; ++x) { PaintCell(terrainSelectedLayer, x, y0, gid); PaintCell(terrainSelectedLayer, x, y1, gid); }
    for (int y = y0; y <= y1; ++y) { PaintCell(terrainSelectedLayer, x0, y, gid); PaintCell(terrainSelectedLayer, x1, y, gid); }
  };
  auto stampRectFilled = [&](int x0, int y0, int x1, int y1, int gid) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        PaintCell(terrainSelectedLayer, x, y, gid);
  };

  // Bounded BFS flood fill on the active layer matching the prev gid.
  auto floodFill = [&](int sx, int sy, int gid) {
    const int target = terrain->GetTile(terrainSelectedLayer, sx, sy);
    if (target == gid) return;
    static constexpr size_t kCap = 100000;
    std::vector<std::pair<int,int>> stack;
    stack.reserve(256);
    stack.push_back({sx, sy});
    std::unordered_set<long long> visited;
    visited.reserve(1024);
    auto key = [](int a, int b){ return (static_cast<long long>(a) << 32) ^ static_cast<unsigned int>(b); };
    while (!stack.empty() && visited.size() < kCap) {
      auto [x, y] = stack.back(); stack.pop_back();
      if (!visited.insert(key(x, y)).second) continue;
      if (terrain->GetTile(terrainSelectedLayer, x, y) != target) continue;
      PaintCell(terrainSelectedLayer, x, y, gid);
      stack.push_back({x + 1, y});
      stack.push_back({x - 1, y});
      stack.push_back({x, y + 1});
      stack.push_back({x, y - 1});
    }
  };

  switch (terrainTool) {
    case TerrainTool::Paint: {
      // Continuous freehand. RMB always erases.
      const int gid = -1;
      (void)gid;
      const int paintTile = EffectivePaintTile();
      if (lmbPressed) {
        terrainHistory.Begin("Paint", terrainSelectedLayer);
        stampBrushAt(tx, ty, paintTile);
        terrainDragging = true;
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      } else if (rmbPressed) {
        terrainHistory.Begin("Erase", terrainSelectedLayer);
        stampBrushAt(tx, ty, -1);
        terrainDragging = true;
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      } else if ((lmbDown || rmbDown) && terrainHistory.IsRecording()) {
        // Interpolate from last cell to current to avoid gaps on fast drags.
        const int paintGid = lmbDown ? paintTile : -1;
        stampLine(terrainDragAnchorTx, terrainDragAnchorTy, tx, ty, paintGid);
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      }
      if (lmbReleased || rmbReleased) {
        if (terrainHistory.IsRecording()) terrainHistory.End();
        terrainDragging = false;
        MarkLevelDirty();
      }
      break;
    }
    case TerrainTool::Rect:
    case TerrainTool::FilledRect: {
      if (lmbPressed) {
        terrainDragging = true;
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      } else if (lmbReleased && terrainDragging) {
        terrainHistory.Begin(terrainTool == TerrainTool::Rect ? "Rect" : "Filled Rect",
                             terrainSelectedLayer);
        const int paintTile = EffectivePaintTile();
        if (terrainTool == TerrainTool::Rect)
          stampRectOutline(terrainDragAnchorTx, terrainDragAnchorTy, tx, ty, paintTile);
        else
          stampRectFilled(terrainDragAnchorTx, terrainDragAnchorTy, tx, ty, paintTile);
        terrainHistory.End();
        terrainDragging = false;
        MarkLevelDirty();
      } else if (rmbPressed) {
        // Right-click erases a rect (tap = single cell).
        terrainHistory.Begin("Erase", terrainSelectedLayer);
        stampBrushAt(tx, ty, -1);
        terrainHistory.End();
        MarkLevelDirty();
      }
      break;
    }
    case TerrainTool::Line: {
      if (lmbPressed) {
        terrainDragging = true;
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      } else if (lmbReleased && terrainDragging) {
        terrainHistory.Begin("Line", terrainSelectedLayer);
        stampLine(terrainDragAnchorTx, terrainDragAnchorTy, tx, ty, EffectivePaintTile());
        terrainHistory.End();
        terrainDragging = false;
        MarkLevelDirty();
      }
      break;
    }
    case TerrainTool::FloodFill: {
      if (lmbPressed) {
        terrainHistory.Begin("Flood Fill", terrainSelectedLayer);
        floodFill(tx, ty, EffectivePaintTile());
        terrainHistory.End();
        MarkLevelDirty();
      } else if (rmbPressed) {
        terrainHistory.Begin("Flood Erase", terrainSelectedLayer);
        floodFill(tx, ty, -1);
        terrainHistory.End();
        MarkLevelDirty();
      }
      break;
    }
    case TerrainTool::Eyedropper: {
      if (lmbPressed) {
        const int picked = terrain->GetTile(terrainSelectedLayer, tx, ty);
        // Picked may be negative (H-flip bit set); accept anything except
        // empty (cell value 0). For tile-index mode, picked >= 0 still holds.
        const bool empty = terrain->UsesGidMode()
                               ? (criogenio::TileGid(picked) == 0)
                               : (picked < 0);
        if (!empty) {
          terrainSelectedTile = picked;
          terrainSelectedIsGid = terrain->UsesGidMode();
          if (terrainSelectedIsGid) {
            // Decompose the picked tile's flip bits into the brush flags so
            // the user can toggle them off / paint a non-flipped variant.
            brushFlipH = criogenio::TileFlipH(picked);
            brushFlipV = criogenio::TileFlipV(picked);
          }
          terrainTool = TerrainTool::Paint;
        }
      }
      break;
    }
    case TerrainTool::Select: {
      // Phase 5 implements selection drag.
      if (lmbPressed) {
        terrainDragging = true;
        terrainDragAnchorTx = tx; terrainDragAnchorTy = ty;
      } else if (lmbReleased && terrainDragging) {
        TileRegion r{
            std::min(terrainDragAnchorTx, tx),
            std::min(terrainDragAnchorTy, ty),
            std::max(terrainDragAnchorTx, tx),
            std::max(terrainDragAnchorTy, ty),
            terrainSelectedLayer};
        tileSelection = r;
        terrainDragging = false;
      } else if (rmbPressed) {
        tileSelection.reset();
      }
      break;
    }
    case TerrainTool::Stamp: {
      // Phase 5 implements stamp commit.
      if (lmbPressed && tileClipboard) {
        terrainHistory.Begin("Stamp", terrainSelectedLayer);
        const auto &p = *tileClipboard;
        for (int y = 0; y < p.h; ++y)
          for (int x = 0; x < p.w; ++x) {
            const int gid = p.tiles[y * p.w + x];
            if (gid != -1) // -1 in pattern = transparent
              PaintCell(terrainSelectedLayer, tx + x, ty + y, gid);
          }
        terrainHistory.End();
        MarkLevelDirty();
      }
      break;
    }
  }
}

void EditorApp::HandleTerrainShortcuts() {
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureKeyboard) return;

  Terrain2D *terrain = GetWorld().GetTerrain();

  // Undo / Redo: Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z.
  if (terrain && io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_Z, /*repeat*/ true)) {
      if (io.KeyShift) terrainHistory.Redo(*terrain);
      else             terrainHistory.Undo(*terrain);
      MarkLevelDirty();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, /*repeat*/ true)) {
      terrainHistory.Redo(*terrain);
      MarkLevelDirty();
    }
  }

  // Clipboard shortcuts (Phase 5).
  if (terrain && io.KeyCtrl && tileSelection) {
    if (ImGui::IsKeyPressed(ImGuiKey_C)) CopyTileSelection();
    else if (ImGui::IsKeyPressed(ImGuiKey_X)) CutTileSelection();
  }
  if (terrain && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && tileClipboard) {
    terrainTool = TerrainTool::Stamp;
  }
  if (terrain && tileSelection && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    DeleteTileSelection();
  }

  // Tool hotkeys — only when in terrain edit mode and no modifier held.
  if (!terrainEditMode || io.KeyCtrl || io.KeyAlt || io.KeySuper) return;
  if (ImGui::IsKeyPressed(ImGuiKey_B))      terrainTool = TerrainTool::Paint;
  else if (ImGui::IsKeyPressed(ImGuiKey_R)) terrainTool = io.KeyShift ? TerrainTool::FilledRect : TerrainTool::Rect;
  else if (ImGui::IsKeyPressed(ImGuiKey_L)) terrainTool = TerrainTool::Line;
  else if (ImGui::IsKeyPressed(ImGuiKey_G)) terrainTool = TerrainTool::FloodFill;
  else if (ImGui::IsKeyPressed(ImGuiKey_I)) terrainTool = TerrainTool::Eyedropper;
  else if (ImGui::IsKeyPressed(ImGuiKey_S)) terrainTool = TerrainTool::Select;
  else if (ImGui::IsKeyPressed(ImGuiKey_F)) terrainTool = TerrainTool::FloodFill;
  // Brush flip toggles. X / Y match Tiled's flip-H / flip-V hotkeys; both
  // affect the next paint so the user can mirror tiles as they go.
  else if (ImGui::IsKeyPressed(ImGuiKey_X)) brushFlipH = !brushFlipH;
  else if (ImGui::IsKeyPressed(ImGuiKey_Y)) brushFlipV = !brushFlipV;

  if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))  terrainBrushW = std::max(1, terrainBrushW - 1), terrainBrushH = std::max(1, terrainBrushH - 1);
  if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) ++terrainBrushW, ++terrainBrushH;
}

// Snap a world-space position to the terrain grid when snapToGrid is on and a
// terrain is present. Returns the position unchanged in free-form mode.
static criogenio::Vec2 MaybeSnapToGrid(criogenio::Vec2 worldPos,
                                       const criogenio::Terrain2D *terrain,
                                       bool snap) {
  if (!snap || !terrain) return worldPos;
  const float gx = static_cast<float>(terrain->GridStepX());
  const float gy = static_cast<float>(terrain->GridStepY());
  if (gx <= 0 || gy <= 0) return worldPos;
  worldPos.x = std::floor((worldPos.x - terrain->origin.x) / gx) * gx + terrain->origin.x;
  worldPos.y = std::floor((worldPos.y - terrain->origin.y) / gy) * gy + terrain->origin.y;
  return worldPos;
}

void EditorApp::AssignToCurrentObjectLayer(int entityId) {
  if (entityId < 0) return;
  auto &world = GetWorld();
  auto &lm = world.AddComponent<criogenio::LayerMembership>(entityId);
  lm.layerName = currentObjectLayer;
}

std::string EditorApp::GetEntityLayerName(int entityId) const {
  if (entityId < 0) return kDefaultObjectLayer;
  if (auto *lm = const_cast<EditorApp*>(this)
                     ->GetWorld()
                     .GetComponent<criogenio::LayerMembership>(entityId)) {
    return lm->layerName.empty() ? std::string(kDefaultObjectLayer) : lm->layerName;
  }
  return kDefaultObjectLayer;
}

void EditorApp::SyncObjectLayerState() {
  auto &world = GetWorld();
  // Discover any layer names introduced by deserialization or scripted spawns
  // that aren't yet in the editor's catalog. Always keep "Default" present.
  bool hasDefault = false;
  for (const auto &name : objectLayers)
    if (name == kDefaultObjectLayer) { hasDefault = true; break; }
  if (!hasDefault)
    objectLayers.insert(objectLayers.begin(), kDefaultObjectLayer);
  std::unordered_set<std::string> known(objectLayers.begin(), objectLayers.end());
  for (int id : world.GetEntitiesWith<criogenio::LayerMembership>()) {
    auto *lm = world.GetComponent<criogenio::LayerMembership>(id);
    if (!lm) continue;
    if (lm->layerName.empty()) lm->layerName = kDefaultObjectLayer;
    if (known.insert(lm->layerName).second)
      objectLayers.push_back(lm->layerName);
  }
  // Mirror hidden state into EditorHidden tags. We add/remove tags lazily so
  // entities that should be visible never carry a stale Hidden tag.
  for (int id : world.GetAllEntities()) {
    const std::string ln = GetEntityLayerName(id);
    const bool shouldHide = hiddenObjectLayers.count(ln) > 0;
    const bool isHidden = world.GetComponent<criogenio::EditorHidden>(id) != nullptr;
    if (shouldHide && !isHidden) {
      world.AddComponent<criogenio::EditorHidden>(id);
    } else if (!shouldHide && isHidden) {
      world.RemoveComponent<criogenio::EditorHidden>(id);
    }
  }
}

void EditorApp::CreateEmptyEntityAtMouse() {
  Vec2 worldPos = MaybeSnapToGrid(GetMouseWorld(), GetWorld().GetTerrain(), snapToGrid);
  int id = GetWorld().CreateEntity("New Entity");
  GetWorld().AddComponent<criogenio::Name>(id,
                                           "New Entity " + std::to_string(id));
  if (sceneMode == SceneMode::Scene3D) {
    auto& t = GetWorld().AddComponent<criogenio::Transform3D>(id);
    t.x = worldPos.x;
    t.z = worldPos.y;
    GetWorld().AddComponent<criogenio::Box3D>(id);
  } else {
    GetWorld().AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
  }
  AssignToCurrentObjectLayer(id);
  MarkLevelDirty();
}

void EditorApp::CreateMapEventZoneAtMouse() {
  Vec2 worldPos = MaybeSnapToGrid(GetMouseWorld(), GetWorld().GetTerrain(), snapToGrid);
  int id = GetWorld().CreateEntity("MapEvent Zone");
  GetWorld().AddComponent<criogenio::Name>(id, "MapEvent " + std::to_string(id));
  GetWorld().AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
  auto &z = GetWorld().AddComponent<criogenio::MapEventZone2D>(id);
  z.width = 64.f;
  z.height = 64.f;
  z.activated = true;
  AssignToCurrentObjectLayer(id);
  selectedEntityId = id;
  MarkLevelDirty();
}

void EditorApp::CreateInteractableZoneAtMouse() {
  Vec2 worldPos = MaybeSnapToGrid(GetMouseWorld(), GetWorld().GetTerrain(), snapToGrid);
  int id = GetWorld().CreateEntity("Interactable Zone");
  GetWorld().AddComponent<criogenio::Name>(id, "Interactable " + std::to_string(id));
  GetWorld().AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
  auto &z = GetWorld().AddComponent<criogenio::InteractableZone2D>(id);
  z.width = 64.f;
  z.height = 64.f;
  AssignToCurrentObjectLayer(id);
  selectedEntityId = id;
  MarkLevelDirty();
}

void EditorApp::SpawnSpriteEntityAt(Vec2 worldPos, const std::string &texturePath) {
  worldPos = MaybeSnapToGrid(worldPos, GetWorld().GetTerrain(), snapToGrid);
  auto &world = GetWorld();
  const std::string baseName = std::filesystem::path(texturePath).stem().string();
  const int id = world.CreateEntity(baseName);
  world.AddComponent<criogenio::Name>(id, baseName);
  world.AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
  auto &sprite = world.AddComponent<criogenio::Sprite>(id);
  sprite.atlas = criogenio::AssetManager::instance()
                     .load<criogenio::TextureResource>(texturePath);
  sprite.atlasPath = texturePath;
  // The dropped image is treated as the full sprite (not an atlas region):
  // src rect starts at (0, 0) and the size fits inside the texture. Using
  // min(w, h) guarantees the source rect stays within bounds for non-square
  // textures (the renderer assumes a square src rect via spriteSize).
  sprite.spriteX = 0;
  sprite.spriteY = 0;
  if (sprite.atlas && sprite.atlas->texture.width > 0 &&
      sprite.atlas->texture.height > 0) {
    sprite.spriteSize = std::min(sprite.atlas->texture.width,
                                 sprite.atlas->texture.height);
  } else {
    sprite.spriteSize = 32;
  }
  AssignToCurrentObjectLayer(id);
  selectedEntityId = id;
  printf("[Editor] Dropped sprite %s at (%.1f, %.1f) -> entity %d\n",
         texturePath.c_str(), worldPos.x, worldPos.y, id);
  MarkLevelDirty();
}

void EditorApp::CreateSpawnPrefabAtMouse() {
  Vec2 worldPos = MaybeSnapToGrid(GetMouseWorld(), GetWorld().GetTerrain(), snapToGrid);
  int id = GetWorld().CreateEntity("Spawn Prefab");
  GetWorld().AddComponent<criogenio::Name>(id, "Spawn " + std::to_string(id));
  GetWorld().AddComponent<criogenio::Transform>(id, worldPos.x, worldPos.y);
  GetWorld().AddComponent<criogenio::WorldSpawnPrefab2D>(id);
  AssignToCurrentObjectLayer(id);
  selectedEntityId = id;
  MarkLevelDirty();
}

void EditorApp::DrawTerrainLayersPanel() {
  if (!ImGui::Begin("Layers")) {
    ImGui::End();
    return;
  }
  Terrain2D *terrain = GetWorld().GetTerrain();

  // -------------------- Tile layers (only when a terrain exists) --------
  if (terrain) {
    if (ImGui::CollapsingHeader("Tile Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
      const int n = static_cast<int>(terrain->layers.size());

      // Helpers to keep editor-side `lockedLayers` (indexed set) in sync with
      // engine-side layer ops; without these, Move/Delete shifts what's locked.
      auto remapDelete = [&](int idx) {
        std::unordered_set<int> next;
        for (int v : lockedLayers) {
          if (v == idx) continue;
          next.insert(v > idx ? v - 1 : v);
        }
        lockedLayers.swap(next);
      };
      auto remapInsert = [&](int idx, bool lockNew) {
        std::unordered_set<int> next;
        for (int v : lockedLayers) next.insert(v >= idx ? v + 1 : v);
        if (lockNew) next.insert(idx);
        lockedLayers.swap(next);
      };
      auto remapMove = [&](int from, int to) {
        bool wasLocked = lockedLayers.count(from) > 0;
        remapDelete(from);
        int adjTo = (to > from) ? to - 1 : to; // Insert position after the erase.
        remapInsert(adjTo, wasLocked);
      };

      if (ImGui::Button("+ Add"))      { terrain->AddLayer(); terrainSelectedLayer = static_cast<int>(terrain->layers.size()) - 1; terrainHistory.Clear(); }
      ImGui::SameLine();
      bool canDel = (n > 0);
      if (!canDel) ImGui::BeginDisabled();
      if (ImGui::Button("- Delete"))   {
        int idx = terrainSelectedLayer;
        terrain->RemoveLayer(idx);
        remapDelete(idx);
        int newN = static_cast<int>(terrain->layers.size());
        terrainSelectedLayer = std::min(idx, std::max(0, newN - 1));
        terrainHistory.Clear();
      }
      if (!canDel) ImGui::EndDisabled();
      ImGui::SameLine();
      bool canUp = (terrainSelectedLayer > 0);
      if (!canUp) ImGui::BeginDisabled();
      if (ImGui::Button("Up"))         {
        int from = terrainSelectedLayer, to = from - 1;
        terrain->MoveLayer(from, to);
        remapMove(from, to);
        --terrainSelectedLayer;
        terrainHistory.Clear();
      }
      if (!canUp) ImGui::EndDisabled();
      ImGui::SameLine();
      bool canDn = (terrainSelectedLayer < n - 1);
      if (!canDn) ImGui::BeginDisabled();
      if (ImGui::Button("Down"))       {
        int from = terrainSelectedLayer, to = from + 1;
        terrain->MoveLayer(from, to);
        remapMove(from, to);
        ++terrainSelectedLayer;
        terrainHistory.Clear();
      }
      if (!canDn) ImGui::EndDisabled();
      ImGui::SameLine();
      if (!canDel) ImGui::BeginDisabled();
      if (ImGui::Button("Duplicate"))  {
        int from = terrainSelectedLayer;
        terrain->DuplicateLayer(from);
        bool wasLocked = lockedLayers.count(from) > 0;
        remapInsert(from + 1, wasLocked);
        ++terrainSelectedLayer;
        terrainHistory.Clear();
      }
      if (!canDel) ImGui::EndDisabled();
      ImGui::Separator();

      while (terrain->tmxMeta.layerInfo.size() < terrain->layers.size()) {
        TiledLayerMeta lm;
        lm.name = "Layer " + std::to_string(terrain->tmxMeta.layerInfo.size());
        terrain->tmxMeta.layerInfo.push_back(std::move(lm));
      }

      for (int i = 0; i < n; ++i) {
        ImGui::PushID(i);
        TiledLayerMeta &lm = terrain->tmxMeta.layerInfo[i];

        bool selected = (terrainSelectedLayer == i);
        if (ImGui::RadioButton("##sel", selected)) terrainSelectedLayer = i;
        ImGui::SameLine();

        if (ImGui::Checkbox("##vis", &lm.visible)) {}
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
        ImGui::SameLine();

        bool locked = lockedLayers.count(i) > 0;
        if (ImGui::Checkbox("##lock", &locked)) {
          if (locked) lockedLayers.insert(i);
          else        lockedLayers.erase(i);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locked (rejects paint)");
        ImGui::SameLine();

        char nameBuf[128]{};
        std::strncpy(nameBuf, lm.name.c_str(), sizeof(nameBuf) - 1);
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
          lm.name = nameBuf;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderFloat("##op", &lm.opacity, 0.f, 1.f, "a%.2f");

        ImGui::PopID();
      }
    }
  }

  // -------------------- Object layers (always available) ---------------
  if (ImGui::CollapsingHeader("Object Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("+ Add Object Layer")) {
      // Pick a unique name like "Group 1", "Group 2", ...
      std::string base = "Group ";
      for (int n = 1;; ++n) {
        std::string candidate = base + std::to_string(n);
        bool taken = false;
        for (const auto &nm : objectLayers) if (nm == candidate) { taken = true; break; }
        if (!taken) { objectLayers.push_back(candidate); currentObjectLayer = candidate; break; }
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(active = newly placed entities land here)");
    ImGui::Separator();

    for (size_t i = 0; i < objectLayers.size(); ++i) {
      const std::string name = objectLayers[i];
      ImGui::PushID(static_cast<int>(i));

      bool isActive = (name == currentObjectLayer);
      if (ImGui::RadioButton("##active", isActive)) currentObjectLayer = name;
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active layer for new entities");
      ImGui::SameLine();

      bool visible = hiddenObjectLayers.count(name) == 0;
      if (ImGui::Checkbox("##vis", &visible)) {
        if (visible) hiddenObjectLayers.erase(name);
        else         hiddenObjectLayers.insert(name);
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible (hide all entities on this layer)");
      ImGui::SameLine();

      bool locked = lockedObjectLayers.count(name) > 0;
      if (ImGui::Checkbox("##lock", &locked)) {
        if (locked) lockedObjectLayers.insert(name);
        else        lockedObjectLayers.erase(name);
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locked (entities can't be selected)");
      ImGui::SameLine();

      // Inline rename. The Default layer is read-only to preserve a stable fallback.
      const bool isDefault = (name == kDefaultObjectLayer);
      char nameBuf[128]{};
      std::strncpy(nameBuf, name.c_str(), sizeof(nameBuf) - 1);
      ImGui::SetNextItemWidth(160);
      if (isDefault) ImGui::BeginDisabled();
      if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newName = nameBuf;
        if (!newName.empty() && newName != name) {
          // Reflect the rename through every entity that pointed to the old name.
          auto &world = GetWorld();
          for (int eid : world.GetEntitiesWith<criogenio::LayerMembership>()) {
            auto *lm = world.GetComponent<criogenio::LayerMembership>(eid);
            if (lm && lm->layerName == name) lm->layerName = newName;
          }
          if (currentObjectLayer == name) currentObjectLayer = newName;
          if (hiddenObjectLayers.erase(name)) hiddenObjectLayers.insert(newName);
          if (lockedObjectLayers.erase(name)) lockedObjectLayers.insert(newName);
          objectLayers[i] = newName;
        }
      }
      if (isDefault) ImGui::EndDisabled();

      ImGui::SameLine();
      if (isDefault) ImGui::BeginDisabled();
      if (ImGui::SmallButton("X")) {
        // Move every entity off this layer back to the Default layer, then
        // forget the name. We don't delete entities; that's a destructive
        // op users should do explicitly via the hierarchy.
        auto &world = GetWorld();
        for (int eid : world.GetEntitiesWith<criogenio::LayerMembership>()) {
          auto *lm = world.GetComponent<criogenio::LayerMembership>(eid);
          if (lm && lm->layerName == name) lm->layerName = kDefaultObjectLayer;
        }
        if (currentObjectLayer == name) currentObjectLayer = kDefaultObjectLayer;
        hiddenObjectLayers.erase(name);
        lockedObjectLayers.erase(name);
        objectLayers.erase(objectLayers.begin() + i);
        ImGui::PopID();
        break; // index invalidated
      }
      if (ImGui::IsItemHovered() && !isDefault)
        ImGui::SetTooltip("Delete layer (entities move to Default)");
      if (isDefault) ImGui::EndDisabled();

      ImGui::PopID();
    }
  }

  ImGui::End();
}

void EditorApp::CopyTileSelection() {
  if (!tileSelection) return;
  auto *terrain = GetWorld().GetTerrain();
  if (!terrain) return;
  const auto &s = *tileSelection;
  TilePattern p;
  p.w = s.x1 - s.x0 + 1;
  p.h = s.y1 - s.y0 + 1;
  if (p.w <= 0 || p.h <= 0) return;
  p.tiles.resize(static_cast<size_t>(p.w * p.h));
  for (int y = 0; y < p.h; ++y)
    for (int x = 0; x < p.w; ++x)
      p.tiles[y * p.w + x] = terrain->GetTile(s.layer, s.x0 + x, s.y0 + y);
  tileClipboard = std::move(p);
  printf("[Editor] Copied %dx%d tiles\n", tileClipboard->w, tileClipboard->h);
}

void EditorApp::CutTileSelection() {
  if (!tileSelection) return;
  auto *terrain = GetWorld().GetTerrain();
  if (!terrain) return;
  CopyTileSelection();
  const auto &s = *tileSelection;
  terrainHistory.Begin("Cut", s.layer);
  for (int y = s.y0; y <= s.y1; ++y)
    for (int x = s.x0; x <= s.x1; ++x)
      PaintCell(s.layer, x, y, -1);
  terrainHistory.End();
  MarkLevelDirty();
}

void EditorApp::DeleteTileSelection() {
  if (!tileSelection) return;
  auto *terrain = GetWorld().GetTerrain();
  if (!terrain) return;
  const auto &s = *tileSelection;
  terrainHistory.Begin("Delete Selection", s.layer);
  for (int y = s.y0; y <= s.y1; ++y)
    for (int x = s.x0; x <= s.x1; ++x)
      PaintCell(s.layer, x, y, -1);
  terrainHistory.End();
  MarkLevelDirty();
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
      MarkLevelDirty();
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

  // ----- Tool toolbar -----
  ImGui::Spacing();
  auto toolButton = [&](const char *label, TerrainTool t, const char *tip) {
    bool sel = (terrainTool == t);
    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button(label)) terrainTool = t;
    if (sel) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::SameLine();
  };
  toolButton("Paint  [B]",     TerrainTool::Paint,      "Freehand brush. LMB paint, RMB erase.");
  toolButton("Rect  [R]",      TerrainTool::Rect,       "Outline rectangle.");
  toolButton("FRect  [Sh+R]",  TerrainTool::FilledRect, "Filled rectangle.");
  toolButton("Line  [L]",      TerrainTool::Line,       "Line between anchor and release.");
  toolButton("Fill  [G/F]",    TerrainTool::FloodFill,  "Flood fill connected matching tiles.");
  toolButton("Pick  [I]",      TerrainTool::Eyedropper, "Pick tile id from cell.");
  toolButton("Select [S]",     TerrainTool::Select,     "Drag region; Ctrl+C/X/V to clipboard.");
  toolButton("Stamp",          TerrainTool::Stamp,      "Stamp the clipboard pattern.");
  ImGui::NewLine();
  ImGui::SetNextItemWidth(80);
  ImGui::DragInt("Brush W", &terrainBrushW, 0.2f, 1, 64);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::DragInt("Brush H", &terrainBrushH, 0.2f, 1, 64);
  if (terrainBrushW < 1) terrainBrushW = 1;
  if (terrainBrushH < 1) terrainBrushH = 1;
  ImGui::SameLine();
  ImGui::TextDisabled("([ / ] to shrink/grow)");

  // Per-tile flip toggles. Only meaningful in TMX/gid mode (the legacy
  // single-atlas tile-index mode doesn't carry flip bits).
  if (terrain->UsesGidMode()) {
    ImGui::Checkbox("Flip H [X]", &brushFlipH);
    ImGui::SameLine();
    ImGui::Checkbox("Flip V [Y]", &brushFlipV);
    ImGui::SameLine();
    ImGui::TextDisabled("(applies to next paint)");
  }

  // Undo/redo buttons.
  ImGui::Spacing();
  if (!terrainHistory.CanUndo()) ImGui::BeginDisabled();
  if (ImGui::Button("Undo (Ctrl+Z)")) {
    terrainHistory.Undo(*terrain);
    MarkLevelDirty();
  }
  if (!terrainHistory.CanUndo()) ImGui::EndDisabled();
  ImGui::SameLine();
  if (!terrainHistory.CanRedo()) ImGui::BeginDisabled();
  if (ImGui::Button("Redo (Ctrl+Y)")) {
    terrainHistory.Redo(*terrain);
    MarkLevelDirty();
  }
  if (!terrainHistory.CanRedo()) ImGui::EndDisabled();
  terrainSelectedLayer = std::max(
      0, std::min(terrainSelectedLayer,
                  static_cast<int>(terrain->layers.size()) - 1));
  ImGui::SameLine();
  if (ImGui::Button("Add Layer")) {
    terrain->AddLayer();
    terrainSelectedLayer = static_cast<int>(terrain->layers.size()) - 1;
    MarkLevelDirty();
  }

  ImGui::Separator();
  ImGui::Text("Chunk size: %d x %d (infinite terrain)", terrain->GetChunkSize(),
              terrain->GetChunkSize());
  if (terrain->UsesGidMode()) {
    ImGui::Text("Selected Tile: gid=%u%s%s", criogenio::TileGid(terrainSelectedTile),
                brushFlipH ? " H" : "", brushFlipV ? " V" : "");
    // Surface per-tile custom properties from the source tileset (Tiled
    // <tile id="N"><properties>...</properties></tile>). Read-only; writing
    // back to the .tsx is out of scope.
    const uint32_t baseGid = criogenio::TileGid(terrainSelectedTile);
    if (baseGid > 0) {
      const criogenio::TmxTilesetEntry *picked = nullptr;
      int localId = -1;
      for (int i = static_cast<int>(terrain->tmxTilesets.size()) - 1; i >= 0; --i) {
        const auto &ts = terrain->tmxTilesets[i];
        if (static_cast<int>(baseGid) >= ts.firstGid) {
          picked = &ts;
          localId = static_cast<int>(baseGid) - ts.firstGid;
          break;
        }
      }
      if (picked) {
        auto it = picked->tileProperties.find(localId);
        if (it != picked->tileProperties.end() && !it->second.empty()) {
          ImGui::Indent();
          ImGui::TextDisabled("Tile properties:");
          if (ImGui::BeginTable("##tileprops", 2,
                                 ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
            for (const auto &p : it->second) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::TextUnformatted(p.name.c_str());
              ImGui::TableNextColumn();
              ImGui::TextUnformatted(p.value.c_str());
            }
            ImGui::EndTable();
          }
          ImGui::Unindent();
        }
      }
    }
  } else {
    ImGui::Text("Selected Tile: %d", terrainSelectedTile);
  }
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

  // ----- Tileset palette -----
  // Helper: render one tileset as a selectable grid. Returns the picked id
  // (firstGid + localIndex when gidMode==true, otherwise localIndex), or -2
  // when nothing was clicked.
  auto drawTilesetGrid = [&](const criogenio::Tileset &sheet, int firstGid,
                              bool gidMode) -> int {
    if (!sheet.atlas) {
      ImGui::TextDisabled("Atlas not loaded");
      return -2;
    }
    const int tileW  = sheet.tileSize;
    const int tileH  = sheet.tileHeightPx > 0 ? sheet.tileHeightPx : sheet.tileSize;
    const int atlasW = sheet.atlas->texture.width;
    const int atlasH = sheet.atlas->texture.height;
    const int margin = sheet.margin;
    const int spacing = sheet.spacing;
    const int cols = sheet.columns > 0 ? sheet.columns : 1;
    const int rows = sheet.rows    > 0 ? sheet.rows    : 1;
    int picked = -2;
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < cols; ++x) {
        const int local = y * cols + x;
        const int id = gidMode ? (firstGid + local) : local;
        const float sx = static_cast<float>(margin + x * (tileW + spacing));
        const float sy = static_cast<float>(margin + y * (tileH + spacing));
        const ImVec2 uv0(sx / atlasW, sy / atlasH);
        const ImVec2 uv1((sx + tileW) / atlasW, (sy + tileH) / atlasH);
        ImGui::PushID(id);
        const bool selected =
            (terrainSelectedTile == id) &&
            (terrainSelectedIsGid == gidMode);
        if (selected)
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
        if (ImGui::ImageButton("",
                               (ImTextureID)(intptr_t)sheet.atlas->texture.opaque,
                               ImVec2(32, 32), uv0, uv1)) {
          terrainSelectedTile = id;
          terrainSelectedIsGid = gidMode;
          picked = id;
        }
        if (selected) ImGui::PopStyleColor();
        if ((x + 1) % cols != 0) ImGui::SameLine();
        ImGui::PopID();
      }
      ImGui::NewLine();
    }
    return picked;
  };

  if (terrain->UsesGidMode() && !terrain->tmxTilesets.empty()) {
    if (ImGui::BeginTabBar("##tilesets")) {
      for (size_t i = 0; i < terrain->tmxTilesets.size(); ++i) {
        const auto &ts = terrain->tmxTilesets[i];
        std::string label = std::filesystem::path(ts.sheet.tilesetPath).stem().string();
        if (label.empty()) label = "tileset " + std::to_string(i);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::BeginTabItem(label.c_str())) {
          ImGui::TextDisabled("firstGid=%d  %dx%dpx", ts.firstGid,
                              ts.tilePixelW, ts.tilePixelH);
          drawTilesetGrid(ts.sheet, ts.firstGid, /*gidMode=*/true);
          ImGui::EndTabItem();
        }
        ImGui::PopID();
      }
      ImGui::EndTabBar();
    }
  } else {
    auto tex = terrain->tileset.atlas;
    if (tex) {
      ImGui::Text("Tileset: %s", terrain->tileset.tilesetPath.c_str());
      ImGui::SameLine();
      if (ImGui::Button("Change Texture")) {
        const char *path = DrawFileBrowserPopup();
        if (path && path[0]) GetWorld().GetTerrain()->SetAtlas(0, path);
      }
      ImGui::DragInt("Tile size", &terrain->tileset.tileSize, 1, 1, 256);
      ImGui::Separator();
      drawTilesetGrid(terrain->tileset, /*firstGid=*/0, /*gidMode=*/false);
    } else {
      ImGui::TextDisabled("Tileset atlas not loaded");
    }
  }

  // Add Tileset button (appends to tmxTilesets, switching to GID mode if needed).
  if (ImGui::Button("Add Tileset...")) {
    const char *path = DrawFileBrowserPopup();
    if (path && path[0]) {
      // Build a new TmxTilesetEntry. firstGid = max(existing firstGid + count) + 1.
      criogenio::TmxTilesetEntry e;
      e.firstGid = 1;
      for (const auto &t : terrain->tmxTilesets) {
        const int last = t.firstGid + t.sheet.columns * t.sheet.rows;
        if (last > e.firstGid) e.firstGid = last;
      }
      e.tilePixelW = terrain->GridStepX();
      e.tilePixelH = terrain->GridStepY();
      e.sheet.tilesetPath = path;
      e.sheet.tileSize    = e.tilePixelW;
      e.sheet.tileHeightPx = e.tilePixelH;
      e.sheet.atlas = criogenio::AssetManager::instance()
                          .load<criogenio::TextureResource>(path);
      if (e.sheet.atlas && e.sheet.atlas->texture.width > 0) {
        e.sheet.columns = std::max(1, e.sheet.atlas->texture.width  / e.tilePixelW);
        e.sheet.rows    = std::max(1, e.sheet.atlas->texture.height / e.tilePixelH);
        terrain->tmxTilesets.push_back(std::move(e));
        printf("[Editor] Added tileset: %s (firstGid=%d)\n", path,
               terrain->tmxTilesets.back().firstGid);
      } else {
        printf("[Editor] Failed to load tileset texture: %s\n", path);
      }
    }
  }

  ImGui::Separator();
  if (ImGui::Button("Delete Terrain", ImVec2(-1, 0))) {
    GetWorld().DeleteTerrain();
  }

  ImGui::End();
}

void EditorApp::DrawLevelMetadataPanel() {
  ImGui::Begin("Level (Subterra)");
  Terrain2D *terrain = GetWorld().GetTerrain();
  if (!terrain) {
    ImGui::TextUnformatted("No terrain. Import TMX or create terrain first.");
    ImGui::End();
    return;
  }
  TmxMapMetadata &m = terrain->tmxMeta;
  ImGui::TextWrapped(
      "Stored under \"level\" in the scene JSON. Subterra uses this for map events, pickups, "
      "mobs, interactables, collision, and lights.");
  ImGui::Spacing();
  ImGui::TextWrapped(
      "Authoring: prefer entities with Map Event / Interactable / World Spawn Prefab (2D) "
      "components for spatial zones (see viewport gizmos under Global Components). "
      "At runtime, ECS zones override duplicate rows from this panel when they share the same "
      "trigger storage_key, interactable tiled_object_id, or spawn prefab + position.");
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 1.f));
  if (ImGui::Button("Bake ECS Zones -> Level Data", ImVec2(-1, 0))) {
    BakeEcsZonesIntoTmxMeta();
    MarkLevelDirty();
  }
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Copies all ECS Map Event / Interactable / Spawn Prefab zone entities\n"
        "into the Level Data tables below (\"ecs_event_zones\" object group,\n"
        "interactables, spawn prefabs). Lets you export as pure JSON without\n"
        "needing a live ECS world.\n"
        "\nUse File > Save Level afterwards to persist the result.");
  }
  ImGui::Spacing();
  ImGui::TextUnformatted("Player spawn (Subterra)");
  if (ImGui::Button("Place player_start at camera target", ImVec2(-1, 0))) {
    if (const criogenio::Camera2D *cam = GetWorld().GetActiveCamera())
      UpsertPlayerStartInLevel(*terrain, cam->target.x, cam->target.y);
    MarkLevelDirty();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Writes a point object named player_start_position in the \"spawns\" group.");
  if (ImGui::Button("Place player_start at mouse (viewport)", ImVec2(-1, 0))) {
    criogenio::Vec2 w =
        MaybeSnapToGrid(GetMouseWorld(), terrain, snapToGrid);
    UpsertPlayerStartInLevel(*terrain, w.x, w.y);
    MarkLevelDirty();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Uses the world position under the scene cursor; respects View > Snap to grid.");
  ImGui::Spacing();

  if (ImGui::CollapsingHeader("Map bounds & layers", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("Infer bounds from painted tiles")) {
      terrain->InferTmxContentBoundsFromTiles();
    }
    ImGui::SameLine();
    if (ImGui::Button("Grow layerInfo to match terrain layers")) {
      while (m.layerInfo.size() < terrain->layers.size()) {
        TiledLayerMeta lm;
        lm.name = "Layer " + std::to_string(m.layerInfo.size());
        m.layerInfo.push_back(std::move(lm));
      }
    }
    ImGui::DragInt("boundsMinTx", &m.boundsMinTx, 1.f);
    ImGui::DragInt("boundsMinTy", &m.boundsMinTy, 1.f);
    ImGui::DragInt("boundsMaxTx (exclusive)", &m.boundsMaxTx, 1.f);
    ImGui::DragInt("boundsMaxTy (exclusive)", &m.boundsMaxTy, 1.f);
    ImGui::Checkbox("infinite (TMX)", &m.infinite);
    ImGui::Separator();
    ImGui::Text("Tile layers (%zu terrain / %zu meta)", terrain->layers.size(), m.layerInfo.size());
    for (size_t li = 0; li < m.layerInfo.size(); ++li) {
      ImGui::PushID(static_cast<int>(li + 7000));
      TiledLayerMeta &lm = m.layerInfo[li];
      char nameBuf[128]{};
      std::strncpy(nameBuf, lm.name.c_str(), sizeof(nameBuf) - 1);
      if (ImGui::InputText("name", nameBuf, sizeof(nameBuf)))
        lm.name = nameBuf;
      ImGui::DragInt("draw index", &lm.mapLayerIndex, 0.25f);
      ImGui::Checkbox("roof", &lm.roof);
      ImGui::SameLine();
      ImGui::Checkbox("windows", &lm.windows);
      ImGui::Checkbox("draw after entities", &lm.drawAfterEntities);
      bool collides = TmxGetPropertyBool(lm.properties, "collides", false);
      if (ImGui::Checkbox("collides (feeds collision mask)", &collides))
        TmxPropSetBool(lm.properties, "collides", collides);
      ImGui::Separator();
      ImGui::PopID();
    }
  }

  if (ImGui::CollapsingHeader("Collision")) {
    ImGui::TextWrapped(
        "Layers whose name contains \"collision\" or have \"collides\" set, plus object groups "
        "whose name contains \"collision\", are baked into the mask when you rebuild.");
    if (ImGui::Button("Rebuild collision mask")) {
      terrain->RebuildCollisionMaskFromTmxRules();
    }
    const int cw = m.boundsMaxTx - m.boundsMinTx;
    const int ch = m.boundsMaxTy - m.boundsMinTy;
    ImGui::Text("Grid: %d x %d  |  solid bytes: %zu", cw, ch, m.collisionSolid.size());
    TiledObjectGroup *cg = nullptr;
    for (auto &g : m.objectGroups) {
      std::string low = g.name;
      for (char &c : low)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (low.find("collision") != std::string::npos) {
        cg = &g;
        break;
      }
    }
    if (!cg) {
      if (ImGui::Button("Add \"collision\" object group (rectangles → tiles)")) {
        m.objectGroups.push_back({"collision", -1, {}});
        cg = &m.objectGroups.back();
      }
    }
    if (cg) {
      ImGui::TextUnformatted("Collision shapes (map pixels, axis-aligned rects):");
      if (ImGui::Button("Add collision rect##col"))
        cg->objects.push_back({});
      for (size_t ci = 0; ci < cg->objects.size();) {
        ImGui::PushID(static_cast<int>(ci + 8000));
        TiledMapObject &co = cg->objects[ci];
        ImGui::InputFloat("x##col", &co.x, 1.f, 8.f);
        ImGui::InputFloat("y##col", &co.y, 1.f, 8.f);
        ImGui::InputFloat("w##col", &co.width, 1.f, 8.f);
        ImGui::InputFloat("h##col", &co.height, 1.f, 8.f);
        if (ImGui::Button("Remove##col")) {
          cg->objects.erase(cg->objects.begin() + static_cast<std::ptrdiff_t>(ci));
          ImGui::PopID();
          continue;
        }
        ImGui::Separator();
        ImGui::PopID();
        ++ci;
      }
    }
    const int maxPreview = 48;
    if (cw > 0 && ch > 0 && cw <= maxPreview && ch <= maxPreview && !m.collisionSolid.empty()) {
      ImGui::TextUnformatted("Preview (click toggles — use Rebuild to regenerate from layers):");
      for (int ty = 0; ty < ch; ++ty) {
        for (int tx = 0; tx < cw; ++tx) {
          ImGui::PushID(ty * maxPreview + tx);
          size_t idx = static_cast<size_t>(static_cast<size_t>(ty) * static_cast<size_t>(cw) +
                                          static_cast<size_t>(tx));
          bool solid = idx < m.collisionSolid.size() && m.collisionSolid[idx] != 0;
          ImVec4 col = solid ? ImVec4(0.9f, 0.3f, 0.2f, 1.f) : ImVec4(0.2f, 0.2f, 0.25f, 1.f);
          ImGui::PushStyleColor(ImGuiCol_Button, col);
          if (ImGui::SmallButton("##c")) {
            if (idx < m.collisionSolid.size())
              m.collisionSolid[idx] = solid ? 0 : 1;
          }
          ImGui::PopStyleColor();
          if (tx + 1 < cw)
            ImGui::SameLine();
          ImGui::PopID();
        }
      }
    }
  }

  if (ImGui::CollapsingHeader("Map lights")) {
    if (ImGui::Button("Add light"))
      m.mapLightSources.push_back({});
    for (size_t i = 0; i < m.mapLightSources.size();) {
      ImGui::PushID(static_cast<int>(i + 9000));
      auto &ls = m.mapLightSources[i];
      ImGui::InputFloat("x##ls", &ls.x, 1.f, 8.f);
      ImGui::InputFloat("y##ls", &ls.y, 1.f, 8.f);
      ImGui::InputFloat("radius", &ls.radius, 1.f, 16.f);
      int r = ls.r, g = ls.g, b = ls.b;
      ImGui::DragInt("R", &r, 1, 0, 255);
      ImGui::DragInt("G", &g, 1, 0, 255);
      ImGui::DragInt("B", &b, 1, 0, 255);
      ls.r = static_cast<unsigned char>(r);
      ls.g = static_cast<unsigned char>(g);
      ls.b = static_cast<unsigned char>(b);
      bool win = ls.is_window;
      if (ImGui::Checkbox("window light", &win))
        ls.is_window = win;
      if (ImGui::Button("Remove##ls")) {
        m.mapLightSources.erase(m.mapLightSources.begin() + static_cast<std::ptrdiff_t>(i));
        ImGui::PopID();
        continue;
      }
      ImGui::Separator();
      ImGui::PopID();
      ++i;
    }
  }

  if (ImGui::CollapsingHeader("Image layers")) {
    if (ImGui::Button("Add image layer"))
      m.imageLayers.push_back({});
    for (size_t i = 0; i < m.imageLayers.size();) {
      ImGui::PushID(static_cast<int>(i + 10000));
      TiledImageLayerMeta &im = m.imageLayers[i];
      char nbuf[128]{};
      std::strncpy(nbuf, im.name.c_str(), sizeof(nbuf) - 1);
      if (ImGui::InputText("name##im", nbuf, sizeof(nbuf)))
        im.name = nbuf;
      char pbuf[512]{};
      if (im.image)
        std::strncpy(pbuf, im.image->path.c_str(), sizeof(pbuf) - 1);
      if (ImGui::InputText("image path", pbuf, sizeof(pbuf))) {
        std::string path(pbuf);
        if (!path.empty()) {
          im.image = AssetManager::instance().load<TextureResource>(path);
          if (!im.image && !path.empty()) {
            fs::path tryRel = fs::path("subterra_guild") / path;
            im.image = AssetManager::instance().load<TextureResource>(tryRel.string());
          }
        } else {
          im.image.reset();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Browse##im")) {
        const char *f[] = {"*.png", "*.jpg", "*.jpeg"};
        const char *picked =
            tinyfd_openFileDialog("Image", "", 3, f, "Image", 0);
        if (picked && picked[0]) {
          im.image = AssetManager::instance().load<TextureResource>(std::string(picked));
        }
      }
      ImGui::InputFloat("offset X", &im.offsetX, 1.f, 8.f);
      ImGui::InputFloat("offset Y", &im.offsetY, 1.f, 8.f);
      ImGui::SliderFloat("opacity", &im.opacity, 0.f, 1.f);
      ImGui::Checkbox("visible", &im.visible);
      if (ImGui::Button("Remove##im")) {
        m.imageLayers.erase(m.imageLayers.begin() + static_cast<std::ptrdiff_t>(i));
        ImGui::PopID();
        continue;
      }
      ImGui::Separator();
      ImGui::PopID();
      ++i;
    }
  }

  if (ImGui::CollapsingHeader("Spawn prefabs (items / mobs)", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("Add spawn prefab"))
      m.spawnPrefabs.push_back({});
    for (size_t i = 0; i < m.spawnPrefabs.size();) {
      ImGui::PushID(static_cast<int>(i));
      TiledSpawnPrefab &sp = m.spawnPrefabs[i];
      ImGui::InputFloat("x##sp", &sp.x, 1.f, 8.f);
      ImGui::SameLine();
      ImGui::InputFloat("y##sp", &sp.y, 1.f, 8.f);
      ImGui::InputFloat("w##sp", &sp.width, 1.f, 8.f);
      ImGui::SameLine();
      ImGui::InputFloat("h##sp", &sp.height, 1.f, 8.f);
      char buf[256]{};
      std::strncpy(buf, sp.prefabName.c_str(), sizeof(buf) - 1);
      if (ImGui::InputText("prefab id", buf, sizeof(buf)))
        sp.prefabName = buf;
      ImGui::InputInt("quantity", &sp.quantity);
      if (ImGui::Button("Remove")) {
        m.spawnPrefabs.erase(m.spawnPrefabs.begin() + static_cast<std::ptrdiff_t>(i));
        ImGui::PopID();
        continue;
      }
      ImGui::Separator();
      ImGui::PopID();
      ++i;
    }
  }

  if (ImGui::CollapsingHeader("Interactables")) {
    if (ImGui::Button("Add interactable"))
      m.interactables.push_back({});
    for (size_t i = 0; i < m.interactables.size();) {
      ImGui::PushID(static_cast<int>(i + 500));
      TiledInteractable &it = m.interactables[i];
      ImGui::InputFloat("x##it", &it.x, 1.f, 8.f);
      ImGui::SameLine();
      ImGui::InputFloat("y##it", &it.y, 1.f, 8.f);
      ImGui::InputFloat("w##it", &it.w, 1.f, 8.f);
      ImGui::SameLine();
      ImGui::InputFloat("h##it", &it.h, 1.f, 8.f);
      ImGui::Checkbox("point", &it.is_point);
      ImGui::InputInt("tiled object id", &it.tiled_object_id);
      char kbuf[128]{};
      std::strncpy(kbuf, it.interactable_type.c_str(), sizeof(kbuf) - 1);
      if (ImGui::InputText("type (door, chest, ...)", kbuf, sizeof(kbuf)))
        it.interactable_type = kbuf;
      ImGui::TextUnformatted("Custom properties:");
      for (size_t pi = 0; pi < it.properties.size();) {
        ImGui::PushID(static_cast<int>(pi + 600));
        char pkn[96]{}, pkv[256]{};
        std::strncpy(pkn, it.properties[pi].name.c_str(), sizeof(pkn) - 1);
        std::strncpy(pkv, it.properties[pi].value.c_str(), sizeof(pkv) - 1);
        if (ImGui::InputText("key", pkn, sizeof(pkn)))
          it.properties[pi].name = pkn;
        if (ImGui::InputText("value", pkv, sizeof(pkv)))
          it.properties[pi].value = pkv;
        if (ImGui::Button("Remove prop")) {
          it.properties.erase(it.properties.begin() + static_cast<std::ptrdiff_t>(pi));
          ImGui::PopID();
          continue;
        }
        ImGui::PopID();
        ++pi;
      }
      if (ImGui::Button("Add property##it"))
        it.properties.push_back({"new_key", "", "string"});
      if (ImGui::Button("Remove##it")) {
        m.interactables.erase(m.interactables.begin() + static_cast<std::ptrdiff_t>(i));
        ImGui::PopID();
        continue;
      }
      ImGui::Separator();
      ImGui::PopID();
      ++i;
    }
  }

  if (ImGui::CollapsingHeader("Event trigger objects")) {
    TiledObjectGroup *eg = nullptr;
    for (auto &g : m.objectGroups) {
      if (g.name == "subterra_events") {
        eg = &g;
        break;
      }
    }
    static std::vector<std::string> gameplayDrafts;
    if (!eg) {
      gameplayDrafts.clear();
      if (ImGui::Button("Create \"subterra_events\" group + add trigger")) {
        m.objectGroups.push_back({"subterra_events", -1, {}});
        eg = &m.objectGroups.back();
        eg->objects.push_back({});
      }
    }
    if (eg && gameplayDrafts.size() != eg->objects.size()) {
      gameplayDrafts.resize(eg->objects.size());
      for (size_t j = 0; j < eg->objects.size(); ++j)
        gameplayDrafts[j] = TmxPropGetString(eg->objects[j].properties, "gameplay_actions");
    }
    if (eg && ImGui::Button("Add trigger object"))
      eg->objects.push_back({});
    if (!eg) {
      ImGui::TextUnformatted("No event group yet. Use the button above to start.");
    }
    for (size_t i = 0; eg && i < eg->objects.size();) {
      ImGui::PushID(static_cast<int>(i + 2000));
      TiledMapObject &o = eg->objects[i];
      void *treeId = reinterpret_cast<void *>(static_cast<uintptr_t>(i + 31000));
      if (ImGui::TreeNodeEx(treeId, ImGuiTreeNodeFlags_DefaultOpen, "Trigger #%zu", i)) {
        ImGui::InputInt("id", &o.id);
        char nbuf[128]{};
        std::strncpy(nbuf, o.name.c_str(), sizeof(nbuf) - 1);
        if (ImGui::InputText("name", nbuf, sizeof(nbuf)))
          o.name = nbuf;
        char tbuf[128]{};
        std::strncpy(tbuf, o.objectType.c_str(), sizeof(tbuf) - 1);
        if (ImGui::InputText("object type (event / teleport …)", tbuf, sizeof(tbuf)))
          o.objectType = tbuf;
        ImGui::InputFloat("x", &o.x, 1.f, 8.f);
        ImGui::InputFloat("y", &o.y, 1.f, 8.f);
        ImGui::InputFloat("width", &o.width, 1.f, 8.f);
        ImGui::InputFloat("height", &o.height, 1.f, 8.f);
        ImGui::Checkbox("point", &o.point);

        bool activated = TmxPropGetBool(o.properties, "activated", true);
        if (ImGui::Checkbox("activated", &activated))
          TmxPropSetBool(o.properties, "activated", activated);

        std::string et = TmxPropGetString(o.properties, "event_trigger");
        if (ImGui::InputText("event_trigger", &et))
          TmxPropSetString(o.properties, "event_trigger", et);

        std::string eid = TmxPropGetString(o.properties, "event_id");
        if (ImGui::InputText("event_id", &eid))
          TmxPropSetString(o.properties, "event_id", eid);

        std::string etype = TmxPropGetString(o.properties, "event_type");
        if (ImGui::InputText("event_type", &etype))
          TmxPropSetString(o.properties, "event_type", etype);

        std::string tel = TmxPropGetString(o.properties, "teleport_to");
        if (ImGui::InputText("teleport_to", &tel))
          TmxPropSetString(o.properties, "teleport_to", tel);

        std::string tgt = TmxPropGetString(o.properties, "target_level");
        if (ImGui::InputText("target_level (alias)", &tgt))
          TmxPropSetString(o.properties, "target_level", tgt);

        std::string sp = TmxPropGetString(o.properties, "spawn_point");
        if (ImGui::InputText("spawn_point", &sp))
          TmxPropSetString(o.properties, "spawn_point", sp);
        std::string sp2 = TmxPropGetString(o.properties, "spaw_point");
        if (ImGui::InputText("spaw_point (typo alias)", &sp2))
          TmxPropSetString(o.properties, "spaw_point", sp2);
        std::string se = TmxPropGetString(o.properties, "spawn_event_id");
        if (ImGui::InputText("spawn_event_id", &se))
          TmxPropSetString(o.properties, "spawn_event_id", se);

        int sc = TmxPropGetInt(o.properties, "spawn_count", 0);
        if (ImGui::InputInt("spawn_count", &sc))
          TmxPropSetInt(o.properties, "spawn_count", sc);
        int mc = TmxPropGetInt(o.properties, "monster_count", 0);
        if (ImGui::InputInt("monster_count", &mc))
          TmxPropSetInt(o.properties, "monster_count", mc);

        if (i < gameplayDrafts.size()) {
          ImGui::TextUnformatted("gameplay_actions (JSON / script)");
          if (ImGui::InputTextMultiline("##ga", &gameplayDrafts[i], ImVec2(-1, 72))) {
            TmxPropSetString(o.properties, "gameplay_actions", gameplayDrafts[i]);
          }
        }

        if (ImGui::Button("Remove##ev")) {
          eg->objects.erase(eg->objects.begin() + static_cast<std::ptrdiff_t>(i));
          gameplayDrafts.resize(eg->objects.size());
          for (size_t j = 0; j < eg->objects.size(); ++j)
            gameplayDrafts[j] = TmxPropGetString(eg->objects[j].properties, "gameplay_actions");
          ImGui::TreePop();
          ImGui::PopID();
          continue;
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
      ++i;
    }
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

void EditorApp::DrawTerrainToolOverlay(const criogenio::Terrain2D &terrain) {
  criogenio::Renderer &ren = GetRenderer();
  const float tsx = static_cast<float>(terrain.GridStepX());
  const float tsy = static_cast<float>(terrain.GridStepY());
  auto cellRect = [&](int tx, int ty) {
    return criogenio::Rect{terrain.origin.x + tx * tsx,
                           terrain.origin.y + ty * tsy, tsx, tsy};
  };
  auto outlineRect = [&](int x0, int y0, int x1, int y1, criogenio::Color c) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    float wx = terrain.origin.x + x0 * tsx;
    float wy = terrain.origin.y + y0 * tsy;
    float ww = (x1 - x0 + 1) * tsx;
    float wh = (y1 - y0 + 1) * tsy;
    ren.DrawRectOutline(wx, wy, ww, wh, c);
  };

  const criogenio::Color blue   {  0, 160, 255, 220 };
  const criogenio::Color yellow { 255, 220,  60, 220 };
  const criogenio::Color magenta{ 255, 110, 255, 220 };

  // Brush footprint at cursor.
  if (terrainTool == TerrainTool::Paint || terrainTool == TerrainTool::Eyedropper) {
    const int hw = std::max(1, terrainBrushW);
    const int hh = std::max(1, terrainBrushH);
    const int x0 = terrainDragCurTx - hw / 2;
    const int y0 = terrainDragCurTy - hh / 2;
    outlineRect(x0, y0, x0 + hw - 1, y0 + hh - 1, yellow);
  }
  // Drag preview for rect/line/select.
  if (terrainDragging && (terrainTool == TerrainTool::Rect ||
                          terrainTool == TerrainTool::FilledRect ||
                          terrainTool == TerrainTool::Select)) {
    outlineRect(terrainDragAnchorTx, terrainDragAnchorTy, terrainDragCurTx,
                terrainDragCurTy,
                terrainTool == TerrainTool::Select ? magenta : blue);
  }
  if (terrainDragging && terrainTool == TerrainTool::Line) {
    // Bresenham preview points.
    int x0 = terrainDragAnchorTx, y0 = terrainDragAnchorTy;
    int x1 = terrainDragCurTx,    y1 = terrainDragCurTy;
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      const auto r = cellRect(x0, y0);
      ren.DrawRectOutline(r.x, r.y, r.width, r.height, blue);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  // Persistent selection rect.
  if (tileSelection) {
    const auto &s = *tileSelection;
    outlineRect(s.x0, s.y0, s.x1, s.y1, magenta);
  }
  // Stamp ghost preview.
  if (terrainTool == TerrainTool::Stamp && tileClipboard) {
    const auto &p = *tileClipboard;
    if (p.w > 0 && p.h > 0)
      outlineRect(terrainDragCurTx, terrainDragCurTy,
                  terrainDragCurTx + p.w - 1, terrainDragCurTy + p.h - 1, yellow);
  }
}

static bool EditorParentingWouldCycle(criogenio::World &w,
                                      criogenio::ecs::EntityId child,
                                      criogenio::ecs::EntityId newParent) {
  namespace ecs = criogenio::ecs;
  for (ecs::EntityId x = newParent; x != ecs::NULL_ENTITY;) {
    if (x == child)
      return true;
    auto *p = w.GetComponent<criogenio::Parent>(x);
    if (!p)
      break;
    x = p->parent;
  }
  return false;
}

static bool EditorIsAncestorOf(criogenio::World &w, criogenio::ecs::EntityId ancestor,
                               criogenio::ecs::EntityId descendant) {
  namespace ecs = criogenio::ecs;
  for (ecs::EntityId x = descendant; x != ecs::NULL_ENTITY;) {
    if (x == ancestor)
      return true;
    auto *p = w.GetComponent<criogenio::Parent>(x);
    if (!p)
      break;
    x = p->parent;
  }
  return false;
}

void EditorApp::SetEntityParent(criogenio::ecs::EntityId child,
                                criogenio::ecs::EntityId newParent) {
  namespace ecs = criogenio::ecs;
  criogenio::World &w = GetWorld();
  if (child == newParent)
    return;
  if (newParent != ecs::NULL_ENTITY && !w.HasEntity(newParent))
    return;
  if (newParent != ecs::NULL_ENTITY && EditorParentingWouldCycle(w, child, newParent))
    return;
  if (newParent == ecs::NULL_ENTITY) {
    if (w.HasComponent<criogenio::Parent>(child))
      w.RemoveComponent<criogenio::Parent>(child);
  } else {
    if (!w.HasComponent<criogenio::Parent>(child))
      w.AddComponent<criogenio::Parent>(child, criogenio::Parent{newParent});
    else
      w.GetComponent<criogenio::Parent>(child)->parent = newParent;
  }
  MarkLevelDirty();
}

void EditorApp::DestroyEntityInEditor(criogenio::ecs::EntityId id) {
  if (!GetWorld().HasEntity(id))
    return;
  if (auto *sprite = GetWorld().GetComponent<criogenio::AnimatedSprite>(id)) {
    if (sprite->animationId != INVALID_ASSET_ID)
      AnimationDatabase::instance().removeReference(sprite->animationId);
  }
  GetWorld().DeleteEntity(id);
  if (selectedEntityId.has_value() &&
      selectedEntityId.value() == static_cast<int>(id))
    selectedEntityId.reset();
  activeZoneHandle = ZoneHandle::None;
  activeColliderHandle = ZoneHandle::None;
  MarkLevelDirty();
}

void EditorApp::DrawHierarchyEntityNode(
    criogenio::ecs::EntityId id,
    const std::unordered_map<criogenio::ecs::EntityId,
                            std::vector<criogenio::ecs::EntityId>> &childrenByParent,
    std::vector<criogenio::ecs::EntityId> &pendingDelete) {
  namespace ecs = criogenio::ecs;
  criogenio::World &w = GetWorld();

  auto nit = childrenByParent.find(id);
  const bool hasKids =
      nit != childrenByParent.end() && !nit->second.empty();

  std::string label = "Entity";
  if (auto *nm = w.GetComponent<criogenio::Name>(id)) {
    if (!nm->name.empty())
      label = nm->name;
  }
  label += " (" + std::to_string(static_cast<unsigned>(id)) + ")";

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_OpenOnArrow;
  if (selectedEntityId.has_value() &&
      selectedEntityId.value() == static_cast<int>(id))
    flags |= ImGuiTreeNodeFlags_Selected;
  if (!hasKids)
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

  ImGui::PushID(static_cast<int>(id));
  const bool nodeOpen = ImGui::TreeNodeEx("node", flags, "%s", label.c_str());
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    selectedEntityId = static_cast<int>(id);

  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload("CAMPSUR_ENTITY_ID", &id, sizeof(id));
    ImGui::Text("%s", label.c_str());
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *pl = ImGui::AcceptDragDropPayload("CAMPSUR_ENTITY_ID")) {
      IM_ASSERT(pl->DataSize == sizeof(ecs::EntityId));
      ecs::EntityId dragged = *(const ecs::EntityId *)pl->Data;
      if (dragged != id)
        SetEntityParent(dragged, id);
    }
    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Create Child")) {
      ecs::EntityId c = w.CreateEntity("GameObject");
      w.AddComponent<criogenio::Transform>(c);
      w.AddComponent<criogenio::Name>(c, "GameObject");
      SetEntityParent(c, id);
      selectedEntityId = static_cast<int>(c);
    }
    if (ImGui::MenuItem("Delete Entity"))
      pendingDelete.push_back(id);
    ImGui::EndPopup();
  }

  if (nodeOpen && hasKids) {
    for (ecs::EntityId c : nit->second)
      DrawHierarchyEntityNode(c, childrenByParent, pendingDelete);
    ImGui::TreePop();
  }
  ImGui::PopID();
}

void EditorApp::HandleScenePicking() {
  if (sceneMode == SceneMode::Scene3D)
    return;
  if (!viewportHovered)
    return;
  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    return;

  Vec2 mouse = GetViewportMousePos();
  float vx = viewportPos.x, vy = viewportPos.y, vw = viewportSize.x, vh = viewportSize.y;
  if (mouse.x < vx || mouse.y < vy || mouse.x > vx + vw || mouse.y > vy + vh)
    return;
  if (!sceneRT.valid())
    return;

  Vec2 local = {mouse.x - vx, mouse.y - vy};
  Vec2 tex = {local.x * ((float)sceneRT.width / vw), local.y * ((float)sceneRT.height / vh)};
  // Match render.cpp WorldToScreen flipY for render targets (ImGui top-left ↔ SDL space).
  tex.y = (float)sceneRT.height - tex.y;

  Vec2 world = criogenio::ScreenToWorld2D(tex, *GetWorld().GetActiveCamera(), (float)sceneRT.width, (float)sceneRT.height);

  // If we have a selection and the click is on the selected entity, keep it selected
  // so we can drag it (otherwise PickEntityAt would clear selection first and might miss).
  if (selectedEntityId.has_value()) {
    float px, py, pw, ph;
    if (ComputeEntityPickBounds(GetWorld(), selectedEntityId.value(), px, py, pw, ph)) {
      criogenio::Rect bounds{px, py, pw, ph};
      if (criogenio::PointInRect(world, bounds))
        return;
    }
  }

  PickEntityAt(world);
}

void EditorApp::PickEntityAt(criogenio::Vec2 worldPos) {
  selectedEntityId.reset();
  int best = -1;
  float bestArea = -1.f;
  for (int entity : GetWorld().GetEntitiesWith<criogenio::Transform>()) {
    // Honor object-layer lock: clicks slip past entities on locked layers.
    if (lockedObjectLayers.count(GetEntityLayerName(entity)) > 0)
      continue;
    float x, y, w, h;
    if (!ComputeEntityPickBounds(GetWorld(), entity, x, y, w, h))
      continue;
    criogenio::Rect bounds{x, y, w, h};
    if (!criogenio::PointInRect(worldPos, bounds))
      continue;
    const float area = w * h;
    if (best < 0 || area < bestArea) {
      best = entity;
      bestArea = area;
    }
  }
  if (best >= 0)
    selectedEntityId = best;
}
bool EditorApp::IsSceneInputAllowed() const {
  return viewportHovered && !ImGui::IsAnyItemActive() &&
         !ImGui::IsAnyItemHovered();
}

criogenio::Vec2 EditorApp::GetViewportMousePos() const {
  ImVec2 p = ImGui::GetIO().MousePos;
  return {p.x, p.y};
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
  // Scene RT drawing uses flipY (render.cpp) so the texture appears upright in ImGui;
  // ScreenToWorld2D expects pre-flip SDL Y (0 = top in internal camera math before flip).
  textureScreen.y = vpH - textureScreen.y;

  return criogenio::ScreenToWorld2D(textureScreen, *GetWorld().GetActiveCamera(), vpW, vpH);
}

criogenio::Vec2 EditorApp::GetMouseWorld() const {
  if (sceneMode == SceneMode::Scene2D && sceneRT.valid())
    return const_cast<EditorApp*>(this)->ScreenToWorldPosition(GetViewportMousePos());
  return criogenio::Engine::GetMouseWorld();
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
  if (GetWorld().HasComponent<criogenio::Transform3D>(entity))
    DrawTransform3DInspector(entity);
  if (GetWorld().HasComponent<criogenio::Camera3D>(entity))
    DrawCamera3DInspector(entity);

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

  if (GetWorld().HasComponent<criogenio::RigidBody>(entity))
    DrawRigidBodyInspector(entity);

  if (GetWorld().HasComponent<criogenio::BoxCollider>(entity))
    DrawBoxColliderInspector(entity);
  if (GetWorld().HasComponent<criogenio::Model3D>(entity))
    DrawModel3DInspector(entity);
  if (GetWorld().HasComponent<criogenio::BoxCollider3D>(entity))
    DrawBoxCollider3DInspector(entity);
  if (GetWorld().HasComponent<criogenio::Box3D>(entity))
    DrawBox3DInspector(entity);
  if (GetWorld().HasComponent<criogenio::PlayerController3D>(entity))
    DrawPlayerController3DInspector(entity);
  if (GetWorld().HasComponent<criogenio::MapEventZone2D>(entity))
    DrawMapEventZone2DInspector(entity);
  if (GetWorld().HasComponent<criogenio::InteractableZone2D>(entity))
    DrawInteractableZone2DInspector(entity);
  if (GetWorld().HasComponent<criogenio::WorldSpawnPrefab2D>(entity))
    DrawWorldSpawnPrefab2DInspector(entity);
  if (GetWorld().HasComponent<criogenio::PrefabInstance>(entity))
    DrawPrefabInstanceInspector(entity);
  if (GetWorld().HasComponent<criogenio::Parent>(entity))
    DrawParentInspector(entity);
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

void EditorApp::DrawTransform3DInspector(int entity) {
  ImGui::PushID("Transform3D");
  ImGui::BeginGroup();
  bool headerOpen =
      ImGui::CollapsingHeader("Transform 3D", ImGuiTreeNodeFlags_DefaultOpen);
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Transform3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *t = GetWorld().GetComponent<criogenio::Transform3D>(entity);
  if (!t)
    return;

  ImGui::DragFloat3("Position", &t->x, 0.1f);
  ImGui::DragFloat3("Rotation", &t->rotationX, 0.5f);
  ImGui::DragFloat3("Scale", &t->scaleX, 0.01f, 0.01f, 100.0f);
}

void EditorApp::DrawCamera3DInspector(int entity) {
  ImGui::PushID("Camera3D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Camera 3D");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Camera3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *cam = GetWorld().GetComponent<criogenio::Camera3D>(entity);
  if (!cam)
    return;

  ImGui::DragFloat3("Position", &cam->positionX, 0.1f);
  ImGui::DragFloat("Yaw", &cam->yaw, 0.01f);
  ImGui::DragFloat("Pitch", &cam->pitch, 0.01f);
  ImGui::DragFloat("FOV", &cam->fovDeg, 0.1f, 20.0f, 160.0f);
  ImGui::DragFloat("Near Plane", &cam->nearPlane, 0.001f, 0.001f, 10.0f);
  ImGui::DragFloat("Far Plane", &cam->farPlane, 0.5f, 1.0f, 2000.0f);
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
      std::string err;
      criogenio::AssetId newId =
          criogenio::LoadSubterraGuildAnimationJson(std::string(path), nullptr, nullptr, &err);
      if (newId == criogenio::INVALID_ASSET_ID)
        printf("[Editor] Animation import failed: %s\n", err.c_str());
      if (newId != criogenio::INVALID_ASSET_ID) {
        if (oldId != criogenio::INVALID_ASSET_ID) {
          criogenio::AnimationDatabase::instance().removeReference(oldId);
        }
        sprite->animationId = newId;
        criogenio::AnimationDatabase::instance().addReference(newId);
        if (const auto *def =
                criogenio::AnimationDatabase::instance().getAnimation(newId);
            def && !def->clips.empty())
          sprite->SetClip(def->clips[0].name);
        else
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

void EditorApp::DrawBoxColliderInspector(int entity) {
  ImGui::PushID("BoxCollider");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Box Collider");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::BoxCollider>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *col = GetWorld().GetComponent<criogenio::BoxCollider>(entity);
  if (!col)
    return;

  if (ImGui::Button("Fit to sprite")) {
    auto *t = GetWorld().GetComponent<Transform>(entity);
    if (t) {
      if (auto *anim = GetWorld().GetComponent<criogenio::AnimatedSprite>(entity)) {
        criogenio::Rect r = anim->GetFrame();
        if (r.width > 0.5f && r.height > 0.5f) {
          col->width = r.width;
          col->height = r.height;
          col->offsetX = 0.f;
          col->offsetY = 0.f;
          MarkLevelDirty();
        }
      } else if (auto *spr = GetWorld().GetComponent<Sprite>(entity)) {
        col->width = static_cast<float>(spr->spriteSize);
        col->height = static_cast<float>(spr->spriteSize);
        col->offsetX = 0.f;
        col->offsetY = 0.f;
        MarkLevelDirty();
      }
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Match collider size to Sprite tile or current animation frame.");
  }

  bool edited = false;
  edited |= ImGui::DragFloat("Width", &col->width, 1.0f, 1.0f, 1000.0f);
  edited |= ImGui::DragFloat("Height", &col->height, 1.0f, 1.0f, 1000.0f);
  edited |= ImGui::DragFloat2("Offset", &col->offsetX, 1.0f, -1000.0f, 1000.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Offset from entity position (collider rect = Transform + Offset, size).");
  }
  edited |= ImGui::Checkbox("One-way platform", &col->isPlatform);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When checked, only collides when entity falls onto it from above.");
  }
  if (edited)
    MarkLevelDirty();

  if (ImGui::BeginPopupContextItem("BoxColliderContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::BoxCollider>(entity);
      MarkLevelDirty();
    }
    ImGui::EndPopup();
  }
}

void EditorApp::DrawModel3DInspector(int entity) {
  ImGui::PushID("Model3D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Model 3D");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Model3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *model = GetWorld().GetComponent<criogenio::Model3D>(entity);
  if (!model)
    return;

  char pathBuf[512] = {0};
  strncpy(pathBuf, model->modelPath.c_str(), sizeof(pathBuf) - 1);
  if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
    model->modelPath = pathBuf;
  }
  ImGui::ColorEdit3("Tint", &model->tintR);
  ImGui::Checkbox("Visible", &model->visible);
}

void EditorApp::DrawBoxCollider3DInspector(int entity) {
  ImGui::PushID("BoxCollider3D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Box Collider 3D");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::BoxCollider3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *col = GetWorld().GetComponent<criogenio::BoxCollider3D>(entity);
  if (!col)
    return;

  ImGui::DragFloat3("Size", &col->sizeX, 0.05f, 0.01f, 1000.0f);
  ImGui::DragFloat3("Offset", &col->offsetX, 0.05f, -1000.0f, 1000.0f);
  ImGui::Checkbox("Is Trigger", &col->isTrigger);
}

void EditorApp::DrawBox3DInspector(int entity) {
  ImGui::PushID("Box3D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Box 3D");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Box3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *box = GetWorld().GetComponent<criogenio::Box3D>(entity);
  if (!box)
    return;

  ImGui::DragFloat3("Half Extents", &box->halfX, 0.05f, 0.01f, 1000.0f);
  ImGui::ColorEdit3("Color", &box->colorR);
  ImGui::Checkbox("Wireframe", &box->wireframe);
}

void EditorApp::DrawPlayerController3DInspector(int entity) {
  ImGui::PushID("PlayerController3D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Player Controller 3D");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::PlayerController3D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *ctrl = GetWorld().GetComponent<criogenio::PlayerController3D>(entity);
  if (!ctrl)
    return;

  ImGui::DragFloat("Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 200.0f);
  ImGui::DragFloat("Vertical Speed", &ctrl->verticalSpeed, 0.1f, 0.1f, 200.0f);
}

void EditorApp::DrawRigidBodyInspector(int entity) {
  ImGui::PushID("RigidBody");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("RigidBody");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::RigidBody>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *rb = GetWorld().GetComponent<criogenio::RigidBody>(entity);
  if (!rb)
    return;

  ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 100.0f);
  ImGui::DragFloat2("Velocity", &rb->velocity.x, 1.0f, -2000.0f, 2000.0f);

  if (ImGui::BeginPopupContextItem("RigidBodyContext")) {
    if (ImGui::MenuItem("Remove Component")) {
      GetWorld().RemoveComponent<criogenio::RigidBody>(entity);
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

void EditorApp::DrawMapEventZone2DInspector(int entity) {
  ImGui::PushID("MapEventZone2D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Map Event Zone (2D)");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::MapEventZone2D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *z = GetWorld().GetComponent<criogenio::MapEventZone2D>(entity);
  if (!z)
    return;

  ImGui::InputText("Storage key", &z->storage_key);
  ImGui::DragFloat("Width", &z->width, 1.f, 1.f, 4096.f);
  ImGui::DragFloat("Height", &z->height, 1.f, 1.f, 4096.f);
  ImGui::Checkbox("Point (ignore size)", &z->point);
  ImGui::InputText("Event id", &z->event_id);
  ImGui::InputText("Event trigger", &z->event_trigger);
  ImGui::InputText("Event type", &z->event_type);
  ImGui::InputText("Object type", &z->object_type);
  ImGui::InputText("Teleport to", &z->teleport_to);
  ImGui::InputText("Target level", &z->target_level);
  ImGui::InputText("Spawn point", &z->spawn_point);
  ImGui::DragInt("Spawn count", &z->spawn_count, 1, 0, 10000);
  ImGui::DragInt("Monster count", &z->monster_count, 1, 0, 10000);
  ImGui::InputTextMultiline("Gameplay actions", &z->gameplay_actions, ImVec2(-1, 72));
  ImGui::Checkbox("Activated", &z->activated);
}

void EditorApp::DrawInteractableZone2DInspector(int entity) {
  ImGui::PushID("InteractableZone2D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Interactable Zone (2D)");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::InteractableZone2D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *z = GetWorld().GetComponent<criogenio::InteractableZone2D>(entity);
  if (!z)
    return;

  ImGui::DragFloat("Width", &z->width, 1.f, 1.f, 4096.f);
  ImGui::DragFloat("Height", &z->height, 1.f, 1.f, 4096.f);
  ImGui::Checkbox("Point (ignore size)", &z->point);
  ImGui::DragInt("Tiled object id", &z->tiled_object_id, 1, 0, 1 << 30);
  ImGui::InputText("Interactable type", &z->interactable_type);
  ImGui::InputTextMultiline("Properties JSON", &z->properties_json, ImVec2(-1, 72));
}

void EditorApp::DrawWorldSpawnPrefab2DInspector(int entity) {
  ImGui::PushID("WorldSpawnPrefab2D");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("World Spawn Prefab (2D)");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::WorldSpawnPrefab2D>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();

  if (!headerOpen)
    return;

  auto *z = GetWorld().GetComponent<criogenio::WorldSpawnPrefab2D>(entity);
  if (!z)
    return;

  ImGui::InputText("Prefab name", &z->prefab_name);
  ImGui::DragInt("Quantity", &z->quantity, 1, 1, 10000);
  ImGui::DragFloat("Spawn area height", &z->height, 1.f, 0.f, 4096.f);
}

void EditorApp::DrawPrefabInstanceInspector(int entity) {
  ImGui::PushID("PrefabInstance");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Prefab instance");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::PrefabInstance>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    MarkLevelDirty();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  if (!headerOpen)
    return;
  auto *p = GetWorld().GetComponent<criogenio::PrefabInstance>(entity);
  if (!p)
    return;
  ImGui::TextWrapped("Source: %s", p->source_path.c_str());
  ImGui::Text("prefab_name: %s", p->prefab_name.c_str());
}

void EditorApp::DrawParentInspector(int entity) {
  ImGui::PushID("ParentInsp");
  ImGui::BeginGroup();
  bool headerOpen = ImGui::CollapsingHeader("Parent");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
  if (ImGui::SmallButton("X")) {
    GetWorld().RemoveComponent<criogenio::Parent>(entity);
    ImGui::EndGroup();
    ImGui::PopID();
    MarkLevelDirty();
    return;
  }
  ImGui::EndGroup();
  ImGui::PopID();
  if (!headerOpen)
    return;
  auto *par = GetWorld().GetComponent<criogenio::Parent>(entity);
  if (!par)
    return;

  criogenio::World &w = GetWorld();
  namespace ecs = criogenio::ecs;

  ecs::EntityId current = par->parent;
  std::string preview =
      (current == ecs::NULL_ENTITY)
          ? std::string("(none)")
          : (w.HasComponent<criogenio::Name>(current)
                 ? w.GetComponent<criogenio::Name>(current)->name + " [" +
                       std::to_string(static_cast<unsigned>(current)) + "]"
                 : std::string("Entity ") +
                       std::to_string(static_cast<unsigned>(current)));

  if (ImGui::BeginCombo("Parent entity", preview.c_str())) {
    if (ImGui::Selectable("(none)", current == ecs::NULL_ENTITY)) {
      SetEntityParent(static_cast<ecs::EntityId>(entity), ecs::NULL_ENTITY);
    }
    for (ecs::EntityId oid : w.GetAllEntities()) {
      if (oid == static_cast<ecs::EntityId>(entity))
        continue;
      if (EditorIsAncestorOf(w, static_cast<ecs::EntityId>(entity), oid))
        continue;
      std::string en = std::to_string(static_cast<unsigned>(oid));
      if (auto *nm = w.GetComponent<criogenio::Name>(oid);
          nm && !nm->name.empty())
        en = nm->name + " [" + std::to_string(static_cast<unsigned>(oid)) + "]";
      if (ImGui::Selectable(en.c_str(), oid == current)) {
        SetEntityParent(static_cast<ecs::EntityId>(entity), oid);
      }
    }
    ImGui::EndCombo();
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
    if (!GetWorld().HasComponent<criogenio::Transform3D>(entity)) {
      if (ImGui::MenuItem("Transform 3D")) {
        GetWorld().AddComponent<criogenio::Transform3D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::Camera3D>(entity)) {
      if (ImGui::MenuItem("Camera 3D")) {
        GetWorld().AddComponent<criogenio::Camera3D>(entity);
        GetWorld().SetMainCamera3DEntity(entity);
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

    if (!GetWorld().HasComponent<criogenio::RigidBody>(entity)) {
      if (ImGui::MenuItem("RigidBody")) {
        GetWorld().AddComponent<criogenio::RigidBody>(entity);
      }
    }

    if (!GetWorld().HasComponent<criogenio::BoxCollider>(entity)) {
      if (ImGui::MenuItem("Box Collider")) {
        GetWorld().AddComponent<criogenio::BoxCollider>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::Model3D>(entity)) {
      if (ImGui::MenuItem("Model 3D")) {
        GetWorld().AddComponent<criogenio::Model3D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::BoxCollider3D>(entity)) {
      if (ImGui::MenuItem("Box Collider 3D")) {
        GetWorld().AddComponent<criogenio::BoxCollider3D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::Box3D>(entity)) {
      if (ImGui::MenuItem("Box 3D")) {
        GetWorld().AddComponent<criogenio::Box3D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::PlayerController3D>(entity)) {
      if (ImGui::MenuItem("Player Controller 3D")) {
        GetWorld().AddComponent<criogenio::PlayerController3D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::MapEventZone2D>(entity)) {
      if (ImGui::MenuItem("Map Event Zone (2D)")) {
        GetWorld().AddComponent<criogenio::MapEventZone2D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::InteractableZone2D>(entity)) {
      if (ImGui::MenuItem("Interactable Zone (2D)")) {
        GetWorld().AddComponent<criogenio::InteractableZone2D>(entity);
      }
    }
    if (!GetWorld().HasComponent<criogenio::WorldSpawnPrefab2D>(entity)) {
      if (ImGui::MenuItem("World Spawn Prefab (2D)")) {
        GetWorld().AddComponent<criogenio::WorldSpawnPrefab2D>(entity);
      }
    }
    ImGui::EndPopup();
  }
}
