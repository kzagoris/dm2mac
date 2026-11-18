"""Print the party's map and a walking route to a square, as control-socket requests (run inside WSL with py.sh).

  route.py SAVE              the whole party map: @ party, S stairs (^ up, v down), D door, # wall, . floor
  route.py SAVE X Y          also the shortest route over floor, doors and stairs to X,Y, as a JSON list of
                             requests for game.sh req (turns wait 12 ticks, steps 44)
"""
import ctypes
import json
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / 'tests'), str(ROOT / 'tools')]

from dm2test.lib import load_lib  # noqa: E402
from dm2test.save import GRAPHICS  # noqa: E402
from dm2test.structs import Game  # noqa: E402

DX, DY = (0, 1, 0, -1), (-1, 0, 1, 0)
WALL, FLOOR, PIT, STAIRS, DOOR = 0, 1, 2, 3, 4
STAIRS_UP = 0x04

lib = load_lib()
gdat = ctypes.c_void_p()
assert lib.gdat_load(str(GRAPHICS).encode(), ctypes.byref(gdat)) == 0
assert lib.save_load(sys.argv[1].encode(), gdat) == 0
game = Game.in_dll(lib, 'g_game')
m, px, py, pd = game.party_map, game.party_x, game.party_y, game.party_dir
w, h = lib.dungeon_map_width(m), lib.dungeon_map_height(m)
square = {(x, y): lib.dungeon_square(m, x, y) for x in range(w) for y in range(h)}
kind = {k: v >> 5 for k, v in square.items()}

print(f'map {m} {w}x{h} party {px},{py} dir {pd} (0 N, 1 E, 2 S, 3 W)')
print('    ' + ''.join(str(x % 10) for x in range(w)))
for y in range(h):
    row = ''
    for x in range(w):
        k = kind[(x, y)]
        if (x, y) == (px, py):
            row += '@'
        elif k == STAIRS:
            row += '^' if square[(x, y)] & STAIRS_UP else 'v'
        else:
            row += {WALL: '#', FLOOR: '.', PIT: 'o', DOOR: 'D'}.get(k, str(k))
    print(f'{y:3} {row}')

if len(sys.argv) == 4:
    goal = (int(sys.argv[2]), int(sys.argv[3]))
    prev = {(px, py): None}
    queue = deque([(px, py)])
    while queue:
        at = queue.popleft()
        if at == goal:
            break
        for d in range(4):
            nxt = (at[0] + DX[d], at[1] + DY[d])
            # 5 and 6 (teleporter, fake wall in DM1 terms) appear in walkable corridors of DM2 maps: allowed, and
            # the walk is checked with a state request afterwards
            if nxt in kind and nxt not in prev and (kind[nxt] in (FLOOR, DOOR, 5, 6) or nxt == goal):
                prev[nxt] = at
                queue.append(nxt)
    assert goal in prev, 'no route over floor and doors'
    path = []
    at = goal
    while at != (px, py):
        path.append(at)
        at = prev[at]
    path.reverse()
    requests, facing, here = [], pd, (px, py)
    for nxt in path:
        d = next(d for d in range(4) if (here[0] + DX[d], here[1] + DY[d]) == nxt)
        turn = (d - facing) % 4
        keys = {0: [], 1: ['right'], 2: ['right', 'right'], 3: ['left']}[turn]
        for key in keys:
            requests += [{'cmd': 'key', 'key': key}, {'cmd': 'wait', 'ticks': 12}]
        requests += [{'cmd': 'key', 'key': 'up'}, {'cmd': 'wait', 'ticks': 44}]
        facing, here = d, nxt
    print(f'route: {len(path)} squares, {len(requests) // 2} requests')
    print(json.dumps(requests))
