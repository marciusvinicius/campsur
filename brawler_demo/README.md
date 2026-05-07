# Brawler demo (belt scroller)

Minimal **Streets of Rage–style** sample on the Campsur engine:

- **`game_mode`: `beat_em_up`** — editor **Play** uses ground-Y sprite sorting and asymmetric walk speeds (faster along X, slower along Y).
- **Standalone**: `campsur_brawler_demo` after `premake5 && make`.

## Assets

Add 32×32 placeholder PNGs (or swap paths in `stage01.campsurlevel`):

- `assets/sprites/dude_monster.png` — player
- `assets/sprites/owlet_monster.png` — enemies

The same filenames are used in the `top_down_demo` README if you have those files elsewhere.

## Run

1. `premake5 gmake2 && make campsur_brawler_demo` (from repo root; use your usual config).
2. `./bin/Debug/campsur_brawler_demo` — working directory must be `brawler_demo/` (the binary sets this from source path).
3. **Editor**: open `project.campsur`, edit `stage01.campsurlevel`, **Play** — WASD / arrows, Shift run.

## Next gameplay steps

- Hitboxes (`BoxCollider`), attack states, i-frames, knockdown.
- Enemy AI + spawning; screen bounds and stage camera (lock scroll axis).
- Optional: `AnimatedSprite` + clips for walk / punch / hit reactions.
