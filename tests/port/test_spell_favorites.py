"""Recorded combinations charge the same rune costs as manual entry; no cheat casting."""
import ctypes
import struct

import pytest

from dm2test import constants as C
from dm2test.scenario import champion_bytes, champion_records
from dm2test.structs import Game, ActionArea
from dm2test.modes import set_mode, ADVANCED


@pytest.fixture(scope="module", autouse=True)
def no_dungeon_drawing(lib):
    # This engine-only fixture has no dungeon/GDAT. Stub only the drawing hook;
    # rune charging, action state, input refresh, and casting remain real.
    hook = ctypes.CFUNCTYPE(None, ctypes.c_bool)(lambda tick: None)
    draw = ctypes.CFUNCTYPE(None, ctypes.c_int16)(lambda caster: None)
    hooks = (ctypes.c_void_p * 24).in_dll(lib, 'g_hooks')
    old = hooks[5], hooks[23]
    hooks[5] = ctypes.cast(hook, ctypes.c_void_p).value
    hooks[23] = ctypes.cast(draw, ctypes.c_void_p).value
    yield
    hooks[5], hooks[23] = old


def prepare(lib):
    set_mode(lib, ADVANCED)
    Game.in_dll(lib, 'g_game').champion_count = 2
    lib.magic_reset_favorites()
    for i in range(2):
        champion_bytes(lib, i, 0, bytes(C.CHAMPION_SIZE))
        champion_bytes(lib, i, 0x36, struct.pack('<h', 100))
        champion_bytes(lib, i, 0x3e, struct.pack('<h', 100))
    a = ActionArea.in_dll(lib, 'g_action')
    a.champion, a.hand = 1, C.ACTION_HAND_SPELL
    a.open_container = C.THING_NONE


def test_recorded_combination_is_private_and_replayed_at_chosen_power(lib):
    prepare(lib)
    lib.magic_add_rune(0)
    lib.magic_add_rune(0)
    assert lib.magic_record_favorite(0, 0)
    assert lib.magic_favorite_rune(0, 0, 1) == 0x66
    assert lib.magic_favorite_rune(1, 0, 1) == 0
    # LO + first second-set rune: 1 + 2; MON: 6 + floor(2 * 28 / 8) = 13.
    assert lib.magic_favorite_mana(0, 0, 1) == 3
    assert lib.magic_favorite_mana(0, 0, 6) == 13
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    before = struct.unpack_from('<h', bytes(champion_records(lib)), 0x3e)[0]
    assert lib.magic_prepare_favorite(0, 0, 6)
    raw = bytes(champion_records(lib))
    assert struct.unpack_from('<h', raw, 0x3e)[0] == before - 13
    assert raw[0x1e] == 2
    assert raw[0x22:0x24] == bytes([0x65, 0x66])


def test_favorites_reject_busy_partial_dead_and_unaffordable_without_spending(lib):
    prepare(lib)
    lib.magic_add_rune(0)
    lib.magic_add_rune(0)
    assert lib.magic_record_favorite(0, 0)
    assert not lib.magic_prepare_favorite(0, 0, 1)  # existing runes
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    for offset, value in [(0x2c, b'\x01'), (0x36, b'\0\0'), (0x3e, b'\x01\0')]:
        old = bytes(champion_records(lib))[offset:offset + len(value)]
        champion_bytes(lib, 0, offset, value)
        before = bytes(champion_records(lib))
        assert not lib.magic_prepare_favorite(0, 0, 1)
        assert bytes(champion_records(lib)) == before
        champion_bytes(lib, 0, offset, old)
    assert not lib.magic_prepare_favorite(-1, 0, 1)
    assert not lib.magic_prepare_favorite(0, 0, 0)
    assert not lib.magic_record_favorite(0, 99)


