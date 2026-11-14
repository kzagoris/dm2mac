# Party Attack Implementation Plan

> Execute directly in this session using executing-plans.

**Goal:** Click visible enemies to repeat each champion's chosen weapon attack.

**Architecture:** Store session-only exact action preferences in champion_action.c. Use a pixel ownership mask built alongside viewport drawing for shared hover/click targeting. Dispatch through normal combat and charge handling while restoring the open action panel.

**Tech Stack:** C11, Python ctypes/pytest, Clang; WSL build.

**Spec:** ../specs/2026-11-14-party-attack-design.md (approved).

## Constraints and review focus

Advanced mode only; preserve object, push, trade and Vanilla interactions. No new dependencies or save format. Check stale weapons, action-menu state restoration, transparent/occluded sprites, RNG-free hover, and death during a volley.

## Tasks

- [x] Add regression tests in tests/port/test_party_attack.py for preference recall, readiness, formation, ammo, targeting, state restoration and input gating; run to establish failures.
- [x] Implement champ_reset_party_attacks, preference recording, champ_can_party_attack and champ_party_attack in src/engine/champion_action.c and declarations in include/champion.h. Reset on new game/load; invalidate on weapon removal.
- [x] Add view attack pixel recording and hit lookup in src/ui/view.c, view_things.c and headers. Preserve sprite transparency and foreground occlusion through existing blits.
- [x] Integrate shared input_party_attack_target and crossed-swords cursor into src/ui/input.c and include/input.h. Keep all existing interactions ahead of party attacking when appropriate.
- [x] Run focused tests, full suite, build and formatting checks; fix introduced failures and document controls in README.md. Full suite remains limited by baseline failures and missing private reference fixtures.

## Execution notes

- Remembered actions reset in save extension loading and new-game setup rather than general party screen initialization, which also runs during redraws.
- Ranged eligibility permits possible passage through grates; normal projectile simulation still rolls for collision.
- A read-only code review covered combat state and target masking; both reported edge cases were addressed.
- Fifteen feature tests pass, including a real mouse click, multiple champions, exact move recall, loaded ammunition consumption, longer ranged rays, and RNG-free grate checks. A wider focused run passed 29 tests with 7 private-fixture skips.
- Windows executable rebuilt as build/dm2.exe; prior binary preserved as build/dm2-before-party-attack.exe. Rendered game scene and cursor inspected in build/party-attack-cursor.png.
- Broad testing is not green: tool-test collection requires Homebrew, standards have existing options/spellbook/movement findings, and options/help crashes also reproduce on unchanged HEAD. Broad parallel runs were interrupted after reproducing the crash and stalled checks; they are not a complete suite result.

No commit or push is requested.

## Approved detection refinement

User approved the Fable review recommendations: fill holes inside each sprite row, retain ownership beneath transient projectiles/explosions, and show grey swords during cooldown while consuming waiting clicks. Implement through the existing rendering and combat paths without altering action costs or readiness. Add regression coverage of row spans, effects versus solid occluders, and a real waiting click with unchanged champion state/RNG. Then rebuild and launch Advanced mode for manual testing.
