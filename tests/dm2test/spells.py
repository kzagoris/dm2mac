"""The spell tests of the advanced mode: two casters with full health and mana, their mana, and the engine-only
fixture without dungeon drawing."""

import ctypes
import struct

import pytest

from dm2test import constants as C
from dm2test.modes import ADVANCED, set_mode
from dm2test.scenario import champion_bytes, champion_records
from dm2test.structs import ActionArea, Game

# Champion record offsets (the Champion struct of engine/champion.h).
CH_HEALTH, CH_MANA = 0x36, 0x3E
FULL_HEALTH = FULL_MANA = 100
CASTER_COUNT = 2
# The g_hooks members (engine/hooks.h) that the engine-only tests stub: update_action_area, draw_champion_state, and
# cast_failed, which writes to the message area.
HOOK_SLOTS, HOOK_UPDATE_ACTION_AREA, HOOK_DRAW_CHAMPION_STATE, HOOK_CAST_FAILED = 32, 5, 23, 31


@pytest.fixture(scope="module")
def no_dungeon_drawing(lib):
    """Stubs only the drawing hooks for a module without dungeon or GDAT; the rune costs, the action state, the input
    refresh and the casting stay real."""
    update = ctypes.CFUNCTYPE(None, ctypes.c_bool)(lambda tick: None)
    draw = ctypes.CFUNCTYPE(None, ctypes.c_int16)(lambda champion_index: None)
    explain = ctypes.CFUNCTYPE(None, ctypes.c_void_p)(lambda failure: None)
    hooks = (ctypes.c_void_p * HOOK_SLOTS).in_dll(lib, "g_hooks")
    slots = (HOOK_UPDATE_ACTION_AREA, HOOK_DRAW_CHAMPION_STATE, HOOK_CAST_FAILED)
    old = [hooks[slot] for slot in slots]
    for slot, stub in zip(slots, (update, draw, explain)):
        hooks[slot] = ctypes.cast(stub, ctypes.c_void_p).value
    yield
    for slot, value in zip(slots, old):
        hooks[slot] = value


def prepare_casters(lib):
    """The advanced mode with two empty champions of full health and mana, no favorites, and the second champion
    entering runes."""
    set_mode(lib, ADVANCED)
    Game.in_dll(lib, "g_game").champion_count = CASTER_COUNT
    lib.magic_reset_favorites()
    for i in range(CASTER_COUNT):
        champion_bytes(lib, i, 0, bytes(C.CHAMPION_SIZE))
        champion_bytes(lib, i, CH_HEALTH, struct.pack("<h", FULL_HEALTH))
        champion_bytes(lib, i, CH_MANA, struct.pack("<h", FULL_MANA))
    a = ActionArea.in_dll(lib, "g_action")
    a.champion, a.hand = 1, C.ACTION_HAND_SPELL
    a.open_container = C.THING_NONE


def mana(lib):
    """The mana of the first champion."""
    return struct.unpack_from("<h", bytes(champion_records(lib)), CH_MANA)[0]
