"""Recovery choices must work independently of slot files or a running dungeon."""

from dm2test import constants as C
from dm2test.structs import Session


def test_recovery_explicit_choices(lib):
    lib.recovery_open(False)
    assert lib.recovery_choice() == 0
    lib.recovery_event(C.INPUT_KEY, C.KEY_QUICK_LOAD, 0)
    assert lib.recovery_choice() == 1
    lib.recovery_open(True)
    lib.recovery_event(C.INPUT_KEY, C.SCAN_ESCAPE, 0)
    assert lib.recovery_choice() == 3


def test_missing_and_corrupt_recovery_preserve_files(lib, tmp_path):
    state = Session.in_dll(lib, "g_session")
    state.save_dir = str(tmp_path).encode()
    slot = tmp_path / "SKSAVE0.DAT"
    slot.write_bytes(b"keep slot")
    assert lib.recovery_load_quick() == 1
    quick = tmp_path / "QUICK.DAT"
    quick.write_bytes(b"broken")
    assert lib.recovery_load_quick() == 2
    assert quick.read_bytes() == b"broken"
    assert slot.read_bytes() == b"keep slot"
    assert not (tmp_path / "QUICK.TMP").exists()


def test_recovery_error_returns_to_chooser_and_restores_palette(lib, tmp_path):
    import ctypes
    from dm2test.structs import MainLoop

    Session.in_dll(lib, 'g_session').save_dir = str(tmp_path).encode()
    loop = MainLoop.in_dll(lib, 'g_main_loop')
    loop.quit = False
    palette = (ctypes.c_uint8 * (C.PALETTE_COLORS * C.RGB_CHANNELS)).in_dll(lib, 'g_video_palette')
    ctypes.memset(palette, 17, len(palette))
    calls = []

    @ctypes.CFUNCTYPE(None)
    def frame():
        calls.append(lib.recovery_is_open())
        # First try the absent quick file, then exit the chooser that must remain open.
        lib.input_key_down(C.KEY_QUICK_LOAD if len(calls) == 1 else C.SCAN_ESCAPE)

    hook = ctypes.c_void_p.in_dll(lib, 'g_play_frame_hook')
    hook.value = ctypes.cast(frame, ctypes.c_void_p).value
    try:
        assert not lib.recovery_run(True)
    finally:
        hook.value = None
    assert calls == [True, True]
    assert bytes(palette) == bytes([17]) * len(palette)
    assert not list(tmp_path.iterdir())


def test_only_quick_save_can_be_recovered_without_a_slot(lib, advanced):
    from dm2test.structs import Game

    _, data = advanced
    Session.in_dll(lib, "g_session").save_dir = b""
    assert lib.advanced_quick_save()
    before = (data / 'QUICK.DAT').read_bytes()
    assert sorted(p.name for p in data.iterdir()) == ['QUICK.DAT']
    Game.in_dll(lib, 'g_game').game_time += 100
    assert lib.recovery_load_quick() == C.QUICK_LOADED
    assert (data / 'QUICK.DAT').read_bytes() == before
    assert sorted(p.name for p in data.iterdir()) == ['QUICK.DAT']
