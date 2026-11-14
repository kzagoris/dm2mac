# Click an enemy to attack with the party

Status: approved; implementation requested by the user.

## Intent

In Advanced mode, hovering over an attackable enemy shows a crossed-swords cursor. Clicking the enemy issues one party volley using each champion's last manually chosen melee or loaded ranged weapon action. This reduces repeated action-menu clicks while retaining combat timing and formation rules.

## Approaches

- Automatically choose a basic attack: less setup, but can replace the player's preferred weapon move.
- Repeat the last manually chosen weapon attack: predictable and respects player choices. **Selected by the user.**
- Include spells, charged item powers, and throwing: broader automation, with additional resource and targeting decisions. Outside this change.

## Behavior

- Remember the exact eligible action, hand, and weapon for each champion when the player chooses it in the normal action menu. Remember the most recent eligible choice per champion; do not fire both hands.
- Eligible actions are melee weapon actions and shooting with a loaded ranged weapon. Choosing a spell, utility action, or throw does not replace the remembered weapon attack.
- A click attempts one action per eligible champion in party order. Retain the engine's existing intrinsic repeats for actions that already have them; the click does not enable continuous party attacking.
- Skip champions who are dead, cooling down, blocked by formation, lack ammunition, or cannot legally use their remembered action against the target. Recheck eligibility before each action because an earlier attack may remove the enemy.
- Reuse normal combat execution, including damage rolls, experience, stamina, ammunition, charges required by an eligible weapon action, and delays. Do not directly apply damage or bypass cooldowns.
- Without a remembered action, select the first legal basic melee or loaded shooting action from the action hand, then the ready hand, including normal unarmed melee actions. Respect range and formation; never select utility, spell or throwing actions. Preserve a valid manual preference; use the basic fallback again after a weapon change or load. This extension was explicitly requested after manual testing.
- Keep the attack cursor over an eligible enemy during cooldown: bright swords when a champion is ready, grey swords when all eligible champions are recovering. Consume a waiting click without combat or falling through to other viewport interactions. Cursor eligibility checks must not mutate combat state or consume random numbers. This refinement was explicitly approved after the Fable review.
- Fill transparent gaps between the leftmost and rightmost creature pixels on each row, preserving the outside silhouette. Missiles and explosions do not erase target ownership; solid foreground pictures still do.
- Use the same visible-enemy hit detection for cursor and click. Respect occlusion and range. Melee continues to target the creature group directly ahead; ranged shots follow normal projectile collision rather than homing toward the pointer.
- Preserve held-object handling, creature trading/pushing, and other existing viewport interactions. Party attack requires an empty leader hand and an idle dungeon interaction state.
- Vanilla mode retains existing controls.
- Show the existing damage-star artwork and cyan digits over the monster when a melee hit or projectile impact resolves. Combine simultaneous damage on one target, expire the star after half a second, and retain its last screen anchor for a killing blow. Draw on the presented screen so feedback cannot erase target pixels. Preserve the existing action-area feedback and combat rules.

## Implementation choices proposed by Codex

Remembered preferences are session state for this first version, cleared on a new game or load; no save-format extension. Target detection must distinguish combat creatures from static interaction targets. Keep existing action-menu state intact while executing a volley.

The engine's existing `last_command` fields cannot supply this preference: they store only a command kind and are cleared during hand reload. A separate exact-action preference is needed.

## Verification

Cover exact-action recall, one action per champion, unavailable or changed weapons, cooldowns, formation, loaded and unloaded ranged weapons, enemy death during a volley, cursor/click agreement, held-object interaction precedence, and Vanilla behavior. Build and run the relevant combat/input tests; visually inspect the cursor in the native game when available.
