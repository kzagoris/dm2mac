# dm2mac feature tweets and phase movies

This page lists 19 standalone posts about the fork. The numbers in parentheses refer to the original feature inventory. Each tweet describes the feature. Its movie is a short MOV built from stills of the game, one still per phase of the feature, and each still stays on screen long enough to read. The tweet says what the feature does, and the movie shows each step in order.

The comparison is fork `54093def` against peterkmet/dm2mac `eab25f18`. Upstream already has manual rune casting, a free-cast cheat catalog, an explored-area automap, quick saves and Scale2x/3x filters. Credit the upstream port in the series introduction. Capture in Advanced mode. The linked commits give the implementation behind each post.

## Movie format

- **Stills.** Take one game screenshot per phase. The control socket `screenshot` gives the 320 × 200 screen with the cursor ([control-socket.md](llm/control-socket.md#screenshot)). Use window captures only where the socket screenshot can't show the feature (post 19).
- **Same framing.** Keep the party position and camera the same across the phases of a post, so the viewer sees only what changed.
- **Scale.** Scale each still 4× with nearest neighbour to 1280 × 800 and put it on a 1280 × 960 canvas. The 160-pixel strip below the game holds the caption.
- **Caption.** Show the phase number and a short label, for example `2/3 · One click`. Show any key the player presses, for example `Alt+C, 1`, because a still can't show a key press. The first still also carries **dm2mac · Advanced**.
- **Timing.** Hold each phase for 2.5 seconds and the last one for 4 seconds. Use a 0.3-second crossfade between phases. Three phases make a movie of about 8 seconds and four make one of about 11 seconds.
- **Export.** Export H.264 at 30 fps, `yuv420p`, without audio, to `captures/feature-tweets/NN-short-title.mov`. Keep the stills in `captures/feature-tweets/NN-short-title/`.

## 1. Attack All (1, 2, 3, 4)

> Attack All in dm2mac: click a monster and every champion who can reach it attacks. Each repeats their last attack, or picks a basic one. The swords on the cursor go grey while the party recovers, and a number over the monster shows the damage that landed.

**Phases**
1. *Party ready* — Two fighters ready, one with a melee action already used once. The pointer is over the monster and shows bright swords.
2. *One click* — The hit lands, with the damage number over the monster.
3. *Recovery* — Both attack hands recover, and the cursor swords are grey.

**Source** — [ec2271fb](https://github.com/kzagoris/dm2mac/commit/ec2271fb), [e43ee970](https://github.com/kzagoris/dm2mac/commit/e43ee970)

## 2. Highlight reachable objects (5)

> Some wall decorations in Dungeon Master II are switches; others are just stonework. Hold Ctrl in dm2mac and everything you can reach gets an outline: cyan for a lever, yellow for a keyhole, white for an item you can pick up.

**Phases**
1. *Without Ctrl* — A view with a lever and a floor item, unmarked.
2. *Ctrl held* — The same view: the lever is outlined in cyan and the item in white.
3. *Keyhole* — A view of a keyhole with Ctrl held, outlined in yellow.

**Source** — [6d772354](https://github.com/kzagoris/dm2mac/commit/6d772354)

## 3. Follow the runes (6, 9)

> dm2mac helps you cast Dungeon Master II spells by hand. After each rune you enter, it highlights the runes that can still lead to a spell in your list. Finish the combination and the spell's name appears while the runes are still in the tray.

**Phases**
1. *Power rune* — A power rune entered, with the possible next runes highlighted.
2. *Next rune* — A highlighted rune chosen; fewer choices are highlighted now.
3. *Spell found* — The final rune entered, with the spell name and the full tray visible.

**Source** — [994df3e1](https://github.com/kzagoris/dm2mac/commit/994df3e1)

## 4. Cast by name from the spell list (7)

> Press C in dm2mac to cast a spell by name: pick the spell, the champion and the power. It isn't a cheat. The champion pays the mana and makes the usual skill check, so a Fireball too difficult to cast by hand is still too difficult from the list.

**Phases**
1. *Empty tray* — A skilled caster with no runes entered.
2. *Press C* — The spell list with Fireball selected. Caption shows `C` and `Space`.
3. *Cast* — The Fireball in flight down the corridor.

**Source** — [994df3e1](https://github.com/kzagoris/dm2mac/commit/994df3e1)

## 5. Known spells or full catalog (8)

> The dm2mac spell list has two modes. Known spells fills up as you cast, so you still discover the combinations yourself. All spells shows the whole catalog if you'd rather look one up. The same setting decides which hints you get when entering runes by hand.

**Phases**
1. *Known spells* — A partly learned list.
2. *Setting* — SPELLS SHOWN changed to All spells.
3. *All spells* — The list on the same save, with the additional names.

**Source** — [994df3e1](https://github.com/kzagoris/dm2mac/commit/994df3e1)

## 6. Rune syllables on hover (10)

> Spell recipes in Dungeon Master II are written as syllables like FUL, but the rune buttons only show symbols. In dm2mac, rest the pointer on a rune to read its syllable. Move away and the symbol comes back.

**Phases**
1. *Symbols* — The rune panel with the pointer away from it.
2. *Hover* — The pointer on FUL, with its syllable shown.
3. *Next rune* — The pointer on a second rune, with its syllable shown.

**Source** — [73820e51](https://github.com/kzagoris/dm2mac/commit/73820e51)

## 7. Save and cast a favorite (11, 12)

> Each champion in dm2mac keeps four favorite spells. With a spell in the rune tray, hold Alt and press C, then 1, to save it and its power in the first slot. Later, with an empty tray, the same keys cast it.

**Phases**
1. *Save* — Light in the tray and the save confirmation. Caption shows `Alt+C, Alt+1`.
2. *Empty tray* — The same caster later, ready with no runes entered.
3. *Cast* — The corridor lit up. Caption shows `Alt+C, Alt+1`.

**Source** — [0376dfa5](https://github.com/kzagoris/dm2mac/commit/0376dfa5), [7cb28f4f](https://github.com/kzagoris/dm2mac/commit/7cb28f4f)

## 8. Why a spell failed (14, 15, 16, 17, 18)

> A fizzle in Dungeon Master II tells you very little. dm2mac explains the failures you can fix. If mana is short, it shows the cost against what you have and the spell list suggests a power you can afford. If a potion needs an empty flask, it names the champion who must hold one.

**Phases**
1. *Not enough mana* — A grey rune clicked, with the cost and the available mana in the message.
2. *Suggestion* — The spell list with the lower, affordable power suggested.
3. *No flask* — A failed potion with the empty-flask message. Keep the message in the same crop as phase 1.

**Source** — [54093def](https://github.com/kzagoris/dm2mac/commit/54093def)

## 9. Ordinary casting and cheat casting (19)

> dm2mac has two spell lists. X is the upstream port's menu for casting spells for free. C is the new one: the same spells, but the champion pays the mana and makes the usual skill check, just as when entering the runes by hand.

**Phases**
1. *C* — Fireball in the C list, with its heading and mana preview.
2. *X* — The same spell in the X list, under the free-cast heading.

**Source** — [4d2130f0](https://github.com/kzagoris/dm2mac/commit/4d2130f0)

## 10. Write and read map notes (20)

> You find a locked door long before its key. The dm2mac automap takes notes for that: double-click a square, type something like NEEDS KEY and press Enter. A small cyan pin marks the square, and clicking it shows the note.

**Phases**
1. *Write* — NEEDS KEY typed on a mapped doorway.
2. *Pin* — The map after Enter, with the cyan pin on the doorway.
3. *Read* — The pin clicked, with the pin and the note visible together.

**Source** — [dc0f1939](https://github.com/kzagoris/dm2mac/commit/dc0f1939)

## 11. See what's on the floor (21, 22)

> The dm2mac automap remembers what you left behind. A yellow plus marks explored squares with items on the floor. Click it to see what's there and how many. Pick up the last item and the plus disappears.

**Phases**
1. *Plus* — The automap with a yellow plus on a square.
2. *Contents* — The plus clicked: two matching items and their quantity.
3. *Picked up* — The same square on the map after the last item was taken, with no plus.

**Source** — [d3340501](https://github.com/kzagoris/dm2mac/commit/d3340501)

## 12. Explore the automap (23, 24, 25, 26)

> Explore the dm2mac automap without moving the party. Q and E switch between floors you've visited, the arrow keys pan and the mouse wheel zooms in where you point. Space takes you back to the party, and Help has a legend for the map symbols.

**Phases**
1. *1×* — The map at 1× with the pointer over a junction.
2. *3×* — The same junction zoomed to 3×. Caption shows `Wheel`.
3. *Another floor* — A different visited floor. Caption shows `Q`.
4. *Legend* — The Map Legend in Help, with a symbol from phase 1 in view.

**Source** — [af358787](https://github.com/kzagoris/dm2mac/commit/af358787), [a3667b4a](https://github.com/kzagoris/dm2mac/commit/a3667b4a), [3e475634](https://github.com/kzagoris/dm2mac/commit/3e475634)

## 13. Compare gear and pin skills (28, 29, 30)

> Pick up armour with the inventory open in dm2mac and a panel shows what wearing it would change: the difference from the piece you have on, its weight and any magic bonuses. With nothing in hand, the same panel can keep the champion's skills pinned.

**Phases**
1. *Equipped* — The inventory with a comparable piece worn.
2. *Compare* — A bonus-bearing item held, with the colored difference and the bonus line.
3. *Skills pinned* — The item put down, with the skills pinned in the panel and the pointer away.

**Source** — [15438992](https://github.com/kzagoris/dm2mac/commit/15438992), [707054ca](https://github.com/kzagoris/dm2mac/commit/707054ca)

## 14. Inventory via champion name (31)

> A small inventory change in dm2mac: while holding an item, click another champion's name. Their pack opens and the item stays on your pointer, so you can put it exactly where you want.

**Phases**
1. *Holding* — A recognizable item on the pointer, over another champion's name.
2. *Their pack* — That champion's pack open, the item still held over an empty slot.

**Source** — [24baa407](https://github.com/kzagoris/dm2mac/commit/24baa407)

## 15. Keyboard shortcuts and Esc (32, 33, 35, 36, 40)

> dm2mac gives Esc a job on the main screens. From the slot picker it goes back to the disk menu, then back to play; in the dungeon it pauses and opens Help. Ctrl+S opens the disk menu, Ctrl+R rests the party and Ctrl+Q starts the quit sequence.

**Phases**
1. *Disk menu* — The disk menu. Caption shows `Ctrl+S`.
2. *Slot picker* — The LOAD slot picker. Caption shows `Esc → disk menu → dungeon`.
3. *Help* — The paused game with Help open. Caption shows `Esc`.
4. *Rest* — The sleeping party. Caption shows `Ctrl+R`.

**Source** — [87bc89ef](https://github.com/kzagoris/dm2mac/commit/87bc89ef), [0376dfa5](https://github.com/kzagoris/dm2mac/commit/0376dfa5), [32dcd433](https://github.com/kzagoris/dm2mac/commit/32dcd433), [e8d19541](https://github.com/kzagoris/dm2mac/commit/e8d19541), [74d0b2af](https://github.com/kzagoris/dm2mac/commit/74d0b2af)

## 16. Classic stepping (37)

> Prefer the way the first Dungeon Master moves? Turn Smooth stepping off in dm2mac and the party steps directly from one square to the next. Collision checks and the usual recovery time between steps still apply.

**Phases**
1. *Smooth on* — A frame in the middle of a step toward a centred doorway.
2. *Setting* — Options with Smooth stepping: Off.
3. *Smooth off* — The same step from the same square: the party is already on the next square.

**Source** — [d3340501](https://github.com/kzagoris/dm2mac/commit/d3340501)

## 17. No bump damage (39)

> Walking into a wall costs a champion one health point in Dungeon Master II. dm2mac makes that an optional cheat: turn on No bump damage and the wall still stops you, but everybody keeps their health.

**Phases**
1. *Before* — The champion facing the wall, with health visible.
2. *Cheat off* — After the bump: one health point lost.
3. *Cheat on* — The same bump from the same save: health unchanged.

**Source** — [c0d1f4ac](https://github.com/kzagoris/dm2mac/commit/c0d1f4ac)

## 18. Save slots and quick-save recovery (41, 42, 43, 44, 45)

> If the party dies and all you have is a quick save, dm2mac lets you load it directly. The disk menu also has Load for numbered saves. Both live in the folder you pass to --saves, so a damaged quick save still leaves you a numbered slot to fall back on.

**Phases**
1. *Damaged quick save* — The error after loading a damaged copy of the quick save.
2. *Load* — The slot picker with the good numbered slot selected.
3. *Restored* — The party back in the dungeon.

**Source** — [e8d19541](https://github.com/kzagoris/dm2mac/commit/e8d19541), [0376dfa5](https://github.com/kzagoris/dm2mac/commit/0376dfa5), [eb0b2785](https://github.com/kzagoris/dm2mac/commit/eb0b2785), [998d6cce](https://github.com/kzagoris/dm2mac/commit/998d6cce)

## 19. Choose the dungeon's look (46, 47, 48)

> dm2mac adds three display options, each with its own switch. 4:3 gives the pixels the taller shape of a VGA monitor, xBRZ smooths the edges and scanlines add dark lines between rows.

**Phases** — Use window captures of the same portrait, because the filters work after the 320 × 200 screen. Keep the crop readable and the same in each phase.
1. *Original* — The unfiltered portrait.
2. *4:3* — The portrait in 4:3, with the outer frame visible.
3. *xBRZ* — The same portrait with xBRZ.
4. *Scanlines* — xBRZ with scanlines added.

**Source** — [6a2146b8](https://github.com/kzagoris/dm2mac/commit/6a2146b8), [eee50f77](https://github.com/kzagoris/dm2mac/commit/eee50f77)

## Capture review

Check each still before assembling it. It must show the phase's claim, with the message text readable and the cursor where the phase needs it. After export, check the duration, watch each movie once muted, and confirm the captions match the tweet. Post 1 is captured; posts 2–19 are storyboards awaiting capture.

## Movie capture prompt

The `phase-movie` project skill (`.claude/skills/phase-movie/`) holds the capture tools, the staging recipe and the known blockers. Start a new session (a smaller model is fine) with:

```text
Use the phase-movie skill to make the phase movies of posts 2 to 19 in docs/feature-tweet-spec.md, in order. Post 1 is done and is the reference (captures/feature-tweets/01-attack-all.mov).

Load the skill first and follow its hard rules, workflow and token-saving rules. Work one post at a time: stage, capture, pick from a contact sheet, assemble, check, then move on. Do not start the next post with a dm2 still running.

Posts 2 (held Ctrl), 12 (mouse-wheel zoom) and 19 (filters applied after the screenshot) have known blockers described in the skill: do what it allows, then skip the blocked phases and report them. If any other phase cannot be staged in about three attempts, skip it, write down why, and continue.

At the end, report per post: the movie path and duration, the captions, anything staged, and every skipped phase with its reason.
```
