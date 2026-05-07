## This is the most simple and customized engine I come up with

**Engine architecture & capabilities:** [engine/ARCHITECTURE.md](engine/ARCHITECTURE.md) · Repo overview: [ARCHITECTURE.md](ARCHITECTURE.md) · Subterra gameplay mechanics: [subterra_guild/GAMEPLAY.md](subterra_guild/GAMEPLAY.md)

**Editor & projects:** Levels and game data use `.campsur*` formats (e.g. `project.campsur`, `.campsurlevel`). The editor supports tile/object layers (visibility, lock, picking), terrain tools with undo/redo, tile flip on the brush, and **View → Reset Layout**. Play-in-editor can target a sample **Subterra** mode or a minimal **free-form 2D** harness via `game_mode` in the project file. Sample projects: [subterra_guild/project.campsur](subterra_guild/project.campsur), [top_down_demo/](top_down_demo/README.md).

### Don't use this in production
  The idea is to create a simple game engine + entity component for make simple RPG games.

![Engine + Editor](https://github.com/marciusvinicius/campsur/blob/main/screenshot.png?raw=true)

![Enet + Engine](https://github.com/marciusvinicius/campsur/blob/main/test_multiplayer_enet.png?raw=true)
