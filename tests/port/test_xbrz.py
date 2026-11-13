"""xBRZ consumes real colours, blends edges, and keeps the opaque screen border."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.structs import Session


def scale(lib, factor, rows):
    h, w = len(rows), len(rows[0])
    src = (ctypes.c_uint32 * (w * h))(*(v for row in rows for v in row))
    count = w * h * factor * factor
    guard = 0x12345678
    dst = (ctypes.c_uint32 * (count + 2))(guard, *([0] * count), guard)
    assert lib.scale_xbrz(src, w, h, factor, ctypes.byref(dst, ctypes.sizeof(ctypes.c_uint32)))
    assert dst[0] == dst[-1] == guard, "the scaler stays inside the output image"
    return list(dst)[1:-1]


@pytest.mark.parametrize("factor", [2, 3, 4])
@pytest.mark.parametrize("rows", [[[0xFF123456]], [[0xFF123456] * 3] * 2,
                                  [[0xFF123456] * 4], [[0xFF123456]] * 4])
def test_flat_colours_and_narrow_images_keep_their_colour_at_the_edges(lib, factor, rows):
    assert set(scale(lib, factor, rows)) == {0xFF123456}


@pytest.mark.parametrize("factor", [2, 3, 4])
def test_diagonals_blend_colours_without_losing_opacity(lib, factor):
    black, red = 0xFF000000, 0xFFFF0000
    rows = [[red if x <= y else black for x in range(5)] for y in range(5)]
    out = scale(lib, factor, rows)
    assert set(out) - {black, red}, "xBRZ blends the diagonal rather than replicating palette indices"
    assert all(v & 0xFF00FFFF == 0xFF000000 for v in out), "opaque red shades, with no swapped channels"
    assert scale(lib, factor, [[black] * 5] * 5) == [black] * (25 * factor * factor)
    assert scale(lib, factor, rows) == out, "a new palette/frame does not leave cached output"


@pytest.mark.parametrize("factor,w,h", [(0, 1, 1), (1, 1, 1), (5, 1, 1), (2, 0, 1), (2, 1, 0)])
def test_invalid_sizes_and_factors_leave_the_output_untouched(lib, factor, w, h):
    src = (ctypes.c_uint32 * 1)(0xFFFFFFFF)
    dst = (ctypes.c_uint32 * 16)(*([0x12345678] * 16))
    assert not lib.scale_xbrz(src, w, h, factor, dst)
    assert list(dst) == [0x12345678] * 16


def test_options_cycle_saves_and_restores_every_xbrz_choice(lib, tmp_path):
    session = Session.in_dll(lib, "g_session")
    mode = ctypes.c_int.in_dll(lib, "g_game_mode")
    previous = session.save_dir, mode.value
    session.save_dir, mode.value = str(tmp_path).encode(), C.GAME_MODE_ADVANCED
    try:
        lib.upscale_start(C.SETTINGS_FILTER_NONE)
        for name in ("scale2x", "scale3x", "xbrz2x", "xbrz3x", "xbrz4x", "none"):
            lib.upscale_cycle_filter()
            expected = lib.settings_filter_parse(name.encode())
            assert lib.upscale_get_filter() == expected
            assert (tmp_path / "SETTINGS.TXT").read_text().splitlines()[0] == f"filter={name}"
            lib.upscale_start(C.SETTINGS_FILTER_UNSET)
            assert lib.upscale_get_filter() == expected
    finally:
        session.save_dir, mode.value = previous


def test_a_refused_xbrz_filter_keeps_the_saved_working_filter(lib, tmp_path):
    session = Session.in_dll(lib, "g_session")
    mode = ctypes.c_int.in_dll(lib, "g_game_mode")
    hook_slot = ctypes.c_void_p.in_dll(lib, "g_upscale_filter_hook")
    previous = session.save_dir, mode.value, hook_slot.value
    hook = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_int)(lambda filter: filter != C.SETTINGS_FILTER_XBRZ2X)
    session.save_dir, mode.value = str(tmp_path).encode(), C.GAME_MODE_ADVANCED
    hook_slot.value = ctypes.cast(hook, ctypes.c_void_p).value
    try:
        lib.upscale_start(C.SETTINGS_FILTER_SCALE2X)
        lib.upscale_cycle_filter()
        before = (tmp_path / "SETTINGS.TXT").read_bytes()
        lib.upscale_cycle_filter()
        assert lib.upscale_get_filter() == C.SETTINGS_FILTER_SCALE3X
        assert (tmp_path / "SETTINGS.TXT").read_bytes() == before
    finally:
        session.save_dir, mode.value, hook_slot.value = previous
