"""Classic steps present the destination on the input tick without advancing the game step."""

import ctypes

import pytest

from dm2test import constants as C
from dm2test.interface import c_screen
from dm2test.scenario import SoundRequest
from dm2test.structs import Bitmap, Game, Location, MainLoop, Party, Position, Session, Sound, SoundCue


@pytest.fixture
def walk(lib, gdat, game_dir):
    """A fresh dungeon and an empty pair of floor squares, without a private save fixture."""
    lib.gdat_select(gdat)
    assert lib.zone_load(gdat) and lib.pal_load_color_ramps(gdat) and lib.text_init(gdat)
    lib.draw_init(gdat)
    Session.in_dll(lib, "g_session").data_dir = str(game_dir).encode()
    ctypes.c_int.in_dll(lib, "g_game_mode").value = C.GAME_MODE_ADVANCED
    loop = MainLoop.in_dll(lib, "g_main_loop")
    loop.tick_count, loop.timer_ticks, loop.end, loop.quit = 0, 0, False, False
    lib.input_init()
    assert lib.session_start_new_game()
    game = Game.in_dll(lib, "g_game")
    at = next(Position(game.party_map, x, y)
              for x in range(1, lib.dungeon_map_width(game.party_map) - 2)
              for y in range(1, lib.dungeon_map_height(game.party_map) - 1)
              if all(lib.dungeon_square(game.party_map, xx, y) >> C.SQUARE_TYPE_SHIFT == C.SQUARE_FLOOR
                     and lib.dungeon_square_first_thing(game.party_map, xx, y) == C.THING_ENDOFLIST for xx in (x, x + 1)))
    lib.move_party_to(at, C.DIR_EAST)
    lib.view_draw_dungeon(Location(at.x, at.y), C.DIR_EAST)
    lib.play_present_viewport()
    lib.move_set_classic_steps(True)
    yield game, loop, at
    lib.move_set_classic_steps(False)
    lib.advanced_reset()
    ctypes.c_int.in_dll(lib, "g_game_mode").value = C.GAME_MODE_VANILLA


@pytest.mark.parametrize("phase", [0, 6, 7])
@pytest.mark.parametrize("mouse", [False, True])
@pytest.mark.parametrize("command, direction, dx, turn", [
    (C.CMD_MOVE_FORWARD, C.DIR_EAST, 1, 0),
    (C.CMD_MOVE_BACK, C.DIR_WEST, 1, 0),
    (C.CMD_MOVE_RIGHT, C.DIR_NORTH, 1, 0),
    (C.CMD_MOVE_LEFT, C.DIR_SOUTH, 1, 0),
    (C.CMD_TURN_LEFT, C.DIR_EAST, 0, -1),
    (C.CMD_TURN_RIGHT, C.DIR_EAST, 0, 1),
])
def test_destination_is_presented_on_the_input_tick(lib, walk, command, direction, dx, turn, mouse, phase):
    game, loop, at = walk
    lib.move_party_to(at, direction)
    for _ in range(phase):
        lib.play_tick()
    lib.seen_reset()
    time = game.game_time
    if mouse:
        lib.input_simulate_command_click(command)
    else:
        key = {C.CMD_MOVE_FORWARD: C.KEYPAD_FORWARD, C.CMD_MOVE_BACK: C.KEYPAD_BACK,
               C.CMD_MOVE_RIGHT: C.KEYPAD_RIGHT, C.CMD_MOVE_LEFT: C.KEYPAD_LEFT,
               C.CMD_TURN_LEFT: C.KEYPAD_TURN_LEFT, C.CMD_TURN_RIGHT: C.KEYPAD_TURN_RIGHT}[command]
        lib.input_key_down(key)
    lib.play_tick()
    assert (game.party_x, game.party_y, game.party_dir) == (at.x + dx, at.y, (direction + turn) % C.DIR_COUNT)
    assert lib.view_shows_party(), "the destination must not wait for the next game step"
    assert game.game_time == time + (phase == 7)
    assert lib.seen_is_seen(Position(game.party_map, game.party_x, game.party_y))
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    assert lib.zone_get_viewport_rect(rect)
    x, y, width, height = rect
    screen = c_screen(lib)
    view = Bitmap.in_dll(lib, "g_view_viewport")
    for row in range(height):
        assert screen[(y + row) * C.SCREEN_W + x:(y + row) * C.SCREEN_W + x + width] == bytes(
            view.pixels[row * C.VIEWPORT_WIDTH:row * C.VIEWPORT_WIDTH + width])


