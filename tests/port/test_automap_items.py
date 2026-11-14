"""Current loose items on the advanced automap: visibility, markers, and inspection (src/ui/automap.c)."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.interface import c_screen
from dm2test.scenario import square_things
from dm2test.structs import Game, Location, Position, Session


@pytest.fixture
def item_map(lib, gdat, game_dir):
    """A fresh dungeon, with real graphics, without requiring the private start.DAT fixture."""
    lib.gdat_select(gdat)
    assert lib.zone_load(gdat) and lib.pal_load_color_ramps(gdat) and lib.text_init(gdat)
    lib.draw_init(gdat)
    Session.in_dll(lib, "g_session").data_dir = str(game_dir).encode()
    ctypes.c_int.in_dll(lib, "g_game_mode").value = C.GAME_MODE_ADVANCED
    assert lib.session_start_new_game()
    game = Game.in_dll(lib, "g_game")
    at = Position(game.party_map, game.party_x, game.party_y)
    for thing in square_things(lib, at.map, at.x, at.y):
        if C.THING_WEAPON <= thing >> C.THING_TYPE_SHIFT & C.THING_TYPE_MASK <= C.THING_MISC:
            lib.dungeon_unlink_thing_from_square(thing, at.map, at.x, at.y)
    lib.seen_reset()
    yield at
    lib.advanced_reset()
    lib.game_set_cheats(0)
    ctypes.c_int.in_dll(lib, "g_game_mode").value = C.GAME_MODE_VANILLA


def level_at(lib, at):
    return Location(lib.dungeon_map_offset_x(at.map) + at.x, lib.dungeon_map_offset_y(at.map) + at.y)


def drop(lib, at, kind=C.THING_MISC, index=0):
    thing = lib.dungeon_unused_thing(kind)
    assert thing != C.THING_NONE
    lib.item_set_gdat_index(thing, index)
    assert lib.dungeon_link_thing_to_square(thing, at.map, at.x, at.y)
    return thing


def items(lib, at, capacity=32):
    buffer = (ctypes.c_uint16 * capacity)()
    count = lib.automap_items(level_at(lib, at), buffer, capacity)
    return count, list(buffer)[:min(count, capacity)]


def viewport(lib):
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    assert lib.zone_get_viewport_rect(rect)
    return tuple(rect)


def party_icon(lib):
    x, y, width, height = viewport(lib)
    return x + (width - C.MAP_CELL_SIZE) // 2 + 1, y + (height - C.MAP_CELL_SIZE) // 2 + 1


def test_items_require_an_explored_square_or_reveal_map(lib, item_map):
    thing = drop(lib, item_map)
    assert items(lib, item_map) == (0, [])
    lib.game_set_cheats(C.CHEAT_MAP)
    assert items(lib, item_map) == (1, [thing])
    assert not lib.seen_is_seen(item_map), "Reveal map does not mark the square explored"
    lib.game_set_cheats(0)
    assert items(lib, item_map) == (0, [])
    lib.seen_mark(item_map, C.DIR_NORTH)
    assert items(lib, item_map) == (1, [thing])


def test_item_count_includes_missing_graphics_and_updates_after_pickup_and_drop(lib, item_map):
    lib.seen_mark(item_map, C.DIR_NORTH)
    things = [drop(lib, item_map, index=255) for _ in range(12)]
    assert items(lib, item_map, capacity=3) == (12, things[:3])
    assert items(lib, item_map, capacity=0) == (12, [])
    for thing in things:
        lib.dungeon_unlink_thing_from_square(thing, item_map.map, item_map.x, item_map.y)
    assert items(lib, item_map) == (0, [])
    assert lib.dungeon_link_thing_to_square(things[0], item_map.map, item_map.x, item_map.y)
    assert items(lib, item_map) == (1, things[:1])


def test_containers_count_once_and_effects_and_creature_possessions_stay_out(lib, item_map):
    lib.seen_mark(item_map, C.DIR_NORTH)
    chest = drop(lib, item_map, kind=C.THING_CONTAINER)
    contents = lib.dungeon_unused_thing(C.THING_MISC)
    lib.thing_set_word(contents, C.THING_NEXT, C.THING_ENDOFLIST)
    lib.thing_set_word(chest, C.CONTAINER_CONTENTS, contents)
    group = drop(lib, item_map, kind=C.THING_GROUP)
    held = lib.dungeon_unused_thing(C.THING_MISC)
    lib.thing_set_word(held, C.THING_NEXT, C.THING_ENDOFLIST)
    lib.thing_set_word(group, C.GROUP_POSSESSIONS, held)
    drop(lib, item_map, kind=C.THING_PROJECTILE)
    drop(lib, item_map, kind=C.THING_EXPLOSION)
    assert items(lib, item_map) == (1, [chest])


def test_item_marker_appears_on_explored_party_square_and_disappears_after_pickup(lib, item_map):
    thing = drop(lib, item_map, index=255)
    x, y = party_icon(lib)
    yellow = ctypes.POINTER(ctypes.c_uint8).in_dll(lib, "g_ui_colors")[C.UI_YELLOW]
    lib.advanced_open_automap()
    hidden = c_screen(lib)
    lib.seen_mark(item_map, C.DIR_NORTH)
    lib.advanced_draw_automap()
    shown = c_screen(lib)
    pixels = [(y + dy) * C.SCREEN_W + x + dx for dy in range(C.MAP_ICON_H) for dx in range(C.MAP_ICON_W)]
    assert any(shown[p] == yellow and hidden[p] != yellow for p in pixels)
    lib.dungeon_unlink_thing_from_square(thing, item_map.map, item_map.x, item_map.y)
    lib.advanced_draw_automap()
    assert not any(c_screen(lib)[p] == yellow for p in pixels)


def test_alcove_items_use_the_wall_square_and_invalid_edges_are_empty(lib, item_map):
    wall = next(Position(item_map.map, x, y)
                for x in range(lib.dungeon_map_width(item_map.map))
                for y in range(lib.dungeon_map_height(item_map.map))
                if lib.dungeon_square(item_map.map, x, y) >> C.SQUARE_TYPE_SHIFT == C.SQUARE_WALL)
    thing = drop(lib, wall)
    lib.game_set_cheats(C.CHEAT_MAP)
    assert thing in items(lib, wall)[1]
    assert lib.automap_items(Location(-100, -100), None, 0) == 0


def test_overlapping_maps_list_only_the_same_owner_as_the_map_picture(lib, item_map):
    location = Location(52, 28)
    owner = Position()
    assert lib.automap_level_square(location, ctypes.byref(owner))
    assert (owner.map, owner.x, owner.y) == (40, 6, 6)
    thing = drop(lib, owner)
    overlap = Position(12, 25, 1)
    drop(lib, overlap)
    lib.seen_mark(overlap, C.DIR_NORTH)
    assert not lib.seen_is_seen(owner)
    buffer = (ctypes.c_uint16 * 2)()
    assert lib.automap_items(location, buffer, 2) == 1
    assert buffer[0] == thing


@pytest.mark.parametrize("count", [1, 2, 9, 10, 15])
def test_plus_symbol_and_party_frame_remain_visible(lib, item_map, count):
    mark = "000010111010000"
    for _ in range(count):
        drop(lib, item_map)
    lib.seen_mark(item_map, C.DIR_NORTH)
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    colors = ctypes.POINTER(ctypes.c_uint8).in_dll(lib, "g_ui_colors")
    screen = c_screen(lib)
    assert [screen[(y + dy) * C.SCREEN_W + x + 4 + dx] for dy in range(5) for dx in range(3)] == [
        colors[C.UI_YELLOW if bit == "1" else C.UI_BLACK] for bit in mark]
    assert screen[(y + 3) * C.SCREEN_W + x - 1] == colors[C.UI_RED]


def click(lib, x, y):
    lib.advanced_screen_event(C.INPUT_BUTTON_LEFT, x, y)
    lib.advanced_screen_event(C.INPUT_LEFT_RELEASED, x, y)


def assert_item_row(lib, row, text):
    """Redrawing the expected complete item line must change no rendered pixel."""
    before = c_screen(lib)
    x, y, _, _ = viewport(lib)
    colors = ctypes.POINTER(ctypes.c_uint8).in_dll(lib, "g_ui_colors")
    lib.text_draw_on_screen(x + 8, y + 21 + row * 10, colors[C.UI_WHITE], colors[C.UI_BLACK], text.ljust(36))
    assert c_screen(lib) == before


def test_short_click_lists_item_names_and_groups_quantities(lib, item_map):
    thing = drop(lib, item_map)
    drop(lib, item_map)
    lib.seen_mark(item_map, C.DIR_NORTH)
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    click(lib, x + 2, y + 2)
    assert lib.automap_items_open()
    name = lib.item_name(thing) or b"UNKNOWN ITEM"
    assert_item_row(lib, 1, b"2 X " + name)
    lib.advanced_screen_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)
    assert not lib.automap_items_open()
    assert ctypes.c_int.in_dll(lib, "g_advanced_screen").value == C.ADVANCED_SCREEN_AUTOMAP
    assert lib.automap_select(x, y)
    lib.advanced_screen_event(C.INPUT_BUTTON_RIGHT, 0, 0)
    assert not lib.automap_items_open()
    assert ctypes.c_int.in_dll(lib, "g_advanced_screen").value == C.ADVANCED_SCREEN_AUTOMAP


def test_click_cannot_inspect_unexplored_items_but_reveal_map_can(lib, item_map):
    drop(lib, item_map)
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    click(lib, x, y)
    assert not lib.automap_items_open()
    assert not lib.automap_select(-1, -1)
    lib.game_set_cheats(C.CHEAT_MAP)
    click(lib, x, y)
    assert lib.automap_items_open()
    lib.advanced_screen_event(C.INPUT_KEY, C.KEY_AUTOMAP, 0)
    assert not lib.automap_items_open()
    assert ctypes.c_int.in_dll(lib, "g_advanced_screen").value == C.ADVANCED_SCREEN_NONE


def test_drag_and_release_without_a_press_never_open_item_details(lib, item_map):
    drop(lib, item_map)
    lib.seen_mark(item_map, C.DIR_NORTH)
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    lib.advanced_screen_event(C.INPUT_LEFT_RELEASED, x, y)
    assert not lib.automap_items_open()
    lib.advanced_screen_event(C.INPUT_BUTTON_LEFT, x, y)
    lib.advanced_automap_drag(x + C.MAP_CELL_SIZE, y)
    lib.advanced_automap_drag(x, y)
    lib.advanced_screen_event(C.INPUT_LEFT_RELEASED, x, y)
    assert not lib.automap_items_open(), "returning a drag to its start must not turn it into a click"
    lib.advanced_screen_event(C.INPUT_BUTTON_LEFT, x, y)
    lib.advanced_screen_event(C.INPUT_LEFT_RELEASED, x + 4, y)
    assert not lib.automap_items_open(), "a short drag below one square must not select"
    lib.automap_scroll(1, 0)
    assert lib.automap_select(x - C.MAP_CELL_SIZE, y), "selection follows the scrolled map"


def test_item_list_pages_expose_the_entire_pile_and_reset_on_close(lib, item_map):
    names = []
    for index in range(256):
        thing = drop(lib, item_map, index=index)
        name = lib.item_name(thing)
        if name and name not in names:
            names.append(name)
        else:
            lib.dungeon_unlink_thing_from_square(thing, item_map.map, item_map.x, item_map.y)
        if len(names) == 10:
            break
    assert len(names) == 10
    lib.seen_mark(item_map, C.DIR_NORTH)
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    click(lib, x, y)
    assert lib.automap_items_open()
    assert_item_row(lib, 1, b"1 X " + names[0])
    lib.advanced_screen_event(C.INPUT_KEY, C.KEYPAD_TURN_RIGHT, 0)
    assert_item_row(lib, 1, b"1 X " + names[8])
    assert_item_row(lib, 2, b"1 X " + names[9])
    lib.advanced_screen_event(C.INPUT_KEY, C.KEYPAD_TURN_LEFT, 0)
    assert_item_row(lib, 1, b"1 X " + names[0])
    rx, ry, _, _ = viewport(lib)
    click(lib, rx + 15, ry + 118)  # NEXT is the last row of the existing rows layout
    assert_item_row(lib, 1, b"1 X " + names[8])
    lib.advanced_reset()
    lib.advanced_open_automap()
    assert not lib.automap_items_open()
    click(lib, x, y)
    assert_item_row(lib, 1, b"1 X " + names[0])


def test_saved_items_and_exploration_reload_without_stale_inspection(lib, item_map, gdat, tmp_path):
    thing = drop(lib, item_map)
    name = lib.item_name(thing)
    lib.seen_mark(item_map, C.DIR_NORTH)
    path, backup = tmp_path / "items.dat", tmp_path / "items.bak"
    assert lib.save_write(str(path).encode(), str(backup).encode(), b"ITEMS", gdat) == C.SAVE_OK
    lib.dungeon_unlink_thing_from_square(thing, item_map.map, item_map.x, item_map.y)
    assert items(lib, item_map) == (0, [])
    assert lib.session_start_save(str(path).encode())
    count, restored = items(lib, item_map)
    assert count == 1 and lib.item_name(restored[0]) == name  # saves renumber thing references
    lib.advanced_open_automap()
    x, y = party_icon(lib)
    click(lib, x, y)
    assert lib.automap_items_open()
    assert lib.session_start_save(str(path).encode())
    assert not lib.automap_items_open()
