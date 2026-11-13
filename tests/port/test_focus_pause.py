"""Switching windows clears held input without automatically pausing gameplay."""
import ctypes

from dm2test import constants as C
from dm2test.structs import Game, MainLoop, UiState
from dm2test.modes import set_mode, ADVANCED, VANILLA


def test_focus_loss_drops_input_without_pausing(lib):
    set_mode(lib, ADVANCED)
    game = Game.in_dll(lib, 'g_game')
    loop = MainLoop.in_dll(lib, 'g_main_loop')
    game.champion_count = 1
    loop.running, loop.quit, loop.end = True, False, False
    lib.input_init()
    lib.input_key_down(C.SCAN_W)
    lib.input_mouse_down(0, 0, C.INPUT_BUTTON_LEFT)
    lib.advanced_focus_changed(False)
    assert loop.running
    assert not ctypes.c_int.in_dll(lib, 'g_advanced_screen').value
    assert lib.input_is_idle()
    assert not ctypes.c_int16.in_dll(lib, 'g_input_mouse_buttons').value
    lib.advanced_focus_changed(True)
    assert not lib.advanced_focus_key(C.SCAN_W, False)
    assert loop.running


def test_focus_change_preserves_manual_pause_and_overlay(lib):
    set_mode(lib, ADVANCED)
    loop = MainLoop.in_dll(lib, 'g_main_loop')
    loop.running = False
    ui = UiState.in_dll(lib, 'g_ui')
    ui.inventory = 1
    screen = ctypes.c_int.in_dll(lib, 'g_advanced_screen')
    screen.value = C.ADVANCED_SCREEN_AUTOMAP
    lib.advanced_focus_changed(False)
    lib.advanced_focus_changed(True)
    lib.advanced_focus_key(C.KEY_ENTER, False)
    assert not loop.running
    assert ui.inventory == 1
    assert screen.value == C.ADVANCED_SCREEN_AUTOMAP
    set_mode(lib, VANILLA)
    lib.advanced_focus_changed(False)


def test_held_repeat_cannot_restart_movement_after_focus_returns(lib):
    set_mode(lib, ADVANCED)
    Game.in_dll(lib, 'g_game').champion_count = 1
    lib.advanced_focus_changed(False)
    lib.advanced_focus_changed(True)
    lib.advanced_focus_key(C.KEY_ENTER, False)
    lib.input_key_repeat(C.SCAN_W)
    assert not lib.input_read_key()
    lib.input_key_down(C.SCAN_W)
    assert lib.input_read_key() == C.SCAN_W
    lib.input_key_repeat(C.SCAN_W)
    assert lib.input_read_key() == C.SCAN_W
