"""The screen filter of the advanced mode (src/ui/filter.c) and its row on the Options/Help screen (src/ui/options.c):
the start from --filter and SETTINGS.TXT, the cycle, the SETTINGS.TXT write, the window hook, and Esc, which opens
Options/Help in the dungeon view and resumes from the pause."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.input import run_ticks
from dm2test.modes import (ADVANCED, INPUT_MODE_MOVE, VANILLA, key_command, press, set_mode)

NONE, SCALE2X, SCALE3X = C.SETTINGS_FILTER_NONE, C.SETTINGS_FILTER_SCALE2X, C.SETTINGS_FILTER_SCALE3X
XBRZ2X, XBRZ3X, XBRZ4X = C.SETTINGS_FILTER_XBRZ2X, C.SETTINGS_FILTER_XBRZ3X, C.SETTINGS_FILTER_XBRZ4X
# The rows and the layout of the options page (src/ui/options.c)
OPTIONS_ROW_FILTER = 4
PANEL_LEFT, PANEL_FIRST_Y, PANEL_ROW_HEIGHT, PANEL_NOTE_Y = 18, 46, 11, 170
SCAN_ESCAPE, SCAN_S, SCAN_W = 0x01, 0x1F, 0x11
HOOK = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_int)
FULLSCREEN_HOOK = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_bool)


def key(lib, scan):
    return lib.options_event(C.INPUT_KEY, scan, 0)


def click(lib, x, y):
    return lib.options_event(C.INPUT_BUTTON_LEFT, x, y)


def row_y(row):
    """A y inside the row: the row starts a text line above its baseline."""
    return PANEL_FIRST_Y + row * PANEL_ROW_HEIGHT - 1


@pytest.fixture
def screen(lib, advanced):
    """The advanced mode with the filter none and Options/Help open on SCREEN FILTER; the filter hook cleared after
    the test."""
    lib.upscale_start(NONE)
    lib.options_open()
    for _ in range(OPTIONS_ROW_FILTER):
        assert key(lib, SCAN_S) == C.OPTIONS_STAY
    yield advanced
    ctypes.c_void_p.in_dll(lib, "g_upscale_filter_hook").value = None


def settings_text(filter):
    """SETTINGS.TXT with the filter and the other values at their defaults."""
    return f"filter={filter}\naspect=off\nscanlines=off\nclassic_steps=off\nspells=known\nfullscreen=off\n"


def cycle(lib):
    assert key(lib, C.KEY_ENTER) == C.OPTIONS_STAY


def test_the_row_cycles_the_filter(lib, screen):
    assert lib.upscale_get_filter() == NONE
    for filter in (SCALE2X, SCALE3X, XBRZ2X, XBRZ3X, XBRZ4X, NONE, SCALE2X):
        cycle(lib)
        assert lib.upscale_get_filter() == filter
    key(lib, SCAN_S)
    key(lib, SCAN_W)
    cycle(lib)
    assert lib.upscale_get_filter() == SCALE3X, "the selection is on the filter row again"


def test_click_cycles_the_filter_and_a_click_below_the_rows_does_nothing(lib, screen):
    x = PANEL_LEFT + 10
    assert click(lib, x, row_y(OPTIONS_ROW_FILTER)) == C.OPTIONS_STAY and lib.upscale_get_filter() == SCALE2X
    assert click(lib, x, PANEL_NOTE_Y) == C.OPTIONS_STAY
    assert lib.upscale_get_filter() == SCALE2X, "the note line"


def test_a_change_goes_to_the_file_once(lib, screen):
    _, data = screen
    cycle(lib)
    assert (data / "SETTINGS.TXT").read_text() == settings_text("scale2x")
    (data / "SETTINGS.TXT").unlink()
    key(lib, SCAN_S)
    key(lib, SCAN_W)
    assert not (data / "SETTINGS.TXT").exists(), "no change, no write"
    cycle(lib)
    assert (data / "SETTINGS.TXT").read_text() == settings_text("scale3x")


def test_the_window_refuses_a_filter(lib, screen):
    seen = []
    hook = HOOK(lambda filter: seen.append(filter) or filter != SCALE3X)
    ctypes.c_void_p.in_dll(lib, "g_upscale_filter_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    cycle(lib)
    cycle(lib)
    assert seen == [SCALE2X, SCALE3X] and lib.upscale_get_filter() == SCALE2X
    assert (screen[1] / "SETTINGS.TXT").read_text() == settings_text("scale2x")


def test_failed_write_keeps_the_filter(lib, screen):
    _, data = screen
    (data / "SETTINGS.TMP").mkdir()
    cycle(lib)
    assert lib.upscale_get_filter() == SCALE2X and not (data / "SETTINGS.TXT").exists()


@pytest.mark.parametrize("given, file, expected", [(C.SETTINGS_FILTER_UNSET, b"filter=scale3x\n", SCALE3X), (SCALE2X, b"filter=scale3x\n", SCALE2X),
                                                   (C.SETTINGS_FILTER_UNSET, None, NONE)])
def test_start_takes_the_file_unless_a_filter_is_given(lib, screen, given, file, expected):
    _, data = screen
    if file:
        (data / "SETTINGS.TXT").write_bytes(file)
    shown = []
    hook = HOOK(lambda filter: shown.append(filter) or True)
    ctypes.c_void_p.in_dll(lib, "g_upscale_filter_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    lib.upscale_start(given)
    assert lib.upscale_get_filter() == expected
    assert shown == ([expected] if expected != NONE and not given else [])
    set_mode(lib, VANILLA)
    lib.upscale_start(C.SETTINGS_FILTER_UNSET)
    assert lib.upscale_get_filter() == NONE, "the vanilla mode reads no file"


def test_escape_opens_options_in_the_advanced_mode_and_resumes_from_the_pause(lib, advanced):
    game, _ = advanced
    screen = ctypes.c_int.in_dll(lib, "g_advanced_screen")
    assert key_command(lib, INPUT_MODE_MOVE, SCAN_ESCAPE) == C.CMD_OPTIONS
    set_mode(lib, VANILLA)
    assert key_command(lib, INPUT_MODE_MOVE, SCAN_ESCAPE) == C.CMD_PAUSE
    press(lib, SCAN_ESCAPE)
    set_mode(lib, ADVANCED)
    assert screen.value == C.ADVANCED_SCREEN_NONE and lib.input_get_mode() == C.INPUT_MODE_PAUSE
    time = game.game_time
    run_ticks(lib, 40)
    assert game.game_time == time, "the time stops"
    press(lib, SCAN_ESCAPE)
    assert lib.input_get_mode() == INPUT_MODE_MOVE
    run_ticks(lib, 40)
    assert game.game_time > time


def test_stepping_option_persists_and_vanilla_ignores_it(lib, tmp_path):
    from dm2test.structs import Session

    data = tmp_path
    state = Session.in_dll(lib, "g_session")
    state.save_dir = str(data).encode()
    set_mode(lib, ADVANCED)
    lib.upscale_start(NONE)
    assert not lib.move_has_classic_steps()
    lib.upscale_toggle_stepping()
    assert lib.move_has_classic_steps()
    assert "classic_steps=on" in (data / "SETTINGS.TXT").read_text()
    lib.upscale_start(NONE)
    assert lib.move_has_classic_steps()
    set_mode(lib, VANILLA)
    lib.upscale_start(NONE)
    assert not lib.move_has_classic_steps()
    state.save_dir = b""


def test_fullscreen_persists_in_the_advanced_mode_only(lib, tmp_path):
    from dm2test.structs import Session

    session = Session.in_dll(lib, "g_session")
    session.save_dir = str(tmp_path).encode()
    settings = tmp_path / "SETTINGS.TXT"
    set_mode(lib, ADVANCED)
    lib.upscale_start(NONE)
    lib.upscale_note_fullscreen(True)
    assert settings.read_text().endswith("fullscreen=on\n")
    shown = []
    hook = FULLSCREEN_HOOK(lambda fullscreen: shown.append(fullscreen) or True)
    ctypes.c_void_p.in_dll(lib, "g_upscale_fullscreen_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    lib.upscale_start(C.SETTINGS_FILTER_UNSET)
    assert shown == [True], "the start makes the window fullscreen"
    lib.upscale_note_fullscreen(False)
    assert settings.read_text().endswith("fullscreen=off\n")
    set_mode(lib, VANILLA)
    lib.upscale_start(C.SETTINGS_FILTER_UNSET)
    lib.upscale_note_fullscreen(True)
    assert settings.read_text().endswith("fullscreen=off\n"), "the vanilla mode writes no file"
    assert shown == [True]
    ctypes.c_void_p.in_dll(lib, "g_upscale_fullscreen_hook").value = None
    session.save_dir = b""
