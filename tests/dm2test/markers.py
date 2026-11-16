"""Paths of the project and the skip marker for tests that need the original game files."""

import zlib
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
GAME = ROOT / "game"
SAVES = ROOT / "tests" / "saves"

# The build of SKULL.EXE that the decompilation, the addresses and the differential tests are of. Another DOS release
# (for example the one of eXoDOS, 522641 bytes) has the same game files but its code at other addresses.
SKULL_SIZE, SKULL_CRC = 522637, 0xC1526F95
SKULL_PLACES = (ROOT / "reference" / "SKULL.EXE", GAME / "SKULL.EXE")
SKULL_MISSING = "no SKULL.EXE of the decompiled build in reference/ or game/"


def find_skull():
    """The first of reference/SKULL.EXE and game/SKULL.EXE that is the decompiled build, or None."""
    for path in SKULL_PLACES:
        if path.is_file() and path.stat().st_size == SKULL_SIZE and zlib.crc32(path.read_bytes()) == SKULL_CRC:
            return path
    return None


SKULL = find_skull()
needs_game = pytest.mark.skipif(SKULL is None, reason=SKULL_MISSING)

# The private start save of dm2-research (a new game after the join of the first champion, 3 timeline events). The
# differential tests that use it are tuned to its records; another save, for example one that build/dm2 writes after
# new_game, serves the port tests only.
START = SAVES / "start.DAT"
PRIVATE_START_SIZE = 50452
START_NOT_PRIVATE = "tests/saves/start.DAT is not the private start save of dm2-research"


def start_is_private():
    return START.is_file() and START.stat().st_size == PRIVATE_START_SIZE
