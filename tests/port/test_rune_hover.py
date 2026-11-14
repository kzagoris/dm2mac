"""Syllable labels follow only the rune button under the mouse pointer."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.modes import ADVANCED, VANILLA, set_mode
from dm2test.structs import ActionArea, Bitmap, Game, MainLoop, Offscreen


@pytest.fixture
def rune_panel(lib, gdat):
    assert lib.zone_load(gdat) and lib.text_init(gdat)
    lib.draw_init(gdat)
    lib.input_init()
    lib.area_reset()
    set_mode(lib, ADVANCED)
    Game.in_dll(lib, 'g_game').champion_count = 1
    action = ActionArea.in_dll(lib, 'g_action')
    action.champion, action.hand, action.mode = 1, C.ACTION_HAND_SPELL, C.ACTION_MODE_SPELL
    lib.input_set_mode(C.INPUT_MODE_MOVE)
    yield
    ctypes.c_int.in_dll(lib, 'g_advanced_screen').value = C.ADVANCED_SCREEN_NONE
    lib.input_reset_states()
    set_mode(lib, VANILLA)


def point_at(lib, rune):
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    assert lib.input_command_rect(C.CMD_RUNE_FIRST + rune, rect)
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_x').value = rect[C.RECT_X]
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_y').value = rect[C.RECT_Y]
    return rect


@pytest.mark.parametrize('rune,name,pixels_count', [(14, 'KATH', 38), (20, 'DAIN', 42), (21, 'NETA', 40)])
def test_four_letter_labels_fit_and_render_every_pixel(lib, rune_panel, rune, name, pixels_count):
    assert lib.magic_get_rune_name(rune).decode().upper() == name
    point_at(lib, rune % C.SPELL_RUNES_PER_SET)
    pixels = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H))()
    bitmap = Bitmap(pixels, C.SCREEN_W, C.SCREEN_H, C.IMAGE_8BPP)
    panel = Offscreen(ctypes.pointer(bitmap), (ctypes.c_int16 * C.RECT_SIZE)(0, 0, C.SCREEN_W, C.SCREEN_H), 0)
    assert lib.spellbook_draw_rune_label(ctypes.byref(panel), C.ZONE_RUNE_CHOICE_FIRST + rune % 6, rune, 14)
    drawn = [i for i, pixel in enumerate(pixels) if pixel == 14]
    assert len(drawn) == pixels_count
    assert max(i % C.SCREEN_W for i in drawn) - min(i % C.SCREEN_W for i in drawn) == 6
    assert max(i // C.SCREEN_W for i in drawn) - min(i // C.SCREEN_W for i in drawn) == 10


def test_hover_pixels_restore_on_leave_and_on_viewport_screen(lib, rune_panel):
    frame = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, 'g_video_fb')
    point_at(lib, 0)
    lib.area_update_rune_hover()
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_x').value = -1
    lib.area_update_rune_hover()
    symbols = bytes(frame)
    point_at(lib, 0)
    lib.area_update_rune_hover()
    assert bytes(frame) != symbols
    ctypes.c_int.in_dll(lib, 'g_advanced_screen').value = C.ADVANCED_SCREEN_SPELLBOOK
    lib.area_update_rune_hover()
    assert bytes(frame) == symbols


def test_hover_redraw_waits_for_pressed_button_feedback(lib, rune_panel):
    frame = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, 'g_video_fb')
    rect = point_at(lib, 0)
    lib.area_update_rune_hover()
    lib.input_mouse_click(rect[C.RECT_X], rect[C.RECT_Y], C.INPUT_BUTTON_LEFT)
    MainLoop.in_dll(lib, 'g_main_loop').timer_ticks += 1
    lib.input_poll()
    assert lib.input_has_click_feedback()
    assert not ctypes.c_bool.in_dll(lib, 'g_input_feedback_busy').value
    pressed = bytes(frame)
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_x').value = -1
    lib.area_update_rune_hover()
    assert bytes(frame) == pressed
    lib.input_reset_states()
    assert not lib.input_has_click_feedback()
    lib.area_update_rune_hover()
    assert bytes(frame) != pressed


def test_options_snapshot_does_not_restore_a_stale_hover(lib, rune_panel):
    frame = (ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, 'g_video_fb')
    point_at(lib, 0)
    lib.area_update_rune_hover()
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_x').value = -1
    lib.area_update_rune_hover()
    symbols = bytes(frame)
    point_at(lib, 0)
    lib.area_update_rune_hover()
    lib.advanced_open_options()
    ctypes.c_int16.in_dll(lib, 'g_input_mouse_x').value = -1
    lib.advanced_screen_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)
    lib.area_update_rune_hover()
    assert bytes(frame) == symbols


def test_hover_tracks_each_button_and_leaving_restores_symbols(lib, gdat):
    assert lib.zone_load(gdat)
    lib.input_init()
    set_mode(lib, ADVANCED)
    Game.in_dll(lib, 'g_game').champion_count = 1
    action = ActionArea.in_dll(lib, 'g_action')
    action.champion, action.hand, action.mode = 1, C.ACTION_HAND_SPELL, C.ACTION_MODE_SPELL
    lib.input_set_mode(C.INPUT_MODE_MOVE)
    x = ctypes.c_int16.in_dll(lib, 'g_input_mouse_x')
    y = ctypes.c_int16.in_dll(lib, 'g_input_mouse_y')
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    for rune in range(C.SPELL_RUNES_PER_SET):
        assert lib.input_command_rect(C.CMD_RUNE_FIRST + rune, rect)
        x.value, y.value = rect[C.RECT_X], rect[C.RECT_Y]
        assert lib.spellbook_hovered_rune() == rune
        x.value, y.value = -1, -1
        assert lib.spellbook_hovered_rune() == -1
    x.value, y.value = rect[C.RECT_X], rect[C.RECT_Y]
    set_mode(lib, VANILLA)
    assert lib.spellbook_hovered_rune() == -1
    set_mode(lib, ADVANCED)
    action.champion = 0
    assert lib.spellbook_hovered_rune() == -1
