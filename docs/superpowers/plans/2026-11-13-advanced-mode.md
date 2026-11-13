# Advanced Mode implementation plan

Implement the approved acceptance criteria in order, in this isolated worktree, without delegates.
Preserve Vanilla and normal rules. No autosave, remapping, push, or PR.

1. Q01: Add an explicit recovery chooser at Advanced startup and death, naming QUICK.DAT separately from slots. Keep errors in the chooser and retain the existing transactional in-game quick load. Test missing/corrupt recovery and routing.
2. Q06: Add per-champion recorded rune combinations and last attempted combination, chosen power and mana/flask feedback. Replay through normal rune entry and casting. Store a versioned optional save extension, with validation and old-save defaults. Test costs, invalid input, champion isolation, persistence and rule equivalence.
3. Q05: Forward focus/minimize events, clear all input capture/queues, latch a separate pause requiring a fresh deliberate resume. Preserve the underlying sleep/pause/dialog/overlay state. Test no time advance and no stale inputs.
4. Q03: Add visible Options/Help and a convenience explanation. Move display/save preferences out of cheats, preserve labeled cheats off by default, and give reopenable in-game control guidance. Test access and Vanilla isolation.
5. Build, run available tests, self-review, update manual/rules/save format and implementation report. Record missing proprietary data and unperformed playtests explicitly.

Validation: use the existing pytest/ctypes framework and synthetic state where game assets are absent. Make Linux library compilation and dependency detection portable so feasible tests can run. GUI/desktop tests require omabox; do not launch the user's game.
