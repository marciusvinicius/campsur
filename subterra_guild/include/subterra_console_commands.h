#pragma once

namespace subterra {

class SubterraEngine;

/** Registers Subterra-specific criogenio::DebugConsole commands on the given engine. */
void RegisterSubterraConsoleCommands(SubterraEngine &engine);

/** Clears movement/run hooks (call from `SubterraEngine` destructor before session is destroyed). */
void SubterraUnregisterMovementHooks();

} // namespace subterra
