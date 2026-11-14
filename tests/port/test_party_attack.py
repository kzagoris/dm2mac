"""Party volleys reuse manually chosen weapon actions and normal combat rules."""
import ctypes
import struct

import pytest

from dm2test import constants as C
from dm2test.modes import ADVANCED, VANILLA, set_mode
from dm2test.structs import ActionArea, Bitmap, Location, Position, ProjectileLaunch, Timeline, TimelineEvent, UiState
from dm2test.interface import things_of
from dm2test.scenario import champion_bytes, champion_records
from dm2test.input import session as game_session
from dm2test.markers import ROOT
from dm2test.gamefiles import require


@pytest.fixture
def session(lib, gdat, tmp_path):
    # These tests normalize the party and arena, so any local saved game is sufficient.
    # Do not substitute it for the private start.DAT used by the differential suite.
    reference = ROOT / 'tests/saves/start.DAT'
    source = reference if reference.exists() else ROOT / 'build/start.DAT'
    return game_session.__wrapped__(lib, gdat, require(source).read_bytes(), tmp_path)


def hand_item(lib, champion=0, hand=1):
    return struct.unpack_from('<H', bytes(champion_records(lib)), champion * C.CHAMPION_SIZE + 0xc3 + hand * 2)[0]


def actions(lib):
    count = ctypes.c_int.in_dll(lib, 'g_champ_action_count').value
    data = (ctypes.c_uint8 * 9).in_dll(lib, 'g_champ_actions')
    return [tuple(data[i * 3:i * 3 + 3]) for i in range(count)]


def choose_weapon(lib, champion=0, command=4, exclude=(), only=None):
    for weapon in things_of(lib, C.THING_WEAPON, 10000):
        if weapon in exclude or (only is not None and weapon != only):
            continue
        lib.champ_list_actions(champion, weapon, 1)
        for i, action in enumerate(actions(lib)):
            if lib.gtext_gdat_field(*action, C.FIELD_COMMAND) == command:
                lib.champ_remove_object_from_slot(champion, 1)
                lib.champ_add_object_to_slot(champion, weapon, 1)
                champion_bytes(lib, champion, 0x2a, bytes(3))
                lib.area_update(False)
                lib.action_area_open_hand(champion, 1)
                assert ActionArea.in_dll(lib, 'g_action').champion == champion + 1
                lib.champ_execute_selected_action(actions(lib).index(action))
                champion_bytes(lib, champion, 0x2a, bytes(3))
                return weapon, action
    raise AssertionError(f'no weapon action with command {command}')


def arena(lib, game):
    # A real dungeon floor with room in front; no wall-breaking or fixture-only combat path.
    game.champion_count = 1
    champion_bytes(lib, 0, 0x36, struct.pack('<hhhh', 100, 100, 1000, 1000))
    lib.dungeon_set_current_map(22)
    for x in range(1, lib.dungeon_map_width(22) - 1):
        for y in range(1, lib.dungeon_map_height(22) - 1):
            if all(lib.dungeon_square_at(Location(x + dx, y)) >> 5 == 1 and
                   lib.creature_get_group_at(Location(x + dx, y)) == C.THING_NONE for dx in (0, 1, 2)):
                lib.save_set_party_position(Position(22, x, y), 1)
                lib.view_enter_map(22)
                for i in range(game.champion_count):
                    champion_bytes(lib, i, 0x1c, bytes([1, (1 + i) % 4]))
                return Location(x + 1, y)
    raise AssertionError('no arena')


