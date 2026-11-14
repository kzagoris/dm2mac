"""The spell list of the advanced mode (src/ui/spellbook.c) on start.DAT: the listed spells, the cast by name through
the normal rune entry, the option that lists every spell, and the hints of the rune panel."""
import ctypes
import struct

import pytest

from dm2test import constants as C
from dm2test.modes import VANILLA, press, set_mode
from dm2test.scenario import champion_bytes, champion_records

LIGHT, MAGIC_TORCH, FIREBALL = 0, 5, 16  # spell table indices: OH IR RA; FUL; FUL IR
LO, FUL, IR, VEN = 0, 3, 3, 0  # the place of the rune in its rune set
GLYPH_OH, GLYPH_FUL = 0x68, 0x69
OPTIONS_ROW_SPELLS = 2
SCAN_1 = 0x02  # the keys 1 to 4 of the spell list
KEY_ALT_C, KEY_ALT_1 = 0x082E, 0x0802  # the favorite keys: Alt+C, then Alt+1 to Alt+4
# Champion record offsets (the Champion struct of engine/champion.h).
CH_RUNE_COUNT, CH_MANA = 0x1E, 0x3E
FULL_MANA = 100


@pytest.fixture
def book(lib, advanced):
    """start.DAT in the advanced mode with no known spell, only the known spells listed, and a caster with mana."""
    lib.magic_reset_favorites()
    if lib.upscale_has_all_spells():
        lib.upscale_toggle_all_spells()
    champion_bytes(lib, 0, CH_MANA, struct.pack('<h', FULL_MANA))
    yield advanced
    if lib.upscale_has_all_spells():
        lib.upscale_toggle_all_spells()
    ctypes.c_int.in_dll(lib, 'g_advanced_screen').value = C.ADVANCED_SCREEN_NONE


def screen(lib):
    return ctypes.c_int.in_dll(lib, 'g_advanced_screen').value


def mana(lib):
    return struct.unpack_from('<h', bytes(champion_records(lib)), CH_MANA)[0]


def key(lib, scan):
    lib.advanced_screen_event(C.INPUT_KEY, scan, 0)


def messages(lib):
    return [lib.text_message_row(row) for row in range(C.MESSAGE_ROWS)]


def test_return_casts_the_marked_known_spell_with_the_normal_costs(lib, book):
    torch = lib.magic_get_spell(MAGIC_TORCH)
    lib.magic_learn_spell(torch)
    lib.advanced_open_spellbook()
    assert screen(lib) == C.ADVANCED_SCREEN_SPELLBOOK
    key(lib, C.KEY_ENTER)
    assert screen(lib) == C.ADVANCED_SCREEN_NONE, "the cast closes the spell list"
    assert mana(lib) == FULL_MANA - lib.magic_spell_mana(torch, 1)
    # the normal cast of entered runes ran: it took the runes and recorded them as the last attempt
    assert bytes(champion_records(lib))[CH_RUNE_COUNT] == 0
    assert lib.magic_favorite_rune(0, C.MAGIC_FAVORITE_LAST, 1) == GLYPH_FUL
    assert lib.magic_favorite_rune(0, C.MAGIC_FAVORITE_LAST, 2) == 0


def test_a_spell_is_listed_after_its_cast_or_when_the_option_lists_all(lib, book):
    lib.advanced_open_spellbook()
    for _ in range(book[0].champion_count):
        key(lib, C.KEY_ENTER)  # no spell is listed: the caster row is selected and goes through the casters
    assert screen(lib) == C.ADVANCED_SCREEN_SPELLBOOK
    assert mana(lib) == FULL_MANA
    key(lib, C.SCAN_ESCAPE)
    assert screen(lib) == C.ADVANCED_SCREEN_NONE
    lib.upscale_toggle_all_spells()
    assert (book[1] / 'SETTINGS.TXT').read_text().endswith('spells=all\n')
    lib.advanced_open_spellbook()
    key(lib, C.KEY_ENTER)  # the first spell of the spell table is marked
    assert screen(lib) == C.ADVANCED_SCREEN_NONE
    assert mana(lib) == FULL_MANA - lib.magic_spell_mana(lib.magic_get_spell(LIGHT), 1)
    assert lib.magic_favorite_rune(0, C.MAGIC_FAVORITE_LAST, 1) == GLYPH_OH


def test_the_favorites_page_goes_back_to_the_spell_list(lib, book):
    lib.magic_learn_spell(lib.magic_get_spell(MAGIC_TORCH))
    lib.advanced_open_spellbook()
    for _ in range(C.ROWS_MAX_COUNT):
        key(lib, C.SCAN_S)  # down to the title item, CLOSE
    key(lib, C.SCAN_W)  # FAVORITES
    key(lib, C.KEY_ENTER)
    key(lib, C.SCAN_ESCAPE)  # back to the spell list with the marked spell selected
    assert screen(lib) == C.ADVANCED_SCREEN_SPELLBOOK
    key(lib, C.KEY_ENTER)
    assert screen(lib) == C.ADVANCED_SCREEN_NONE
    assert lib.magic_favorite_rune(0, C.MAGIC_FAVORITE_LAST, 1) == GLYPH_FUL


