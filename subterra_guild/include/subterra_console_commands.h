#pragma once

namespace subterra {

class SubterraEngine;

/** Registers Subterra-specific criogenio::DebugConsole commands on the given engine. */
void RegisterSubterraConsoleCommands(SubterraEngine &engine);

} // namespace subterra
