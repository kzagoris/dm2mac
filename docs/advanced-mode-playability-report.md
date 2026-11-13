# Advanced mode: playability and quality-of-life audit

## Implementation update — 2026-11-13

Implemented on `feat/advanced-mode-qol`, in the requested order Q01 → Q06 → Q05 → Q03:

| ID | Delivered behavior | Validation limit |
|---|---|---|
| Q01 | Explicit Advanced startup/death chooser: QUICK.DAT, slot saves, startup new game, exit. Missing/corrupt quick saves return to the chooser without writing backups or changing save files. | Headless chooser/error/file-safety tests pass; actual quick-only recovery test requires missing game assets. |
| Q06 | Four player-recorded favorites plus last attempt per champion, selectable power, mana/flask feedback, ordinary rune entry and ordinary cast path. Versioned optional FAVS save extension; old saves start empty. | Synthetic mana/guard/failure/persistence tests pass. Three full manual-versus-favorite saved-state comparisons are present but require missing assets. |
| Q05 | Advanced focus-loss/minimize latch, input/capture clearing, fresh Return resume, held-repeat suppression, underlying pause/sleep/dialog/overlay preserved. | Headless clock/input/state regressions pass; SDL focus/minimize events compile but have not been playtested with the game. |
| Q03 | Visible clickable F1 Options/Help, mode explanation, reopenable controls, ordinary filter/save actions separated from labeled cheats. | Headless input/context/screen-restoration tests pass. Actual font/layout and pointer usability need an asset-backed playtest. |

The original audit below remains a baseline, not a description of these updated paths. The original checkout's
untracked report was left untouched. See the updated player manual, Advanced rules and save-format documentation.

Validation on Linux: `make` builds the executable and test library with warnings treated as errors; Ruff passes.
The full suite was attempted inside omabox. It exposed pre-existing environment blockers: the AI table comparison
reads missing `game/SKULL.EXE` without skipping, and Ghidra tests try to execute missing Homebrew during collection.
Most asset-backed tests skip because the proprietary game and saves are absent. The final feasible run (`pytest -n 4 -q --ignore=tests/tools/test_ghidra.py -k 'not test_tables_match_the_original'`,
inside omabox) passed **251 tests**, with **541 skipped** and one harmless read-only pytest-cache warning.
The focused feature/doc check passed 16 tests and skipped four asset-backed cases. Author self-review was used,
following the explicit no-delegates instruction; no supported-platform gameplay, visual QA, or manual newcomer playtest
is claimed. No autosave, remapping, push, or PR was added.


Date: 13 November 2026
Repository: DM2Mac, Dungeon Master II: The Legend of Skullkeep
Audited revision: `561b93589981f76a6957625c4c0666885cfbd456`

## Assessment

Advanced mode is a useful modernization of Dungeon Master II, but it still asks players to understand many original interface conventions. Its strongest additions are modern movement keys, an automap, quick save/load, and small input improvements. The largest remaining opportunities are reliable recovery after death, discoverable controls, less repetitive spell entry, and better information about items and combat readiness.

The recommended direction is to preserve the dungeon, puzzles, resource management, and combat rules while reducing interface effort. Offer changes to difficulty and timing separately as optional assists. A player should not need to enable cheats to get a convenient interface.

This is a source-code and documentation audit, not a completed playtest. **Implemented** below means that implementation was inspected; it does not mean that the feature was exercised in a running game. **Partial** means a related feature exists but does not provide the whole proposed experience. **Not found** means no implementation was found in the inspected paths and repository searches; it is not proof about other branches or releases.

## Scope and verification limits

- Read the player manual, advanced-mode rules, development instructions, open TODOs, relevant implementation, and existing test cases.
- Traced input routing, quick-save recovery, map controls, cheat spell casting, rendering preferences, and the platform event loop.
- Inspected automated test coverage; did not execute gameplay tests. This Linux checkout has no `build/` artifacts or `game/SKULL.EXE`, and `brew` is unavailable. The pytest configuration invokes Homebrew and the Makefile uses a macOS dynamic-library link command. These are local verification limitations, not evidence that gameplay tests fail.
- No interactive gameplay, controller, readability, or accessibility usability testing was performed. No gameplay code was changed.
- Priorities and effort estimates are engineering judgments, not measured player outcomes. Small means localized UI work; medium spans several modules; large needs new interaction or persistence design.