def test_a_cast_of_entered_runes_makes_the_spell_known(lib, book):
    lib.action_area_open_spells(0)
    for rune in (LO, FUL):
        lib.spellbook_add_rune(rune)
    lib.magic_cast_entered_runes_failed()
    assert lib.magic_is_spell_known(lib.magic_get_spell(MAGIC_TORCH))
    assert not lib.magic_is_spell_known(lib.magic_get_spell(FIREBALL))


def test_the_rune_panel_names_listed_spells_and_marks_their_next_runes(lib, book):
    lib.action_area_open_spells(0)
    assert not lib.spellbook_is_next_rune(FUL), "no hint before the power rune"
    for rune in (LO, FUL):
        lib.spellbook_add_rune(rune)
    assert not lib.spellbook_is_next_rune(IR), "no spell is known"
    assert not any(b'MAGIC TORCH' in row for row in messages(lib))
    lib.magic_learn_spell(lib.magic_get_spell(FIREBALL))
    assert lib.spellbook_is_next_rune(IR)
    assert not lib.spellbook_is_next_rune(VEN)
    lib.spellbook_add_rune(IR)
    assert b'FUL IR: FIREBALL' in messages(lib)
    assert not lib.spellbook_is_next_rune(0), "no known spell is longer"
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    lib.upscale_toggle_all_spells()
    lib.spellbook_add_rune(FUL)
    assert b'FUL: MAGIC TORCH' in messages(lib)
    lib.spellbook_add_rune(VEN)
    assert b'NO SPELL HAS THESE RUNES' in messages(lib)
    set_mode(lib, VANILLA)
    assert not lib.spellbook_is_next_rune(IR)


def test_the_options_row_changes_the_listed_spells(lib, book):
    lib.advanced_open_options()
    for _ in range(OPTIONS_ROW_SPELLS):
        key(lib, C.SCAN_S)
    key(lib, C.KEY_ENTER)
    assert lib.upscale_has_all_spells()
    assert (book[1] / 'SETTINGS.TXT').read_text().endswith('spells=all\n')
    key(lib, C.KEY_ENTER)
    assert not lib.upscale_has_all_spells()
    assert (book[1] / 'SETTINGS.TXT').read_text().endswith('spells=known\n')


def test_the_favorite_keys_set_the_entered_runes_and_cast_the_favorite(lib, book):
    torch = lib.magic_get_spell(MAGIC_TORCH)
    lib.magic_learn_spell(torch)
    lib.action_area_open_spells(0)
    for rune in (LO, FUL):
        lib.spellbook_add_rune(rune)
    press(lib, KEY_ALT_1)
    assert lib.magic_favorite_rune(0, 0, 1) == 0, "Alt+1 without Alt+C before it does nothing"
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1)
    assert lib.magic_favorite_rune(0, 0, 1) == GLYPH_FUL
    assert b'MAGIC TORCH SET AS FAVORITE 1' in messages(lib)
    assert bytes(champion_records(lib))[CH_RUNE_COUNT] == 2, "the entered runes stay"
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    champion_bytes(lib, 0, CH_MANA, struct.pack('<h', FULL_MANA))
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1 + 1)
    assert b'FAVORITE 2 IS EMPTY' in messages(lib)
    assert mana(lib) == FULL_MANA
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1)
    assert b'CAST FAVORITE 1: MAGIC TORCH' in messages(lib)
    assert bytes(champion_records(lib))[CH_RUNE_COUNT] == 0
    assert mana(lib) == FULL_MANA - lib.magic_spell_mana(torch, 1)


def test_a_number_key_sets_the_marked_spell_as_a_favorite_and_backspace_clears_it(lib, book):
    lib.magic_learn_spell(lib.magic_get_spell(MAGIC_TORCH))
    lib.advanced_open_spellbook()
    key(lib, SCAN_1 + 1)
    assert screen(lib) == C.ADVANCED_SCREEN_SPELLBOOK
    assert lib.magic_favorite_rune(0, 1, 0) == 0x60, "the shown power, LO"
    assert lib.magic_favorite_rune(0, 1, 1) == GLYPH_FUL
    assert lib.magic_favorite_rune(0, 1, 2) == 0
    assert mana(lib) == FULL_MANA and bytes(champion_records(lib))[CH_RUNE_COUNT] == 0
    for _ in range(C.ROWS_MAX_COUNT):
        key(lib, C.SCAN_S)
    key(lib, C.SCAN_W)  # FAVORITES
    key(lib, C.KEY_ENTER)  # the favorites page, on favorite 1
    key(lib, C.SCAN_S)  # favorite 2
    key(lib, C.KEY_BACKSPACE)
    assert lib.magic_favorite_rune(0, 1, 1) == 0