def test_extra_redraw_preserves_game_state_and_random_seed(lib, walk):
    game, loop, _ = walk
    lib.explosion_create(C.SPELL_FIREBALL, 100, game.party_x + 1, game.party_y, C.EXPLOSION_AT_CENTER)
    seed = ctypes.c_uint32.in_dll(lib, "g_random_seed").value
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    assert ctypes.c_uint32.in_dll(lib, "g_random_seed").value != seed, "exercise drawing that uses random numbers"
    lib.move_party(C.CMD_MOVE_FORWARD)
    lib.seen_mark(Position(game.party_map, game.party_x, game.party_y), game.party_dir)
    seed = ctypes.c_uint32.in_dll(lib, "g_random_seed").value
    state = bytes(game)
    party = bytes(Party.in_dll(lib, "g_party"))
    lib.play_tick()
    assert lib.view_shows_party()
    assert bytes(game) == state
    assert bytes(Party.in_dll(lib, "g_party")) == party
    assert ctypes.c_uint32.in_dll(lib, "g_random_seed").value == seed
    assert loop.tick_count == 1


@pytest.mark.parametrize("mode, classic", [(C.GAME_MODE_ADVANCED, False), (C.GAME_MODE_VANILLA, True),
                                          (C.GAME_MODE_TURNBASED, True)])
def test_other_movement_modes_keep_the_game_step_redraw(lib, walk, mode, classic):
    game, _, at = walk
    ctypes.c_int.in_dll(lib, "g_game_mode").value = mode
    lib.move_set_classic_steps(classic)
    lib.input_key_down(C.KEYPAD_TURN_RIGHT)
    lib.play_tick()
    assert game.party_dir == C.DIR_SOUTH
    assert not lib.view_shows_party()
    assert not lib.input_is_idle(), "other modes retain the original feedback script"
    for _ in range(C.STEP_TICKS - 1):
        lib.play_tick()
    assert lib.view_shows_party()


def test_cooldown_still_blocks_the_next_move(lib, walk):
    game, _, at = walk
    game.party_move_cooldown = 2
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert (game.party_x, game.party_y) == (at.x, at.y)
    assert game.party_move_cooldown == 2


def test_pending_map_transition_waits_for_the_game_step(lib, walk):
    game, _, at = walk
    party = Party.in_dll(lib, "g_party")
    lib.move_party_turn(C.CMD_TURN_RIGHT)
    party.new_map = game.party_map
    lib.play_tick()
    assert party.new_map == game.party_map
    assert not lib.view_shows_party()


def audible_floor(lib, gdat, at):
    """A real floor sensor sound generated by stepping onto the destination."""
    sensor = lib.dungeon_unused_thing(C.THING_SENSOR)
    assert sensor != C.THING_NONE
    lib.thing_set_word(sensor, C.SENSOR_TYPE_DATA, 1)  # FLOOR_ANY
    lib.thing_set_word(sensor, C.SENSOR_FLAGS, C.SENSOR_AUDIBLE)
    lib.thing_set_word(sensor, C.SENSOR_TARGET, at.x << C.SENSOR_TARGET_X_SHIFT | at.y << C.SENSOR_TARGET_Y_SHIFT)
    assert lib.dungeon_link_thing_to_square(sensor, at.map, at.x + 1, at.y)
    cue = lib.sndreq_cue_with_fallback(SoundCue(C.CAT_FLOOR_ORNAMENT, 0, C.SOUND_ENTRY_ACTIVATE,
                                             C.SOUND_PRIORITY_CLICK, C.SOUND_VOLUME_NORMAL), C.SOUND_FALLBACK)
    sample = Sound()
    assert lib.sndreq_load(gdat, cue.category, cue.index, cue.entry, ctypes.byref(sample)) == 0
    return cue


