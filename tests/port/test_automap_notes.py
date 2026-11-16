"""Notes of the player on automap squares (src/engine/seen.c, ui/automap.c, ui/advanced.c): a double click types the
note of a square, a click shows it in the readout, a cyan pin marks the square, and the AMAP save block keeps it."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.interface import c_screen
from dm2test.modes import advanced_new_game, map_party_icon, screen_key
from dm2test.structs import MainLoop, Position

NOTE_MARK = "111111010010000"  # the pin, three pixels per row
SCAN_BYTES = range(0x100)  # every scan code byte of a key


@pytest.fixture
def new_game(lib, gdat, game_dir):
    """A fresh advanced game whose party square is seen, without notes."""
    with advanced_new_game(lib, gdat, game_dir) as game:
        lib.seen_reset()
        lib.seen_mark(party(game), game.party_dir)
        yield game


def party(game):
    return Position(game.party_map, game.party_x, game.party_y)


def note(lib, at):
    return lib.seen_note(at)


def screen_open(lib):
    return ctypes.c_int.in_dll(lib, "g_advanced_screen").value


def press(lib, x, y):
    lib.advanced_screen_event(C.INPUT_BUTTON_LEFT, x, y)
    lib.advanced_screen_event(C.INPUT_LEFT_RELEASED, x, y)


def wait_out_double_click(lib):
    MainLoop.in_dll(lib, "g_main_loop").timer_ticks += C.DOUBLE_CLICK_TICKS + 1



def type_text(lib, text):
    """Types text with the keys whose characters kbd_key_to_ascii gives."""
    keys = {}
    for shift in (C.KEY_SHIFTED, 0):  # the unshifted key wins for a letter
        for scan in SCAN_BYTES:
            ch = lib.kbd_key_to_ascii(scan | shift)
            if ch > 0:
                keys[chr(ch)] = scan | shift
    for ch in text:
        screen_key(lib, keys[ch])


def write_note(lib, x, y, text):
    """Double clicks the square at (x, y), types text and keeps it with Return."""
    press(lib, x, y)
    press(lib, x, y)
    type_text(lib, text)
    screen_key(lib, C.KEY_ENTER)
    wait_out_double_click(lib)


def other_squares(lib, count):
    """count map squares, map by map."""
    squares = []
    for m in range(lib.dungeon_map_count()):
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                squares.append(Position(m, x, y))
                if len(squares) == count:
                    return squares
    raise AssertionError("the dungeon has fewer squares")


def test_a_note_is_one_trimmed_line_of_note_characters(lib, new_game):
    at = party(new_game)
    assert note(lib, at) is None
    assert lib.seen_set_note(at, b"  NEEDS GOLD KEY ")
    assert note(lib, at) == b"NEEDS GOLD KEY"
    assert lib.seen_set_note(at, b"SHOP: ARMOUR (2)?")
    assert note(lib, at) == b"SHOP: ARMOUR (2)?", "a second note replaces the first"
    for bad in (b"lower", b"A#B", b"X" * (C.SEEN_NOTE_MAX + 1)):
        assert not lib.seen_set_note(at, bad)
    assert note(lib, at) == b"SHOP: ARMOUR (2)?"
    assert lib.seen_set_note(at, b"X" * C.SEEN_NOTE_MAX)
    assert not lib.seen_set_note(Position(at.map, -1, 0), b"OFF"), "outside the map"
    assert lib.seen_set_note(at, b"   ")
    assert note(lib, at) is None, "an empty note removes it"
    lib.seen_set_note(at, b"GONE")
    lib.seen_reset()
    assert note(lib, at) is None, "a reset forgets the notes"


def test_the_note_list_is_full_at_the_most_notes(lib, new_game):
    squares = other_squares(lib, C.SEEN_NOTES_MAX + 1)
    for i, at in enumerate(squares[:-1]):
        assert lib.seen_set_note(at, str(i).encode())
    assert not lib.seen_set_note(squares[-1], b"ONE MORE")
    assert lib.seen_set_note(squares[0], b"CHANGED"), "a full list still changes a note"
    assert lib.seen_set_note(squares[1], b"")
    assert lib.seen_set_note(squares[-1], b"ONE MORE"), "a removed note makes room"


def test_double_click_types_a_note_with_the_letters_that_move_the_map(lib, new_game):
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    level = lib.automap_level()
    write_note(lib, x, y, "doors: wasd qe m!")
    assert note(lib, party(new_game)) == b"DOORS: WASD QE M!"
    assert screen_open(lib) == C.ADVANCED_SCREEN_AUTOMAP, "M typed a letter and did not close the map"
    assert lib.automap_level() == level


def test_escape_cancels_and_an_emptied_note_is_removed(lib, new_game):
    at = party(new_game)
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    press(lib, x, y)
    press(lib, x, y)
    type_text(lib, "LOST")
    screen_key(lib, C.SCAN_ESCAPE)
    assert note(lib, at) is None
    assert screen_open(lib) == C.ADVANCED_SCREEN_AUTOMAP, "Esc leaves the editor, not the map"
    wait_out_double_click(lib)
    write_note(lib, x, y, "KEY")
    press(lib, x, y)
    press(lib, x, y)
    for _ in range(3):
        screen_key(lib, C.KEY_BACKSPACE)
    screen_key(lib, C.KEY_ENTER)
    assert note(lib, at) is None
    lib.seen_set_note(at, b"KEEP")
    wait_out_double_click(lib)
    press(lib, x, y)
    press(lib, x, y)
    type_text(lib, " MORE")
    lib.advanced_screen_event(C.INPUT_BUTTON_RIGHT, 0, 0)
    assert note(lib, at) == b"KEEP", "a right click cancels"


def test_two_slow_clicks_do_not_open_the_editor(lib, new_game):
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    press(lib, x, y)
    wait_out_double_click(lib)
    press(lib, x, y)
    screen_key(lib, C.KEY_AUTOMAP_M)
    assert screen_open(lib) == C.ADVANCED_SCREEN_NONE, "M closes the map outside the editor"


def test_double_click_on_an_unseen_square_types_no_note(lib, new_game):
    lib.seen_reset()
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    write_note(lib, x, y, "Q")
    assert note(lib, party(new_game)) is None


def test_the_pin_marks_a_noted_square(lib, new_game):
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    colors = ctypes.POINTER(ctypes.c_uint8).in_dll(lib, "g_ui_colors")
    screen = c_screen(lib)
    without = [screen[(y + dy) * C.SCREEN_W + x + dx] for dy in range(5) for dx in range(3)]
    lib.seen_set_note(party(new_game), b"PIN")
    lib.advanced_draw_automap()
    screen = c_screen(lib)
    pin = [screen[(y + dy) * C.SCREEN_W + x + dx] for dy in range(5) for dx in range(3)]
    assert pin == [colors[C.UI_CYAN if bit == "1" else C.UI_BLACK] for bit in NOTE_MARK] and pin != without


def test_a_click_shows_the_note_in_the_readout(lib, new_game):
    lib.advanced_open_automap()
    blank = c_screen(lib)
    lib.seen_set_note(party(new_game), b"NEEDS GOLD KEY")
    lib.advanced_draw_automap()
    x, y = map_party_icon(lib)
    before = c_screen(lib)
    press(lib, x, y)
    shown = c_screen(lib)
    assert shown != before
    wait_out_double_click(lib)
    screen_key(lib, C.SCAN_SPACE)
    assert c_screen(lib) == shown, "the note stays in the readout"
    lib.seen_set_note(party(new_game), b"")
    lib.advanced_draw_automap()
    assert c_screen(lib) == blank, "a removed note leaves the readout and the map"


def test_double_click_on_an_item_square_closes_the_list_and_types(lib, new_game):
    at = party(new_game)
    thing = lib.dungeon_unused_thing(C.THING_MISC)
    assert lib.dungeon_link_thing_to_square(thing, at.map, at.x, at.y)
    lib.advanced_open_automap()
    x, y = map_party_icon(lib)
    press(lib, x, y)
    assert lib.automap_items_open()
    lib.advanced_screen_event(C.INPUT_BUTTON_LEFT, x, y)
    assert not lib.automap_items_open()
    type_text(lib, "LOOT")
    screen_key(lib, C.KEY_ENTER)
    assert note(lib, at) == b"LOOT"


def save_and_load(lib, gdat, path, change=None):
    assert lib.save_write(str(path).encode(), str(path.with_suffix(".BAK")).encode(), b"NOTES", gdat) == C.SAVE_OK
    if change:
        path.write_bytes(change(path.read_bytes()))
    lib.seen_reset()
    assert lib.save_load(str(path).encode(), gdat) == C.SAVE_OK


def test_notes_are_saved_with_the_seen_squares(lib, gdat, new_game, tmp_path):
    at = party(new_game)
    other = other_squares(lib, 1)[0]
    lib.seen_set_note(at, b"NEEDS GOLD KEY")
    lib.seen_set_note(other, b"SHOP: ARMOUR")
    save_and_load(lib, gdat, tmp_path / "N.DAT")
    assert lib.seen_is_seen(at)
    assert note(lib, at) == b"NEEDS GOLD KEY" and note(lib, other) == b"SHOP: ARMOUR"


def test_a_version_1_block_loads_the_squares_without_notes(lib, gdat, new_game, tmp_path):
    at = party(new_game)
    lib.seen_set_note(at, b"NEW")

    def version_1(data):
        start = data.index(b"AMAP\x02")
        end = data.index(b"CHTS\x01", start)
        squares = end - len(b"NEW") - 4 - 2  # the note text, its 4-byte header and the note count
        return data[:start] + b"AMAP\x01" + data[start + 5:squares] + data[end:]

    save_and_load(lib, gdat, tmp_path / "V1.DAT", version_1)
    assert lib.seen_is_seen(at) and note(lib, at) is None


@pytest.mark.parametrize("damage", ["character", "length", "count"])
def test_a_damaged_note_gives_an_empty_automap(lib, gdat, new_game, tmp_path, damage):
    at = party(new_game)
    lib.seen_set_note(at, b"NEW")

    def damaged(data):
        end = data.index(b"CHTS\x01", data.index(b"AMAP\x02"))
        data = bytearray(data)
        if damage == "character":
            data[end - 1] = ord("#")
        elif damage == "length":
            data[end - len(b"NEW") - 1] = 0
        else:
            data[end - len(b"NEW") - 4 - 2] = 2  # a second note that is not there
        return bytes(data)

    save_and_load(lib, gdat, tmp_path / "D.DAT", damaged)
    assert not lib.seen_is_seen(at) and note(lib, at) is None
