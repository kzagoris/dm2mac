---
name: phase-movie
description: Make the phase movies of docs/feature-tweet-spec.md — stage a scene in the real dm2 game (headless Linux build in WSL, driven through the control socket), capture one 320x200 still per phase, and assemble the captioned 1280x960 MOV. Use when asked to make, redo or fix a feature-tweet movie or still of this repository.
---

# Phase movies for the feature tweets

`docs/feature-tweet-spec.md` is the source: each post has a tweet (the claim), a **Phases** list (one still per
phase) and the shared **Movie format**. Post 1 is done and is the reference:
`captures/feature-tweets/01-attack-all.mov` and its stills in `captures/feature-tweets/01-attack-all/`.

## Hard rules

- Never touch `game/DATA` or the user's saves; work only on copies in `captures/feature-tweets/work/`.
- Never stop or restart a game the user is running (Windows `dm2.exe`). Only stop the headless `dm2` you started in
  WSL, with `game.sh stop`. Never use `pkill -f` (it matches and kills your own shell).
- Do not edit `src/`, do not commit, do not push. `captures/` and this skill stay untracked unless the user says so.
- Every still comes from the running game. Staging a save (moving a creature, see below) is allowed; drawing or
  editing pixels is not. If a phase cannot be captured, skip it and report why — never fake it.

## Environment (Windows host, Git Bash)

- The Windows build has **no control socket**. Use the Linux `build/dm2` inside WSL (Ubuntu). It is current; if
  `src/` changed since, run `MSYS_NO_PATHCONV=1 wsl -e make` first.
- Prefix every `wsl` call from Git Bash with `MSYS_NO_PATHCONV=1`, or Git Bash rewrites the paths. Use paths relative
  to the repository root; WSL starts in the same folder.
- Run Python only inside WSL, through `scripts/py.sh`. **Never run `uv` from Git Bash or PowerShell in this
  repository:** the repository's `.venv` is the WSL (Linux) venv, and Windows `uv` deletes it while trying to replace
  it. (If that happens, `MSYS_NO_PATHCONV=1 wsl -e bash -lc 'uv sync --frozen'` repairs it.)
- ffmpeg/ffprobe are the Windows ones; `sheet.sh` and `phase_movie.sh` run directly in Git Bash.

## Scripts (`.claude/skills/phase-movie/scripts/`)

Let `K=.claude/skills/phase-movie/scripts` and `W=captures/feature-tweets/work`.

