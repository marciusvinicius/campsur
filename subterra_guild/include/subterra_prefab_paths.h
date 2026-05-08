#pragma once

namespace subterra {

/** Search order matches startup in main: cwd `data/...` first, then repo-root `subterra_guild/...`. */
inline constexpr const char *kItemPrefabJsonPaths[] = {
    "data/prefabs/entities_items.campsurmeta",
    "subterra_guild/data/prefabs/entities_items.campsurmeta",
};

inline constexpr const char *kInteractablePrefabJsonPaths[] = {
    "data/prefabs/entities_interactable.campsurmeta",
    "subterra_guild/data/prefabs/entities_interactable.campsurmeta",
};

inline constexpr const char *kMobPrefabJsonPaths[] = {
    "data/prefabs/entities_mobs.campsurmeta",
    "subterra_guild/data/prefabs/entities_mobs.campsurmeta",
};

} // namespace subterra
