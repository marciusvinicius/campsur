#pragma once

#include <string>

namespace subterra {

bool SubterraItemConsumableTryLoadFromPath(const std::string &path);

/** Returns true if this prefab restores vitals on use (from entities_items.json). */
bool SubterraItemConsumableLookup(const std::string &item_id, float &out_restore_health,
                                  float &out_restore_food);

} // namespace subterra
