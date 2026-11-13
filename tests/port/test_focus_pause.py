"""Focus pause freezes clocks and commands until a fresh deliberate resume."""
import ctypes

from dm2test import constants as C
from dm2test.structs import Game, MainLoop, UiState
from dm2test.modes import set_mode, ADVANCED, VANILLA


def test_focus_pause_freezes_clocks_and_drops_pending_input(lib):
    set_mode(lib, ADVANCED)
    game = Game.in_dll(lib, 'g_game')
    loop = MainLoop.in_dll(lib, 'g_main_loop')
    game.champion_count, game.game_time = 1, 123
    loop.running, loop.quit, loop.end = True, False, False
    lib.input_init()
    lib.input_key_down(C.SCAN_W)
    lib.input_mouse_down(0, 0, C.INPUT_BUTTON_LEFT)
    lib.advanced_focus_changed(False)
    assert lib.advanced_focus_paused()
    before = bytes(loop)
    for _ in range(20):
        lib.play_tick()
        lib.command_process_queue()
    assert bytes(loop) == before
    assert game.game_time == 123
    assert lib.input_is_idle()
    assert not ctypes.c_int16.in_dll(lib, 'g_input_mouse_buttons').value
    assert lib.advanced_focus_key(C.KEY_ENTER, False)  # still away
    assert lib.advanced_focus_paused()
    lib.advanced_focus_changed(True)
    assert lib.advanced_focus_paused()
    lib.advanced_focus_key(C.KEY_ENTER, True)  # held key repeat cannot resume
    assert lib.advanced_focus_paused()
    lib.advanced_focus_key(C.SCAN_W, False)
    assert lib.advanced_focus_paused()
    lib.advanced_focus_key(C.KEY_ENTER, False)
    assert not lib.advanced_focus_paused()
    assert lib.input_is_idle()


def test_focus_pause_preserves_underlying_pause_and_overlay_and_vanilla(lib):
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
    assert not lib.advanced_focus_paused()


def test_held_repeat_cannot_restart_movement_after_resume(lib):
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
