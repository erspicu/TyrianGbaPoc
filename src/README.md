# Runtime source organization

The GBA runtime deliberately uses a small number of C translation units and
ordered `.inc` fragments. The fragments are not independent headers and
should not be compiled on their own: they share static state and helpers
inside their owning translation unit.

This layout keeps ARM/IWRAM placement, static linkage, function order and
generated machine code stable while making each subsystem practical to read
and edit.

## `main.c` translation unit

`main.c` owns global runtime state and includes these broad subsystems:

- `background_runtime.inc`: streamed three-layer background cache.
- `layer_runtime.inc`: source draw-order and GBA priority adapter.
- `gba_platform.inc`: IRQ, DMA, audio and platform primitives.
- `level_setup.inc`: selected-level loading and runtime preparation.
- `frontend_runtime.inc`: aggregator for `frontend/`.
- `entity_runtime.inc`: entity and effect helpers.
- `combat_runtime.inc`: player weapons, collision responses and damage.
- `source_runtime.inc`: source enemy/projectile graphics and caches.
- `level_update.inc`: source-tick gameplay update.
- `gba_oam.inc`: final OAM ordering and emission.
- `jukebox_runtime.inc`: JukeBox state and visualization.
- `gba_hud.inc`: gameplay overlays.
- `gba_scene.inc`: gameplay frame composition.
- `autotest.inc`: aggregator for `autotest/`, excluded from release builds.
- `main_loop.inc`: hardware initialization, fixed timestep and main loop.

### `frontend/`

- `frontend_core.inc`: front-end state, Mode-4 pages, dirty rectangles and
  reusable font/drawing primitives.
- `frontend_source_art.inc`: stock source-frame overlays and ship/item art.
- `frontend_menus.inc`: pre-game, Game Menu, Upgrade Ship and Quit dialog.
- `frontend_navigation.inc`: Next Level planet/dot OBJ presentation.
- `frontend_flow.inc`: statistics, transitions, demo playback and input
  state machine.

### `autotest/`

- `autotest_core.inc`: common SRAM helpers and source equipment fixture.
- `autotest_romfs_matrix.inc`: all-episode data and route validation.
- `autotest_scenarios.inc`: stress, death, JukeBox and Demo scenarios.
- `autotest_telemetry.inc`: final gameplay invariants and SRAM report.
- `autotest_input.inc`: deterministic route/input and capture helpers.

## `opentyrian_level_port.c` translation unit

The direct OpenTyrian gameplay translation is ordered as:

- `level_port/level_port_spawn.inc`: data reads, RNG, enemy construction and
  level initialization.
- `level_port/level_port_events.inc`: direct event-system translation.
- `level_port/level_port_enemy_motion.inc`: firing, launching and four-pool
  enemy motion.
- `level_port/level_port_collisions.inc`: death products, rewards, player
  shots, pickups, contact and enemy projectiles.
- `level_port/level_port_advance.inc`: parallax, timers and per-tick driver.

Keep source-order-sensitive work in this order. A refactor that needs public
interfaces or independent compilation should first add a narrow header and
tests; do not silently duplicate shared static state between translation
units.
