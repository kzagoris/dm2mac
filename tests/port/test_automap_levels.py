"""The levels of the advanced automap (src/ui/automap.c, ui/advanced.c): Q and E show the visited levels above and below,
Space shows the party again, and closing the map forgets the shown level."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.modes import advanced_new_game, screen_key
from dm2test.structs import Game, Location, Position

SCAN_Q, SCAN_E, SCAN_SPACE, SCAN_ESCAPE = C.SCAN_Q, C.SCAN_E, C.SCAN_SPACE, C.SCAN_ESCAPE


@pytest.fixture
def new_game(lib, gdat, game_dir):
    """A fresh advanced game with an empty automap, without requiring the private start.DAT fixture."""
    with advanced_new_game(lib, gdat, game_dir) as game:
        lib.seen_reset()
        yield game


def level_maps(lib, level):
    maps = ctypes.cast(lib.dungeon_level_maps(level), ctypes.POINTER(ctypes.c_uint8))
    found = []
    while maps[len(found)] != C.LEVEL_LIST_END:
        found.append(maps[len(found)])
    return found


def levels_with_maps(lib):
    return [level for level in range(C.LEVEL_COUNT) if level_maps(lib, level)]



def see_a_square(lib, level):
    """Marks the first open square of the first map of level as seen."""
    map_ = level_maps(lib, level)[0]
    for x in range(lib.dungeon_map_width(map_)):
        for y in range(lib.dungeon_map_height(map_)):
            if not lib.dungeon_square_blocks_movement(lib.dungeon_square(map_, x, y)):
                lib.seen_mark(Position(map_, x, y), C.DIR_NORTH)
                assert lib.seen_map_has_seen(map_)
                return map_
    raise AssertionError(f"level {level} has no open square")


def test_only_the_party_level_is_visited_on_an_empty_automap(lib, new_game):
    party = lib.dungeon_map_level(new_game.party_map)
    assert [level for level in range(C.LEVEL_COUNT) if lib.automap_level_visited(level)] == [party]
    lib.advanced_open_automap()
    fb = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, "g_video_fb")
    before = bytes(fb)
    for scan in (SCAN_Q, SCAN_E):
        screen_key(lib, scan)
        assert lib.automap_level() == party
        assert bytes(fb) != before, "the readout tells that there is no visited level"
    screen_key(lib, C.SCAN_SPACE)
    assert bytes(fb) == before, "the next key clears the notice"


def test_reveal_map_visits_every_level_with_maps(lib, new_game):
    lib.game_set_cheats(C.CHEAT_MAP)
    visited = [level for level in range(C.LEVEL_COUNT) if lib.automap_level_visited(level)]
    assert visited == levels_with_maps(lib) and len(visited) > 1
    assert not lib.automap_level_visited(-1) and not lib.automap_level_visited(C.LEVEL_COUNT)


def test_q_and_e_skip_unvisited_levels_and_space_shows_the_party(lib, new_game):
    party = lib.dungeon_map_level(new_game.party_map)
    others = [level for level in levels_with_maps(lib) if level != party]
    below = [level for level in others if level > party]
    target = below[-1] if below else others[0]  # the farthest level: the levels between stay unvisited
    see_a_square(lib, target)
    step, back = (SCAN_E, SCAN_Q) if target > party else (SCAN_Q, SCAN_E)
    lib.advanced_open_automap()
    screen_key(lib, back)
    assert lib.automap_level() == party, "no visited level on the other side"
    screen_key(lib, step)
    assert lib.automap_level() == target
    screen_key(lib, step)
    assert lib.automap_level() == target, "no visited level past it"
    screen_key(lib, back)
    assert lib.automap_level() == party
    screen_key(lib, step)
    screen_key(lib, SCAN_SPACE)
    assert lib.automap_level() == party
    screen_key(lib, step)
    screen_key(lib, SCAN_ESCAPE)
    assert not ctypes.c_int.in_dll(lib, "g_advanced_screen").value
    assert lib.automap_level() == party, "a closed automap opens on the party level"


def test_squares_of_the_shown_level_are_drawn(lib, new_game):
    party = lib.dungeon_map_level(new_game.party_map)
    target = next(level for level in levels_with_maps(lib) if level != party)
    map_ = see_a_square(lib, target)
    seen = next((x, y) for x in range(lib.dungeon_map_width(map_)) for y in range(lib.dungeon_map_height(map_))
                if lib.seen_is_seen(Position(map_, x, y)))
    at = Location(lib.dungeon_map_offset_x(map_) + seen[0], lib.dungeon_map_offset_y(map_) + seen[1])
    assert not lib.automap_level_seen(at), "the party level does not show the square"
    lib.advanced_open_automap()
    screen_key(lib, SCAN_E if target > party else SCAN_Q)
    assert lib.automap_level() == target
    assert lib.automap_level_seen(at)
    square = Position()
    assert lib.automap_level_square(at, ctypes.byref(square)) and lib.dungeon_map_level(square.map) == target


def party_frame(lib, dx=0):
    """A pixel of the red frame around the party square, dx squares right of the middle: the middle of the left side,
    or of the top side when the party faces west (the side toward the party direction is white)."""
    rect = (ctypes.c_int16 * 4)()
    assert lib.zone_get_rect(C.ZONE_VIEWPORT, rect)
    x, y, w, h = rect
    cell = C.MAP_CELL_SIZE
    px, py = (cell // 2, 0) if Game.in_dll(lib, "g_game").party_dir == C.DIR_WEST else (0, cell // 2)
    fb = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, "g_video_fb")
    return fb[(y + (h - cell) // 2 + py) * C.SCREEN_W + x + (w - cell) // 2 + dx * cell + px]


def test_arrows_and_letters_scroll_the_map_one_square(lib, new_game):
    red = ctypes.POINTER(ctypes.c_uint8).in_dll(lib, "g_ui_colors")[C.UI_RED]
    state = (new_game.game_time, new_game.party_x, new_game.party_y, new_game.party_dir)
    lib.seen_mark(Position(new_game.party_map, new_game.party_x, new_game.party_y), new_game.party_dir)
    lib.advanced_open_automap()
    assert party_frame(lib) == red
    for left, right in ((C.KEYPAD_TURN_LEFT, C.KEYPAD_TURN_RIGHT), (C.SCAN_A, C.SCAN_D)):
        screen_key(lib, left)  # the middle moves one square left: the party shows one square right
        assert party_frame(lib, 1) == red and party_frame(lib) != red
        screen_key(lib, right)
        assert party_frame(lib) == red
    screen_key(lib, C.KEYPAD_FORWARD)
    assert party_frame(lib) != red
    for scan in (C.KEYPAD_BACK, C.SCAN_S, C.SCAN_W):
        screen_key(lib, scan)
    assert party_frame(lib) == red
    screen_key(lib, C.KEYPAD_TURN_RIGHT)
    screen_key(lib, SCAN_SPACE)
    assert party_frame(lib) == red, "Space shows the party again"
    assert (new_game.game_time, new_game.party_x, new_game.party_y, new_game.party_dir) == state
