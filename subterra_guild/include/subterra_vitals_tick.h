#pragma once

namespace criogenio {
class World;
}

namespace subterra {

struct SubterraSession;

/** Fixed-step vitals + status effects (reference `player_update_vitals` + `status_effects_tick`). */
void SubterraTickVitalsAndStatus(SubterraSession &session, criogenio::World &world, float dt);

} // namespace subterra