@pytest.mark.parametrize("mouse", [False, True])
@pytest.mark.parametrize("phase", [0, 6, 7])
def test_movement_sound_starts_with_the_destination_and_does_not_repeat(lib, gdat, walk, mouse, phase):
    game, _, at = walk
    cue = audible_floor(lib, gdat, at)
    for _ in range(phase):
        lib.play_tick()
    lib.sndreq_reset_requests()
    lib.sound_stop_all()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    started = ctypes.c_int.in_dll(lib, "g_sound_started_count")
    played.value = started.value = 0
    if mouse:
        lib.input_simulate_command_click(C.CMD_MOVE_FORWARD)
    else:
        lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert game.party_x == at.x + 1 and lib.view_shows_party()
    assert started.value >= 1 and played.value >= 1, "movement sound must start on the same tick as its destination"
    request = (SoundRequest * C.SOUND_PLAYED_MAX).in_dll(lib, "g_sound_played")[0]
    assert (request.category, request.index, request.entry) == (cue.category, cue.index, cue.entry)
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
    requests = (SoundRequest * C.SOUND_PLAYED_MAX).in_dll(lib, "g_sound_played")
    assert sum((r.category, r.index, r.entry) == (cue.category, cue.index, cue.entry)
               for r in requests[:played.value]) == 1, "the next game step must not replay the movement sound"


def test_movement_sound_does_not_flush_unrelated_queued_sounds(lib, gdat, walk):
    game, _, at = walk
    audible_floor(lib, gdat, at)
    lib.sndreq_reset_requests()
    lib.sound_stop_all()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    played.value = 0
    lib.sndreq_request(SoundCue(C.CAT_DUNGEON, 0, C.SOUND_EXPLODE, C.SOUND_PRIORITY_CLICK,
                               C.SOUND_VOLUME_NORMAL), game.party_x, game.party_y, C.SOUND_STEP_END)
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert played.value == 1
    requests = (SoundRequest * C.SOUND_PLAYED_MAX).in_dll(lib, "g_sound_played")
    assert requests[0].category == C.CAT_FLOOR_ORNAMENT
    for _ in range(C.STEP_TICKS - 1):
        lib.play_tick()
    assert sum((r.category, r.index, r.entry) == (C.CAT_DUNGEON, 0, C.SOUND_EXPLODE)
               for r in requests[:played.value]) == 1


@pytest.mark.parametrize("mode, classic", [(C.GAME_MODE_ADVANCED, False), (C.GAME_MODE_VANILLA, True)])
def test_other_movement_modes_keep_step_end_sound_timing(lib, gdat, walk, mode, classic):
    _, _, at = walk
    audible_floor(lib, gdat, at)
    ctypes.c_int.in_dll(lib, "g_game_mode").value = mode
    lib.move_set_classic_steps(classic)
    lib.sndreq_reset_requests()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    played.value = 0
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert played.value == 0
    for _ in range(C.STEP_TICKS - 1):
        lib.play_tick()
    assert played.value >= 1


def test_movement_sound_and_next_move_still_wait_for_the_cooldown(lib, gdat, walk):
    game, _, at = walk
    cue = audible_floor(lib, gdat, at)
    lib.sndreq_reset_requests()
    lib.sound_stop_all()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    played.value = 0
    game.party_move_cooldown = 1
    lib.input_key_down(C.KEYPAD_FORWARD)
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
        assert game.party_x == at.x
    requests = (SoundRequest * C.SOUND_PLAYED_MAX).in_dll(lib, "g_sound_played")
    assert not any((r.category, r.index, r.entry) == (cue.category, cue.index, cue.entry)
                   for r in requests[:played.value])
    assert game.party_move_cooldown == 0
    lib.play_tick()
    assert game.party_x == at.x + 1 and lib.view_shows_party()
    assert any((r.category, r.index, r.entry) == (cue.category, cue.index, cue.entry)
               for r in requests[:played.value])


def test_immediate_sound_batch_deduplicates_and_keeps_the_existing_queue(lib, gdat, walk):
    game, _, at = walk
    cue = audible_floor(lib, gdat, at)
    lib.sndreq_reset_requests()
    lib.sound_stop_all()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    played.value = 0
    lib.sndreq_request(cue, game.party_x, game.party_y, C.SOUND_STEP_END)
    lib.sndreq_set_immediate(True)
    for _ in range(2):
        lib.sndreq_request(cue, game.party_x, game.party_y, C.SOUND_STEP_END)
    lib.sndreq_set_immediate(False)
    assert played.value == 1
    lib.sndreq_flush_requests(False)
    assert played.value == 2