| Call | Does |
| --- | --- |
| `MSYS_NO_PATHCONV=1 wsl -e bash $K/game.sh start $W/saves/QUICK.DAT` | start headless Advanced mode on a copy of the save |
| `... game.sh req '{"cmd":"key","key":"c"}' '{"cmd":"wait","ticks":10}'` | send requests (protocol: `docs/llm/control-socket.md`) |
| `... game.sh shot NAME` | `$W/shots/NAME.png` (320x200 with cursor) |
| `... game.sh burst NAME 2 4 8 16 32` | screenshots at those tick offsets from now |
| `... game.sh stop` | quit, and confirm no `dm2` is left |
| `... wsl -e bash $K/py.sh $K/dungeon.py party SAVE` | party map, position, direction, the map around it |
| `... dungeon.py creatures SAVE` | mobile creatures of every map (kind, position, max hp, solid/ghost) |
| `... dungeon.py stage SAVE OUT MAP:KIND` | move a creature in front of the party, full health, write OUT |
| `bash $K/sheet.sh OUT.png A.png B.png ...` | contact sheet, 4 per row, labelled |
| `bash $K/sheet.sh OUT.png --grid A.png` | one still 3x with a 20 px grid labelled in game coordinates |
| `... game.sh run FILE` | run a JSON list of requests from a file's last line, print the party position |
| `... dungeon.py faces/place/items/moveitem/mana ...` | find reactive walls, move the party or an item, set mana (see the file's docstring) |
| `... py.sh $K/route.py SAVE [X Y]` | the whole party map; with X Y a walking route as requests, for `game.sh run` |
| `DM2_VIDEO=x11 ... game.sh start SAVE` | the same game in a WSLg window (needed for a held key, the wheel, filters) |
| `... wsl -e bash -c 'DISPLAY=:0 python3 $K/xwheel.py X Y 2 up'` | mouse wheel over game pixel X,Y in that window |
| `... wsl -e bash -c 'DISPLAY=:0 python3 $K/xkey.py down Control_L'` | hold (`down`) or release (`up`) a key in that window |
| `... wsl -e bash $K/winshot.sh NAME SAVE` / `ctrlshot.sh NAME SAVE` | window capture with filters / shots without and with Ctrl held |
| `bash $K/phase_movie.sh OUT.mov "a.png|Caption" "b.png|Caption"` | the finished movie; prints its duration |
| `bash $K/check.sh MOVIE OUT.png` / `mk.sh NN-name "still|Caption"...` | a frame per phase in one sheet / copy stills, build, check |

Requests: `key` (`esc`, `space`, `f1`, `up`, `alt+c`, `ctrl+s`, single characters…), `click`/`move` with `x`,`y`
in 320x200 game pixels, `press`/`release`, `wait` (60 ticks per second), `state`. The protocol has **no mouse wheel
and no held key**: start the game with `DM2_VIDEO=x11` and use `xwheel.py` / `xkey.py` (XTest). For window runs keep
`fullscreen=off` in `SETTINGS.TXT`, or the window covers the user's screen.

## Workflow per post

1. Read only that post in the spec. Decide the save and what to stage. Keep `$W/saves/SETTINGS.TXT` right for the
   post (`spells=known|all`, `classic_steps=on` means Smooth stepping **off**); it is read at start.
2. `game.sh start`, take one `shot`, look at it with `sheet.sh --grid` to find click positions.
3. Drive the scene. Around the moment that matters, use `burst` rather than guessing the tick.
4. Look at a **contact sheet** of the burst (one image read), pick the stills.
5. `game.sh stop` (`start` refuses while a dm2 runs: stop before the next start, or requests go to the old game). Copy the picks to `captures/feature-tweets/NN-short-title/1-name.png`, `2-…`.
6. `phase_movie.sh captures/feature-tweets/NN-short-title.mov` with the captions: the phase name from the spec, plus the
   keys pressed when the input is a key (for example `Save it: Alt+C, Alt+1`). A caption describes what its still shows,
   not an action that happens after it. The first caption shares the strip with the tag: at most 26 characters.
7. Check one frame of each phase: `ffmpeg -ss T -i movie -frames:v 1 -vf scale=640:480 check.png`, all in one sheet.
8. Before writing a new `NN-*.mov`, move the old gameplay clip of that number to `captures/feature-tweets/gameplay/`.

## Save tokens

- Read contact sheets, not single stills; read `--grid` only to find coordinates. Never read 4x images to pick.
- Batch requests in one `game.sh req`; `game.sh` already cuts replies to 300 characters.
- Do not reread the spec, this file or the protocol doc per post; reuse the coordinates below.

## Known facts (save `QUICK.DAT`, map 8, party 8,21 facing north)

- No hostile creature is near any save. Post 1 used `dungeon.py stage $W/saves/QUICK.DAT $W/staged/attack-15.DAT 21:15`
  (a toad, solid, 250 hp) — already in `$W/staged/attack-15.DAT`. Pick `solid` creatures; `ghost` ones ignore most
  melee. Never set health above the maximum (a 1500-hp toad died at the first hit).
- Party: Torham (green), Bane (yellow, front), Seri (red, the caster: 207 mana), Equus (blue, front). Torham has
  only 5 of 27 mana — useful for "not enough mana".
- Main screen: the action icons are the row at y≈49, x 232–318. Only 262,49 is verified: it opened Equus's menu,
  whose first entry (CHOP) is at 277,58. Find the others with `--grid`. The creature in front: hover/click 104,148. Recovery after an attack: about 240 ticks.
  The hit's damage burst is clearest about 8 ticks after the click; icons stay hatched until about 32 ticks.
- The bottom 30 rows of the screen are the message area; keep it in the still (text appears there).
- Spell panel: click the `!` symbol beside the caster's icon in the party formation (Seri, back right: 312,92).
  Runes: the row at y≈81, x 237, 253, 270, 284, 297, 312 (symbols 0–5 of the current set); the entered-runes bar at
  262,96 casts; 298,96 deletes the last rune. LIGHT = 237 270 284 297, MAGIC TORCH = 237 284, DARKNESS =
  237 297 284 312. A cast spell joins the Known spells list (`spells=known`). The panel closes after a cast: open it
  again before the next spell, or the clicks hit the formation and swap champions.
- Explored map: `$W/staged/explored.DAT` is QUICK.DAT after a real walk across floor 8 (level 5), down to floor 11
  (level 6) and back; the party stands at 12,2. Automap: `tab`; at 3× (two wheel clicks at 118,130) squares are 24 px
  and the door at 13,5 is at 124,136. `e` shows the floor below. Double-click a square for a note.

## Posts that need care

- **2 (Ctrl highlight):** the highlight reads the real keyboard state; `ctrlshot.sh` holds Ctrl in a WSLg window.
- **5, 10, 12:** done with the facts above (`$W/post05.sh`, `$W/post10.sh`, `$W/post12.sh` replay them).
- **16 (classic stepping):** two runs from the same save, `classic_steps=off` then `on` in `SETTINGS.TXT`;
  `key up` then `burst` at 2 4 6 8 12 to catch the in-between frame.
- **17 (bump):** the X cheat screen toggles No bump damage; compare `state` health before and after `key up` into a
  wall.
- **18 (quick save):** make a damaged copy (`head -c 20000 good > $W/saves/QUICK.DAT`) of a save, never the original.
- **19 (display):** the socket screenshot is taken before the filters. Use a window capture: run the WSL game with a
  window (`SDL_VIDEO_DRIVER=x11`, `fullscreen=off`, `filter=xbrz…`, `aspect=on`, `scanlines=on` in `SETTINGS.TXT`, or
  `--filter`) and capture with ImageMagick `import -window` inside WSL. `phase_movie.sh` stretches every still to
  1280x800, so crop each capture to the same 16:10 region (pad the 4:3 one with black at the sides; never stretch it).
  If it does not work in two tries, report it.

## Report

For each post: the movie path and duration, the captions used, anything staged, and any skipped phase with the
reason. Do not mark a post done without having looked at its check sheet.
