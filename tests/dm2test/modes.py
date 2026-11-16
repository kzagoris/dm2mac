"""The game modes of the C port: the mode switch, the advanced mode fixture, and the keys."""

import ctypes
from contextlib import contextmanager

import pytest

from dm2test import constants as C
from dm2test.input import run_ticks
from dm2test.structs import Game, Session


VANILLA, ADVANCED = C.GAME_MODE_VANILLA, C.GAME_MODE_ADVANCED
SCAN_Q, SCAN_W, SCAN_E, SCAN_A, SCAN_S, SCAN_D = 0x10, 0x11, 0x12, 0x1E, 0x1F, 0x20
KEY_F5, KEY_F9, KEY_PAUSE = C.KEY_QUICK_SAVE, C.KEY_QUICK_LOAD, 0x01
INPUT_MODE_DIALOG, INPUT_MODE_MOVE, INPUT_MODE_INVENTORY, INPUT_MODE_NAME = (
    C.INPUT_MODE_DIALOG, C.INPUT_MODE_MOVE, C.INPUT_MODE_INVENTORY, C.INPUT_MODE_NAME)
ZONE_MESSAGE_AREA, TEXT_LINE_HEIGHT, SCREEN_W, UI_WHITE = C.ZONE_MESSAGE_AREA, C.TEXT_LINE_HEIGHT, C.SCREEN_W, C.UI_WHITE
STEP_TICKS = 40  # enough timer ticks for a key to give its command and the party to finish a step


def set_mode(lib, mode):
    ctypes.c_int.in_dll(lib, "g_game_mode").value = mode


@pytest.fixture
def advanced(lib, session, tmp_path):
    """start.DAT in the advanced mode, with the game folder in tmp_path; vanilla again after the test."""
    (tmp_path / "DATA").mkdir()
    state = Session.in_dll(lib, "g_session")
    state.data_dir = str(tmp_path).encode()
    state.slot, state.loaded, state.new_game = -1, 0, 0
    set_mode(lib, ADVANCED)
    yield session[0], tmp_path / "DATA"
    set_mode(lib, VANILLA)


@contextmanager
def advanced_new_game(lib, gdat, game_dir):
    """A new game of game_dir in the advanced mode with real graphics and no cheats, without the private start.DAT
    fixture; yields g_game, then closes the advanced screens, turns the cheats off and sets the vanilla mode again."""
    lib.gdat_select(gdat)
    assert lib.zone_load(gdat) and lib.pal_load_color_ramps(gdat) and lib.text_init(gdat)
    lib.draw_init(gdat)
    Session.in_dll(lib, "g_session").data_dir = str(game_dir).encode()
    set_mode(lib, ADVANCED)
    assert lib.session_start_new_game()
    lib.game_set_cheats(0)
    try:
        yield Game.in_dll(lib, "g_game")
    finally:
        lib.advanced_reset()
        lib.game_set_cheats(0)
        set_mode(lib, VANILLA)


def screen_key(lib, scan):
    """A key on the open viewport screen of the advanced mode."""
    lib.advanced_screen_event(C.INPUT_KEY, scan, 0)


def viewport(lib):
    """The viewport rectangle: x, y, width, height."""
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    assert lib.zone_get_viewport_rect(rect)
    return tuple(rect)


def map_party_icon(lib):
    """A screen place inside the party icon at the center of the automap."""
    x, y, width, height = viewport(lib)
    return x + (width - C.MAP_CELL_SIZE) // 2 + 1, y + (height - C.MAP_CELL_SIZE) // 2 + 1


def press(lib, key):
    lib.input_key_down(key)
    run_ticks(lib, STEP_TICKS)


def key_command(lib, mode, key):
    rect = (ctypes.c_int16 * 4)()
    return lib.input_key_command(mode, key, rect)