def enemy(lib, at):
    # The first non-static creature represented in the dungeon provides valid graphics/AI data.
    for group in things_of(lib, C.THING_GROUP, 10000):
        kind = lib.thing_word(group, C.GROUP_CREATURE) & 255
        lib.creature_group_info.restype = ctypes.c_void_p
        info = lib.creature_group_info(group)
        flags = ctypes.c_uint16.from_address(info).value
        if not flags & (C.CREATURE_STATIC | C.CREATURE_NON_MATERIAL):
            # The original save has a fixed thing pool. Move a real group rather than requiring a free record.
            for m in range(lib.dungeon_map_count()):
                lib.dungeon_set_current_map(m)
                for x in range(lib.dungeon_map_width(m)):
                    for y in range(lib.dungeon_map_height(m)):
                        if lib.creature_get_group_at(Location(x, y)) == group:
                            lib.dungeon_unlink_thing_from_square(group, m, x, y)
                            lib.dungeon_set_current_map(22)
                            assert lib.dungeon_link_thing_to_square(group, 22, at.x, at.y)
                            lib.thing_set_word(group, C.GROUP_CREATURE, kind | 0xff00)
                            lib.thing_set_word(group, C.GROUP_HEALTH, 10000)
                            return group
    raise AssertionError('no enemy')


