#include "project_context.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Recent-paths registry (in-process, not persisted to disk)
// ---------------------------------------------------------------------------

std::vector<std::string> &ProjectContext::RecentPaths() {
    static std::vector<std::string> s_recent;
    return s_recent;
}

void ProjectContext::PushRecentPath(const std::string &path) {
    auto &v = RecentPaths();
    v.erase(std::remove(v.begin(), v.end(), path), v.end());
    v.insert(v.begin(), path);
    if (v.size() > 8)
        v.resize(8);
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::string ProjectContext::Resolve(const std::string &relPath) const {
    if (relPath.empty())
        return {};
    fs::path p(relPath);
    if (p.is_absolute())
        return relPath;
    return (fs::path(projectRoot) / p).string();
}

// ---------------------------------------------------------------------------
// LoadFromFile
// ---------------------------------------------------------------------------

bool ProjectContext::LoadFromFile(const std::string &campsurPath, ProjectContext &out) {
    std::ifstream f(campsurPath);
    if (!f.good()) {
        std::fprintf(stderr, "[Project] Cannot open: %s\n", campsurPath.c_str());
        return false;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[Project] Parse error in %s: %s\n", campsurPath.c_str(), e.what());
        return false;
    }

    // Determine project root from the file's parent directory.
    fs::path root = fs::absolute(fs::path(campsurPath)).parent_path();

    auto resolveField = [&](const std::string &rel) -> std::string {
        if (rel.empty()) return {};
        fs::path p(rel);
        if (p.is_absolute()) return rel;
        return (root / p).string();
    };

    ProjectContext ctx;
    ctx.projectRoot   = root.string();
    ctx.name          = j.value("name", "Unnamed Project");
    ctx.gameModeId    = j.value("game_mode", "");
    ctx.levelsDir     = resolveField(j.value("levels_dir", ""));
    ctx.animationsDir = resolveField(j.value("animations_dir", ""));
    ctx.prefabsDir    = resolveField(j.value("prefabs_dir", ""));
    ctx.effectsDir    = resolveField(j.value("effects_dir", ""));
    ctx.configDir     = resolveField(j.value("config_dir", ""));
    ctx.spritesDir    = resolveField(j.value("sprites_dir", ""));
    ctx.initLevel     = resolveField(j.value("init_level", ""));

    if (j.contains("game_config") && j["game_config"].is_object())
        ctx.gameConfig = j["game_config"];

    out = std::move(ctx);
    PushRecentPath(fs::absolute(campsurPath).string());
    std::printf("[Project] Opened: %s (%s)\n", out.name.c_str(), campsurPath.c_str());
    return true;
}
