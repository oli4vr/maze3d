# maze3d — 2.5D terminal raycaster

Created by **oli4vr**.

## Build & run

```sh
make            # requires libncursesw
make clean
make win        # cross-compile maze3d.exe for Windows (requires mingw-w64)
./maze3d        # interactive game
./maze3d --demo # auto-pilot screensaver demo
./maze3d --text # text-based debug mode
```

Maze generation at startup — takes ~2s.

**Windows cross-compile** — first fetch the PDCursesMod submodule:

```sh
git submodule update --init
make win
```

Produces `maze3d.exe` linked against
[PDCursesMod](https://github.com/Bill-Gray/PDCursesMod) (bundled as
`lib/pdcursesmod/` submodule) instead of libncursesw.

## Controls

- Arrow keys: UP/DOWN move forward/backward one block, LEFT/RIGHT turn 90°
- Space: Attack enemy in front, use item at feet, or exit through skull wall
- Enter: Confirm in death overlay
- TAB: Open inventory / stat upgrade menu
- H: Show high scores
- F1 / ?: Help screen
- Q: Quit

Turn-based with ~250-350ms animation (16-22 frames at 16ms). Next input can be queued during animation.

## Gameplay

- Navigate a procedurally generated 512×512 maze inside a necromancer's mind
- Defeat the necromancer at level 40 to break free
- Collect water and potions to restore HP and water
- Kill enemies for souls, spend on stat upgrades (cost starts at 3 souls, +1 per upgrade)
- All UI and log messages use Shakespearean English
- Manage water — moving forward/backward consumes it; combat does not
- HUD panel at bottom: green health bar, blue water bar, level, steps, souls
- Right-side log panel shows action messages
- 25% enemy drop chance (50/50 health potion or water bottle) with pop-up
- 5×5 clear zone around player start (no enemies or items)
- High scores (5 entries) stored in `~/.maze3d/maze3d.keep`
- 6 upgradeable stats spendable from Souls inventory item
- Score is a running tally (+1 per step/combat, +50 per level, souls×5 per kill, boss bonus)
- On death: high score saved first, then restart/quit overlay

## Stats

- **ATK** — how hard you hit
- **DEF** — chance to dodge attacks
- **TGH** — reduces damage taken (halved: effective = `tgh / 2`)
- **STA** — increases max HP (+10 per point); also increases potion capacity
- **END** — increases max water (+10 per point); also increases potion capacity
- **LCK** — improves hit & drop chance (±1 per point, capped ±3)

## Enemies

- Skeletons (grey), Orcs (red), Cultists (blue) — each has 12 levels of difficulty
- Necromancer boss (purple) at level 40 — high attack (55), defense (40), 500 HP
- Enemy tier determined by `/39`/`/34`/`/29` progression (skel L1 at lv1, orc L1 at lv5, cult L1 at lv10)
- Cultists are glass cannons with all-level magic; all enemies get +1 ATK per ~4.5 game levels via `ENEMY_ATK_LEVEL_BONUS`
- Death strike: when slain, enemies lash out one last time (magic-capable have 75% magic chance)
- TGH is halved (`>> 1`) when reducing enemy damage — it's valuable but never grants immunity
- Players can dodge attacks based on defense vs enemy attack roll
- Enemies cannot spawn within 1 cell of another enemy

## Animation system

- **Type 0 (move)**: linear interpolation to target cell over ANIM_FRAMES (22)
- **Type 1 (turn)**: 90° rotation of dir/plane vectors over ANIM_FRAMES
- **Type 2 (attack lunge)**: bounce forward 0.25×dir then back over ATTACK_FRAMES (16), with red fullscreen flash at peak; `attack_enemies()` called on completion
- **Flash overlays**: red (attack peak / thirst damage), blue (water pickup), green (health pickup), grey (item pickup) — one frame of solid colour over whole terminal before refresh

## Maze generation

- 8×8 block zones with 4 colours × 2 textures (stone/brick)
- Random walk carving algorithm
- Water puddles, potions, skeletons, orcs, and cultists placed randomly
- Exit (skull wall) at farthest point from start
- Boss replaces skull wall at level 40

## Texture system

- 40 wall textures: 4 colours × 2 styles × 5 variations
- Skull texture only appears as the exit wall
- Water sprite: elliptical blue puddle with drop shadow
- Enemy sprites tinted by family: grey skeletons, red orcs, blue cultists, purple boss

## Architecture

Source files: `engine.{c,h}` (renderer + textures + sprites), `gentex.{c,h}` (procedural wall & sprite texture gen), `gameplay.{c,h}` (RPG logic, combat, inventory, high scores), `maze3d.c` (interactive game + auto-pilot demo), `maze3d-text.c` (text debug mode), `progression.h` (tunable balance values), `enemies.{c,h}` (enemy type definitions and sprite data).

- DDA raycasting with 5 shade levels using Unicode block characters
- Billboard projection sprites with z-ordering
- xterm-256 color support
- Colour pairs: flash_pair (red/red), green_pair (green/green), blue_pair (blue/blue), health_pair (green/black), water_pair (blue/black)
