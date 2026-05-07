#include "editor.h"
#include "criogenio_io.h"
#include "animated_component.h"
#include "animation_database.h"
#include "subterra_player_json_io.h"

#include "imgui.h"

#include <filesystem>
#include <vector>
#include <string>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

struct AssetEntry {
    std::string name;     // filename only (e.g. "Cave.campsurlevel")
    std::string fullPath; // absolute path
};

static std::vector<AssetEntry> s_levels;
static std::vector<AssetEntry> s_anims;
static std::vector<AssetEntry> s_meta;
static std::vector<AssetEntry> s_configs;
static std::vector<AssetEntry> s_sprites;

static void CollectFiles(const std::string &dir, const char *ext, std::vector<AssetEntry> &out) {
    out.clear();
    if (dir.empty()) return;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension().string() == ext) {
            out.push_back({entry.path().filename().string(), entry.path().string()});
        }
    }
    std::sort(out.begin(), out.end(), [](const AssetEntry &a, const AssetEntry &b) {
        return a.name < b.name;
    });
}

// Recursive scan for image files (used by the Sprites section).
static void CollectImagesRecursive(const std::string &dir, std::vector<AssetEntry> &out) {
    out.clear();
    if (dir.empty()) return;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
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
    CollectFiles(proj.levelsDir,     ".campsurlevel", s_levels);
    CollectFiles(proj.animationsDir, ".campsuranims",  s_anims);
    CollectFiles(proj.prefabsDir,    ".campsurmeta",   s_meta);
    // Collect from both effects and config dirs.
    std::vector<AssetEntry> tmp;
    CollectFiles(proj.effectsDir, ".campsurconfig", tmp);
    s_configs = tmp;
    CollectFiles(proj.configDir,  ".campsurconfig", tmp);
    for (auto &e : tmp) s_configs.push_back(std::move(e));
    std::sort(s_configs.begin(), s_configs.end(), [](const AssetEntry &a, const AssetEntry &b) {
        return a.name < b.name;
    });
    CollectImagesRecursive(proj.spritesDir, s_sprites);
}

} // namespace

void EditorApp::DrawAssetBrowser() {
    // Refresh when flagged (project opened, file saved, etc.)
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

    // Project name header
    ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.f), "%s", proj.name.c_str());
    ImGui::Separator();

    // Helper: draw one section with collapsible header.
    auto drawSection = [&](const char *label, std::vector<AssetEntry> &entries,
                           std::function<void(const AssetEntry &)> onDoubleClick) {
        bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
        if (!open) return;
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

    // ---- Levels (.campsurlevel) ----
    drawSection("Levels", s_levels, [&](const AssetEntry &e) {
        auto &w = GetWorld();
        for (auto id : w.GetAllEntities()) w.DeleteEntity(id);
        w.DeleteTerrain();
        if (criogenio::LoadWorldFromFile(w, e.fullPath)) {
            printf("[AssetBrowser] Opened level: %s\n", e.fullPath.c_str());
            EditorAppReset();
        } else {
            printf("[AssetBrowser] ERROR: could not load level: %s\n", e.fullPath.c_str());
        }
    });

    // ---- Animations (.campsuranims) ----
    drawSection("Animations", s_anims, [&](const AssetEntry &e) {
        if (!selectedEntityId.has_value()) {
            printf("[AssetBrowser] Select an entity first to assign an animation.\n");
            return;
        }
        std::string err;
        criogenio::AssetId nid = criogenio::LoadSubterraGuildAnimationJson(e.fullPath, nullptr, nullptr, &err);
        if (nid == criogenio::INVALID_ASSET_ID) {
            printf("[AssetBrowser] Animation load failed: %s — %s\n", e.name.c_str(), err.c_str());
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

    // ---- Prefabs / entity data (.campsurmeta) ----
    drawSection("Prefabs", s_meta, [&](const AssetEntry &e) {
        // Just log the path for now — future: open a read-only JSON inspector.
        printf("[AssetBrowser] Prefab: %s\n", e.fullPath.c_str());
    });

    // ---- Config (.campsurconfig) ----
    drawSection("Config", s_configs, [&](const AssetEntry &e) {
        printf("[AssetBrowser] Config: %s\n", e.fullPath.c_str());
    });

    // ---- Sprites (.png / .jpg) — drag into the viewport to spawn an entity ----
    if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (s_sprites.empty()) {
            ImGui::Indent();
            ImGui::TextDisabled("(no images in %s)",
                                project->spritesDir.empty() ? "<sprites_dir not set>" : project->spritesDir.c_str());
            ImGui::Unindent();
        } else {
            ImGui::Indent();
            for (const auto &e : s_sprites) {
                ImGui::Selectable(e.name.c_str());
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    // Payload contains the absolute file path (NUL-terminated).
                    ImGui::SetDragDropPayload("CAMPSUR_SPRITE_PATH",
                                              e.fullPath.c_str(),
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