The general recommendation for configurable controls follows [Xbox Accessibility Guideline 107](https://learn.microsoft.com/en-my/gaming/accessibility/xbox-accessibility-guidelines/107). Broader considerations such as simpler inputs and making accessibility features discoverable are supported by [Game Accessibility Guidelines](https://gameaccessibilityguidelines.com/full-list/). The feature status findings below come from this repository, not those external sources.

## What is already implemented

| Feature | Status and actual behavior | Evidence |
|---|---|---|
| Modern movement | Implemented: WASD strafes/moves; Q/E turns. Context routing preserves text entry. Bindings are fixed. | [advanced.c](../src/ui/advanced.c), `advanced_movement_key`; [input_tree.c](../src/ui/input_tree.c); [mode tests](../tests/port/test_modes.py) |
| Less movement overshoot | Implemented: Advanced accepts held-key repeats only when input is idle. | [main.c](../src/platform/main.c), `pump_events`; [mode tests](../tests/port/test_modes.py), `test_key_repeat_waits_for_idle_input` |
| Quick save/load | Implemented: F5/F9, one `QUICK.DAT`, success/error messages, protection against a failed save and attempted restoration after a damaged load. | [advanced.c](../src/ui/advanced.c), `advanced_quick_save`, `advanced_quick_load`; [mode tests](../tests/port/test_modes.py) |
| Explored automap | Implemented: current level, seen squares, facing, coordinates, mouse dragging, doors/pits/stairs/teleporters and selected other features. Opening it pauses time. | [automap.c](../src/ui/automap.c), [advanced.c](../src/ui/advanced.c), [play.c](../src/ui/play.c), [map tests](../tests/port/test_automap_squares.py) |
| Persistent exploration | Implemented: slot saves and quick saves retain seen squares and cheat state. Reveal-map does not permanently mark everything seen. | [advanced rules](rules/advanced-mode.md), [save format](formats/save.md), [save.c](../src/engine/save.c) |
| Inventory conveniences | Implemented: status-box click stores a held item or opens inventory with an empty hand; Tab closes inventory in eligible contexts. Original keys 1–4 already open champion inventories. | [manual](manual.md), [input.c](../src/ui/input.c), [input_tree.c](../src/ui/input_tree.c), [mode tests](../tests/port/test_modes.py) |
| Back/cancel interactions | Implemented in specific screens: right-click closes map/cheats or backs out of selected save/quit dialogs; spell pages return to cheats. Not a universal interface rule. | [advanced rules](rules/advanced-mode.md), [advanced.c](../src/ui/advanced.c), [cheats.c](../src/ui/cheats.c) |
| Pause | Implemented: original Esc pause; map and cheat overlays also stop time. This is not automatic pause on focus loss or a general inventory-pause feature. | [input.c](../src/ui/input.c), `pause_game`; [play.c](../src/ui/play.c), `play_tick` |
| Display improvements | Implemented: resizable window and NONE/SCALE2X/SCALE3X filters; filter preference persists. Filters do not provide independent text or hit-target scaling. | [platform.c](../src/platform/platform.c), [filter.c](../src/ui/filter.c), [settings.h](../src/data/include/settings.h), [settings tests](../tests/port/test_settings.py) |
| Combat feedback | Implemented: successful-hit damage display lasts one extra game step; hand/spell icons already track busy states. No explicit remaining-time display was found. | [action_area.c](../src/ui/action_area.c), `show_hand_icons`, `show_party_hands`; [mode tests](../tests/port/test_modes.py) |
| Smooth projectiles | Implemented: visual interpolation in a stationary dungeon view; game simulation remains unchanged. | [play.c](../src/ui/play.c), [view.c](../src/ui/view.c), [projectile tests](../tests/port/test_projectile_flight.py) |
| Reliable antidotes | Implemented: Advanced cures poison reliably, unlike the retained original behavior in Vanilla. | [advanced rules](rules/advanced-mode.md), [poison tests](../tests/port/test_poison.py) |
| Six cheats | Implemented: full light, immortality, endless food/water, one-hit kills, wall walking, map reveal. New games start with cheats off. | [cheats.c](../src/ui/cheats.c), [advanced rules](rules/advanced-mode.md), [cheat tests](../tests/port/test_cheats.py) |
| Spell browser | Implemented **as a cheat**, not a normal spellbook: casts at maximum power for the current spell table, without mana cost, failure roll, or experience; flask requirements and action delay remain. | [cheats.c](../src/ui/cheats.c), `cast_spell`; [magic.c](../src/engine/magic.c), `magic_cast_cheat_spell`; [spell-screen tests](../tests/port/test_cheat_screen.py) |

The existing turn-based mode must not be counted as a player-facing tactical mode. It is an automation/control-socket mode, and its window loop deliberately ignores ordinary game keyboard and mouse input. See [main.c](../src/platform/main.c), `pump_quit_events` and `run_turnbased`, and [turn-based documentation](llm/turn-based.md).

## Recommended improvements and implementation status

### P1: first modernization release

These address lost progress, basic discoverability, and frequent interaction costs. Their order is a suggested sequence, not a requirement to ship them as one release.

| ID | Proposal and player benefit | Current status / evidence | Effort and acceptance criteria |
|---|---|---|---|
| Q01 | Integrate quick saves into death recovery and a visible load/continue flow. Players who use F5 should be able to recover after dying without knowing a separate slot-save convention. | **Partial.** F5/F9 work, but quick saves are excluded from slot menus and death restart. Explicit in [advanced rules](rules/advanced-mode.md#quick-save-and-quick-load). | **Medium.** With only `QUICK.DAT` present, death and startup offer an explicit quick-save load. Missing/corrupt files produce a clear explanation and preserve other saves. Show which save is being loaded. |
| Q02 | Add rotating recovery saves and optional autosave. Preserve several generations so an accidental bad save does not erase the only useful checkpoint. | **Not found.** `advanced_quick_save` deletes its backup after success; that backup is error protection, not history. | **Medium–large.** Keep at least three independent recovery generations. Autosave at defined stable boundaries, with no save taken halfway through a drag, modal operation, or load. Keep manual slots separate. Test disk-full, interrupted-write, death-loop, and old-save compatibility cases. |
| Q03 | Add a clearly named Options/Help screen and a short first-run explanation. Explain that Advanced means convenience additions, not increased difficulty. Move normal preferences out of CHEATS. | **Partial.** A filter selector and save actions exist inside CHEATS; the initial mode screen only names VANILLA/ADVANCED. See [cheats.c](../src/ui/cheats.c), [mode_screen.c](../src/ui/mode_screen.c). | **Medium.** Explain movement, inventory, inspect, spell entry, save, and pause in-game. Options are reachable without opening cheats. Cheats remain separately labeled and off by default. Help can be reopened. |
| Q04 | Support configurable keyboard and mouse bindings, including alternatives to function keys and prolonged holds. | **Not found.** Fixed SDL-to-PC mappings and `advanced_movement_key`; settings currently contain only the filter. | **Medium–large.** Rebind movement, turn, map, save/load, inventory, inspect, and pause. Detect conflicts, restore defaults, persist bindings, and update help labels. Name entry must still type normally. Test alternate keyboard layouts and a one-handed layout. |
| Q05 | Pause on focus loss, clear held input, and require deliberate resume. Prevent interruptions from costing health or causing unexpected movement. | **Not found.** [platform.c](../src/platform/platform.c), `plat_poll`, handles quit, key-down and mouse events but does not forward focus loss; the real-time loop keeps ticking. This is a code finding, not an observed playtest incident. | **Medium.** Switching windows or minimizing stops simulation in eligible gameplay states. Returning does not auto-resume or replay movement. Existing pause, sleep, dialogs, and overlay states remain coherent. |
| Q06 | Add spell favorites and “repeat last spell” using normal rules. Reduce repeated rune entry while retaining meaningful caster/power choices. | **Partial.** Rune casting and the cheat spell list exist; normal favorites were not found. See [magic.c](../src/engine/magic.c), [cheats.c](../src/ui/cheats.c). | **Medium–large.** Store favorites per champion; choose power; show required mana and flask status. Route through normal rune-entry/casting behavior, preserving mana charging, skill/failure, experience, and delays. Merely calling `magic_cast_spell` is insufficient if it skips rune-entry costs. Default to player-recorded combinations to avoid exposing every solution. |

### P2: reduce recurring friction

| ID | Proposal and player benefit | Current status / evidence | Effort and acceptance criteria |
|---|---|---|---|
| Q07 | Enrich the explored map with player notes, pins, a legend, visited-level browsing, zoom, and keyboard panning. Make returning after several days easier. | **Partial.** Current-level explored map and mouse drag exist; the inspected map event handler has close/drag interactions, not these tools. | **Large as a group; split into increments.** Persist notes with the correct save/exploration state. Permit browsing only previously visited levels by default. A recenter command and keyboard navigation work. No unknown rooms, items, or puzzle solutions are revealed. |
| Q08 | Add click-to-toggle item inspection and readable item help, with optional equipment comparison. Reduce reliance on holding the eye icon and memorizing item behavior. | **Partial.** Original eye-panel inspection exists; hold behavior is documented in [manual checklist B16a](manual-test-checklist.md). Modern hover/toggle comparison was not found. | **Medium–large.** Toggle inspect without a sustained hold; Escape closes it. Show known name, weight, usage, and available equipment information. Unknown/hidden properties remain hidden unless explicitly enabled. Avoid implying that a single damage number captures all weapon behavior. |
| Q09 | Add explicit transfer commands and clearer carrying-load feedback. Reduce inventory shuffling without changing capacities. | **Partial.** Status-box deposit and numbered inventory shortcuts exist. A related item-in-hand/name-click change is already a **proposal**, not completed work, in [TODO](todo.md). | **Medium.** Transfer selected item to a named champion or open container, with full/incompatible reasons. Preserve slot restrictions, weight, identity, and contents. Do not silently auto-equip or redistribute possessions. |
| Q10 | Show clearer cooldown/readiness and optional per-hand repeat-action shortcuts. Reduce searching and repetitive clicks in combat. | **Partial.** Busy hand/spell icons and extended damage feedback exist in [action_area.c](../src/ui/action_area.c). Explicit progress and a modern repeat-action binding were not found. | **Medium.** Show a readable ready/busy indication, optionally remaining time. A shortcut executes one valid selected action; changing equipment invalidates incompatible selections. Preserve action delay, stamina, targeting, and failure rules. Automatic attacks should be a separate assist. |
| Q11 | Add configurable readable text, larger pointer/targets, and status symbols that do not depend on color alone. | **Partial.** Window scaling and image filters exist; neither is independent UI scaling. | **Large for full UI scaling.** Start with new help/options/map overlays and a larger cursor. Check readability at common laptop sizes and increased display scaling. Verify pointer alignment at every supported window size and that high-contrast indicators identify states without color. |
| Q12 | Add optional textual alerts and a readable message history. Make poisoning, hunger/thirst, load problems, and action failures easier to understand. | **Partial.** Original bars, messages, and audio exist; a dedicated persistent history and configurable modern alert layer were not found. See [text.c](../src/ui/text.c), [ui.c](../src/ui/ui.c). | **Medium.** Explain condition and affected champion in text. Rate-limit repeated warnings. Allow review while paused. Visual sound cues should represent sounds actually audible to the party, without exposing unseen creatures through walls. |
| Q13 | Preserve audio/display/control preferences independently of saved-game state. Loading an old save should not unexpectedly change the player's chosen volume. | **Partial.** Filter is a global preference; quick load explicitly restores saved music/sound volumes. [settings.h](../src/data/include/settings.h), [advanced.c](../src/ui/advanced.c), `test_quick_load_sets_the_loaded_volumes`. | **Medium.** Define global preferences versus adventure state; migrate conservatively. Advanced honors current global audio settings after load. Preserve Vanilla behavior and old-save readability. Clearly report preference-write errors. |
| Q14 | Offer a configurable quick-load confirmation showing the save identity/time. Protect against accidental F9 while keeping expert use fast. | **Not found.** F9 loads immediately by design. | **Small–medium.** Confirmation defaults on for newcomers and can be disabled. Cancel leaves state unchanged. Show save age only if reliable metadata exists. Missing/corrupt saves remain recoverable without discarding the current game. |

### P3: broader accessibility and optional assistance

| ID | Proposal and player benefit | Current status / evidence | Effort and acceptance criteria |
|---|---|---|---|
| Q15 | Add full controller navigation and keyboard alternatives for mouse-only actions. | **Not found as a complete input path.** No gamepad event handling was found in the SDL adapter; keyboard movement alone does not make the inventory/spell interface controller accessible. | **Large.** Complete recruit, inventory, casting, shopping, saves, dialogs, and map interactions without a mouse. Include visible focus, remapping, and device-disconnection behavior. Test on actual target devices before advertising support. |
| Q16 | Offer configurable slower simulation and optional pause during inventory/spell planning. Help players who need more decision time. | **Partial foundations only.** Esc/map pause and separate automation turn-based mode exist; a player-facing tactical mode was not found. | **Large.** Make assists opt-in and visible. Slow all simulation systems consistently, not only enemies. Pause must freeze hazards, hunger, projectiles, cooldowns, and time consistently. Explain the effect on combat difficulty. |
| Q17 | Add a journal of player notes and previously read clues, with optional progressive hints. Reduce dependence on external notes after a long break. | **Not found.** No player journal UI/persistence was found in inspected UI/settings/save paths. | **Large.** Record only encountered information. Do not infer objectives or reveal puzzle answers by default. Let players request hints in stages. Persist journal state with the appropriate adventure. |
| Q18 | Separate accessibility assists from unrestricted cheats and add gentler difficulty choices. | **Partial.** Six binary cheats exist, including endless food/water and immortality; a graduated assist configuration was not found. | **Medium–large.** Clearly explain effects; retain normal rules by default. Options such as slower resource depletion need explicit balance design. Make active assists visible without a stigmatizing label; keep wall walking/one-hit kills clearly distinguished. |

## Important distinctions and risks

1. **Quick-save reliability is not save discoverability.** Existing damaged-file recovery is valuable, but does not solve quick-save access after death. Keep its protections when changing the load UI.
2. **A cheat spell list is not ordinary spell assistance.** Reusing its free-cast path for favorites would change progression, resource use, and difficulty. The normal mana charge may occur during rune entry, so review the entire path.
3. **Inventory and planning pauses change combat.** Treat them as optional assists rather than quietly changing Advanced for everyone.
4. **Map information affects exploration.** The current map already marks some fake walls and level boundaries. New notes/legends can explain existing information, but unrestricted objective markers or auto-routing could remove puzzle decisions.
5. **Original features must not be proposed as wholly missing.** Champion inventory keys, pause, item inspection, sound/music volume, and busy action icons already exist. Improve their discoverability and interaction rather than duplicating them.
6. **Do not silently sort or stack everything.** Slot legality, container contents, item identity, charges, weight, and player placement can matter. Begin with explicit transfers; evaluate sorting and stacking separately.
7. **Spell names need verification.** The project's [TODO](todo.md) already flags several port-guessed names. A player-facing reference should mark uncertain names until checked against authoritative game material.
8. **Keep compatibility deliberate.** Store interface preferences outside adventure state. New notes, save history, and discovery state need a defined compatibility/versioning strategy; do not overload the existing filter-only settings format without extending it safely.

## Suggested delivery sequence

1. **Protect progress and explain the interface:** Q01, Q03, Q05, Q14. Deliver a usable recovery/Options/Help experience first.
2. **Build reusable input and preference support:** Q04 and Q13, then Q02. Autosave needs its own careful state-safety and persistence work.
3. **Reduce frequent actions:** Q06, Q08, Q09, Q10. Preserve normal rules and offer concise contextual feedback.
4. **Improve return visits and readability:** split Q07 into legend/keyboard navigation, then notes/level browsing; implement Q11 and Q12 incrementally.
5. **Expand audiences:** Q15–Q18, validated with players who need those controls or assists.

Avoid starting with new combat systems, unrestricted auto-loot, full automatic puzzle navigation, or graphic replacement. Those are larger changes with less direct connection to the interface friction identified here.

## Validation plan for future implementation

Existing tests provide useful foundations, but their presence does not certify proposed features:

| Area | Existing evidence to retain | Additional verification needed |
|---|---|---|
| Input and contexts | [test_modes.py](../tests/port/test_modes.py): movement, text contexts, quick-key restrictions, inventory shortcuts, repeats | Rebinding conflicts, help labels, lost focus, held buttons, one-handed use; no text-entry commands leaking into gameplay |
| Save safety | [test_modes.py](../tests/port/test_modes.py): missing/damaged quick saves, failed writes/restores, alternate save directory | Death with only a quick save, startup recovery, history rotation, interrupted writes, accidental-load cancellation |
| Map | [test_automap_squares.py](../tests/port/test_automap_squares.py), [advanced rules](rules/advanced-mode.md) | Notes across save/load/new game, overlapping maps, visited-level restrictions, zoom/pointer alignment, keyboard navigation |
| Spells | [test_cheat_screen.py](../tests/port/test_cheat_screen.py), [test_magic.py](../tests/differential/test_magic.py) | Favorites compared with manual rune entry for mana, results, experience, failure, flask conditions, and cooldown |
| Preferences and visuals | [test_settings.py](../tests/port/test_settings.py), [test_projectile_flight.py](../tests/port/test_projectile_flight.py) | Preference migration, audio after load, readable layouts, high display scaling, saved setting errors |

For supported-platform playtests, ask a newcomer to recruit, equip, inspect an item, cast a normal healing spell, save, die, recover, and return to a previously visited shop. Ask a returning player to resume after several days without external notes. Record where they need help, action counts, accidental commands, and recovery failures before and after changes. These measurements are proposed, not collected results.

The project's [manual checklist](manual-test-checklist.md) contains remaining unchecked cases, and [TODO](todo.md) identifies sound, long-play, and other verification gaps. Carry those forward rather than implying this audit closes them.

## Recommendation

Prioritize **quick-save recovery, discoverable Options/Help, remappable controls, focus-loss pause, and normal-rule spell favorites**. These directly reduce lost progress, learning effort, and repetitive input. Keep the existing Advanced improvements, and make changes to difficulty or simulation timing explicit choices.
