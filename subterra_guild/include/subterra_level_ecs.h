#pragma once

#include "map_events.h"
#include "tmx_metadata.h"
#include <vector>

namespace criogenio {
class World;
}

namespace subterra {

/** Append `MapEventTrigger` entries from entities with `MapEventZone2D` + `Transform`. */
void CollectMapEventTriggersFromEntities(criogenio::World &world,
                                        std::vector<MapEventTrigger> &out);
void AppendMapEventTriggersFromEntities(criogenio::World &world,
                                       std::vector<MapEventTrigger> &out);

/** Append `TiledInteractable` entries from `InteractableZone2D` + `Transform`. */
void CollectInteractablesFromEntities(criogenio::World &world,
                                     std::vector<criogenio::TiledInteractable> &out);
void AppendInteractablesFromEntities(criogenio::World &world,
                                    std::vector<criogenio::TiledInteractable> &out);

} // namespace subterra
