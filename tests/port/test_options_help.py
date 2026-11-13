"""Convenience options are reachable without cheats and respect text-entry contexts."""
import ctypes

from dm2test import constants as C
from dm2test.modes import set_mode, ADVANCED, VANILLA
from dm2test.structs import UiState


def test_help_key_is_available_in_gameplay_but_not_text_entry_or_vanilla(lib):
    lib.input_init()
    set_mode(lib, ADVANCED)
    UiState.in_dll(lib, 'g_ui').candidate = 0
    rect = (ctypes.c_int16 * 4)()
    for mode in (C.INPUT_MODE_MOVE, C.INPUT_MODE_INVENTORY, C.INPUT_MODE_PAUSE, C.INPUT_MODE_SLEEP):
        assert lib.input_key_command(mode, 0x3b, rect) == C.CMD_OPTIONS
    assert not lib.input_key_command(C.INPUT_MODE_NAME, 0x3b, rect)
    set_mode(lib, VANILLA)
    assert not lib.input_key_command(C.INPUT_MODE_MOVE, 0x3b, rect)


def test_options_help_reopens_without_enabling_cheats(lib):
    set_mode(lib, ADVANCED)
    lib.game_set_cheats(0)
    lib.advanced_open_options()
    screen = ctypes.c_int.in_dll(lib, 'g_advanced_screen')
    assert screen.value == C.ADVANCED_SCREEN_OPTIONS
    lib.advanced_screen_event(C.INPUT_KEY, C.KEY_ENTER, 0)  # first row: help
    assert screen.value == C.ADVANCED_SCREEN_OPTIONS
    lib.advanced_screen_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)  # back to options
    lib.advanced_screen_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)  # close
    assert not screen.value
    lib.advanced_open_options()
    assert screen.value == C.ADVANCED_SCREEN_OPTIONS
    assert not lib.game_get_cheats()


def test_closing_options_restores_the_paused_or_inventory_picture(lib):
    set_mode(lib, ADVANCED)
    pixels = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, 'g_video_fb')
    ctypes.memset(pixels, 42, len(pixels))
    lib.advanced_open_options()
    ctypes.memset(pixels, 99, len(pixels))
    lib.advanced_screen_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)
    assert bytes(pixels) == bytes([42]) * len(pixels)
