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

Press **Tab** or **M** in the dungeon view to open the automap. Game time stops while you inspect it. The map normally shows explored squares; drag with the left mouse button to look around, then reopen it to center on the party again. The position readout gives the level, coordinates, and facing direction.

![Automap with yellow item markers and pile counts](docs/images/advanced-automap-items.png)

*Yellow markers identify loose items. Counts distinguish small piles, while a plus sign represents ten or more items. This demonstration uses Reveal map, so it includes squares beyond normal exploration.*

The markers answer a practical question: where did I leave that pile? They reflect the current dungeon state, including alcove items and items without map graphics. Containers count as one item; their contents, champion and creature inventories, missiles, and effects are excluded. Moving creatures and merchants are not shown as map creatures.

A single item has a yellow diamond; piles of two through nine display a count, and larger piles display `+`. Exploration remains meaningful: without Reveal map, items appear only on explored squares. Turning Reveal map on does not permanently mark those squares as explored.

#### Inspect a pile without walking to it

Short-click a square containing items to open its list. Matching names are grouped with quantities, and the subtitle reports the exact total. Large collections span multiple pages.

![Item inspection showing twelve items over two pages](docs/images/advanced-item-list.png)

*The selected square contains twelve items. Page 1 of 2 lists eight groups, including a compass, copper coin, and small plank. NEXT opens the remaining groups.*

Use **Left/Right**, **A/D**, or **PREVIOUS/NEXT** to change pages. Click **MAP**, press **Esc**, or right-click to return to the map; **Tab/M** close the automap from either view. Inspection reads the current items whenever drawn, so collecting or dropping items changes the next view without a separate saved item history.

### Ordinary options and help in one place

Press **F1**, or **Esc** in the dungeon view, to open **Options/Help**. This pauses game time and provides controls, spell favorites, screen filtering, quick save/load, and an explicit entry to optional cheats.

![Options and Help menu](docs/images/advanced-options.png)

*The ordinary menu makes common actions discoverable and labels cheats as optional.*

F1 also opens help from inventory, sleep, and pause. Save and cast actions require the dungeon view; name entry, recruiting, and dialogs retain their original input behavior. This avoids turning a typed name into a movement or save command.

![Controls page with movement and spell instructions](docs/images/advanced-controls.png)

*The controls page explains movement, inventory, saves, normal rune entry, and spell favorites inside the game.*

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

### Record spell combinations for reuse

**B** opens four favorites and the last attempted rune combination for each champion. Record runes already entered in the normal spell panel, then choose the power for the mana preview and later replay. Unknown combinations can be recorded too; recording does not spend mana or remove the entered runes.

Replay follows normal casting: it requires a living, ready caster, enough mana, and an empty rune tray. Rune costs, skill rolls, failures, experience, flask requirements, and action delays still apply. Failed attempts are recorded as LAST as well. Favorites are stored in the optional `FAVS` save block; older saves start with empty favorites.

The separate cheat spell catalog provides free casting. It does not describe how ordinary favorites work. The screenshots above show the favorites entry and help text; they do not demonstrate a populated favorites list or a successful replay.

### Recover quickly and pause when focus changes

**F5** writes `QUICK.DAT`; **F9** restores it without a slot-selection dialog. The quick save lives in the selected saves directory (`--saves DIR`), otherwise in the game data's `DATA` folder. It is separate from numbered save slots. Advanced startup and party-death recovery offer the quick save explicitly, even when no numbered slot exists.

Messages distinguish a successful save/load from a missing quick save, damaged quick save, or inability to save. Keep numbered slots when you want multiple recovery points: each quick save replaces the single quick-save checkpoint. Explored squares, cheat settings, and recorded favorites persist with saves.

Losing focus or minimizing automatically latches an Advanced pause and clears queued input and held interactions. Returning focus alone leaves the game paused. Press a fresh **Return** to resume, then press movement keys again. This prevents old queued movement or mouse holds from continuing when you return to the game.

### More comfortable movement and presentation

Held movement repeats only while the game waits for input, allowing the party to stop when the key is released. Larger champion status areas make inventory interaction easier: with an empty leader hand, clicking outside the hand slots opens inventory; with an object held, it places that object into the champion's inventory.

Advanced mode also extends the display of a successful damage star by one game step, without changing action delays. Projectile rendering interpolates flight at timer frequency while the party view stays still; simulation movement and combat rules retain their original steps. A still screenshot cannot demonstrate either timing improvement.

Screen filters, 4:3 aspect correction, and scanlines provide presentation choices. Their settings persist in `SETTINGS.TXT` beside `QUICK.DAT`. Startup filter overrides from `--filter` or `DM2_FILTER` take precedence over the stored filter. Advanced mode also makes antidotes reliably cure poison.

### Optional cheats remain explicit

**C** opens six independent switches: full light, immortality, endless food and water, one-hit kills, walking through walls, and Reveal map. New games start with all six off; saves restore their stored values. The cheat screen also contains the free spell catalog, which bypasses mana cost and the failure roll and grants no experience, but still requires a flask for potion spells and retains action delay.

These additions belong to Advanced mode. Vanilla retains the original behavior, and turn-based mode has its own documented control model.

### Screenshot notes

These are actual Windows game captures retained from the improvement documentation session. The map and item-list captures use an item-rich demonstration state; the revealed map is illustrative rather than a claim about normal starting exploration. The options and controls captures show their native low-resolution interface. They document visible UI; save recovery, focus handling, spell replay, and animation behavior above are described from the repository rules rather than asserted as verified by these still images.

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
- [tinf](https://github.com/jibsen/tinf) by Jørgen Ibsen: ZIP decompression.
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
