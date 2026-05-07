#pragma once

#include "json.hpp"
#include <string>
#include <vector>

/**
 * Loaded from a project.campsur file. Holds the absolute paths for all
 * asset directories so the editor and play mode can resolve files without
 * hardcoded search arrays.
 *
 * Canonical project layout (Subterra is the reference):
 *   project.campsur
 *   assets/levels/      *.campsurlevel
 *   data/animations/    *.campsuranims
 *   data/prefabs/       *.campsurmeta
 *   data/effects/       *.campsurconfig
 *   data/server_configuration/  *.campsurconfig
 *   assets/sprites/
 */
struct ProjectContext {
    std::string name;
    std::string projectRoot;    // absolute directory containing project.campsur
    std::string gameModeId;     // matches a key in the GameModeRegistry
    std::string levelsDir;      // absolute
    std::string animationsDir;
    std::string prefabsDir;
    std::string effectsDir;
    std::string configDir;
    std::string spritesDir;
    std::string initLevel;      // absolute path to default .campsurlevel
    nlohmann::json gameConfig;  // raw "game_config" block from project.campsur

    /** List of recently opened project.campsur absolute paths (most-recent first). */
    static std::vector<std::string> &RecentPaths();
    /** Push to the front of RecentPaths, dedup, cap at 8. */
    static void PushRecentPath(const std::string &path);

    /**
     * Parse a project.campsur file.
     * All relative paths in the file are resolved relative to the directory
     * containing the .campsur file.
     * Returns false (and leaves out unchanged) on any parse / IO error.
     */
    static bool LoadFromFile(const std::string &campsurPath, ProjectContext &out);

    /**
     * Write project descriptor to disk. Merges with existing JSON at `campsurAbsPath`
     * when present so unknown keys are preserved. Paths on `ctx` must be absolute
     * (as after LoadFromFile); they are stored relative to the project root.
     */
    static bool SaveToFile(const std::string &campsurAbsPath, const ProjectContext &ctx);

    /**
     * Resolve a project-relative path to absolute.
     * If relPath is already absolute it is returned as-is.
     */
    std::string Resolve(const std::string &relPath) const;
};
