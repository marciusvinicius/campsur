#pragma once

#include <string>

namespace subterra {

struct SubterraSession;

/** Place mob sprites in a small grid around (cx, cy) in world pixels (center point). */
void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count);

/** Single mob centered at (cx, cy). */
void SpawnMobAt(SubterraSession &session, float cx, float cy);

/** World pickup at (cx, cy) center. */
void SpawnPickupAt(SubterraSession &session, float cx, float cy, const std::string &item_id,
                   int count);

} // namespace subterra
