#include "editor.h"
#include "criogenio_io.h"
#include "animated_component.h"
#include "animation_database.h"
#include "subterra_player_json_io.h"
#include "gameplay_tags.h"
#include "components.h"
#include "subterra_mob_prefabs.h"
#include "player_anim.h"

#include "imgui.h"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct AssetEntry {
  std::string name;
  std::string fullPath;
};

struct ItemBrief {
  std::string displayName;
  std::string prefabName;
  int size = 32;
  std::string textureRel;
  int texX = 0;
  int texY = 0;
};

static std::vector<AssetEntry> s_levels;
static std::vector<AssetEntry> s_anims;
static std::vector<AssetEntry> s_meta;
static std::vector<AssetEntry> s_configs;
static std::vector<AssetEntry> s_sprites;

static std::vector<subterra::SubterraMobPrefabDef> s_pickMobs;
static std::vector<ItemBrief> s_pickItems;
static std::string s_pickMetaPath;

static std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::string stripLineCommentsOutsideStrings(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  bool in_str = false;
  bool esc = false;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (esc) {
      out += c;
      esc = false;
      continue;
    }
    if (in_str) {
      if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      out += c;
      continue;
    }
    if (c == '"') {
      in_str = true;
      out += c;
      continue;
    }
    if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
      while (i < in.size() && in[i] != '\n')
        ++i;
      continue;
    }
    out += c;
  }
  return out;
}

static int jsonNumberToInt(const nlohmann::json &j, int fallback = 0) {
  if (j.is_number_integer())
    return j.get<int>();
  if (j.is_number_float())
    return static_cast<int>(j.get<float>());
  return fallback;
}

static void CollectFiles(const std::string &dir, const char *ext, std::vector<AssetEntry> &out) {
  out.clear();
  if (dir.empty())
    return;
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file(ec))
      continue;
    if (entry.path().extension().string() == ext) {
      out.push_back({entry.path().filename().string(), entry.path().string()});
    }
  }
  std::sort(out.begin(), out.end(), [](const AssetEntry &a, const AssetEntry &b) {
    return a.name < b.name;
  });
}

static void CollectImagesRecursive(const std::string &dir, std::vector<AssetEntry> &out) {
  out.clear();
  if (dir.empty())
    return;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec)
      break;
    if (!it->is_regular_file(ec))
      continue;
    const std::string ext = it->path().extension().string();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      out.push_back({it->path().filename().string(), it->path().string()});
    }
  }
  std::sort(out.begin(), out.end(), [](const AssetEntry &a, const AssetEntry &b) {
    return a.name < b.name;
  });
}

static void RefreshBrowser(const ProjectContext &proj) {
  CollectFiles(proj.levelsDir, ".campsurlevel", s_levels);
  CollectFiles(proj.animationsDir, ".campsuranims", s_anims);
  CollectFiles(proj.prefabsDir, ".campsurmeta", s_meta);
  std::vector<AssetEntry> tmp;
  CollectFiles(proj.effectsDir, ".campsurconfig", tmp);
  s_configs = tmp;
  CollectFiles(proj.configDir, ".campsurconfig", tmp);
  for (auto &e : tmp)
    s_configs.push_back(std::move(e));
  std::sort(s_configs.begin(), s_configs.end(), [](const AssetEntry &a, const AssetEntry &b) {
    return a.name < b.name;
  });
  CollectImagesRecursive(proj.spritesDir, s_sprites);
}

static std::string ResolveFirstRegularFile(const ProjectContext &proj,
                                           const std::string &rel) {
  if (rel.empty())
    return {};
  std::vector<std::string> cands = {rel, proj.Resolve(rel),
                                    proj.projectRoot + "/" + rel};
  for (const auto &c : cands) {
    if (c.empty())
      continue;
    std::error_code ec;
    if (fs::is_regular_file(c, ec))
      return c;
  }
  return proj.Resolve(rel);
}

static bool ReadItemBriefsFromMeta(const std::string &path, std::vector<ItemBrief> &out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (raw.empty())
    return false;
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(stripLineCommentsOutsideStrings(raw));
  } catch (...) {
    return false;
  }
  if (!root.is_object() || !root.contains("list") || !root["list"].is_array())
    return false;
  for (const auto &el : root["list"]) {
    if (!el.is_object() || !el.contains("type") || !el["type"].is_string())
      continue;
    if (lowerAscii(el["type"].get<std::string>()) != "item")
      continue;
    ItemBrief b;
    if (el.contains("name") && el["name"].is_string())
      b.displayName = el["name"].get<std::string>();
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      b.prefabName = el["prefab_name"].get<std::string>();
    if (b.prefabName.empty())
      continue;
    if (el.contains("size"))
      b.size = std::max(1, jsonNumberToInt(el["size"], 32));
    if (el.contains("texture") && el["texture"].is_string())
      b.textureRel = el["texture"].get<std::string>();
    if (el.contains("position_on_texture") && el["position_on_texture"].is_array() &&
        el["position_on_texture"].size() >= 2) {
      b.texX = jsonNumberToInt(el["position_on_texture"][0], 0);
      b.texY = jsonNumberToInt(el["position_on_texture"][1], 0);
    }
    out.push_back(std::move(b));
  }
  return !out.empty();
}

} // namespace

