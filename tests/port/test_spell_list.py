"""The known spells and the cast of a spell by name (src/engine/magic.c): a spell becomes known only in the advanced
mode, costs the same runes and mana as manual entry, and is saved in the KNOW block after the favorites."""
import ctypes
import struct
import sys

import pytest

from dm2test import constants as C
from dm2test.modes import ADVANCED, VANILLA, set_mode
from dm2test.scenario import champion_bytes, champion_records
from dm2test.spells import CH_HEALTH, CH_MANA, FULL_MANA, mana, prepare_casters

SPELL_COUNT = 34
MAGIC_TORCH, FIREBALL, HEALTH_POTION = 5, 16, 25  # spell table indices: FUL; FUL IR; VI
FUL, IR = 3, 3  # the place of the rune in its rune set
# Champion record offsets (the Champion struct of engine/champion.h).
CH_RUNE_COUNT, CH_RUNES, CH_SPELL_DELAY = 0x1E, 0x22, 0x2C
KNOWN_BLOCK_SIZE = 5 + SPELL_COUNT  # the magic, the version, one byte per spell


pytestmark = pytest.mark.usefixtures("no_dungeon_drawing")


def test_a_spell_becomes_known_only_in_the_advanced_mode(lib):
    prepare_casters(lib)
    fireball = lib.magic_get_spell(FIREBALL)
    assert not lib.magic_is_spell_known(fireball)
    set_mode(lib, VANILLA)
    lib.magic_learn_spell(fireball)
    assert not lib.magic_is_spell_known(fireball)
    set_mode(lib, ADVANCED)
    lib.magic_learn_spell(fireball)
    assert lib.magic_is_spell_known(fireball)
    assert not lib.magic_is_spell_known(lib.magic_get_spell(MAGIC_TORCH))
    lib.magic_reset_favorites()  # a new adventure
    assert not lib.magic_is_spell_known(fireball)


@pytest.mark.parametrize('power', [1, 3, 6])
def test_a_spell_by_name_costs_the_runes_of_manual_entry(lib, power):
    prepare_casters(lib)
    fireball = lib.magic_get_spell(FIREBALL)
    for rune in (power - 1, FUL, IR):
        lib.magic_add_rune(rune)
    manual = bytes(champion_records(lib))
    spent = FULL_MANA - mana(lib)
    assert lib.magic_spell_mana(fireball, power) == spent
    for _ in range(3):
        lib.magic_remove_last_rune()
    champion_bytes(lib, 0, CH_MANA, struct.pack('<h', FULL_MANA))
    assert lib.magic_prepare_spell(0, fireball, power)
    by_name = bytes(champion_records(lib))
    assert mana(lib) == FULL_MANA - spent
    assert by_name[CH_RUNE_COUNT] == 3
    assert by_name[CH_RUNES:CH_RUNES + 3] == manual[CH_RUNES:CH_RUNES + 3]


def test_spell_mana_rejects_a_power_outside_the_power_runes(lib):
    prepare_casters(lib)
    fireball = lib.magic_get_spell(FIREBALL)
    assert lib.magic_spell_mana(fireball, 0) == -1
    assert lib.magic_spell_mana(fireball, C.POWER_MON + 1) == -1


def test_a_potion_spell_needs_a_flask(lib):
    assert lib.magic_spell_needs_flask(lib.magic_get_spell(HEALTH_POTION))
    assert not lib.magic_spell_needs_flask(lib.magic_get_spell(FIREBALL))


def test_prepare_rejects_busy_dead_and_unaffordable_without_spending(lib):
    prepare_casters(lib)
    fireball = lib.magic_get_spell(FIREBALL)
    for offset, value in [(CH_SPELL_DELAY, b'\x01'), (CH_HEALTH, b'\0\0'), (CH_MANA, b'\x01\0')]:
        old = bytes(champion_records(lib))[offset:offset + len(value)]
        champion_bytes(lib, 0, offset, value)
        before = bytes(champion_records(lib))
        assert not lib.magic_prepare_spell(0, fireball, 1)
        assert bytes(champion_records(lib)) == before
        champion_bytes(lib, 0, offset, old)
    before = bytes(champion_records(lib))
    assert not lib.magic_prepare_spell(-1, fireball, 1)
    assert not lib.magic_prepare_spell(2, fireball, 1)  # no such champion
    assert not lib.magic_prepare_spell(0, fireball, 0)
    set_mode(lib, VANILLA)
    assert not lib.magic_prepare_spell(0, fireball, 1)
    set_mode(lib, ADVANCED)
    assert bytes(champion_records(lib)) == before
    lib.magic_add_rune(0)
    assert not lib.magic_prepare_spell(0, fireball, 1)  # entered runes


def test_known_spells_round_trip_and_reject_an_invalid_block(lib, tmp_path):
    prepare_casters(lib)
    fireball = lib.magic_get_spell(FIREBALL)
    lib.magic_learn_spell(fireball)
    # FILE* is intentionally outside the game's ctypes prototype parser; the lib links the UCRT on Windows.
    libc = ctypes.CDLL('ucrtbase.dll' if sys.platform == 'win32' else None)
    libc.fopen.argtypes, libc.fopen.restype = [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_void_p
    libc.fclose.argtypes = [ctypes.c_void_p]
    lib.magic_write_favorites.argtypes = lib.magic_read_favorites.argtypes = [ctypes.c_void_p]
    path = tmp_path / 'favorites'
    f = libc.fopen(str(path).encode(), b'wb')
    assert lib.magic_write_favorites(f)
    libc.fclose(f)
    saved = path.read_bytes()
    block = saved[-KNOWN_BLOCK_SIZE:]
    assert block[:5] == b'KNOW\x01'
    assert block[5:] == bytes(i == FIREBALL for i in range(SPELL_COUNT))
    favorites_only = saved[:-KNOWN_BLOCK_SIZE]  # a save of a version without the known spells
    bad_flag = saved[:-1] + b'\x02'
    bad_version = saved[:-SPELL_COUNT - 1] + b'\x02' + saved[-SPELL_COUNT:]
    for data, valid in [(saved, True), (favorites_only, True), (saved[:-1], False), (saved + b'x', False),
                        (bad_flag, False), (bad_version, False)]:
        lib.magic_learn_spell(lib.magic_get_spell(MAGIC_TORCH))  # the read clears what was known
        path.write_bytes(data)
        f = libc.fopen(str(path).encode(), b'rb')
        assert bool(lib.magic_read_favorites(f)) == valid
        libc.fclose(f)
        assert bool(lib.magic_is_spell_known(fireball)) == (data == saved)
        assert not lib.magic_is_spell_known(lib.magic_get_spell(MAGIC_TORCH))
