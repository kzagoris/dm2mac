import ctypes
import pytest


@pytest.fixture(scope="module", autouse=True)
def lib_prototypes(lib):
    lib.video_layout.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_bool]  # VideoLayout is a mirror of this module
    lib.video_layout.restype = VideoLayout  # VideoLayout is a mirror of this module
    lib.video_map_mouse.restype = VideoPoint  # VideoPoint is a mirror of this module


def test_vga_palette_conversion(lib):
    assert [lib.video_vga_to_8bit(v) for v in (0, 32, 63)] == [0, 130, 255]


class VideoPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int)]


class VideoLayout(ctypes.Structure):
    _fields_ = [("scale_x", ctypes.c_float), ("scale_y", ctypes.c_float), ("x", ctypes.c_float), ("y", ctypes.c_float)]


def test_layout_fractional(lib):
    layout = lib.video_layout(1000, 700, False)
    assert (layout.scale_x, layout.scale_y, layout.x, layout.y) == pytest.approx((3.125, 3.125, 0, 37.5))
    assert lib.video_layout(0, 0, False).scale_x == 1  # minimized window


def test_layout_of_the_4_3_picture(lib):
    layout = lib.video_layout(1000, 700, True)  # 320 x 240 square pixels: the height limits the scale
    # the fields are float: a build without fused multiply-add rounds the left edge 4e-5 away
    assert (layout.scale_x, layout.scale_y, layout.x, layout.y) == pytest.approx((700 / 240, 3.5, 100 / 3, 0), rel=1e-5)
    layout = lib.video_layout(640, 600, True)  # the width limits the scale
    assert (layout.scale_x, layout.scale_y, layout.x, layout.y) == pytest.approx((2, 2.4, 0, 60))


def test_map_mouse_fractional(lib):
    # 1000 x 700 output: scale 3.125, image at (0, 37.5)
    for x, y, expected in [(0, 37, (0, 0)), (3, 41, (0, 1)), (4, 44, (1, 2)), (999, 699, (319, 199)), (500, 0, (160, 0))]:
        at = lib.video_map_mouse(1000, 700, x, y, False)
        assert (at.x, at.y) == expected


def test_map_mouse_in_the_4_3_picture(lib):
    # 640 x 600 output: 2 pixels across and 2.4 down per screen pixel, image at (0, 60)
    for x, y, expected in [(0, 59, (0, 0)), (1, 62, (0, 0)), (2, 63, (1, 1)), (639, 539, (319, 199)), (320, 599, (160, 199))]:
        at = lib.video_map_mouse(640, 600, x, y, True)
        assert (at.x, at.y) == expected
