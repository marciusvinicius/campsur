#pragma once

#include <cstddef>
#include <string>

namespace subterra {

struct SubterraSession;

/** Try paths in order; sets `session.worldConfigPath` on success. */
bool SubterraTryLoadServerConfigurationFromPaths(const char *const *paths, size_t pathCount,
                                               SubterraSession &session);

bool SubterraTryLoadInputConfigFromPaths(const char *const *paths, size_t pathCount,
                                         SubterraSession &session);

/**
 * Reload `world_config.json` + `input_config.json` from `session.*ConfigPath`, reapply vitals caps,
 * camera runtime, day phase size. Optionally reload current TMX when `reloadMap` is true.
 */
bool SubterraHotReloadServerConfiguration(SubterraSession &session, bool reloadMap,
                                         std::string *logOut);

} // namespace subterra