void EditorApp::DrawAssetBrowser() {
  if (assetBrowserDirty && project.has_value()) {
    RefreshBrowser(*project);
    assetBrowserDirty = false;
  }

  if (!ImGui::Begin("Asset Browser")) {
    ImGui::End();
    return;
  }

  if (!project.has_value()) {
    ImGui::TextDisabled("No project open.");
    ImGui::Spacing();
    ImGui::TextWrapped("Use File > Open Project... to load a project.campsur file.");
    ImGui::End();
    return;
  }

  const ProjectContext &proj = *project;

  auto spawnMobFromDef = [&](const subterra::SubterraMobPrefabDef &def,
                             const std::string &metaPath) {
    using namespace criogenio;
    namespace ecs = criogenio::ecs;
    World &w = GetWorld();
    std::string animPath;
    const std::string &ap = def.animation_path;
    std::vector<std::string> tryPaths;
    tryPaths.push_back(ap);
    tryPaths.push_back(ResolveFirstRegularFile(proj, ap));
    if (ap.size() >= 5 && ap.rfind(".json") == ap.size() - 5) {
      std::string alt = ap.substr(0, ap.size() - 5) + ".campsuranims";
      tryPaths.push_back(ResolveFirstRegularFile(proj, alt));
    }
    for (const auto &tp : tryPaths) {
      if (tp.empty())
        continue;
      std::error_code ec;
      if (fs::is_regular_file(tp, ec)) {
        animPath = tp;
        break;
      }
    }
    if (animPath.empty() && !ap.empty())
      animPath = ap;

    subterra::PlayerAnimConfig cfg{};
    AssetId animId =
        subterra::LoadPlayerAnimationDatabaseFromJson(animPath, &cfg);
    if (animId == INVALID_ASSET_ID) {
      printf("[AssetBrowser] Mob spawn: could not load animation %s for %s\n",
             animPath.c_str(), def.prefab_name.c_str());
      return;
    }

    Camera2D *cam = w.GetActiveCamera();
    float cx = cam ? cam->target.x : 0.f;
    float cy = cam ? cam->target.y : 0.f;
    const float sc = 0.85f;
    float dw = static_cast<float>(def.size) * sc;
    float dh = static_cast<float>(def.size) * sc;
    ecs::EntityId e = w.CreateEntity("mob");
    w.AddComponent<Transform>(e, cx - dw * 0.5f, cy - dh * 0.5f);
    if (auto *t = w.GetComponent<Transform>(e)) {
      t->scale_x = sc;
      t->scale_y = sc;
    }
    w.AddComponent<AnimationState>(e);
    w.AddComponent<AnimatedSprite>(e, animId);
    w.AddComponent<Name>(
        e, def.display_name.empty() ? def.prefab_name : def.display_name);
    w.AddComponent<MobTag>(e);
    auto &ai = w.AddComponent<AIController>(e);
    ai.velocity = {90.f, 90.f};
    ai.brainState = AIBrainState::ENEMY;
    ai.entityTarget = -1;
    w.AddComponent<PrefabInstance>(
        e, PrefabInstance(metaPath, def.prefab_name));
    selectedEntityId = static_cast<int>(e);
    MarkLevelDirty();
  };

  auto spawnItemBrief = [&](const ItemBrief &it, const std::string &metaPath) {
    using namespace criogenio;
    namespace ecs = criogenio::ecs;
    World &w = GetWorld();
    Camera2D *cam = w.GetActiveCamera();
    float cx = cam ? cam->target.x : 0.f;
    float cy = cam ? cam->target.y : 0.f;
    float sz = static_cast<float>(std::max(1, it.size));
    ecs::EntityId e = w.CreateEntity("pickup");
    w.AddComponent<Transform>(e, cx - sz * 0.5f, cy - sz * 0.5f);
    w.AddComponent<WorldPickup>(e, WorldPickup(it.prefabName, 1, sz, sz));
    w.AddComponent<Name>(e,
                           it.displayName.empty() ? it.prefabName : it.displayName);
    w.AddComponent<PrefabInstance>(e, PrefabInstance(metaPath, it.prefabName));
    std::string texPath = ResolveFirstRegularFile(proj, it.textureRel);
    if (!texPath.empty()) {
      auto &sp = w.AddComponent<Sprite>(e);
      sp.SetTexture(texPath.c_str());
      sp.spriteX = it.texX;
      sp.spriteY = it.texY;
      sp.spriteSize = std::max(1, it.size);
    }
    selectedEntityId = static_cast<int>(e);
    MarkLevelDirty();
  };

  ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.f), "%s", proj.name.c_str());
  ImGui::Separator();

  auto drawSection = [&](const char *label, std::vector<AssetEntry> &entries,
                         std::function<void(const AssetEntry &)> onDoubleClick) {
    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    if (!open)
      return;
    if (entries.empty()) {
      ImGui::Indent();
      ImGui::TextDisabled("(empty)");
      ImGui::Unindent();
      return;
    }
    ImGui::Indent();
    for (const auto &e : entries) {
      ImGui::Selectable(e.name.c_str());
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        onDoubleClick(e);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", e.fullPath.c_str());
    }
    ImGui::Unindent();
  };

  drawSection("Levels", s_levels, [&](const AssetEntry &e) {
    auto &w = GetWorld();
    for (auto id : w.GetAllEntities())
      w.DeleteEntity(id);
    w.DeleteTerrain();
    if (criogenio::LoadWorldFromFile(w, e.fullPath)) {
      printf("[AssetBrowser] Opened level: %s\n", e.fullPath.c_str());
      EditorAppReset();
    } else {
      printf("[AssetBrowser] ERROR: could not load level: %s\n", e.fullPath.c_str());
    }
  });

  drawSection("Animations", s_anims, [&](const AssetEntry &e) {
    if (!selectedEntityId.has_value()) {
      printf("[AssetBrowser] Select an entity first to assign an animation.\n");
      return;
    }
    std::string err;
    criogenio::AssetId nid =
        criogenio::LoadSubterraGuildAnimationJson(e.fullPath, nullptr, nullptr, &err);
    if (nid == criogenio::INVALID_ASSET_ID) {
      printf("[AssetBrowser] Animation load failed: %s — %s\n", e.name.c_str(),
             err.c_str());
      return;
    }
    auto &w = GetWorld();
    const int eid = selectedEntityId.value();
    if (!w.HasComponent<criogenio::AnimatedSprite>(eid))
      w.AddComponent<criogenio::AnimatedSprite>(eid, nid);
    else {
      auto *sp = w.GetComponent<criogenio::AnimatedSprite>(eid);
      if (sp->animationId != criogenio::INVALID_ASSET_ID)
        criogenio::AnimationDatabase::instance().removeReference(sp->animationId);
      sp->animationId = nid;
      criogenio::AnimationDatabase::instance().addReference(nid);
    }
    if (!w.HasComponent<criogenio::AnimationState>(eid))
      w.AddComponent<criogenio::AnimationState>(eid);
    printf("[AssetBrowser] Assigned animation %s to entity %d\n", e.name.c_str(), eid);
  });

  drawSection("Prefabs", s_meta, [&](const AssetEntry &e) {
    std::vector<subterra::SubterraMobPrefabDef> mobs;
    std::vector<ItemBrief> items;
    subterra::SubterraMobPrefabsReadListFromPath(e.fullPath, mobs);
    ReadItemBriefsFromMeta(e.fullPath, items);
    if (mobs.empty() && items.empty()) {
      printf("[AssetBrowser] No mob/item entries in %s\n", e.fullPath.c_str());
      return;
    }
    if (mobs.size() + items.size() == 1) {
      if (!mobs.empty())
        spawnMobFromDef(mobs[0], e.fullPath);
      else
        spawnItemBrief(items[0], e.fullPath);
      return;
    }
    s_pickMobs = std::move(mobs);
    s_pickItems = std::move(items);
    s_pickMetaPath = e.fullPath;
    ImGui::OpenPopup("prefab_spawn_pick");
  });

  if (ImGui::BeginPopupModal("prefab_spawn_pick", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Choose an entry to spawn at the camera target:");
    ImGui::Separator();
    for (const auto &def : s_pickMobs) {
      std::string label = "[Mob] " +
                          (def.display_name.empty() ? def.prefab_name : def.display_name);
      if (ImGui::Selectable(label.c_str())) {
        spawnMobFromDef(def, s_pickMetaPath);
        ImGui::CloseCurrentPopup();
      }
    }
    for (const auto &it : s_pickItems) {
      std::string label =
          "[Item] " + (it.displayName.empty() ? it.prefabName : it.displayName);
      if (ImGui::Selectable(label.c_str())) {
        spawnItemBrief(it, s_pickMetaPath);
        ImGui::CloseCurrentPopup();
      }
    }
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  drawSection("Config", s_configs, [&](const AssetEntry &e) {
    printf("[AssetBrowser] Config: %s\n", e.fullPath.c_str());
  });

  if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (s_sprites.empty()) {
      ImGui::Indent();
      ImGui::TextDisabled("(no images in %s)",
                          project->spritesDir.empty() ? "<sprites_dir not set>"
                                                        : project->spritesDir.c_str());
      ImGui::Unindent();
    } else {
      ImGui::Indent();
      for (const auto &e : s_sprites) {
        ImGui::Selectable(e.name.c_str());
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          ImGui::SetDragDropPayload("CAMPSUR_SPRITE_PATH", e.fullPath.c_str(),
                                    e.fullPath.size() + 1);
          ImGui::Text("Drop in viewport: %s", e.name.c_str());
          ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(0))
          ImGui::SetTooltip("%s", e.fullPath.c_str());
      }
      ImGui::Unindent();
    }
  }

  ImGui::End();
}
