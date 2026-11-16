"""Why a spell cannot be cast, in the advanced mode (src/ui/spellbook.c) on start.DAT: the rune without enough mana,
the dimmed runes, the failed cast with the missing skill levels, the runes of no spell, the missing flask, the busy
caster, and the refusal of a favorite with the mana numbers."""
import ctypes
import struct

import pytest

from dm2test import constants as C
from dm2test.modes import VANILLA, press, set_mode
from dm2test.scenario import champion_bytes, champion_records
from dm2test.spells import CH_MANA, FULL_MANA
from dm2test.structs import ActionArea, CastFailure, Spell

FIREBALL = 16  # the spell table index of FUL IR
LO, FUL, IR, MON = 0, 3, 3, 5  # the place of the rune in its rune set
POWER_MON = 6
KEY_ALT_C, KEY_ALT_1 = 0x082E, 0x0802  # the favorite keys: Alt+C, then Alt+1 to Alt+4
CH_RUNE_COUNT = 0x1E  # the rune count of the Champion struct (engine/champion.h)
SEED = 1  # a seed whose rolls fail the fireball at MON power of the first champion of start.DAT


@pytest.fixture
def feedback(lib, advanced):
    """start.DAT in the advanced mode with no favorites and a first champion with full mana."""
    lib.magic_reset_favorites()
    champion_bytes(lib, 0, CH_MANA, struct.pack('<h', FULL_MANA))
    yield advanced


def messages(lib):
    return [lib.text_message_row(row) for row in range(C.MESSAGE_ROWS)]


def shows(lib, text):
    return any(text in row for row in messages(lib))


def set_mana(lib, value):
    champion_bytes(lib, 0, CH_MANA, struct.pack('<h', value))


def explain(lib, result, runes=b'', levels_short=0, skill=0):
    failure = CastFailure(champion_index=0, result=result, skill=skill, levels_short=levels_short)
    failure.runes[:len(runes)] = list(runes)
    lib.spellbook_explain_failure(ctypes.addressof(failure))


def test_a_rune_without_enough_mana_names_its_cost_once(lib, feedback):
    lib.action_area_open_spells(0)
    set_mana(lib, 0)
    assert lib.spellbook_is_rune_too_costly(LO)
    lib.spellbook_add_rune(LO)
    assert bytes(champion_records(lib))[CH_RUNE_COUNT] == 0
    assert shows(lib, b'NEEDS 1 MANA FOR LO, HAS 0')
    before = messages(lib)
    lib.spellbook_add_rune(LO)
    assert messages(lib) == before, "a repeated click adds no line"
    set_mana(lib, FULL_MANA)
    assert not lib.spellbook_is_rune_too_costly(LO)
    lib.spellbook_add_rune(LO)
    assert bytes(champion_records(lib))[CH_RUNE_COUNT] == 1


def test_the_vanilla_mode_keeps_the_silent_rune_panel(lib, feedback):
    lib.action_area_open_spells(0)
    set_mana(lib, 0)
    set_mode(lib, VANILLA)
    before = messages(lib)
    assert not lib.spellbook_is_rune_too_costly(LO)
    lib.spellbook_add_rune(LO)
    explain(lib, C.CAST_NEEDS_FLASK)
    lib.spellbook_refuse_busy_caster(0)
    assert messages(lib) == before


def test_a_failed_cast_names_the_missing_skill_levels(lib, feedback):
    fireball = lib.magic_get_spell(FIREBALL)
    spell = Spell.from_address(fireball)
    short = POWER_MON + spell.difficulty - lib.champ_get_skill_level(0, spell.skill, True)
    lib.rand_set_seed(SEED)
    assert lib.magic_prepare_spell(0, fireball, POWER_MON)
    assert lib.magic_cast_entered_runes_failed()
    assert ActionArea.in_dll(lib, 'g_action').result in (C.RESULT_WIZARD_FAILED, C.RESULT_PRIEST_FAILED)
    name = lib.ui_get_skill_name(spell.skill)
    assert shows(lib, b'FAILED: %d %s LEVEL%s SHORT' % (short, name, b'S' if short != 1 else b''))


def test_the_failure_texts(lib, feedback):
    explain(lib, C.CAST_FAILED, levels_short=1, skill=C.SKILL_WIZARD)
    assert shows(lib, b'FAILED: 1 WIZARD LEVEL SHORT')
    explain(lib, C.CAST_NEEDS_FLASK)
    assert shows(lib, b'NEEDS AN EMPTY FLASK IN HAND')
    before = messages(lib)
    explain(lib, C.CAST_OK)
    assert messages(lib) == before


def test_entered_runes_of_no_spell_are_named(lib, feedback):
    lib.action_area_open_spells(0)
    places = (LO, LO, MON)  # LO, then the first and the last rune of the next sets: no spell
    for place in places:
        lib.spellbook_add_rune(place)
    names = b' '.join(lib.magic_get_rune_name(step * C.SPELL_RUNES_PER_SET + place).upper()
                      for step, place in enumerate(places))
    assert lib.magic_cast_entered_runes_failed()
    assert ActionArea.in_dll(lib, 'g_action').result == C.RESULT_UNKNOWN_SPELL
    assert shows(lib, b'NO SPELL HAS THE RUNES ' + names)


def test_a_busy_caster_is_named_once(lib, feedback):
    lib.spellbook_refuse_busy_caster(0)
    assert shows(lib, b'IS STILL RECOVERING FROM A SPELL')
    before = messages(lib)
    lib.spellbook_refuse_busy_caster(0)
    assert messages(lib) == before


def test_a_favorite_without_enough_mana_gives_the_numbers_and_the_highest_power(lib, feedback):
    lib.action_area_open_spells(0)
    for rune in (MON, FUL):
        lib.spellbook_add_rune(rune)
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1)  # MON FUL becomes favorite 1
    lib.magic_remove_last_rune()
    lib.magic_remove_last_rune()
    costs = [lib.magic_favorite_mana(0, 0, power) for power in range(1, POWER_MON + 1)]
    have = costs[0] + 1
    set_mana(lib, have)
    best = max(power for power in range(1, POWER_MON + 1) if costs[power - 1] <= have)
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1)
    rune = lib.magic_get_rune_name(best - 1).upper()
    assert shows(lib, b'NEEDS %d MANA, HAS %d - TRY %s' % (costs[POWER_MON - 1], have, rune))
    set_mana(lib, 0)
    press(lib, KEY_ALT_C)
    press(lib, KEY_ALT_1)
    assert shows(lib, b'NEEDS %d MANA, HAS 0 - REST' % costs[POWER_MON - 1])
