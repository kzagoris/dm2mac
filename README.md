# DM2Mac: Dungeon Master II for macOS and Windows

DM2Mac is a native C11 and SDL3 port of **Dungeon Master II: The Legend of Skullkeep** (PC DOS, 1995) for macOS
and Windows. It is a recompilation: Ghidra decompiled the original DOS program to C, the game was rewritten in C11
with SDL3 from that code, and the result runs natively without DOSBox.

<p align="center">
  <img src="docs/images/screenshot.png" alt="The party of four champions in front of a merchant at his table">
  <img src="docs/images/screenshot-boss.png" alt="The party facing the main villain">
</p>

## What you need

Your own copy of the PC DOS version of Dungeon Master II (English): its folder, its ZIP archive, or its CD image
(`.iso`, `.img`, `.bin`). This repository and the release contain no game files. Other versions (Amiga, FM Towns, other
languages) do not work.

## Play

1. Download the [latest release](https://github.com/peterkmet/dm2mac/releases/latest): `DM2Mac-macos-arm64.zip` or
   `DM2Mac-windows-x64.zip`. Extract it.
2. Copy your game (the folder, the ZIP archive, or the CD image) into the extracted `DM2Mac` folder.
3. Start the game:
   - **macOS:** In Terminal, remove the quarantine attribute in the `DM2Mac` folder:

     ```sh
     xattr -dr com.apple.quarantine DM2Mac.app
     ```

     Then double-click `DM2Mac.app` in the Finder.

   - **Windows:** Run `dm2.exe`. If SmartScreen warns about an unknown publisher, select "More info", then
     "Run anyway".

On first startup the game checks your files and copies the 10 files it needs into `game/` in the `DM2Mac` folder. If
a file is missing or wrong, a message names it. The game keeps all its files in `game/`: the game files, your saved
games in `game/DATA`, the settings, and the messages in `dm2.log`. To remove the game, delete the `DM2Mac` folder.

## Modes

On startup you choose how to play:

- **Vanilla**: the original game, unchanged.
- **Advanced** (the default): the original game with a few additions:
  - WASD movement and Q/E turning;
  - an automap (Tab) of the squares you have seen;
  - quick save (F5) and quick load (F9);
  - a cheat screen (C);
  - screen filters (Scale2x, Scale3x, xBRZ 2x/3x/4x), 4:3 aspect correction and scanlines.

In both modes the mouse and the arrow keys work as in the original game. The keys, the cheats, and the screen filter
are in the [advanced mode section of the Player manual](docs/manual.md#advanced-mode).

## Advanced mode improvements

Advanced mode keeps the real-time dungeon game and adds tools for navigation, repeated actions, recovery, and learning the controls. Choose **Advanced** on startup (the default), or start with `--mode advanced`. Optional cheats start off in a new game.

The improvements include automap item inspection, spell favorites, in-game help, and safer recovery and input handling. For exact rules and edge cases, see [Advanced mode rules](docs/rules/advanced-mode.md) and the [Player manual](docs/manual.md#advanced-mode).

### Find items on the automap

Press **Tab** or **M** in the dungeon view to open the automap. Game time stops while you inspect it. The map normally shows explored squares; drag with the left mouse button to look around, then reopen it to center on the party again. The position readout gives the level, coordinates, and facing direction. Scroll the mouse wheel over the map to zoom between 1×, 2×, and 3× around the pointer. Reopening the map restores 1×; the readout and item list keep their normal text size.

![Automap with a single yellow plus per tile containing items](docs/images/advanced-automap-items.png)

*A single yellow plus identifies each tile containing loose items, regardless of quantity. This demonstration uses Reveal map, so it includes squares beyond normal exploration.*

The markers answer a practical question: where did I leave that pile? They reflect the current dungeon state, including alcove items and items without map graphics. Containers count as one item; their contents, champion and creature inventories, missiles, and effects are excluded. Moving creatures and merchants are not shown as map creatures.

Every tile containing items has one yellow `+`. Click the tile to see item names and quantities. Exploration remains meaningful: without Reveal map, items appear only on explored squares. Turning Reveal map on does not permanently mark those squares as explored.

#### Inspect a pile without walking to it

Short-click a square containing items to open its list. Matching names are grouped with quantities, and the subtitle reports the exact total. Large collections span multiple pages.

![Item inspection showing ten items over two pages](docs/images/advanced-item-list.png)

*The selected square contains ten items. Page 1 of 2 lists eight groups, including a compass, copper coin, and small plank. NEXT opens the remaining groups.*

Use **Left/Right**, **A/D**, or **PREVIOUS/NEXT** to change pages. Click **MAP**, press **Esc**, or right-click to return to the map; **Tab/M** close the automap from either view. Inspection reads the current items whenever drawn, so collecting or dropping items changes the next view without a separate saved item history.

### Ordinary options and help in one place

Press **F1**, or **Esc** in the dungeon view, to open **Options/Help**. This pauses game time and provides controls, spell favorites, screen filtering, quick save/load, and an explicit entry to optional cheats.

![Options and Help menu](docs/images/advanced-options.png)

*Options/Help, enlarged 3Ã— with increased text brightness for readability. This earlier capture predates the SPELLS SHOWN, aspect, scanlines, and SMOOTH STEPPING rows described below.*

F1 also opens help from inventory, sleep, and pause. Save and cast actions require the dungeon view; name entry, recruiting, and dialogs retain their original input behavior. This avoids turning a typed name into a movement or save command.

![Controls page with movement and spell instructions](docs/images/advanced-controls.png)

*Controls, enlarged 3Ã— with increased text brightness. This earlier capture labels B as favorites; the current B screen opens the spell list with favorites available inside it.*

| Key | Action in the dungeon view |
| --- | --- |
| W / A / S / D | Move |
| Q / E | Turn left / right |
| Arrow keys | Original movement controls |
| Tab / M | Open the automap |
| F5 / F9 | Quick save / quick load |
| B | Open recorded spell favorites |
| F1 / Esc | Open Options/Help |
| C | Open optional cheats |

Tab or Esc closes an open inventory, except while editing a name or recruiting. Advanced shortcuts require an idle party: inspection holds, champion dragging, and push mode prevent dungeon shortcuts from firing.

### Spells: cast by name, learn runes, and save combinations

Press **B** in the dungeon view, or choose **SPELL LIST (B)** in Options/Help. The list pauses game time while you browse. Each spell shows its name and the rune names after the power rune; the caster and power are separate choices at the top.

#### Known spells or the full spell catalog

**Options/Help → SPELLS SHOWN** switches between **KNOWN SPELLS**, which lists spells you have cast, and **ALL SPELLS**, which reveals the full catalog. The same setting controls spell-name hints during manual rune entry and is remembered in `SETTINGS.TXT`.

![Known spells screen before learning a spell](docs/images/advanced-spells-known.png)

*With no known spells, the list directs you to enter runes. You can learn through ordinary casting, or choose All spells to browse the catalog.*

![All spells with caster, power, rune names and mana preview](docs/images/advanced-spells-all.png)

*LIGHT is selected at power 1/6 for Torham. MANA 16/14 means the cast needs 16 mana while this caster currently has 14; it does not mean 16 mana remains. NO FLASK NEEDED identifies a spell that does not create a potion.*

The list displays six spells per page. **Left/Right** or **A/D** change pages, and **MORE SPELLS** advances to the next page, wrapping around after the last. **Up/Down** or **W/S** select a row; **Return** activates it. The selected spell is yellow and marked with `>`.

![Second spell page with aura spells and Haste](docs/images/advanced-spells-page2.png)

*Page 2 shows another six spells, with rune combinations alongside the names. The footer previews the selected spell's requirements.*

#### Choose a caster and power, then cast

1. Activate **CASTER … CHANGE** to cycle champions. Choose a living champion who is ready to cast.
2. Activate **SPELL POWER … CHANGE** to cycle power 1 through 6. Check the updated mana preview; reduce power if the caster cannot afford the spell.
3. Select a spell and check its flask requirement. Potion spells require an empty flask in one of that caster's hands; the status reports **FLASK READY** or **FLASK MISSING**.
4. Activate the selected spell to cast. With the mouse, the first click on a different spell selects it and shows its costs; a second click casts. Clicking an already selected spell activates it immediately. With the keyboard, Return activates the selected spell.

The list uses normal rune entry and casting: mana costs, skill checks, failures, experience, flask requirements, and action delays apply. Merely showing **All spells** does not grant free casting. The game reports why an attempt cannot start, such as **CHOOSE A LIVING CASTER**, **CASTER BUSY - WAIT AND TRY AGAIN**, or **NOT ENOUGH MANA - LOWER SPELL POWER**. If runes are already entered, close the list and cast or remove them first.

Press **B** to close the list. **Esc** or right-click closes the main spell list; from favorites, either returns to the list.

#### Rune hints while casting manually

The original spell panel remains available: select a champion's spell hand, enter a power rune, then enter the spell runes. In Advanced mode, the spellbook identifies possible next runes for the current prefix. When the entered combination completes a visible spell, a cyan message names its runes and spell.

With **Known spells**, hints respect discovery and name only known spells. With **All spells**, hints can identify every spell; if no spell begins with the entered combination, the message says **NO SPELL HAS THESE RUNES**. A power rune by itself does not produce a spell-name message because it can begin every spell.

#### Four favorites per champion, plus the last attempt

Choose **FAVORITES AND LAST ATTEMPT** from the list. Each champion has four saved combinations and a separate **LAST ATTEMPT** entry. Unknown rune combinations can be recorded too.

![Favorites screen with empty slots and recording instructions](docs/images/advanced-spells-favorites.png)

*Empty favorites show the next step: enter runes in the normal spell panel before saving a combination. This capture demonstrates the initial state, not a successful recorded cast.*

To record a favorite, close the overlay, enter the desired runes in the champion's normal spell panel, then press B and open favorites. Select **FAVORITE 1–4** and activate **SAVE ENTERED RUNES TO FAVORITE …**. Recording leaves both the entered runes and mana alone. Close the overlay to cast the runes already entered.

To replay later, begin with an empty rune tray, choose the champion and power, select the saved favorite, and activate **CAST SELECTED SPELL**. The mana preview and flask status help check readiness. Replay uses the normal casting path; it cannot bypass mana costs, cooldowns, failure rolls, or potion-flask requirements. An empty slot reports **EMPTY SLOT - SAVE A SPELL FIRST**.

**LAST ATTEMPT** records failed attempts as well as successful ones. It is a separate automatically maintained entry; select one of the four favorite slots when recording manually. A potion cast without a flask retains its paid runes as in ordinary casting. Favorites persist in the optional `FAVS` save block; new games and older saves without that block start empty.

#### Free casting is a separate cheat

The **C → SPELLS - FREE CAST CHEAT** catalog casts at maximum power without mana cost or a failure roll and grants no experience. Flask requirements and action delays still apply. Use the ordinary B list or favorites when you want normal casting rules.

### Recover quickly and pause when focus changes

**F5** writes `QUICK.DAT`; **F9** restores it without a slot-selection dialog. The quick save lives in the selected saves directory (`--saves DIR`), otherwise in the game data's `DATA` folder. It is separate from numbered save slots. Advanced startup and party-death recovery offer the quick save explicitly, even when no numbered slot exists.

Messages distinguish a successful save/load from a missing quick save, damaged quick save, or inability to save. Keep numbered slots when you want multiple recovery points: each quick save replaces the single quick-save checkpoint. Explored squares, cheat settings, and recorded favorites persist with saves.

Losing focus or minimizing automatically latches an Advanced pause and clears queued input and held interactions. Returning focus alone leaves the game paused. Press a fresh **Return** to resume, then press movement keys again. This prevents old queued movement or mouse holds from continuing when you return to the game.

### Choose animated or direct stepping

**Options/Help â†’ SMOOTH STEPPING** lets you choose how walking between dungeon squares looks:

- **On** (the default): retain the intermediate walking frames for an animated transition between squares.
- **Off**: skip the intermediate walking frames for a quicker, direct step, similar to Dungeon Master I or Eye of the Beholder.

Collision checks and champion movement cooldowns still apply in either setting. This changes the presentation of walking rather than removing movement restrictions. The choice persists in `SETTINGS.TXT`, so you do not need to reset it each session. Walking animation is separate from the smooth projectile flight described below.

### More comfortable movement and presentation

In Advanced mode, hover over an enemy: crossed swords indicate an eligible target. Bright swords mean at least one champion is ready; grey swords mean the party is still recovering. Left-click for one party volley using each champion's last chosen weapon attack. Without a remembered choice, a champion uses the first basic melee or shooting action that can reach the target, checking the action hand first. No manual attack is required to start. A click while everyone is recovering is ignored. Living, ready champions attack within normal range and formation rules; ranged weapons need loaded ammunition. Normal stamina, ammunition, charges, and cooldowns apply. Spells and throwing are excluded. Keep the leader hand empty to use this shortcut. Small gaps inside a creature's body remain targetable, and passing missiles and explosions do not interrupt detection. Walls, doors, and foreground objects still block targeting.

Changing a weapon, starting a new game, or loading a save resets the remembered choice to the basic fallback. Choose a different action normally whenever you want to change it. Damage stars also appear over the monster, using the normal attack-result artwork and damage digits. Simultaneous hits share a damage total; projectile damage appears on impact, not on launch. The feedback clears shortly afterward and does not cover up the monster's clickable area.

Held movement repeats only while the game waits for input, allowing the party to stop when the key is released. Larger champion status areas make inventory interaction easier: with an empty leader hand, clicking outside the hand slots opens inventory; with an object held, it places that object into the champion's inventory.

Advanced mode also extends the display of a successful damage star by one game step, without changing action delays. Projectile rendering interpolates flight at timer frequency while the party view stays still; simulation movement and combat rules retain their original steps. A still screenshot cannot demonstrate either timing improvement.

Screen filters, 4:3 aspect correction, and scanlines provide presentation choices. Their settings persist in `SETTINGS.TXT` beside `QUICK.DAT`. Startup filter overrides from `--filter` or `DM2_FILTER` take precedence over the stored filter. Advanced mode also makes antidotes reliably cure poison.

### Optional cheats remain explicit

**C** opens six independent switches: full light, immortality, endless food and water, one-hit kills, walking through walls, and Reveal map. New games start with all six off; saves restore their stored values. The cheat screen also contains the free spell catalog, which bypasses mana cost and the failure roll and grants no experience, but still requires a flask for potion spells and retains action delay.

These additions belong to Advanced mode. Vanilla retains the original behavior, and turn-based mode has its own documented control model.

### Screenshot notes

The automap and item-list images are game captures retained from the improvement documentation session. The map and item-list captures use an item-rich demonstration state; the revealed map is illustrative rather than a claim about normal starting exploration. The options, controls, and spell screens are rendered from the current game UI and original game assets in an isolated test session, using the VGA palette conversion and 3× pixel scaling. Spell screenshots use a new-game demonstration state; All spells is enabled for the catalog examples. They document visible UI; save recovery, focus handling, spell replay, and animation behavior above are described from the repository rules rather than asserted as verified by these still images.

## Let an LLM play

On macOS, a program such as an LLM can play the game turn by turn over a Unix socket. In the turn-based mode the game
time stops between requests; each request is one turn, and the reply gives the state and the view as JSON. Build the
game first (see [Development](docs/development.md)), then:

```sh
build/dm2 --mode turnbased &
uv run dm2-play --brief get_state
uv run dm2-play act move_forward
```

[Control socket](docs/llm/control-socket.md) has the start, `dm2-play`, and the protocol;
[Turn-based mode](docs/llm/turn-based.md) has the turns, the actions, and the reply fields. [LLM runs](docs/llm/llm-runs.md)
describes playthroughs with a coordinator agent and player agents, and the
[LLM playbook](docs/llm/llm-playbook.md) is the page the player agent reads.

## For developers

DM2Mac is a C11 and SDL3 reimplementation of `SKULL.EXE`, built from its own Ghidra decompilation and checked
against the original in an emulator. Build instructions and tests are in [Development](docs/development.md); the documentation index is
[docs/README.md](docs/README.md).

## Acknowledgements

- [SDL3](https://github.com/libsdl-org/SDL): windowing, input, and audio output.
- [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3) by Nuke.YKT: OPL3 emulation for the music.
- [tinf](https://github.com/jibsen/tinf) by JÃ¸rgen Ibsen: ZIP decompression.
- [xBRZ](https://sourceforge.net/projects/xbrz/) by Zenju: colour-blending pixel-art upscaling.
- [Unicorn](https://www.unicorn-engine.org): CPU emulation for the differential tests.
- [reDM2](https://codeberg.org/CyCl0ne/reDM2): an analysis of the same `SKULL.EXE`, read for names, formats, and
  logic.

## License

GPL-3.0-or-later ([LICENSE](LICENSE)). Nuked-OPL3 in `src/third_party/opl3/` is under the LGPL-2.1-or-later license
([opl3-LICENSE](src/third_party/opl3/opl3-LICENSE)); tinf in `src/third_party/tinf/` is under the zlib license
([tinf-LICENSE](src/third_party/tinf/tinf-LICENSE)); xBRZ in `src/third_party/xbrz/` is under GPL-3.0 with a linking
exception (notices in its source files and [License.txt](src/third_party/xbrz/License.txt)). Dungeon Master II is the property of its rights holders. This
project is not affiliated with or endorsed by them. The game data is not included and is not covered by this license.