def test_no_preference_never_attacks(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    lib.champ_reset_party_attacks()
    at = Location(game.party_x, game.party_y)
    assert not lib.champ_can_party_attack(C.THING_NONE, at)
    assert not lib.champ_party_attack(C.THING_NONE, at)


def test_empty_view_has_no_attack_target_and_vanilla_keeps_cursor(lib, session):
    set_mode(lib, VANILLA)
    at = Location()
    assert lib.view_attack_target(-1, 10, ctypes.byref(at)) == C.THING_NONE
    assert lib.input_party_attack_target(112, 100, ctypes.byref(at)) == C.THING_NONE
    assert lib.input_cursor_at(112, 100) != C.CURSOR_ATTACK


def test_chosen_attack_readiness_and_changed_weapon(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    weapon, chosen = choose_weapon(lib)
    group = enemy(lib, at)
    assert lib.champ_can_party_attack(group, at), (weapon, chosen, hand_item(lib),
        bytes(champion_records(lib))[0x1c:0x2d].hex(), game.champion_count,
        lib.creature_get_group_at(at), lib.gtext_gdat_field(*chosen, C.FIELD_REPEATS))
    before = bytes(champion_records(lib)), bytes(ActionArea.in_dll(lib, 'g_action')), actions(lib)
    seed = ctypes.c_uint32.in_dll(lib, 'g_random_seed').value
    for _ in range(5):
        assert lib.champ_can_party_attack(group, at)
    assert before == (bytes(champion_records(lib)), bytes(ActionArea.in_dll(lib, 'g_action')), actions(lib))
    assert ctypes.c_uint32.in_dll(lib, 'g_random_seed').value == seed
    champion_bytes(lib, 0, 0x2b, b'\x01')
    assert not lib.champ_can_party_attack(group, at)
    champion_bytes(lib, 0, 0x2b, b'\0')
    assert lib.champ_party_attack(group, at)
    assert bytes(champion_records(lib))[0x21] == lib.gtext_gdat_field(*chosen, C.FIELD_COMMAND)
    assert bytes(champion_records(lib))[0x2b] > 0
    champion_bytes(lib, 0, 0x2a, bytes(3))
    lib.champ_remove_object_from_slot(0, 1)
    lib.champ_add_object_to_slot(0, weapon, 1)
    assert lib.champ_can_party_attack(group, at)  # a changed weapon gets its basic attack


@pytest.mark.parametrize('condition', ['dead', 'back_row', 'turned', 'vanilla'])
def test_ineligible_champions_skip_without_spending(lib, session, condition):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib)
    group = enemy(lib, at)
    assert lib.champ_can_party_attack(group, at)
    if condition == 'dead':
        champion_bytes(lib, 0, 0x36, bytes(2))
    elif condition == 'back_row':
        # Facing east: relative back-right (west) is blocked by relative front-right (south).
        game.champion_count = 2
        champion_bytes(lib, 1, 0, bytes(champion_records(lib))[:C.CHAMPION_SIZE])
        champion_bytes(lib, 0, 0x1d, b'\x03')
        champion_bytes(lib, 1, 0x1d, b'\x02')
        champion_bytes(lib, 1, 0x2a, bytes([10, 10, 0]))  # the front champion is recovering too
    elif condition == 'turned':
        champion_bytes(lib, 0, 0x1c, b'\x00')
    elif condition == 'vanilla':
        set_mode(lib, VANILLA)
    before = bytes(champion_records(lib))
    assert not lib.champ_can_party_attack(group, at)
    assert not lib.champ_party_attack(group, at)
    assert bytes(champion_records(lib)) == before


class DrawCtx(ctypes.Structure):
    _fields_ = [('image', ctypes.POINTER(Bitmap)), ('x', ctypes.c_int16), ('y', ctypes.c_int16),
                ('w', ctypes.c_int16), ('h', ctypes.c_int16), ('category', ctypes.c_uint8),
                ('index', ctypes.c_uint8), ('entry', ctypes.c_uint8), ('item', ctypes.c_int16),
                ('zone', ctypes.c_uint16), ('anchor', ctypes.c_int16), ('dx', ctypes.c_int16),
                ('dy', ctypes.c_int16), ('src_x', ctypes.c_int16), ('src_y', ctypes.c_int16),
                ('rect', ctypes.c_int16 * 4), ('dst', ctypes.POINTER(Bitmap)), ('transparent', ctypes.c_int16),
                ('mode', ctypes.c_int), ('flip', ctypes.c_int16), ('scale_x', ctypes.c_int16),
                ('scale_y', ctypes.c_int16), ('palette_count', ctypes.c_int16), ('palette', ctypes.c_uint8 * 256)]


def test_target_pixels_follow_transparency_mirroring_and_occlusion(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    pixels = (ctypes.c_uint8 * 6)(12, 15, 15, 12, 12, 15)
    image = Bitmap(pixels, 3, 2, C.IMAGE_8BPP)
    ctx = DrawCtx(image=ctypes.pointer(image), w=3, h=2, zone=0,
                  rect=(ctypes.c_int16 * 4)(10, 20, 3, 2), transparent=12,
                  dst=ctypes.pointer(Bitmap.in_dll(lib, 'g_view_viewport')), flip=C.MIRROR_H)
    lib.draw_ctx_draw(ctypes.byref(ctx))
    lib.view_record_attack_pixels(ctypes.byref(ctx), 123, Location(4, 5))
    at = Location()
    assert lib.view_attack_target(10, 20, ctypes.byref(at)) == 123
    assert (at.x, at.y) == (4, 5)
    assert lib.view_attack_target(12, 20, ctypes.byref(at)) == C.THING_NONE
    # An opaque foreground object clears ownership, while its transparent pixels keep the enemy below.
    pixels[:] = (12, 3, 12, 12, 12, 12)
    ctx.src_x = ctx.src_y = 0
    lib.draw_ctx_draw(ctypes.byref(ctx))
    lib.view_record_attack_pixels(ctypes.byref(ctx), C.THING_NONE, Location())
    assert lib.view_attack_target(11, 20, ctypes.byref(at)) == C.THING_NONE
    assert lib.view_attack_target(10, 20, ctypes.byref(at)) == 123


def test_loaded_ranged_attack_consumes_real_ammo_and_keeps_preference(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib, command=0x20)
    group = enemy(lib, at)
    lib.champ_remove_object_from_slot(0, 0)
    assert not lib.champ_can_party_attack(group, at)
    ammo = next(t for t in things_of(lib, C.THING_WEAPON, 10000)
                if lib.item_is_ammo_for_hand(0, 1, t))
    lib.champ_add_object_to_slot(0, ammo, 0)
    assert lib.champ_can_party_attack(group, at)
    assert lib.champ_party_attack(group, at)
    assert hand_item(lib, hand=0) == C.THING_NONE
    assert bytes(champion_records(lib))[0x2b] > 0
    champion_bytes(lib, 0, 0x2a, bytes(3))
    assert not lib.champ_can_party_attack(group, at)
    lib.champ_add_object_to_slot(0, ammo, 0)
    assert lib.champ_can_party_attack(group, at)


def test_hover_and_real_click_dispatch_one_volley(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib)
    group = enemy(lib, at)
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    lib.input_set_mode(C.INPUT_MODE_MOVE)
    viewport = (ctypes.c_int16 * 4)()
    assert lib.zone_get_rect(C.ZONE_VIEWPORT, viewport)
    hit = Location()
    points = [(x + viewport[0], y + viewport[1]) for y in range(viewport[3]) for x in range(viewport[2])
              if lib.view_attack_target(x, y, ctypes.byref(hit)) == group]
    assert points, 'the real creature sprite must register target pixels'
    x, y = points[len(points) // 2]
    assert lib.input_party_attack_target(x, y, ctypes.byref(hit)) == group
    assert lib.input_cursor_at(x, y) == C.CURSOR_ATTACK
    old_hand = game.leader_hand
    game.leader_hand = hand_item(lib)
    assert lib.input_party_attack_target(x, y, ctypes.byref(hit)) == C.THING_NONE
    game.leader_hand = old_hand
    ui = UiState.in_dll(lib, 'g_ui')
    ui.eye = True
    assert lib.input_party_attack_target(x, y, ctypes.byref(hit)) == C.THING_NONE
    ui.eye = False
    lib.input_set_mode(C.INPUT_MODE_PAUSE)
    assert lib.input_party_attack_target(x, y, ctypes.byref(hit)) == C.THING_NONE
    lib.input_set_mode(C.INPUT_MODE_MOVE)
    lib.input_mouse_click(x, y, C.INPUT_BUTTON_LEFT)
    from dm2test.input import run_ticks
    run_ticks(lib, 8)
    assert bytes(champion_records(lib))[0x2b] > 0
    assert lib.input_party_attack_target(x, y, ctypes.byref(hit)) == group
    assert lib.input_cursor_at(x, y) == C.CURSOR_ATTACK_WAITING
    # A waiting click is consumed; it neither spends resources nor picks up a floor object behind the creature.
    raw = bytes(champion_records(lib))
    seed = ctypes.c_uint32.in_dll(lib, 'g_random_seed').value
    from dm2test.scenario import Command
    click = Command()
    click.command, click.x, click.y = C.CMD_VIEWPORT, x, y
    lib.command_execute(ctypes.byref(click))
    assert bytes(champion_records(lib)) == raw
    assert ctypes.c_uint32.in_dll(lib, 'g_random_seed').value == seed
    assert game.leader_hand == old_hand


def test_volley_attacks_once_per_champion_and_preserves_other_menu(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    game.champion_count = 2
    champion_bytes(lib, 1, 0, bytes(champion_records(lib))[:C.CHAMPION_SIZE])
    champion_bytes(lib, 1, 0x1d, b'\x02')
    first, _ = choose_weapon(lib)
    choose_weapon(lib, champion=1, exclude=(first,))
    group = enemy(lib, at)
    lib.action_area_open_spells(1)
    area = ActionArea.in_dll(lib, 'g_action')
    panel = area.champion, area.hand, area.mode
    result = ctypes.c_int16.in_dll(lib, 'g_champ_action_result')
    result.value = 17
    assert lib.champ_party_attack(group, at)
    assert (area.champion, area.hand, area.mode) == panel
    assert result.value == 17
    raw = bytes(champion_records(lib))
    assert raw[0x2b] > 0 and raw[C.CHAMPION_SIZE + 0x2b] > 0
    assert not lib.champ_party_attack(group, at)
    assert bytes(champion_records(lib)) == raw


def test_remembers_exact_move_when_two_moves_share_melee_command(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    champion_bytes(lib, 0, 0x5f, struct.pack('<20I', *([1000000] * 20)))
    weapon = None
    for candidate in things_of(lib, C.THING_WEAPON, 10000):
        lib.champ_list_actions(0, candidate, 1)
        delays = {lib.gtext_gdat_field(*a, C.FIELD_DELAY) for a in actions(lib)
                  if lib.gtext_gdat_field(*a, C.FIELD_COMMAND) == 4}
        if len(delays) > 1:
            weapon = candidate
            break
    assert weapon is not None, 'a skilled champion must have a weapon with distinct melee moves'
    weapon, first = choose_weapon(lib, only=weapon)
    lib.champ_list_actions(0, weapon, 1)
    choices = [a for a in actions(lib) if lib.gtext_gdat_field(*a, C.FIELD_COMMAND) == 4]
    # Pick a distinct move of the same weapon; distinguish it through its normal action delay.
    other = next(a for a in choices if lib.gtext_gdat_field(*a, C.FIELD_DELAY) !=
                 lib.gtext_gdat_field(*first, C.FIELD_DELAY))
    lib.action_area_open_hand(0, 1)
    lib.champ_execute_selected_action(actions(lib).index(other))
    champion_bytes(lib, 0, 0x2a, bytes(3))
    lib.champ_add_action_delay(0, lib.gtext_gdat_field(*other, C.FIELD_DELAY), 1)
    expected = bytes(champion_records(lib))[0x2b]
    champion_bytes(lib, 0, 0x2a, bytes(3))
    group = enemy(lib, at)
    assert lib.champ_party_attack(group, at)
    assert bytes(champion_records(lib))[0x2b] == expected


def test_ranged_ray_respects_distance_walls_and_direction(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib, command=0x20)
    lib.champ_remove_object_from_slot(0, 0)
    ammo = next(t for t in things_of(lib, C.THING_WEAPON, 10000) if lib.item_is_ammo_for_hand(0, 1, t))
    lib.champ_add_object_to_slot(0, ammo, 0)
    group = enemy(lib, at)
    far = Location(at.x + 1, at.y)
    lib.dungeon_unlink_thing_from_square(group, 22, at.x, at.y)
    assert lib.dungeon_link_thing_to_square(group, 22, far.x, far.y)
    assert lib.champ_can_party_attack(group, far)
    square = ctypes.cast(lib.dungeon_square_byte(22, at.x, at.y), ctypes.POINTER(ctypes.c_uint8))
    old = square[0]
    square[0] = C.SQUARE_WALL << 5
    assert not lib.champ_can_party_attack(group, far)
    square[0] = old
    lib.dungeon_unlink_thing_from_square(group, 22, far.x, far.y)
    far.y += 1
    assert lib.dungeon_link_thing_to_square(group, 22, far.x, far.y)
    assert not lib.champ_can_party_attack(group, far)


def test_grate_eligibility_is_possible_passage_without_a_random_roll(lib, session):
    weapons = things_of(lib, C.THING_WEAPON, 10000)
    arrow = next(t for t in weapons if lib.item_attr(t, C.ATTR_FLAGS) & C.ITEM_FLAG_GRATE_PASSES)
    solid = next(t for t in weapons if not lib.item_attr(t, C.ATTR_FLAGS) & C.ITEM_FLAG_GRATE_PASSES)
    for m in range(lib.dungeon_map_count()):
        lib.dungeon_set_current_map(m)
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                at = Location(x, y)
                if lib.dungeon_square_at(at) >> 5 != C.SQUARE_DOOR:
                    continue
                door = lib.dungeon_first_thing_at(at)
                if not lib.sensor_door_value(door, 0x10):
                    continue
                square = ctypes.cast(lib.dungeon_square_byte(m, x, y), ctypes.POINTER(ctypes.c_uint8))
                square[0] = (square[0] & ~7) | C.DOOR_STATE_CLOSED
                seed = ctypes.c_uint32.in_dll(lib, 'g_random_seed').value
                assert lib.projectile_can_pass_square(at, arrow)
                assert not lib.projectile_can_pass_square(at, solid)
                assert ctypes.c_uint32.in_dll(lib, 'g_random_seed').value == seed
                return
    raise AssertionError('no grate in the dungeon')


def test_creature_body_holes_and_transient_effects_keep_target(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    pixels = (ctypes.c_uint8 * 10)(12, 15, 12, 15, 12, 12, 12, 15, 12, 12)
    image = Bitmap(pixels, 5, 2, C.IMAGE_8BPP)
    ctx = DrawCtx(image=ctypes.pointer(image), w=5, h=2,
                  rect=(ctypes.c_int16 * 4)(10, 20, 5, 2), transparent=12,
                  dst=ctypes.pointer(Bitmap.in_dll(lib, 'g_view_viewport')))
    lib.view_draw_attack_picture(ctypes.byref(ctx), 123, Location(4, 5), False)
    at = Location()
    assert lib.view_attack_target(12, 20, ctypes.byref(at)) == 123  # hole inside the row's body span
    assert lib.view_attack_target(10, 20, ctypes.byref(at)) == C.THING_NONE  # outside silhouette
    assert lib.view_attack_target(11, 21, ctypes.byref(at)) == C.THING_NONE  # no rectangular expansion
    pixels[:] = (15,) * 10
    ctx.src_x = ctx.src_y = 0
    lib.view_draw_attack_picture(ctypes.byref(ctx), C.THING_NONE, Location(), True)
    assert lib.view_attack_target(12, 20, ctypes.byref(at)) == 123  # projectile/cloud overlap
    ctx.src_x = ctx.src_y = 0
    lib.view_draw_attack_picture(ctypes.byref(ctx), C.THING_NONE, Location(), False)
    assert lib.view_attack_target(12, 20, ctypes.byref(at)) == C.THING_NONE  # real foreground object


def test_basic_attack_works_before_any_manual_attack_and_hover_is_pure(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    weapon, _ = choose_weapon(lib)
    lib.champ_reset_party_attacks()  # nothing is remembered, as immediately after a load
    lib.champ_list_actions(0, weapon, 1)
    basic = next(a for a in actions(lib) if lib.gtext_gdat_field(*a, C.FIELD_COMMAND) in (4, 8, 0x20))
    group = enemy(lib, at)
    raw = bytes(champion_records(lib)), bytes(ActionArea.in_dll(lib, 'g_action')), actions(lib)
    seed = ctypes.c_uint32.in_dll(lib, 'g_random_seed').value
    assert lib.champ_can_party_attack(group, at)
    assert raw == (bytes(champion_records(lib)), bytes(ActionArea.in_dll(lib, 'g_action')), actions(lib))
    assert ctypes.c_uint32.in_dll(lib, 'g_random_seed').value == seed
    assert lib.champ_party_attack(group, at)
    assert bytes(champion_records(lib))[0x21] == lib.gtext_gdat_field(*basic, C.FIELD_COMMAND)
    assert bytes(champion_records(lib))[0x2b] > 0


def test_party_melee_shows_damage_feedback_over_the_creature_and_expires(lib, session):
    game, loop = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib)
    group = enemy(lib, at)
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    lib.combat_reset_feedback()
    frame = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, 'g_video_fb')
    viewport = (ctypes.c_int16 * 4)()
    assert lib.zone_get_rect(C.ZONE_VIEWPORT, viewport)

    def view_pixels():
        return b''.join(bytes(frame[(viewport[1] + y) * 320 + viewport[0]:
                                    (viewport[1] + y) * 320 + viewport[0] + viewport[2]]) for y in range(viewport[3]))

    lib.play_present_viewport()
    baseline = view_pixels()
    assert lib.champ_party_attack(group, at)
    lib.play_present_viewport()
    assert view_pixels() != baseline
    loop.timer_ticks += 100
    lib.play_present_viewport()
    assert view_pixels() == baseline


def test_container_does_not_inherit_stale_weapon_actions_as_a_fallback(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    weapon, _ = choose_weapon(lib)
    group = enemy(lib, at)
    boxes = things_of(lib, C.THING_CONTAINER, 2, lib.item_is_chest)
    assert boxes
    for hand in (0, 1):
        lib.champ_remove_object_from_slot(0, hand)
        lib.champ_add_object_to_slot(0, boxes[0], hand)
    lib.champ_reset_party_attacks()
    lib.champ_list_actions(0, weapon, 1)  # leave a real weapon menu as shared scratch state
    assert not lib.champ_can_party_attack(group, at)
    assert not lib.champ_party_attack(group, at)


@pytest.mark.parametrize('zero_damage', [False, True])
def test_projectile_feedback_waits_for_a_resolved_impact(lib, session, zero_damage):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib, command=0x20)
    group = enemy(lib, at)
    ammo = next(t for t in things_of(lib, C.THING_WEAPON, 10000) if lib.item_is_ammo_for_hand(0, 1, t))
    lib.view_draw_dungeon(Location(game.party_x, game.party_y), game.party_dir)
    lib.combat_reset_feedback()
    frame = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, 'g_video_fb')
    lib.play_present_viewport()
    baseline = bytes(frame)
    timeline = Timeline.in_dll(lib, 'g_timeline')
    before = [bytes(timeline.events[i]) for i in range(timeline.capacity)]
    class GroupNear(ctypes.Structure):
        _fields_ = [('group', ctypes.c_uint16), ('at', Location)]

    # This result struct is local to the test; find an actual body contact using the normal collision query.
    lib.creature_find_group_near_point.restype = GroupNear
    lib.creature_find_group_near_point.argtypes = [Location, ctypes.c_int16]
    points = [(x, y, cell) for x in (at.x, at.x - 1, at.x + 1) for y in (at.y, at.y - 1, at.y + 1)
              for cell in range(4) if (x, y) != (game.party_x, game.party_y) and
              lib.creature_find_group_near_point(Location(x, y), cell).group == group]
    assert points, 'the fixture creature needs a projectile collision point'
    x, y, cell = points[0]
    # Exhausted projectiles have zero attack after the normal energy cap; no creature data is modified.
    launch = ProjectileLaunch(ammo, x, y, cell, 1, 0 if zero_damage else 200, 0 if zero_damage else 255, 1, True)
    lib.projectile_create(ctypes.byref(launch))
    lib.play_present_viewport()
    assert bytes(frame) == baseline  # launching does not claim a hit
    index = next(i for i in range(timeline.capacity)
                 if timeline.events[i].type == C.EVENT_PROJECTILE_MOVE_2 and bytes(timeline.events[i]) != before[i])
    event = TimelineEvent.from_buffer_copy(timeline.events[index])
    lib.timeline_delete_event(index)  # the event runner removes an event before invoking its handler
    lib.projectile_process_event(ctypes.byref(event))
    lib.play_present_viewport()
    assert bytes(frame) != baseline


def test_basic_fallback_can_use_a_ready_offhand_when_action_hand_is_recovering(lib, session):
    game, _ = session
    set_mode(lib, ADVANCED)
    at = arena(lib, game)
    choose_weapon(lib)
    lib.champ_remove_object_from_slot(0, 0)  # an empty ready hand has the champion's basic punch
    lib.champ_reset_party_attacks()
    champion_bytes(lib, 0, 0x2a, bytes([0, 10, 0]))
    group = enemy(lib, at)
    assert lib.champ_can_party_attack(group, at)
    assert lib.champ_party_attack(group, at)
    raw = bytes(champion_records(lib))
    assert raw[0x2a] > 0  # the ready hand attacked
    assert raw[0x2b] == 10  # the recovering action hand was not forced
