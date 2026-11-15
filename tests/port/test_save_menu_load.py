"""The save menu of the advanced mode: LOAD in place of RESUME, and a close button, Esc and a right click that resume."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.dialog import DialogState, InputMatch, main_loop_quit
from dm2test.interface import c_view
from dm2test.modes import set_mode, ADVANCED, VANILLA

CLOSE_CHOICE = 0x7F  # DIALOG_CLOSE_CHOICE of dialog.c


@pytest.fixture
def menu(lib, gdat, program_start_after):
    """The save menu drawn in the viewport (screen mode 0) in input mode DIALOG; vanilla again after the test."""
    lib.gdat_select(gdat)
    assert lib.zone_load(gdat) and lib.text_init(gdat)
    lib.input_init()
    state = DialogState.in_dll(lib, "g_dialog")
    state.screen_mode = 0
    lib.input_push_mode(C.INPUT_MODE_DIALOG)
    yield state
    lib.input_pop_mode()
    state.close_button, state.right_choice = False, 0
    set_mode(lib, VANILLA)


def draw(lib, state, mode):
    set_mode(lib, mode)
    lib.dialog_draw(C.DIALOG_SAVE_MENU, 0)
    # dialog_run_cancel sets these for the dialog while it runs
    state.close_button = mode == ADVANCED
    state.right_choice = CLOSE_CHOICE if mode == ADVANCED else 4
    return c_view(lib)


def test_advanced_menu_differs_from_vanilla_only_in_load_and_close_button(lib, menu):
    vanilla = draw(lib, menu, VANILLA)
    advanced = draw(lib, menu, ADVANCED)
    assert vanilla != advanced
    r = menu.close_rect
    assert r[2] > 0 and r[3] > 0
    assert draw(lib, menu, VANILLA) == vanilla


@pytest.mark.parametrize("event", ["escape", "right", "close"])
def test_escape_right_click_and_close_button_close_the_advanced_menu(lib, menu, event):
    draw(lib, menu, ADVANCED)
    r = menu.close_rect
    match = InputMatch.in_dll(lib, "g_input_match")
    match.choice = C.DIALOG_NO_CHOICE
    args = {"escape": (C.INPUT_KEY, C.SCAN_ESCAPE, 0), "right": (C.INPUT_BUTTON_RIGHT, 1, 1),
            "close": (C.INPUT_BUTTON_LEFT, r[0] + r[2] // 2, r[1] + r[3] // 2)}[event]
    assert lib.input_cancels_dialog(*args)
    assert match.choice == CLOSE_CHOICE


def test_left_click_beside_the_close_button_and_vanilla_escape_do_not_close(lib, menu):
    draw(lib, menu, ADVANCED)
    r = menu.close_rect
    assert not lib.input_cancels_dialog(C.INPUT_BUTTON_LEFT, r[0] - 1, r[1])
    draw(lib, menu, VANILLA)
    assert not lib.input_cancels_dialog(C.INPUT_KEY, C.SCAN_ESCAPE, 0)


def test_escape_closes_the_running_advanced_menu(lib, menu):
    # the wait loop of the dialog reads the keys itself: Esc must close the menu there too
    lib.input_pop_mode()
    set_mode(lib, ADVANCED)
    frames = []

    @ctypes.CFUNCTYPE(None)
    def frame():
        frames.append(1)
        if len(frames) == 2:
            lib.input_key_down(C.SCAN_ESCAPE)
        if len(frames) > 100:
            main_loop_quit(lib).value = 1

    hook = ctypes.c_void_p.in_dll(lib, "g_play_frame_hook")
    hook.value = ctypes.cast(frame, ctypes.c_void_p).value
    try:
        lib.dialog_draw(C.DIALOG_SAVE_MENU, 0)
        assert lib.dialog_run_cancel(C.DIALOG_SAVE_MENU, 3) == C.DIALOG_CLOSED
        assert not main_loop_quit(lib).value
    finally:
        hook.value = None
        lib.input_push_mode(C.INPUT_MODE_DIALOG)


def test_focus_loss_keeps_the_dialog_waiting(lib, menu):
    from dm2test.structs import Game

    set_mode(lib, ADVANCED)
    Game.in_dll(lib, "g_game").champion_count = 1
    match = InputMatch.in_dll(lib, "g_input_match")
    match.choice = C.DIALOG_NO_CHOICE
    lib.advanced_focus_changed(False)
    lib.advanced_focus_changed(True)
    assert match.choice == C.DIALOG_NO_CHOICE