def test_immediate_sound_batch_keeps_explicit_timeline_delays(lib, gdat, walk):
    game, _, at = walk
    cue = audible_floor(lib, gdat, at)
    lib.sndreq_reset_requests()
    played = ctypes.c_int.in_dll(lib, "g_sound_played_count")
    played.value = 0
    lib.sndreq_set_immediate(True)
    lib.sndreq_request_after(cue, game.party_x, game.party_y, 2)
    lib.sndreq_set_immediate(False)
    assert played.value == 0
    for _ in range(2 * C.STEP_TICKS):
        lib.play_tick()
    requests = (SoundRequest * C.SOUND_PLAYED_MAX).in_dll(lib, "g_sound_played")
    assert any((r.category, r.index, r.entry) == (cue.category, cue.index, cue.entry)
               for r in requests[:played.value])


@pytest.mark.parametrize("mouse", [False, True])
def test_arrow_feedback_does_not_block_the_next_command_or_key_repeat(lib, walk, mouse):
    game, loop, at = walk
    initial_time = game.game_time
    for direction in (C.DIR_SOUTH, C.DIR_WEST, C.DIR_NORTH):
        if mouse:
            lib.input_simulate_command_click(C.CMD_TURN_RIGHT)
        elif direction == C.DIR_SOUTH:
            lib.input_key_down(C.KEYPAD_TURN_RIGHT)
        else:
            lib.input_key_repeat(C.KEYPAD_TURN_RIGHT)
        lib.play_tick()
        assert game.party_dir == direction, "pressed-arrow feedback must not delay a ready command"
        assert lib.input_is_idle(), "visual feedback must not block autorepeat"
    assert game.game_time == initial_time and loop.tick_count == 3


def arrow_pixels(lib, command):
    rect = (ctypes.c_int16 * C.RECT_SIZE)()
    assert lib.input_command_rect(command, rect)
    x, y, width, height = rect
    screen = c_screen(lib)
    return b"".join(screen[(y + row) * C.SCREEN_W + x:(y + row) * C.SCREEN_W + x + width] for row in range(height))


def test_arrow_releases_on_its_own_timer_between_game_steps(lib, walk):
    _, loop, _ = walk
    released = arrow_pixels(lib, C.CMD_TURN_RIGHT)
    lib.input_key_down(C.KEYPAD_TURN_RIGHT)
    lib.play_tick()
    assert arrow_pixels(lib, C.CMD_TURN_RIGHT) != released
    for _ in range(4):
        lib.play_tick()
    assert loop.tick_count == 5
    assert arrow_pixels(lib, C.CMD_TURN_RIGHT) == released


def test_a_second_arrow_does_not_leave_the_first_one_pressed(lib, walk):
    right = arrow_pixels(lib, C.CMD_TURN_RIGHT)
    left = arrow_pixels(lib, C.CMD_TURN_LEFT)
    lib.input_key_down(C.KEYPAD_TURN_RIGHT)
    lib.play_tick()
    lib.input_key_down(C.KEYPAD_TURN_LEFT)
    lib.play_tick()
    assert arrow_pixels(lib, C.CMD_TURN_RIGHT) == right
    assert arrow_pixels(lib, C.CMD_TURN_LEFT) != left


def audible_teleporter(lib, at, same_map=False):
    maps = [at.map] if same_map else [m for m in range(lib.dungeon_map_count()) if m != at.map]
    destination = next(Position(m, x, y) for m in maps
                       for x in range(1, lib.dungeon_map_width(m) - 1)
                       for y in range(1, lib.dungeon_map_height(m) - 1)
                       if (m, x, y) not in ((at.map, at.x, at.y), (at.map, at.x + 1, at.y))
                       and lib.dungeon_square(m, x, y) >> C.SQUARE_TYPE_SHIFT == C.SQUARE_FLOOR
                       and lib.dungeon_square_first_thing(m, x, y) == C.THING_ENDOFLIST)
    teleporter = lib.dungeon_unused_thing(C.THING_TELEPORTER)
    assert teleporter != C.THING_NONE
    party_scope = C.TELEPORTER_SCOPE_MASK & ~C.TELEPORTER_SCOPE_CREATURES
    lib.thing_set_word(teleporter, C.TELEPORTER_TARGET,
                      destination.x | destination.y << C.LOCATION_Y_SHIFT
                      | party_scope << C.TELEPORTER_SCOPE_SHIFT | C.TELEPORTER_AUDIBLE)
    lib.thing_set_word(teleporter, C.TELEPORTER_MAP, destination.map << C.TELEPORTER_MAP_SHIFT)
    square = ctypes.c_uint8.from_address(lib.dungeon_square_byte(at.map, at.x + 1, at.y))
    square.value = C.SQUARE_TELEPORTER << C.SQUARE_TYPE_SHIFT | C.SQUARE_TELEPORTER_ON
    assert lib.dungeon_link_thing_to_square(teleporter, at.map, at.x + 1, at.y)
    return destination


