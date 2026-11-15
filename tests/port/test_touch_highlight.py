"""The touch highlight of the advanced mode (src/ui/highlight.c): while Alt is held, brackets mark the click zones of
the view that a hand click reaches; sensor_wall_touch (src/engine/sensor.c) tells which wall faces react."""

import ctypes

from dm2test import constants as C
from dm2test.modes import VANILLA, set_mode
from dm2test.scenario import DIRS, sensor_squares
from dm2test.structs import Bitmap, CurrentMap, Location, Position


def lib_prototypes(lib):
    lib.sensor_wall_touch.argtypes = [Location, ctypes.c_int]
    lib.highlight_set_held.argtypes = [ctypes.c_bool]
    lib.highlight_is_due.restype = ctypes.c_bool
    lib.dungeon_set_party_position.argtypes = [Position]
    lib.view_draw_dungeon.argtypes = [Location, ctypes.c_int16]


def viewport_pixels(lib):
    rect = (ctypes.c_int16 * 4)()
    assert lib.zone_get_viewport_rect(rect)
    screen = Bitmap.in_dll(lib, "g_draw_screen")
    base = ctypes.addressof(screen.pixels.contents)
    return b"".join(ctypes.string_at(base + (rect[1] + y) * screen.width + rect[0], rect[2]) for y in range(rect[3]))


def zone_touches(lib):
    count = ctypes.c_int.in_dll(lib, "g_view_click_zone_count").value
    return [lib.input_zone_touch(i) for i in range(count)]


def reactive_walls(lib, touch):
    """(map, x, y, dir): a floor square that faces a wall face of the touch kind."""
    out = []
    for map_, x, y, cell in sensor_squares(lib):
        if lib.dungeon_square(map_, x, y) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_WALL:
            continue
        px, py = x + DIRS[cell][0], y + DIRS[cell][1]
        inside = 0 <= px < lib.dungeon_map_width(map_) and 0 <= py < lib.dungeon_map_height(map_)
        if not inside or lib.dungeon_square(map_, px, py) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_FLOOR:
            continue
        lib.dungeon_set_current_map(map_)
        if lib.sensor_wall_touch(Location(x, y), cell) == touch:
            out.append((map_, px, py, (cell + 2) & 3))
    return out


def face(lib, game, place):
    """Puts the party on the square of place, facing its wall, and shows the view."""
    map_, x, y, d = place
    game.party_dir = d
    CurrentMap.in_dll(lib, "g_current_map").map = -1
    lib.dungeon_set_party_position(Position(map_, x, y))
    lib.view_enter_map(map_)
    lib.dungeon_set_current_map(map_)
    lib.view_draw_dungeon(Location(x, y), d)
    lib.play_present_viewport()


def test_alt_marks_a_wall_button_and_its_release_restores_the_view(lib, advanced):
    lib_prototypes(lib)
    game, _ = advanced
    for place in reactive_walls(lib, C.WALL_TOUCH_HAND):
        face(lib, game, place)
        if C.TOUCH_HAND in zone_touches(lib):
            break
    else:
        raise AssertionError("no wall face in the view reacts to the hand")
    plain = viewport_pixels(lib)
    lib.highlight_set_held(True)
    assert lib.highlight_is_due()
    lib.play_present_viewport()
    marked = viewport_pixels(lib)
    cyan = ctypes.c_uint8.from_address(ctypes.c_void_p.in_dll(lib, "g_ui_colors").value + C.UI_CYAN).value
    assert marked.count(bytes([cyan])) > plain.count(bytes([cyan]))
    assert not lib.highlight_is_due()
    lib.highlight_set_held(False)
    assert lib.highlight_is_due()
    lib.play_present_viewport()
    assert viewport_pixels(lib) == plain


def test_the_vanilla_mode_shows_no_highlight(lib, advanced):
    lib_prototypes(lib)
    game, _ = advanced
    face(lib, game, reactive_walls(lib, C.WALL_TOUCH_HAND)[0])
    set_mode(lib, VANILLA)
    plain = viewport_pixels(lib)
    lib.highlight_set_held(True)
    assert not lib.highlight_is_due()
    lib.play_present_viewport()
    assert viewport_pixels(lib) == plain


def test_wall_faces_tell_buttons_locks_and_plain_walls_apart(lib, session):
    # every wall face of the dungeon with a sensor or a text: buttons react to the hand, keyholes and slots want an
    # item, and faces with only other sensors (floor and timer logic) or plain texts stay unmarked
    lib_prototypes(lib)
    kinds = {C.WALL_TOUCH_NONE: 0, C.WALL_TOUCH_HAND: 0, C.WALL_TOUCH_ITEM: 0}
    keyholes = []
    for map_, x, y, cell in sensor_squares(lib):
        if lib.dungeon_square(map_, x, y) >> C.SQUARE_TYPE_SHIFT != C.SQUARE_WALL:
            continue
        lib.dungeon_set_current_map(map_)
        touch = lib.sensor_wall_touch(Location(x, y), cell)
        kinds[touch] += 1
        t = lib.dungeon_square_first_thing(map_, x, y)
        while t not in (C.THING_ENDOFLIST, C.THING_NONE):
            if t >> 14 == cell and t >> 10 & 15 == C.THING_SENSOR and lib.thing_word(t, 2) & 0x7F == C.WALL_CLICK_ITEM:
                keyholes.append(touch)
            t = lib.dungeon_next_thing(t)
    assert all(kinds.values()), kinds
    assert keyholes and set(keyholes) == {C.WALL_TOUCH_ITEM}, keyholes
