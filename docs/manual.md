# Player manual

DM2Mac plays Dungeon Master II as the original PC game does. This manual covers only what is new: how to start
the game, the vanilla and advanced modes, and the advanced-mode extras. For the game itself, see the original game's
manual. Item values are in [items.md](items.md).

## Run the game

Install the game as the [README](../README.md#play) says, or [build it](development.md) and start `build/dm2`
(`build/dm2.exe` on Windows) from the folder that holds your copy of the game.

The game uses the folder `game/` in the folder where you start it. If that folder has no `game` entry, the game uses
the folder of the program instead: the folder that holds `DM2Mac.app` on macOS, the folder that holds `dm2.exe` on
Windows. A double click on `DM2Mac.app` in the Finder thus uses `game/` in the `DM2Mac` folder
([Startup](formats/install.md#startup)). A `game` file or folder in the folder where you start the game keeps that
folder.

On first startup the game copies the 10 files it needs from your folder, ZIP archive, or CD image into `game/`
([Game files and install](formats/install.md)). In `DM2Mac.app` and on Windows, the game's messages go to
`game/dm2.log`.

## Options

You need these only when you start the game from Terminal or a shortcut:

| Option | What it does |
|---|---|
| `--mode vanilla`, `--mode advanced` | Starts in that mode and skips the mode screen. `--mode turnbased` lets a program play ([Turn-based mode](llm/turn-based.md), macOS only). |
| `--save PATH` | Loads a save and starts it immediately, in advanced mode unless `--mode` says otherwise. |
| `--filter NAME` | The window's pixel-art filter: `none`, `scale2x`, or `scale3x`. The `DM2_FILTER` environment variable does the same. |
| `--data DIR` | Uses the game files in `DIR` instead of `game/`, and skips the check and the install. Saves go to `DIR/DATA`. |
| `--saves DIR` | Keeps the quick save and `SETTINGS.TXT` in `DIR` instead of `game/DATA`. The folder must exist. |
| `--control PATH` | Opens a control socket for a program (macOS only). |

```sh
build/dm2 --mode vanilla
build/dm2 --save game/DATA/SKSAVE0.DAT
```

A relative path in an option is relative to the folder where you start the game, even when the game changes to the
folder of the program. An invalid option prints a message that says what is wrong. You can resize the window to any size.

## Start screens

Without `--mode` and `--save`, the game first asks for the mode: VANILLA or ADVANCED. Choose with the arrow keys and
Return, or click. Then the intro movies play (click to skip one), and the original game's start menu follows.
RESTART in the save menu keeps the mode.

## Modes

| Mode | What you get |
|---|---|
| `vanilla` | The original game, nothing added. |
| `advanced` | The default. The original game with the [advanced features](#advanced-mode). |
| `turnbased` | A program plays the game turn by turn ([Turn-based mode](llm/turn-based.md)). macOS only. |

The game rules are the same in all modes, except for the cheats and the few changes under
[Advanced mode](#advanced-mode).

## Controls

The mouse, arrow keys, and numeric keypad work as in the original game. Keys that are easy to miss:

| Key | Action |
|---|---|
| `1` to `4` | Open a champion's inventory |
| Shift+Left, Shift+Right | Step sideways |
| Alt+S (Option+S on a Mac) | Save menu |
| `Esc` | Pause |

## Saved games

Save in a slot with Alt+S and load a slot from the start menu, as in the original game. The slots are in
`game/DATA`.

## Advanced mode

Advanced means convenience additions. Cheats are separate, optional, and off in a new game. Press `Esc` or `F1`, or click the visible **ESC OPTIONS/HELP** shortcut for controls, display preferences, saves, and spell favorites. Advanced mode also
changes a few things:

- When you release a held key, the party stops. In vanilla mode the party keeps walking, as in the original.
- An antidote always cures poison. In vanilla mode it can fail, as in the original.
- A click on a champion's status box puts the object in your hand into that champion's inventory, or opens the
  inventory if your hand is empty.
- After a hit, the damage star stays a moment longer in the action area. A click ends it.
- Projectiles fly smoothly from square to square in the dungeon view.
- A save keeps the automap and the cheats.

The automap, Options/Help, spell favorites, and cheat screen stop game time while open. The exact rules are in
[Advanced mode rules](rules/advanced-mode.md).

### Advanced keys

| Key | Action |
|---|---|
| `W` `A` `S` `D` | Move forward, left, back, right |
| `Q`, `E` | Turn left, turn right |
| `F5`, `F9` | Quick save, quick load |
| `Tab` or `M` | Automap |
| `C` | Cheat screen |
| `Esc` / `F1` | Pause with Options/Help; close to resume (also in inventory, sleep, and pause) |
| `B` | Recorded spell favorites |
| `Tab` | Close the inventory |

The keys work only in the dungeon view. In the inventory, a dialog, or a menu they do nothing, except `Tab`, which
closes the inventory, and `F1`, which opens Options/Help in inventory, sleep, and pause. Save/cast options require the dungeon view. `Esc` and a right click close the automap and the cheat screen.

### Quick save and quick load

In advanced mode, `F5` saves to one quick slot (`game/DATA/QUICK.DAT`) and `F9` loads it, without a dialog. The
message area says what happened: `GAME SAVED`, `GAME LOADED`, `NO QUICK SAVE`, or `QUICK SAVE DAMAGED`.
`UNABLE TO SAVE` means that the game could not write a file: after `F5` the earlier quick save stays, and after `F9`
the game does not load. Advanced startup and death recovery offer **LOAD QUICK.DAT** (`F9`) separately from **LOAD SLOT SAVE** (`L`), even if no slot save exists. Startup also offers `N` for a new game; `Esc` exits. Missing or damaged quick saves keep the chooser open and preserve save files. `--save PATH` still starts that save directly.

### Spell favorites

Open a champion's spell panel, enter a power rune and the combination you want to remember, then press `B`.
Select one of four favorites and **RECORD ENTERED RUNES IN FAVORITE**. Recording keeps the entered runes and
charges no extra mana. Cast those runes normally, or remove them before replaying a favorite; removing runes
retains the original no-refund rule.

In Favorites, choose the caster, power (1â€“6), and recorded favorite or **LAST**. The screen shows the required
mana and whether an empty flask is required and present. **CAST** enters the runes at the chosen power and casts
once through the normal game rules: mana, skill checks, failures, experience, and action delays all apply.
A dead or busy caster, insufficient mana, or existing entered runes prevents replay. A missing flask uses the
normal failure behavior and retains the paid runes. The last attempted combination is remembered even after a
failed cast. Favorites are private to each champion and persist with Advanced saves; old saves start empty.
The Favorites screen does not reveal the spell catalog. Use Up/Down and Return or click a row; Esc/right click closes.

### Switching windows

Switching windows or minimizing does not pause the game. Press **Esc** to open Options/Help and pause
before switching away; close it to resume. Held and queued input is cleared when focus is lost, so
movement does not remain stuck when you return.

### Automap

`Tab` or `M` shows the squares of the current level that the party has seen, with north up and the party in the
middle (the red frame; its white side shows where the party faces). The top line gives the level, the position,
and the direction. Drag the map with the mouse to scroll it.

The map shows walls, floors, pits, stairs, doors, ornaments, teleporters, pressure plates, and the creatures that do
not move. A fake wall that you can walk through has a gray checkerboard. A white line marks the edge of the level.
In low light the party sees fewer squares, so fewer squares appear on the map.

### Cheats

The cheat screen (`C`) switches six cheats on and off. A save keeps them, and a new game turns them off.

- **Full light:** you see the dungeon as in full light. Creatures still see the real light.
- **Immortal:** a champion keeps at least 1 health.
- **Endless food and water:** food and water do not go down.
- **One-hit kill:** every hit kills, except creatures that cannot be hurt.
- **Walk through walls:** the party walks through walls, fake walls, and closed doors, but not through creatures or
  off the map.
- **Reveal map:** the automap shows the whole level.

### Cheat screen

```
CHEATS                         CLOSE
 FULL LIGHT              OFF
 IMMORTAL                ON
 ENDLESS FOOD AND WATER  ON
 ONE-HIT KILL            OFF
 WALK THROUGH WALLS      OFF
 REVEAL MAP              OFF
 SPELLS - FREE CAST CHEAT
```

Click a row, or choose it with the arrow keys (or `W`, `S`) and Return. CLOSE, `C`, `Esc`, or a right click closes
the screen.

- The six cheat rows show `ON` or `OFF`.
- Options/Help (`F1`) holds SCREEN FILTER (`NONE`, `SCALE2X`, `SCALE3X`), 4:3 ASPECT, SCANLINES, and the ordinary
  quick-save/load actions.
- SPELLS lists all the game's spells with their runes, for example `FUL IR (X,4,4) FIREBALL`. A number is the
  rune's position in its row; `X` is any power. The spell names come from this port because the game has no names
  for them. A right click on a spell page goes back to the cheat page.

Click a spell to cast it at full power, with no mana cost and no chance of failure. The champion with the open spell
panel casts it, otherwise the leader. A potion spell still needs an empty flask in a hand.

Sound and music volume stay in the original game's volume dialog (the speaker button in the inventory).

### Screen filter

Options/Help (`F1`) has three rows for the picture of the window:

- SCREEN FILTER smooths the pixels: `NONE`, `SCALE2X`, or `SCALE3X`.
- 4:3 ASPECT shows the picture as a VGA monitor did: 4:3, with pixels a little taller than wide. `OFF` shows square
  pixels. A window that fits the picture exactly changes its height with the row.
- SCANLINES draws a soft dark line below every line of the game screen, as on a monitor tube. It works with every
  filter. The lines show when the picture is at least 400 pixels high; the movies have none.

The game remembers the three rows in `game/DATA/SETTINGS.TXT` and uses them on the next startup.
`--filter` overrides the filter. If the file cannot be written, the message area shows `UNABLE TO SAVE OPTIONS`. The file is
described in [Save files](formats/save.md#settingstxt).
