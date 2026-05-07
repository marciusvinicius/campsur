#pragma once

namespace criogenio {
class GameUi;
}

namespace subterra {

struct SubterraSession;

/** In-game HUD: interact prompt, equipment, action bar (engine `GameUi`, not ImGui). */
void SubterraGameUiDrawHud(SubterraSession &session, criogenio::GameUi &ui);
/** Full inventory / vitals panel when `session.showInventoryPanel`. */
void SubterraGameUiDrawInventory(SubterraSession &session, criogenio::GameUi &ui);

} // namespace subterra
