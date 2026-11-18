"""Inspect and stage disposable saves through build/libdm2.so (run inside WSL with py.sh).

  dungeon.py party SAVE                  party map/position/direction and the map around it (@ party, M creature)
  dungeon.py creatures SAVE              the mobile creature groups of every map (static ones are left out)
  dungeon.py stage SAVE OUT MAP:KIND     move the first group of creature KIND on MAP to the square in front of the
                                         party, at its normal full health, and write OUT

  dungeon.py faces SAVE KIND [MAP]       floor squares that face a wall that reacts: KIND hand (1, a lever or button) or item (2,
                                         a keyhole or slot); prints MAP X Y DIR and, in [], the things on the square ahead
  dungeon.py place SAVE OUT MAP X Y DIR  put the party on that square, facing DIR (0 N, 1 E, 2 S, 3 W), write OUT
  dungeon.py items SAVE [MAP]            squares with floor items: MAP X Y and the thing types

  dungeon.py moveitem SAVE OUT FMAP FX FY TMAP TX TY [TYPE]
                                         move the first floor item (thing type 5-11, or TYPE) of FMAP FX,FY onto TMAP TX,TY

Staging only places things; everything filmed afterwards is played in the real game.
"""
import ctypes
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / 'tests'), str(ROOT / 'tools')]

from dm2test import constants as C  # noqa: E402
from dm2test.lib import load_lib  # noqa: E402
from dm2test.save import GRAPHICS  # noqa: E402
from dm2test.structs import Game, Location  # noqa: E402

DX, DY = (0, 1, 0, -1), (-1, 0, 1, 0)
CREATURE_STATIC, CREATURE_NON_MATERIAL = 0x0001, 0x0020

lib = load_lib()
lib.creature_group_info.restype = ctypes.c_void_p
gdat = ctypes.c_void_p()
assert lib.gdat_load(str(GRAPHICS).encode(), ctypes.byref(gdat)) == 0


def load(path):
    assert lib.save_load(str(path).encode(), gdat) == 0, f'cannot load {path}'
    return Game.in_dll(lib, 'g_game')


def info(group):
    """(flags, max health) of the creature type of group."""
    address = lib.creature_group_info(group)
    return ctypes.c_uint16.from_address(address).value, ctypes.c_uint16.from_address(address + 4).value


def groups(m):
    lib.dungeon_set_current_map(m)
    for x in range(lib.dungeon_map_width(m)):
        for y in range(lib.dungeon_map_height(m)):
            group = lib.creature_get_group_at(Location(x, y))
            if group != C.THING_NONE:
                yield group, x, y


def party(path):
    game = load(path)
    m, px, py = game.party_map, game.party_x, game.party_y
    print(f'map {m} party {px},{py} dir {game.party_dir} (0 N, 1 E, 2 S, 3 W)')
    found = {(x, y): g for g, x, y in groups(m)}
    for y in range(max(0, py - 8), min(lib.dungeon_map_height(m), py + 9)):
        row = ''
        for x in range(max(0, px - 12), min(lib.dungeon_map_width(m), px + 13)):
            kind = lib.dungeon_square_at(Location(x, y)) >> 5
            row += '@' if (x, y) == (px, py) else 'M' if (x, y) in found else '#.'[kind] if kind < 2 else str(kind)
        print('  ', row)
    print('  # wall, . floor, digits other square types')


def creatures(path):
    load(path)
    for m in range(lib.dungeon_map_count()):
        out = []
        for g, x, y in groups(m):
            flags, health = info(g)
            if not flags & CREATURE_STATIC:
                melee = 'ghost' if flags & CREATURE_NON_MATERIAL else 'solid'
                out.append(f'kind {lib.thing_word(g, C.GROUP_CREATURE) & 255} at {x},{y} hp {health} {melee}')
        if out:
            print(f'map {m}: ' + '; '.join(out[:6]))


