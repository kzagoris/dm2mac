"""The settings file SETTINGS.TXT (src/data/settings.c): the parser, the loader, and the writer."""

import ctypes

import pytest

from dm2test import constants as C

NONE, SCALE2X, SCALE3X = C.SETTINGS_FILTER_NONE, C.SETTINGS_FILTER_SCALE2X, C.SETTINGS_FILTER_SCALE3X
XBRZ2X, XBRZ3X, XBRZ4X = C.SETTINGS_FILTER_XBRZ2X, C.SETTINGS_FILTER_XBRZ3X, C.SETTINGS_FILTER_XBRZ4X


class Settings(ctypes.Structure):
    _fields_ = [("filter", ctypes.c_int), ("aspect", ctypes.c_bool), ("scanlines", ctypes.c_bool), ("classic_steps", ctypes.c_bool)]


@pytest.fixture(scope="module", autouse=True)
def lib_prototypes(lib):
    lib.settings_parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t]  # Settings is a mirror of this module
    lib.settings_parse.restype = Settings  # Settings is a mirror of this module
    lib.settings_load.argtypes = [ctypes.c_char_p]  # Settings is a mirror of this module
    lib.settings_load.restype = Settings  # Settings is a mirror of this module


def parse(lib, text):
    return lib.settings_parse(text, len(text)).filter


@pytest.mark.parametrize("text, filter", [
    (b"filter=scale2x\n", SCALE2X),
    (b"filter=scale3x", SCALE3X),  # no final newline
    (b"filter=xbrz2x\n", XBRZ2X),
    (b"filter=xbrz3x\n", XBRZ3X),
    (b"filter=xbrz4x\n", XBRZ4X),
    (b"filter=scale2x\r\nfilter=scale3x\r\n", SCALE3X),  # CR stripped, the last line wins
    (b"", NONE),
    (b"music=fm-towns\nfilter=scale2x\nvolume=3\n", SCALE2X),  # unknown keys
    (b"filter=scale4x\n", NONE),  # bad value
    (b"filter=scale2x\nfilter=bad\n", SCALE2X),  # a bad line keeps the earlier value
    (b"filter\nfilter =scale2x\n=scale2x\n", NONE),
    (b"filter=" + b"x" * 80 + b"\nfilter=scale2x\n", SCALE2X),  # an overlong line is skipped, the next one counts
    (b"filter=scale3x" + b" " * 60 + b"\n", NONE),  # a valid start of an overlong line is not read
])
def test_parse(lib, text, filter):
    assert parse(lib, text) == filter


@pytest.mark.parametrize("text, aspect, scanlines", [
    (b"", False, False),
    (b"aspect=on\n", True, False),
    (b"scanlines=on\naspect=off\n", False, True),
    (b"aspect=on\r\nscanlines=on\r\nfilter=scale2x\r\n", True, True),
    (b"aspect=on\naspect=yes\nscanlines=1\n", True, False),  # a bad value keeps the earlier value
    (b"aspect=on\naspect=off\n", False, False),  # the last line wins
])
def test_parse_the_switches(lib, text, aspect, scanlines):
    settings = lib.settings_parse(text, len(text))
    assert (settings.aspect, settings.scanlines) == (aspect, scanlines)


def test_a_file_over_the_limit_gives_the_defaults(lib):
    line = b"filter=scale2x\n"
    assert parse(lib, line + b"#" * (C.SETTINGS_MAX_SIZE - len(line))) == SCALE2X
    assert parse(lib, line + b"#" * (C.SETTINGS_MAX_SIZE - len(line) + 1)) == NONE


def test_save_and_load(lib, tmp_path):
    folder = str(tmp_path).encode()
    assert lib.settings_load(folder).filter == NONE, "a missing file"
    for filter, name in ((SCALE3X, "scale3x"), (SCALE2X, "scale2x"), (NONE, "none"),
                         (XBRZ2X, "xbrz2x"), (XBRZ3X, "xbrz3x"), (XBRZ4X, "xbrz4x")):
        assert lib.settings_save(folder, ctypes.byref(Settings(filter)))
        assert (tmp_path / "SETTINGS.TXT").read_text() == f"filter={name}\naspect=off\nscanlines=off\nclassic_steps=off\n"
        assert lib.settings_load(folder).filter == filter
    assert lib.settings_save(folder, ctypes.byref(Settings(SCALE2X, True, True)))
    assert (tmp_path / "SETTINGS.TXT").read_text() == "filter=scale2x\naspect=on\nscanlines=on\nclassic_steps=off\n"
    loaded = lib.settings_load(folder)
    assert (loaded.filter, loaded.aspect, loaded.scanlines) == (SCALE2X, True, True)
    assert [p.name for p in tmp_path.iterdir()] == ["SETTINGS.TXT"], "no temporary file stays"


def test_failed_save_keeps_the_old_file(lib, tmp_path):
    folder = str(tmp_path).encode()
    lib.settings_save(folder, ctypes.byref(Settings(SCALE2X)))
    (tmp_path / "SETTINGS.TMP").mkdir()  # the temporary file cannot be created
    assert not lib.settings_save(folder, ctypes.byref(Settings(SCALE3X)))
    assert (tmp_path / "SETTINGS.TXT").read_text() == "filter=scale2x\naspect=off\nscanlines=off\nclassic_steps=off\n"
    assert not lib.settings_save(str(tmp_path / "missing").encode(), ctypes.byref(Settings(SCALE3X)))
    assert not lib.settings_save(folder, ctypes.byref(Settings(0)))


@pytest.mark.parametrize("filter,factor", [(NONE, 1), (SCALE2X, 2), (SCALE3X, 3),
                                          (XBRZ2X, 2), (XBRZ3X, 3), (XBRZ4X, 4), (0, 0), (99, 0)])
def test_filter_texture_scale_is_independent_of_the_filter_id(lib, filter, factor):
    assert lib.settings_filter_factor(filter) == factor


@pytest.mark.parametrize("text, expected", [
    (b"", False),
    (b"classic_steps=on\n", True),
    (b"classic_steps=on\nclassic_steps=bad\n", True),
    (b"classic_steps=on\nclassic_steps=off\n", False),
])
def test_classic_steps_parse(lib, text, expected):
    assert lib.settings_parse(text, len(text)).classic_steps == expected


def test_classic_steps_round_trip(lib, tmp_path):
    folder = str(tmp_path).encode()
    assert lib.settings_save(folder, ctypes.byref(Settings(NONE, False, False, True)))
    assert lib.settings_load(folder).classic_steps
