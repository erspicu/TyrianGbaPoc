# Exit / Pickup / Secret Route source-parity investigation

Date: 2026-07-28

## Confirmed source behavior

### Level completion

OpenTyrian `mainint.c::JE_endLevelAni()` does not clear the screen before
drawing the completion summary.  It draws the glowing Completed/Exiting,
Cash, Enemies Destroyed and Cubes lines directly onto `VGAScreenSeg`, which
still contains the last fully composed gameplay frame.  Only after the user
continues does it call `fade_black(15)` and clear the screen.

The GBA implementation currently does the opposite: `build_assets.py`
creates every statistics frame from `np.zeros(...)`, and
`frontend_render_stats()` switches to that prebuilt Mode-4 frame.  This is
the direct cause of the black completion background.

Implemented correction: the last Mode-0 gameplay scene remains frozen and
a transparent BG3 text/tile overlay now supplies the staged lines, values
and cube graphics.  The existing timings, cube reveal sounds and
input-acceleration behavior remain the direct `JE_endLevelAni()` flow.
An mGBA capture confirmed that the authored level scene remains visible
behind the first completion-summary stage.

### Player death / GAME OVER

OpenTyrian continues composing the level while
`player.exploding_ticks` counts from 60 to zero, then draws
`miscText[21]` (GAME OVER) on that live frame.  It does not load a black
GAME OVER page.

Current GBA HEAD already intends to do the same and does not enter Mode 4.
An mGBA death-capture ROM was built with invincibility disabled.  The final
capture contains the authored level background plus GAME OVER, not black:

`build/death_current_capture.png`

Therefore the black death page is not reproducible on current HEAD.  The
fix will retain this Mode-0 path and add a regression assertion/capture so
the completion-screen rewrite cannot accidentally route death through the
old black Mode-4 asset.

### Player collision / special pickups

`mainint.c::JE_playerCollide()` has the following ordered single-player
branches, all of which must be preserved:

1. `evalue == 30000`: cash + purple-ball power progression.
2. `evalue > 32100`: special weapon pickup.
3. `evalue > 32000`, `>31000`, `>30000`: Arcade/two-player equipment.
   In normal one-player Full Game the source deliberately leaves most of
   these objects unconsumed; making them collectible would not be parity.
4. `evalue > 20000`: armor.
5. `evalue > 10000 && enemyAvail == 2`: secret/bonus route.
6. `scoreitem`: cube, front/rear power-up, orbiting asteroid killer
   (`evalue == -3`, source weapon 104), superbomb (`-4`), HOT DOG (`-5`),
   or ordinary cash.

The GBA collision branch ordering, secret portal, cubes, power-ups,
superbomb count and HOT DOG state are present.  The concrete missing
gameplay response is `evalue == -3`: it records acquisition but does not
create source player weapon 104.  Weapon 104 is an authored circular,
player-following shot (`circlesize=8`, `sx=sy=120`, graphic 98, duration
250); the current `PlayerShot` adapter also lacks the source
`shotComplicated` circular-motion fields.  This is now translated rather
than replaced with a custom pickup effect: the runtime reads weapon 104
from the mounted `tyrian.hdt`, creates it in the ordinary player-shot pool,
and advances the source `shotDev*`, `shotDir*` and `shotCirSize*` motion
state each tick.

## Secret/bonus feature audit

The stock engine has one generic mechanism for orbs, bubbles and revealed
terrain portals:

`evalue > 10000` + `enemyAvail == 2` ->
`bonusLevel=true`, `nextLevel=evalue-10000`, play song 30, and display
`Secret Level!` for 150 ticks.

The visual object and the authored destruction/event sequence come from
HDT/LVL data.  There is no separate hard-coded “warp bubble engine.”  The
GBA port already translates the generic trigger, source event data,
`edlevel == -1` reveal state, route override and 150-tick notice.  Once the
pickup matrix is completed, the same path applies to all episodes without
per-level GBA tables.

The quoted descriptions contain two items that are not stock hidden-level
mechanics:

- TYRIAN-X / SAVARA-Y / NEW DELI are not three levels automatically
  unlocked by completing Episode 3 on Hard in the checked OpenTyrian
  source.  The source does contain end-of-episode Super Arcade ship/code
  unlock progression; inventing a Hard-only level route would be wrong.
- “Zinglon's Ale” is not present in the checked OpenTyrian source or the
  mounted Tyrian data.  The source contains Zinglon's Revenge, Zinglon
  difficulty and Soul of Zinglon, but no Ale collection mini-game.

Destruct is real, but it is a separate approximately 2,000-line artillery
mini-game (`destruct.c`) entered by a title-screen keyboard code.  It is
not part of `JE_playerCollide()` or a secret-level portal.  The current GBA
project does not contain a Destruct port.  It should be handled as its own
controller-adapted source-port milestone rather than silently substituting
a small custom mini-game in this exit/pickup correction.

## Implemented scope and verification

1. Replace black Mode-4 statistics frames with the frozen gameplay
   Mode-0 scene plus transparent BG3 source-text/cube overlay.
2. Preserve and regression-test the existing live-background death flow.
3. Translate weapon-104 circular shot state and spawn it on `evalue == -3`.
4. Expand the focused collision regression for the formerly missing
   weapon-104 award and the generic portal route.
5. Keep all route decisions data-driven through ROMFS HDT/LVL/levelsN.dat.

The High-detail / Normal-speed full build passed the gameplay, death,
Jukebox, all-62-section ROMFS matrix, four-level Episode 1 campaign and
Episode 2 route smoke tests.  The matrix reports zero failed sections,
zero ROMFS failures and zero background approximations.  The generic
secret-route collision probe also passes.

Release ROM:

- `build/tyrian_gba_level1_pc_flow_mode4_romfs_v37_detail_high_speed_normal.gba`
- 14,517,752 bytes (43.2663% of the 32 MiB GBA limit)
- SHA-256:
  `b14f8f3cbbd5f51ea8602eb164b43a7204c15fbe9070bc952841ad034b0e25ea`
