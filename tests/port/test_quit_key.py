"""Ctrl+Q of the advanced mode: the QUIT button of the save menu, where the disk icon works."""
import ctypes

from dm2test import constants as C
from dm2test.modes import set_mode, ADVANCED, VANILLA
from dm2test.structs import UiState


def test_quit_key_follows_disk_icon_availability(lib):
    lib.input_init()
    rect = (ctypes.c_int16 * 4)()
    ui = UiState.in_dll(lib, 'g_ui')
    ui.candidate = 0
    set_mode(lib, ADVANCED)
    try:
        assert lib.input_key_command(C.INPUT_MODE_MOVE, C.KEY_QUIT_GAME, rect) == C.CMD_QUIT_GAME
        for mode in (C.INPUT_MODE_NAME, C.INPUT_MODE_DIALOG, C.INPUT_MODE_SLEEP, C.INPUT_MODE_PAUSE):
            assert not lib.input_key_command(mode, C.KEY_QUIT_GAME, rect)
        ui.candidate = 1
        assert not lib.input_key_command(C.INPUT_MODE_MOVE, C.KEY_QUIT_GAME, rect)
        ui.candidate = 0
        # without control, Q still turns the party left
        assert lib.input_key_command(C.INPUT_MODE_MOVE, C.SCAN_Q, rect) == C.CMD_TURN_LEFT
    finally:
        set_mode(lib, VANILLA)
    assert lib.input_key_command(C.INPUT_MODE_MOVE, C.KEY_QUIT_GAME, rect) != C.CMD_QUIT_GAME


def test_quit_command_reaches_execution(lib):
    """The input loop drops a command without a click feedback script: Ctrl+Q must have one."""
    from dm2test.scenario import Command

    event = Command()
    event.command = C.CMD_QUIT_GAME
    assert lib.input_click_feedback_script(ctypes.byref(event))