def test_repeat_last_keeps_normal_unknown_spell_failure(lib):
    prepare(lib)
    # YA + EE has no spell. Manual and recorded attempts must fail normally.
    lib.magic_add_rune(0)
    lib.magic_add_rune(0)
    lib.magic_add_rune(5)
    assert lib.magic_cast_entered_runes_failed()
    assert lib.magic_favorite_rune(0, C.MAGIC_FAVORITE_LAST, 2) == 0x71
    assert lib.magic_prepare_favorite(0, C.MAGIC_FAVORITE_LAST, 1)
    assert lib.magic_cast_entered_runes_failed()
    assert ActionArea.in_dll(lib, 'g_action').result == C.RESULT_UNKNOWN_SPELL


def test_favorites_versioned_round_trip_and_reject_invalid_tail(lib, tmp_path):
    prepare(lib)
    lib.magic_add_rune(0)
    lib.magic_add_rune(0)
    assert lib.magic_record_favorite(0, 0)
    # FILE* is intentionally outside the game's ctypes prototype parser.
    libc = ctypes.CDLL(None)
    libc.fopen.argtypes, libc.fopen.restype = [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_void_p
    libc.fclose.argtypes = [ctypes.c_void_p]
    lib.magic_write_favorites.argtypes = lib.magic_read_favorites.argtypes = [ctypes.c_void_p]
    path = tmp_path / 'favorites'
    f = libc.fopen(str(path).encode(), b'wb')
    assert lib.magic_write_favorites(f)
    libc.fclose(f)
    saved = path.read_bytes()
    assert saved[:5] == b'FAVS\x01'
    for data, valid in [(saved, True), (b'', True), (saved[:-1], False),
                        (saved[:4] + b'\x02' + saved[5:], False),
                        (saved[:6] + b'\xff' + saved[7:], False), (saved + b'x', False)]:
        path.write_bytes(data)
        f = libc.fopen(str(path).encode(), b'rb')
        assert bool(lib.magic_read_favorites(f)) == valid
        libc.fclose(f)
        assert lib.magic_favorite_rune(0, 0, 1) == (0x66 if data == saved else 0)


def test_record_ignores_stale_glyphs_after_removed_runes(lib):
    prepare(lib)
    for rune in (0, 0, 0, 0):
        lib.magic_add_rune(rune)
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    assert lib.magic_record_favorite(0, 0)
    assert lib.magic_favorite_mana(0, 0, 1) == 3
    assert lib.magic_favorite_rune(0, 0, 3) == 0


@pytest.mark.parametrize('runes,power,seed', [((2, 3, 4), 1, 1), ((2, 3, 4), 6, 1), ((0,), 6, 2)])
def test_favorite_cast_matches_manual_mana_rng_experience_delay_and_save(lib, session, gdat, tmp_path, runes, power, seed):
    # Requires real assets: compare the entire saved adventure after the same normal cast,
    # including failure/experience, cooldown, timeline and spell effects, plus RNG separately.
    set_mode(lib, ADVANCED)
    lib.action_area_open_spells(0)
    lib.magic_add_rune(power - 1)
    for rune in runes:
        lib.magic_add_rune(rune)
    assert lib.magic_record_favorite(0, 0)
    for _ in range(len(runes) + 1):
        lib.magic_remove_last_rune()
    champion_bytes(lib, 0, 0x3e, struct.pack('<h', 100))
    snapshot = tmp_path / 'before.DAT'
    assert lib.save_write(str(snapshot).encode(), str(snapshot.with_suffix('.BAK')).encode(), b'BEFORE', gdat) == C.SAVE_OK
    results = []
    for favorite in (False, True):
        assert lib.session_start_save(str(snapshot).encode())
        lib.rand_set_seed(seed)
        lib.action_area_open_spells(0)
        if favorite:
            assert lib.magic_prepare_favorite(0, 0, power)
        else:
            lib.magic_add_rune(power - 1)
            for rune in runes:
                lib.magic_add_rune(rune)
        failed = lib.magic_cast_entered_runes_failed()
        rng = ctypes.c_uint32.in_dll(lib, 'g_random_seed').value
        after = tmp_path / 'after.DAT'
        assert lib.save_write(str(after).encode(), str(after.with_suffix('.BAK')).encode(), b'AFTER', gdat) == C.SAVE_OK
        results.append((failed, rng, after.read_bytes()))
    assert results[0] == results[1]