def stage(path, out, spec):
    src_map, kind = map(int, spec.split(':'))
    game = load(path)
    m, d = game.party_map, game.party_dir
    at = Location(game.party_x + DX[d], game.party_y + DY[d])
    lib.dungeon_set_current_map(m)
    assert lib.dungeon_square_at(at) >> 5 == 1, 'no floor in front of the party'
    assert lib.creature_get_group_at(at) == C.THING_NONE, 'a creature already stands in front of the party'
    group = next(((g, x, y) for g, x, y in groups(src_map) if lib.thing_word(g, C.GROUP_CREATURE) & 255 == kind), None)
    assert group, f'no creature kind {kind} on map {src_map}'
    g, x, y = group
    lib.dungeon_unlink_thing_from_square(g, src_map, x, y)
    lib.dungeon_set_current_map(m)
    assert lib.dungeon_link_thing_to_square(g, m, at.x, at.y)
    # never above the maximum: a group staged with 1500 health died at the first hit
    lib.thing_set_word(g, C.GROUP_HEALTH, info(g)[1])
    out = Path(out)
    assert lib.save_write(str(out).encode(), str(out.with_suffix('.BAK')).encode(), b'STAGED', gdat) == C.SAVE_OK
    out.with_suffix('.BAK').unlink(missing_ok=True)
    print(f'wrote {out}: kind {kind} from map {src_map} {x},{y} to map {m} {at.x},{at.y}')


def things(m, x, y):
    out, t = [], lib.dungeon_square_first_thing(m, x, y)
    while t != 0xFFFE:
        out.append(t >> 10 & 15)
        t = lib.dungeon_next_thing(t)
    return out


def faces(path, kind, only=None):
    from dm2test.scenario import sensor_squares
    load(path)
    lib.sensor_wall_touch.argtypes = [Location, ctypes.c_int]
    want = {'hand': C.WALL_TOUCH_HAND, 'item': C.WALL_TOUCH_ITEM}[kind]
    for m, x, y, cell in sensor_squares(lib):
        if only is not None and m != int(only):
            continue
        if lib.dungeon_square(m, x, y) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_WALL:
            continue
        px, py = x + DX[cell], y + DY[cell]
        if not (0 <= px < lib.dungeon_map_width(m) and 0 <= py < lib.dungeon_map_height(m)):
            continue
        if lib.dungeon_square(m, px, py) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_FLOOR:
            continue
        lib.dungeon_set_current_map(m)
        if lib.sensor_wall_touch(Location(x, y), cell) == want:
            d = (cell + 2) & 3
            ax, ay = px + DX[d], py + DY[d]
            print(f'map {m} at {px},{py} dir {d}  wall {x},{y} things_here {things(m, px, py)}')


def place(path, out, m, x, y, d):
    game = load(path)
    game.party_map, game.party_x, game.party_y, game.party_dir = int(m), int(x), int(y), int(d)
    out = Path(out)
    assert lib.save_write(str(out).encode(), str(out.with_suffix('.BAK')).encode(), b'STAGED', gdat) == C.SAVE_OK
    out.with_suffix('.BAK').unlink(missing_ok=True)
    print(f'wrote {out}: map {m} {x},{y} dir {d}')


def moveitem(path, out, fm, fx, fy, tm, tx, ty, kind=None):
    load(path)
    fm, fx, fy, tm, tx, ty = map(int, (fm, fx, fy, tm, tx, ty))
    t = lib.dungeon_square_first_thing(fm, fx, fy)
    while t != 0xFFFE and not (5 <= (t >> 10 & 15) <= 11 and (kind is None or t >> 10 & 15 == int(kind))):
        t = lib.dungeon_next_thing(t)
    assert t != 0xFFFE, 'no item there'
    lib.dungeon_set_current_map(fm)
    lib.dungeon_unlink_thing_from_square(t, fm, fx, fy)
    lib.dungeon_set_current_map(tm)
    assert lib.dungeon_link_thing_to_square(t, tm, tx, ty)
    out = Path(out)
    assert lib.save_write(str(out).encode(), str(out.with_suffix('.BAK')).encode(), b'STAGED', gdat) == C.SAVE_OK
    out.with_suffix('.BAK').unlink(missing_ok=True)
    print(f'wrote {out}: item type {t >> 10 & 15} moved {fm}:{fx},{fy} -> {tm}:{tx},{ty}')


def items(path, only=None):
    load(path)
    for m in range(lib.dungeon_map_count()):
        if only is not None and m != int(only):
            continue
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                if lib.dungeon_square(m, x, y) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_FLOOR:
                    continue
                t = [k for k in things(m, x, y) if 5 <= k <= 11]
                if t:
                    print(f'map {m} {x},{y} {t}')


if __name__ == '__main__':
    command, *args = sys.argv[1:]
    {'party': party, 'creatures': creatures, 'stage': stage, 'faces': faces, 'place': place, 'items': items, 'moveitem': moveitem}[command](*args)
