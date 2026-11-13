# Automap Items Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan inline. Steps use checkbox syntax for tracking.

**Goal:** Make current loot visible and inspectable on explored automap squares, or all squares with Reveal map.

**Architecture:** Query loose items from the same level square owner and visibility gate used by drawing. Draw clipped yellow markers after terrain, then use the existing rows screen for a paginated item list. Advanced screen input distinguishes a short click from a drag.

**Tech Stack:** C11, existing game font/rows renderer, pytest/ctypes, Windows MinGW build.

**Spec:** `docs/superpowers/specs/2026-11-13-automap-items-design.md`.

## Global Constraints

- Show current floor and alcove items on explored squares; Reveal map also exposes unexplored squares.
- No new saved state or save format change.
- Preserve unrelated local work; do not restart the user's running game.

## Review Focus

- Unknown squares and synthetic edge walls must never expose unrelated objects.
- Items without graphics and piles must remain discoverable.
- Container contents and creature inventories must stay excluded.
- Drag release, movement back to the start, and focus loss must not become accidental selection.
- Paging must expose the entire pile without drawing outside the viewport.

## Task 1: Item queries and markers

**Files:** `src/ui/automap.c`, `src/ui/include/automap.h`, `tests/port/test_automap_items.py`.

**Interfaces:** `automap_items(Location level_at, uint16_t *items, int capacity)` returns the total loose item count, filling up to capacity references, through the automap visibility and ownership rules.

- [x] Write and run failing tests for explored/unexplored/reveal behavior, piles, missing graphics, exclusions, and current pickup/drop state.
- [x] Implement the shared query and clipped marker/count drawing without changing minion map behavior.
- [x] Run the item tests and existing automap tests (private start.DAT cases skip).

## Task 2: Inspection and input

**Files:** `src/ui/automap.c`, `src/ui/include/automap.h`, `src/ui/advanced.c`, `src/ui/include/advanced.h`, `tests/port/test_automap_items.py`.

**Interfaces:** Automap selection maps viewport coordinates back through the drawing origin; item list drawing and input stay in the automap module. Advanced owns click-versus-drag state.

- [x] Write and run failing tests for click selection, grouping, pagination, Escape/right click, drag release, and reset.
- [x] Implement list drawing, paging, return controls, and drag suppression.
- [x] Run focused regression tests and inspect a rendered frame with real game graphics.

## Task 3: Delivery checks and documentation

**Files:** `docs/rules/advanced-mode.md`, `docs/manual.md`, `docs/manual-test-checklist.md`.

- [x] Document markers, counts, inspection, paging, and visibility.
- [x] Run formatting, lint/standard checks, the available full test suite, and the Windows build.
- [x] Review the scoped diff and report verification limits. Leave changes uncommitted unless requested.

## Execution record

The user approved the proposal and explicitly requested implementation. Continue inline without another approval round. The checkout contains unrelated local graphics/filter changes, so implement only in the files above and preserve those edits.

Verification: all 15 item tests and three existing focus/input tests pass with real dungeon/graphics data. Windows builds successfully; changed C files pass clang-format and the new test file passes ruff. Rendered map/list PNGs were visually inspected. A read-only independent review found no correctness issues. The full suite has missing private fixtures, unavailable Linux program/research tools, mismatched movie assets, Options/Help crashes, and pre-existing style failures; detailed results are kept under build/automap-full-suite.log. No game process was restarted, no save format changed, and no commit/push was made by this task.
