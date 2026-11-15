# Player manual

DM2Mac plays Dungeon Master II as the original PC game does. This manual covers only what is new: how to start
the game, the vanilla and advanced modes, and the advanced-mode extras. For the game itself, see the original game's
manual. Item values are in [items.md](items.md).

## Run the game

Install the game as the [README](../README.md#play) says, or [build it](development.md) and start `build/dm2`
(`build/dm2.exe` on Windows) from the folder that holds your copy of the game.

The game uses the folder `game/` in the folder where you start it. If that folder has no `game` entry, the game uses
the folder of the program instead: the folder that holds `DM2Mac.app` on macOS, the folder that holds `dm2.exe` on
Windows, or the folder that holds `dm2` on Linux. A double click on `DM2Mac.app` in the Finder thus uses `game/` in
the `DM2Mac` folder ([Startup](formats/install.md#startup)). A `game` file or folder in the folder where you start
the game keeps that folder.

On first startup the game copies the 10 files it needs from your folder, ZIP archive, or CD image into `game/`
([Game files and install](formats/install.md)). In `DM2Mac.app` and on Windows, the game's messages go to
`game/dm2.log`.

## Options

You need these only when you start the game from Terminal or a shortcut:

| Option | What it does |
|---|---|
| `--mode vanilla`, `--mode advanced` | Starts in that mode and skips the mode screen. `--mode turnbased` lets a program play ([Turn-based mode](llm/turn-based.md), macOS and Linux). |
| `--save PATH` | Loads a save and starts it immediately, in advanced mode unless `--mode` says otherwise. |
| `--filter NAME` | The window's pixel-art filter: `none`, `scale2x`, `scale3x`, `xbrz2x`, `xbrz3x`, or `xbrz4x`. The `DM2_FILTER` environment variable does the same. |
| `--data DIR` | Uses the game files in `DIR` instead of `game/`, and skips the check and the install. Saves go to `DIR/DATA`. |
| `--saves DIR` | Keeps the quick save and `SETTINGS.TXT` in `DIR` instead of `game/DATA`. The folder must exist. |
| `--control PATH` | Opens a control socket for a program (macOS and Linux). |
| `--music fm`, `--music sc55` | The music: the FM music of the original sound card (the default), or a Roland SC-55 ([Roland SC-55 music](#roland-sc-55-music)). |
| `--sc55-plugin FILE` | The SC-55 plugin `Nuked-SC55.clap`, when it is not in a `plugins` folder that the game searches. |
| `--sc55-roms DIR` | The folder of the SC-55 ROMs (the folder of the model folders), when it is not a `soundcanvas-roms` folder that the game searches. |

```sh
build/dm2 --mode vanilla
build/dm2 --save game/DATA/SKSAVE0.DAT
```

A relative path in an option is relative to the folder where you start the game, even when the game changes to the
folder of the program. An invalid option prints a message that says what is wrong. You can resize the window to any size.

### Roland SC-55 music

The music of the game is also written for General MIDI. With `--music sc55` it plays on an emulated Roland Sound
Canvas SC-55, the way DOSBox Staging plays it with `mididevice = soundcanvas`. The game does not include the
emulator. You need two things:

- **The plugin** `Nuked-SC55.clap`: the [Nuked-SC55 CLAP plugin](https://github.com/johnnovak/Nuked-SC55-CLAP),
  the SC-55 emulator that DOSBox Staging uses. DOSBox Staging includes it; you can also download it alone. Its license
  does not allow it inside this game, so the game loads it at startup. Do not add it to the game's repository.
- **The ROMs**: dumps of the chips of a real SC-55. Roland owns them; neither the game, nor DOSBox Staging, nor the
  plugin includes them. Get them from your own SC-55.

Both ways below give the game the same plugin and ROMs; take the one that suits you.

#### The ROM folder

The ROMs go into a folder `soundcanvas-roms` with one subfolder for each SC-55 model. The names of the subfolders
and of the files must be exactly these; the plugin checks each file by its SHA-256 sum. One model is enough.

```text
soundcanvas-roms/
  SC-55-v1.21/          (also SC-55-v1.00, SC-55-v1.10, SC-55-v1.20, SC-55-v2.00)
    sc55_rom1.bin           32 KB
    sc55_rom2.bin          256 KB
    sc55_waverom1.bin     1024 KB
    sc55_waverom2.bin     1024 KB
    sc55_waverom3.bin     1024 KB
  SC-55mk2-v1.01/
    rom1.bin                32 KB
    rom2.bin               512 KB
    rom_sm.bin               4 KB
    waverom1.bin          2048 KB
    waverom2.bin          1024 KB
```

The game plays the first model whose ROMs it finds, in the order of DOSBox Staging: SC-55 v1.21, v1.20, v1.10,
v1.00, v2.00, then SC-55mk2 v1.01.

#### With DOSBox Staging

If you already play DOS games with [DOSBox Staging](https://www.dosbox-staging.org/) and its Sound Canvas, the game
finds both on its own:

1. Install DOSBox Staging with its installer. It puts the plugin into its `plugins` folder.
2. Put the ROM folder into the DOSBox configuration folder:
   - Windows: `%LOCALAPPDATA%\DOSBox\soundcanvas-roms` (paste `%LOCALAPPDATA%\DOSBox` into the address bar of the
     Explorer).
   - macOS: `~/Library/Preferences/DOSBox/soundcanvas-roms`.
   - Linux: `~/.local/share/dosbox/soundcanvas-roms` or `~/.config/dosbox/soundcanvas-roms`.
3. Start the game with `--music sc55`, for example `dm2.exe --music sc55` on Windows.

The game looks for the plugin in these folders, in this order, and takes the first one:

| System | Plugin folders of DOSBox Staging |
|---|---|
| Windows | `%LOCALAPPDATA%\Programs\DOSBox Staging\plugins`, `%ProgramFiles%\DOSBox Staging\plugins`, `%LOCALAPPDATA%\DOSBox\plugins` |
| macOS | `/Applications/DOSBox Staging.app/Contents/PlugIns`, `~/Applications/DOSBox Staging.app/Contents/PlugIns`, `~/Library/Preferences/DOSBox/plugins` |
| Linux | `~/.local/share/dosbox-staging/plugins`, `/usr/local/share/dosbox-staging/plugins`, `/usr/share/dosbox-staging/plugins`, `~/.config/dosbox/plugins` |

A DOSBox Staging that comes inside another program (a game launcher, for example) keeps the plugin in its own
`plugins` folder, which the game does not know. Copy `Nuked-SC55.clap` from there into one of the folders of the
next section, or name it with `--sc55-plugin`.

#### Without DOSBox Staging

You can use the plugin alone and keep everything next to the game:

1. Download the zip archive of your system from the
   [releases of the plugin](https://github.com/johnnovak/Nuked-SC55-CLAP/releases): `windows-x64` for most Windows
   computers, `windows-arm64` for Windows on ARM, `macos`, or `linux-x86_64` / `linux-arm64`.
2. In the folder that holds `game/` (the folder where you start the game, or the folder of the program; see
   [Run the game](#run-the-game)), create a folder `plugins`.
3. Unzip the archive and copy `Nuked-SC55.clap` into `plugins`. On macOS it is a bundle (a folder); copy all of it.
4. In the same folder, create the [ROM folder](#the-rom-folder) `soundcanvas-roms` with your ROMs.
5. Start the game with `--music sc55`.

The folder then looks like this:

```text
game/
plugins/
  Nuked-SC55.clap
soundcanvas-roms/
  SC-55-v1.21/
    sc55_rom1.bin
    ...
```

The game looks in these folders before those of DOSBox Staging, so they also win over an installed DOSBox Staging.
The game also finds the plugin in a `plugins` folder next to the program (`dm2.exe`, `dm2`, or `DM2Mac.app`).

The game does not look in the usual CLAP folders (`C:\Program Files\Common Files\CLAP`,
`~/Library/Audio/Plug-Ins/CLAP`, `~/.clap`), nor in the `Nuked-SC55-Resources` folder that the plugin's own
documentation describes. If the plugin or the ROMs are elsewhere, name them:

```sh
dm2.exe --music sc55 --sc55-plugin "D:\Audio\Nuked-SC55.clap" --sc55-roms "D:\Audio\soundcanvas-roms"
build/dm2 --music sc55 --sc55-plugin ~/clap/Nuked-SC55.clap --sc55-roms ~/sc55/soundcanvas-roms
```

`--sc55-roms` names the folder that holds the model folders, and the game then looks only there.

On macOS, Gatekeeper may stop a downloaded plugin. Allow it in Terminal:

```sh
xattr -rd com.apple.quarantine plugins/Nuked-SC55.clap
```

#### Check that it works

`dm2.log` in the game folder (or the terminal on macOS and Linux) names the plugin, the ROM folders, and the model:

```text
SC-55: ROM folders C:\Users\you\AppData\Local/DOSBox/soundcanvas-roms
SC-55: plugin C:\Users\you\AppData\Local/Programs/DOSBox Staging/plugins/Nuked-SC55.clap
SC-55: playing on Roland SC-55 v1.21
```

If something is missing, the log says what, and the FM music plays:

| Message | What to do |
|---|---|
| `no ROM folder soundcanvas-roms or …` | Create the [ROM folder](#the-rom-folder) in one of the places above. |
| `no Nuked-SC55.clap in the plugins folder …` | Install DOSBox Staging, or put the plugin into a `plugins` folder ([Without DOSBox Staging](#without-dosbox-staging)). |
| `cannot load …` | The plugin is for another system or processor (take the right zip archive), or macOS Gatekeeper blocks it. |
| `the plugin finds no ROMs of an SC-55 model` | A folder or file name is wrong, a file is missing, or a ROM is a bad dump. Compare with [The ROM folder](#the-rom-folder). |

## Start screens

Without `--mode` and `--save`, the game first asks for the mode: VANILLA or ADVANCED. Choose with the arrow keys and
Return, or click. Then the intro movies play (click to skip one), and the original game's start menu follows.
RESTART in the save menu keeps the mode.

## Modes

| Mode | What you get |
|---|---|
| `vanilla` | The original game, nothing added. |
| `advanced` | The default. The original game with the [advanced features](#advanced-mode). |
| `turnbased` | A program plays the game turn by turn ([Turn-based mode](llm/turn-based.md)). macOS and Linux. |

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
| Alt+Enter (Option+Enter on a Mac), `F11` | Fullscreen on or off, in every mode; the advanced mode remembers it in `SETTINGS.TXT` |

## Saved games

Save in a slot with Alt+S and load a slot from the start menu, as in the original game. The slots are in
`game/DATA`.

## Advanced mode

Advanced means convenience additions. Cheats are separate, optional, and off in a new game. Press `Esc` in the dungeon view, or `F1`, for controls, display preferences, saves, and spell favorites. Advanced mode also
changes a few things:

- When you release a held key, the party stops. In vanilla mode the party keeps walking, as in the original.
- An antidote always cures poison. In vanilla mode it can fail, as in the original.
- A click on a champion's status box puts the object in your hand into that champion's inventory, or opens the
  inventory if your hand is empty.
- After a hit, the damage star stays a moment longer in the action area. A click ends it.
- A click on an enemy orders a party attack: over a creature that the party can attack, the pointer becomes crossed
  swords, bright when a champion is ready and grey while the party recovers. Each ready champion repeats their last
  melee or shooting action, or uses a basic one. Spells and throwing are excluded. Your hand must be empty.
- A damage star with the damage shows for a moment over a creature that takes a hit.
- Projectiles fly smoothly from square to square in the dungeon view.
- An object in your hand shows its details in the open inventory at once, without holding it on the eye.
- An object held on the eye in the inventory is compared with the item that the champion wears in that slot or holds
  in the hand: the gauge shows in green what the object has more and in red what it has less. Below the gauges stand
  the exact numbers and, in cyan, what a magic item adds (mana, statistics, skills).
- A save keeps the automap, its notes, and the cheats.

The automap, Options/Help, spell favorites, and cheat screen stop game time while open. The exact rules are in
[Advanced mode rules](rules/advanced-mode.md).

On the automap, a single yellow plus marks each explored square containing items, regardless of quantity.
Click a marked square to see item names and quantities; Left/Right or A/D and PREVIOUS/NEXT change pages.
Click anywhere in the list (except PREVIOUS/NEXT), press Esc, or right-click to return to the map. Dragging still scrolls; Tab/M close the automap.
Reveal map also shows items in unexplored areas. Markers update after pickup/drop, including items in alcoves;
containers are marked without revealing their contents.

### Spell rune hints

In advanced mode, hovering over a grey rune button replaces its symbol with syllable letters (for example `FUL`).
Moving the pointer away restores the rune symbol. Yellow highlighting still marks choices that continue a listed spell.

Four-letter syllables wrap onto two lines to fit the button.

### Advanced keys

| Key | Action |
|---|---|
| `W` `A` `S` `D` | Move forward, left, back, right |
| `Q`, `E` | Turn left, turn right |
| `F5`, `F9` | Quick save, quick load |
| `Ctrl+S` | Open the disk menu (same as the disk icon) |
| `Ctrl+R` | Rest (same as the Zzz icon) |
| `Ctrl+Q` | Quit (same as QUIT in the disk menu, with its question when the game is not saved) |
| `Tab` or `M` | Automap |
| `X` | Cheat screen |
| `Esc` | Pause with Options/Help in the dungeon view; close to resume. In the inventory it closes the inventory (the original pauses) |
| `F1` | Pause with Options/Help; close to resume (also in inventory, sleep, and pause) |
| `C` | Recorded spell favorites |
| `Alt+C`, then `Alt+1` to `Alt+4` | Set the entered runes as favorite 1 to 4, or cast that favorite when no runes are entered |
| `Tab` or `Esc` | Close the inventory |
| `Esc` | With a champion from a mirror on the screen: do not take it and return to the dungeon view |

The keys work only in the dungeon view. In the inventory, a dialog, or a menu they do nothing, except `Tab`, which
closes the inventory, `Esc`, which closes the inventory (the original pauses there) and resumes from the pause, and `F1`, which opens Options/Help in inventory, sleep, and pause. Save/cast options require the dungeon view. `Esc` and a right click close the automap and the cheat screen.

### Quick save and quick load

In advanced mode, `F5` saves to one quick slot (`game/DATA/QUICK.DAT`) and `F9` loads it, without a dialog. The
message area says what happened: `GAME SAVED`, `GAME LOADED`, `NO QUICK SAVE`, or `QUICK SAVE DAMAGED`.
`UNABLE TO SAVE` means that the game could not write a file: after `F5` the earlier quick save stays, and after `F9`
the game does not load. Advanced startup and death recovery offer **LOAD QUICK.DAT** (`F9`) separately from **LOAD SLOT SAVE** (`L`), even if no slot save exists. Startup also offers `N` for a new game; `Esc` exits. Missing or damaged quick saves keep the chooser open and preserve save files. `--save PATH` still starts that save directly.

### Spell favorites

The quick way: hold `Alt`, press `C`, then `1` to `4`. With runes entered in the spell panel, they become that
favorite and stay entered; the message area says, for example, `LIGHT SET AS FAVORITE 1`. With no runes entered,
the champion casts that favorite at its recorded power by the normal rules, with `CAST FAVORITE 1: LIGHT`. The
champion is the one of the open spell panel, otherwise the last caster of the spell list, otherwise the first
living champion. A spell that is not in the spell list yet shows its rune names instead of its name.

In the spell list (`C`), mark a spell and press `1` to `4`: the spell becomes that favorite of the shown caster at
the shown power, and the last line says, for example, `FIREBALL SET AS FAVORITE 2`. The list shows the favorite
number in front of the spell. This enters no runes and costs no mana. A combination that is not in the list is
set with `Alt+C` from the spell panel, as above.

In Favorites, choose the caster, power (1Ã¢â‚¬â€œ6), and recorded favorite or **LAST**. The screen shows the required
mana and whether an empty flask is required and present. **CAST** enters the runes at the chosen power and casts
once through the normal game rules: mana, skill checks, failures, experience, and action delays all apply.
A dead or busy caster, insufficient mana, or existing entered runes prevents replay. A missing flask uses the
normal failure behavior and retains the paid runes. The last attempted combination is remembered even after a
failed cast. Selecting a favorite shows its recorded power; Backspace on a favorite row empties it.
Favorites are private to each champion and persist with Advanced saves; old saves start empty.
The Favorites screen does not reveal the spell catalog. Use Up/Down and Return or click a row; Esc/right click closes.

### Switching windows

Switching windows or minimizing does not pause the game. Press **Esc** to open Options/Help and pause
before switching away; close it to resume. Held and queued input is cleared when focus is lost, so
movement does not remain stuck when you return.

### Automap

`Tab` or `M` shows the squares of the current level that the party has seen, with north up and the party in the
middle (the red frame; its white side shows where the party faces). The top line gives the level, the position,
and the direction. Drag the map with the mouse, or press the arrow keys or `W`, `A`, `S`, `D`, to scroll it one
square at a time. `Q` and `E` show the next level above and below that the party has visited; the top line then
reads `LEVEL n  PARTY ON p`. Only levels explored with the automap count; without one the map says
`NO VISITED LEVEL ABOVE` or `BELOW`. `Space` shows the party again. Use the mouse wheel over the map to zoom in or out (1×, 2×, or 3×), centered on the pointer. Reopening the map restores 1×.

The map shows walls, floors, pits, stairs, doors, ornaments, teleporters, pressure plates, and the creatures that do
not move. A fake wall that you can walk through has a gray checkerboard. A white line marks the edge of the level.
MAP LEGEND in Options/Help (`F1`) shows these marks: the checkered wall, the gray square of a pressure plate, the
yellow plus of items, the white line of the level edge, and the cyan pin of a note.
In low light the party sees fewer squares, so fewer squares appear on the map.

Double-click a square to write a one-line note on it, such as `NEEDS GOLD KEY` or `SHOP: ARMOUR` (up to 28
letters, digits, spaces and `,.;:-()'?!`). `Return` keeps the note, `Esc` cancels, and an empty note removes it. A
cyan pin marks a square with a note; click the square to read the note in the top lines of the map. A save keeps the
notes with the map.

### Cheats

The cheat screen (`X`) switches seven cheats on and off. A save keeps them, and a new game turns them off.

- **Full light:** you see the dungeon as in full light. Creatures still see the real light.
- **Immortal:** a champion keeps at least 1 health.
- **Endless food and water:** food and water do not go down.
- **One-hit kill:** every hit kills, except creatures that cannot be hurt.
- **Walk through walls:** the party walks through walls, fake walls, and closed doors, but not through creatures or
  off the map.
- **Reveal map:** the automap shows the whole level.
- **No bump damage:** collisions with walls, doors, and objects do not cause the usual 1-point damage.

### Cheat screen

```
CHEATS                         CLOSE
 FULL LIGHT              OFF
 IMMORTAL                ON
 ENDLESS FOOD AND WATER  ON
 ONE-HIT KILL            OFF
 WALK THROUGH WALLS      OFF
 REVEAL MAP              OFF
 NO BUMP DAMAGE          OFF
 SPELLS - FREE CAST CHEAT
```

Click a row, or choose it with the arrow keys (or `W`, `S`) and Return. CLOSE, `X`, `Esc`, or a right click closes
the screen.

- The seven cheat rows show `ON` or `OFF`.
- Options/Help (`F1`) holds SCREEN FILTER (`NONE`, `SCALE2X`, `SCALE3X`, `XBRZ2X`, `XBRZ3X`, `XBRZ4X`), 4:3 ASPECT, SCANLINES, and the ordinary
  quick-save/load actions.
- SPELLS lists all the game's spells with their runes, for example `FUL IR (X,4,4) FIREBALL`. A number is the
  rune's position in its row; `X` is any power. The spell names come from this port because the game has no names
  for them. A right click on a spell page goes back to the cheat page.

Click a spell to cast it at full power, with no mana cost and no chance of failure. The champion with the open spell
panel casts it, otherwise the leader. A potion spell still needs an empty flask in a hand.

Sound and music volume stay in the original game's volume dialog (the speaker button in the inventory).

### Stepping

Open Options with F1 or Esc in Advanced mode and set **Smooth stepping: Off** for a quicker, direct step between squares, like Dungeon Master I or Eye of the Beholder. Collision checks and champion movement cooldowns still apply. The setting is remembered in `SETTINGS.TXT`; smooth stepping is On by default.

### Screen filter

Options/Help (`F1`) has three rows for the picture of the window:

- SCREEN FILTER cycles through `NONE`, `SCALE2X`, `SCALE3X`, `XBRZ2X`, `XBRZ3X`, and `XBRZ4X`.
  xBRZ blends colours to smooth diagonal edges at 2x, 3x or 4x resolution. It can make dithered shading look painted;
  choose `NONE` for the original pixels. The filter does not change the window size.
- 4:3 ASPECT shows the picture as a VGA monitor did: 4:3, with pixels a little taller than wide. `OFF` shows square
  pixels. A window that fits the picture exactly changes its height with the row.
- SCANLINES draws a soft dark line below every line of the game screen, as on a monitor tube. It works with every
  filter. The lines show when the picture is at least 400 pixels high; the movies have none.

Alt+Enter or `F11` makes the window fill the screen and back. The picture keeps its aspect, with black bars at the
sides or above and below.

The game remembers the three rows and the fullscreen window in `game/DATA/SETTINGS.TXT` and uses them on the next
startup.
`--filter` overrides the filter. If the file cannot be written, the message area shows `UNABLE TO SAVE OPTIONS`. The file is
described in [Save files](formats/save.md#settingstxt).

For example, start with `dm2.exe --filter xbrz4x` on Windows or `build/dm2 --filter xbrz4x` on macOS.
