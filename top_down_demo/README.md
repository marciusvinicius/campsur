# Top Down Demo

A minimal Campsur project that exercises the editor's free-form 2D mode and
the project system end-to-end (no terrain, no Subterra-specific gameplay).

## What's here

```
top_down_demo/
  project.campsur                 # project descriptor (loaded by File > Open Project...)
  assets/
    sprites/
      dude_monster.png            # sample sprite (drag from Asset Browser)
      owlet_monster.png
    levels/
      init.campsurlevel           # tiny starter scene with two sprite entities
  data/                           # placeholders for animations/prefabs/configs
```

## Quickstart

1. Build the editor: `make config=debug_arm64 -j` (any config works).
2. Run the editor: `./bin/Debug/campsur_editor`.
3. `File > Open Project...` and pick `top_down_demo/project.campsur`.
4. The starter level loads two sprites: one is named `player` (WASD moves it in
   play mode), the other is `Owlet` (static prop).
5. Click **Play**: with `game_mode: "free_form_2d"`, the editor runs a minimal
   core-only play mode (movement + sprite/anim render, camera follows the
   played entity). **Stop** with **Revert** checked to discard changes.
6. Drag sprites from the Asset Browser, move entities, save (`Ctrl+S`), restart, reopen — sprites round-trip.

## What this demo validates

- Project loader (`ProjectContext`) resolving asset paths for a non-Subterra project.
- Free-form 2D scene (no `Terrain2D`, `snapToGrid` defaults to off after load).
- Asset Browser drag-drop spawning entities with `Sprite` + `Transform`.
- `Sprite::atlasPath` round-tripping through the level serializer.
- Object Layer panel: assignment, visibility toggle, lock toggle.
- `LayerMembership` persistence in `.campsurlevel`.
- In-editor **Play** for `free_form_2d` via `FreeForm2DEditorGameMode` (WASD / arrows, Shift run).

## What this demo does NOT have

- No Subterra systems (vitals, mobs, map events). Projects with
  `game_mode: "subterra_guild"` still use the full Subterra play mode.
- No animations / prefabs / configs are populated; the `data/` subdirs are
  conventional placeholders.