@pytest.fixture
def teleport_audio(lib, gdat, walk):
    """Trace the actual voice start, retaining the real sound mixer."""
    sample = Sound()
    assert lib.sndreq_load(gdat, C.CAT_TELEPORTER, 0, C.SOUND_ENTRY_TELEPORT, ctypes.byref(sample)) == 0
    data = ctypes.cast(sample.data, ctypes.c_void_p).value
    signature = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.POINTER(Sound), ctypes.c_uint8, ctypes.c_int8, ctypes.c_int8)
    hooks = (ctypes.c_void_p * 3).in_dll(lib, "g_hooks")
    saved = hooks[2]
    original = signature(saved)
    events = []

    @signature
    def record(sound, volume, lateral, forward):
        if ctypes.cast(sound.contents.data, ctypes.c_void_p).value == data:
            events.append((MainLoop.in_dll(lib, "g_main_loop").timer_ticks, bool(lib.view_shows_party()),
                           Party.in_dll(lib, "g_party").new_map))
        return original(sound, volume, lateral, forward)

    lib.sndreq_reset_requests()
    lib.sound_stop_all()
    hooks[2] = ctypes.cast(record, ctypes.c_void_p).value
    yield events
    hooks[2] = saved


@pytest.mark.parametrize("phase", [0, 6, 7])
@pytest.mark.parametrize("same_map", [False, True])
def test_teleport_sound_starts_after_the_destination_is_drawn(lib, walk, teleport_audio, phase, same_map):
    game, loop, at = walk
    destination = audible_teleporter(lib, at, same_map)
    for _ in range(phase):
        lib.play_tick()
    first_tick = loop.timer_ticks
    lib.input_key_down(C.KEYPAD_FORWARD)
    for _ in range(C.STEP_TICKS + 1):
        lib.play_tick()
    assert (game.party_map, game.party_x, game.party_y) == (destination.map, destination.x, destination.y)
    expected = first_tick + (1 if same_map else C.STEP_TICKS - phase)
    assert teleport_audio == [(expected, True, -1)], "teleport audio must start once, after drawing the arrival view"


def test_teleport_sound_waits_while_the_inventory_covers_the_destination(lib, walk, teleport_audio):
    _, _, at = walk
    audible_teleporter(lib, at)
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    lib.game_set_open_inventory(1)
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
    assert teleport_audio == []
    lib.game_set_open_inventory(0)
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
    assert len(teleport_audio) == 1 and teleport_audio[0][1:] == (True, -1)


def test_a_new_session_discards_the_previous_pending_teleport_sound(lib, walk, teleport_audio):
    _, _, at = walk
    audible_teleporter(lib, at)
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert teleport_audio == []
    assert lib.session_start_new_game()
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
    assert teleport_audio == []


def test_loading_discards_the_previous_pending_teleport_sound(lib, gdat, walk, teleport_audio, tmp_path):
    _, _, at = walk
    path, backup = tmp_path / "BEFORE.DAT", tmp_path / "BEFORE.BAK"
    assert lib.save_write(str(path).encode(), str(backup).encode(), b"BEFORE", gdat) == 0
    audible_teleporter(lib, at)
    lib.input_key_down(C.KEYPAD_FORWARD)
    lib.play_tick()
    assert teleport_audio == []
    assert lib.session_start_save(str(path).encode())
    for _ in range(C.STEP_TICKS):
        lib.play_tick()
    assert teleport_audio == []


@pytest.mark.parametrize("mode, classic", [(C.GAME_MODE_ADVANCED, False), (C.GAME_MODE_VANILLA, True)])
def test_other_modes_retain_original_deferred_teleport_timing(lib, walk, teleport_audio, mode, classic):
    _, _, at = walk
    lib.move_party_to(at, C.DIR_NORTH)
    lib.view_draw_dungeon(Location(at.x, at.y), C.DIR_NORTH)
    lib.play_present_viewport()
    audible_teleporter(lib, at)
    ctypes.c_int.in_dll(lib, "g_game_mode").value = mode
    lib.move_set_classic_steps(classic)
    lib.input_key_down(C.KEYPAD_RIGHT)  # a sidestep has no walking frames in the original
    lib.play_tick()
    assert teleport_audio == [(1, False, -1)]
