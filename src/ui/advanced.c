/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The advanced mode: the movement letter keys, the quick save and quick load, the automap, and the cheat screen. */
#include "advanced.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "automap.h"
#include "cheats.h"
#include "dialog.h"
#include "draw.h"
#include "dungeon.h"
#include "game.h"
#include "game_text.h"
#include "ids.h"
#include "interface_state.h"
#include "magic.h"
#include "minion.h"
#include "music.h"
#include "options.h"
#include "os.h"
#include "recovery.h"
#include "save.h"
#include "session.h"
#include "sound.h"
#include "spellbook.h"
#include "text.h"
#include "timeline.h"
#include "ui.h"
#include "video.h"
#include "zone.h"

/* The name in the header of QUICK.DAT. */
#define QUICK_SAVE_NAME "QUICK SAVE"

/* The layout of the readout: its margin, then its rows. */
#define READOUT_MARGIN    2 /* pixels between the viewport edge and the readout */
#define READOUT_POSITION  0
#define READOUT_DIRECTION 1

enum AdvancedScreen g_advanced_screen;

/* Options may cover an inventory, sleep or pause picture that will not redraw on its own. */
static uint8_t options_picture[SCREEN_H][SCREEN_W];

/* The state of the viewport screens and of the quick load. */
static struct {
    struct {
        bool active;
        int x, y; /* the mouse position that matches the current scroll */
    } drag;
    bool lost;            /* a quick load lost the game: there is no running game to keep */
    uint16_t pending_key; /* the key that the closed screen leaves for the game (F9 of QUICK LOAD), 0 for none */
} advanced;

static const char *const direction_names[DIR_COUNT] = {"NORTH", "EAST", "SOUTH", "WEST"};

/* Closes the open viewport screen and ends a drag of the map. */
static void close_screen(void)
{
    if (g_advanced_screen == ADVANCED_SCREEN_OPTIONS)
        memcpy(g_video_fb, options_picture, sizeof options_picture);
    g_advanced_screen = ADVANCED_SCREEN_NONE;
    advanced.drag.active = false;
    automap_scroll_reset();
}

/* Setup */

void advanced_reset(void)
{
    close_screen();
    advanced.lost = false;
    advanced.pending_key = 0;
}

/* Movement keys */

uint16_t advanced_movement_key(uint16_t key)
{
    if (g_game_mode != GAME_MODE_ADVANCED)
        return 0;
    switch (key) {
    case SCAN_W:
        return KEYPAD_FORWARD;
    case SCAN_S:
        return KEYPAD_BACK;
    case SCAN_A:
        return KEYPAD_LEFT;
    case SCAN_D:
        return KEYPAD_RIGHT;
    case SCAN_Q:
        return KEYPAD_TURN_LEFT;
    case SCAN_E:
        return KEYPAD_TURN_RIGHT;
    }
    return 0;
}

/* Quick save and load */

/* <save folder>/QUICK.<extension>: the --saves folder, else the DATA folder of the game. */
static char *quick_path(char *path, const char *extension)
{
    if (g_session.save_dir[0])
        snprintf(path, OS_PATH_SIZE, "%s/QUICK.%s", g_session.save_dir, extension);
    else
        snprintf(path, OS_PATH_SIZE, "%s/DATA/QUICK.%s", g_session.data_dir, extension);
    return path;
}

/* A white line in the message area. */
static void print_line(const char *text)
{
    char line[TEXT_SIZE];
    snprintf(line, sizeof line, "\n%s", text);
    text_print_message(UI_WHITE, line);
}

/* Writes the game to path (session_save prepares the timeline the same way); returns false on an error. */
static bool write_game(const char *path, const char *bak)
{
    timeline_rebuild_free_list();
    return save_write(path, bak, QUICK_SAVE_NAME, gdat_current()) == SAVE_OK;
}

bool advanced_quick_save(void)
{
    char path[OS_PATH_SIZE], bak[OS_PATH_SIZE];
    bool ok = write_game(quick_path(path, "DAT"), quick_path(bak, "BAK"));
    if (ok)
        remove(bak);
    else
        rename(bak, path); /* the old quick save stays */
    print_line(ok ? "GAME SAVED" : "UNABLE TO SAVE");
    return ok;
}

enum QuickLoadResult advanced_quick_load(void)
{
    char path[OS_PATH_SIZE], keep[OS_PATH_SIZE];
    FILE *f = fopen(quick_path(path, "DAT"), "rb");
    if (!f) {
        print_line("NO QUICK SAVE");
        return QUICK_MISSING;
    }
    fclose(f);
    /* the running game goes to QUICK.TMP first: a damaged quick save loads it back (the same path: no backup) */
    quick_path(keep, "TMP");
    if (!advanced.lost && !write_game(keep, keep)) {
        print_line("UNABLE TO SAVE");
        return QUICK_BLOCKED;
    }
    Session kept = g_session;
    /* play_start_session shows no LOADED dialog and starts no new game */
    game_set_session_loaded(false);
    game_set_new_game(0);
    bool loaded = session_start_save(path);
    bool restored = loaded || (!advanced.lost && session_start_save(keep));
    if (restored)
        remove(keep); /* else QUICK.TMP keeps the running game */
    game_restore_session(&kept);
    if (!restored) {
        /* the dungeon is empty: the game cannot go on */
        os_log("cannot load %s or the running game in %s", path, keep);
        if (g_game_mode != GAME_MODE_TURNBASED) {
            session_quit();
            game_set_exit_code(EXIT_GAME_LOST);
        }
        advanced.lost = true;
        return QUICK_LOST;
    }
    advanced.lost = false;
    if (loaded) {
        /* a quick load is not a saved game for the QUIT question */
        game_clear_save_times();
        sound_set_volume(g_game.sound_volume);
        music_set_volume(g_game.music_volume);
    }
    print_line(loaded ? "GAME LOADED" : "QUICK SAVE DAMAGED");
    return loaded ? QUICK_LOADED : QUICK_DAMAGED;
}

/* The automap */

void advanced_open_automap(void)
{
    g_advanced_screen = ADVANCED_SCREEN_AUTOMAP;
    advanced_draw_automap();
}

/* Line row of the readout at the top left of the viewport, in white on black. */
static void render_readout_line(Offscreen *o, int row, const char *text)
{
    int baseline = o->rect[RECT_Y] + READOUT_MARGIN + row * TEXT_LINE_HEIGHT + FONT_GLYPH_HEIGHT - FONT_BASELINE - 1;
    offscreen_draw_text_at(o, o->rect[RECT_X] + READOUT_MARGIN, baseline,
                           (TextColors){g_ui_colors[UI_WHITE], g_ui_colors[UI_BLACK]}, text);
}

void advanced_draw_automap(void)
{
    Offscreen o;
    char line[TEXT_SIZE];
    if (!zone_get_viewport_rect(o.rect))
        return;
    offscreen_create(&o, ZONE_NONE, true);
    if (!o.bitmap)
        return;
    automap_draw(&o, o.rect);
    snprintf(line, sizeof line, "LEVEL %d  X %d  Y %d", dungeon_map_level(g_game.party_map), g_game.party_x,
             g_game.party_y);
    render_readout_line(&o, READOUT_POSITION, line);
    render_readout_line(&o, READOUT_DIRECTION, direction_names[g_game.party_dir & DIR_MASK]);
    offscreen_flush(&o, true);
    draw_free_temp();
}

void advanced_automap_drag(int16_t x, int16_t y)
{
    if (!advanced.drag.active)
        return;
    int dx = (x - advanced.drag.x) / MAP_CELL_SIZE;
    int dy = (y - advanced.drag.y) / MAP_CELL_SIZE;
    if (!dx && !dy)
        return;
    advanced.drag.x += dx * MAP_CELL_SIZE;
    advanced.drag.y += dy * MAP_CELL_SIZE;
    automap_scroll(-dx, -dy);
    advanced_draw_automap();
}

/* Viewport screens */

/* An input event on the open automap. */
static void map_event(int type, int16_t x, int16_t y)
{
    int16_t r[RECT_SIZE];
    if (type == INPUT_KEY) {
        if (x == KEY_AUTOMAP || x == KEY_AUTOMAP_M || x == SCAN_ESCAPE)
            close_screen();
    } else if (type == INPUT_LEFT_RELEASED) {
        advanced_automap_drag(x, y);
        advanced.drag.active = false;
    } else if (type & INPUT_BUTTON_LEFT && zone_get_viewport_rect(r) && x >= r[RECT_X] && x < r[RECT_X] + r[RECT_W] &&
               y >= r[RECT_Y] && y < r[RECT_Y] + r[RECT_H]) {
        advanced.drag.active = true;
        advanced.drag.x = x;
        advanced.drag.y = y;
    }
}

void advanced_open_cheats(void)
{
    g_advanced_screen = ADVANCED_SCREEN_CHEATS;
    cheats_open();
}

/* What a key on the open cheat screen asks for; the arrows and W and S move the selection. */
static enum CheatsResult process_cheat_key(int16_t key)
{
    switch (key) {
    case KEY_CHEATS:
    case SCAN_ESCAPE:
        return CHEATS_CLOSE;
    case KEY_QUICK_SAVE:
        return CHEATS_QUICK_SAVE;
    case KEY_QUICK_LOAD:
        return CHEATS_QUICK_LOAD;
    case KEY_ENTER:
        return cheats_run_selected_row();
    case KEYPAD_FORWARD:
    case SCAN_W:
        cheats_move_selection(-1);
        break;
    case KEYPAD_BACK:
    case SCAN_S:
        cheats_move_selection(1);
        break;
    }
    return CHEATS_STAY;
}

/* An input event on the open cheat screen: a key or a click, then what its row asks for. */
static void cheat_event(int type, int16_t x, int16_t y)
{
    enum CheatsResult result = CHEATS_STAY;
    if (type == INPUT_BUTTON_RIGHT)
        result = cheats_process_right_click();
    else if (type == INPUT_KEY)
        result = process_cheat_key(x);
    else if (type != INPUT_LEFT_RELEASED && type & INPUT_BUTTON_LEFT)
        result = cheats_process_click(x, y);
    if (result == CHEATS_QUICK_SAVE) {
        advanced_quick_save();
    } else if (result == CHEATS_QUICK_LOAD) {
        close_screen();
        advanced.pending_key = KEY_QUICK_LOAD; /* the load runs as a game command, not inside the input poll */
    } else if (result == CHEATS_CLOSE) {
        close_screen();
    }
}

void advanced_open_spellbook(void)
{
    g_advanced_screen = ADVANCED_SCREEN_SPELLBOOK;
    spellbook_open();
}

void advanced_open_options(void)
{
    memcpy(options_picture, g_video_fb, sizeof options_picture);
    g_advanced_screen = ADVANCED_SCREEN_OPTIONS;
    options_open();
}

void advanced_screen_event(int type, int16_t x, int16_t y)
{
    if (g_advanced_screen == ADVANCED_SCREEN_OPTIONS) {
        OptionsResult result = options_event(type, x, y);
        if (result == OPTIONS_CLOSE)
            close_screen();
        else if (result == OPTIONS_CHEATS) {
            close_screen();
            advanced_open_cheats();
        } else if (result == OPTIONS_SPELLBOOK) {
            close_screen();
            advanced_open_spellbook();
        } else if ((result == OPTIONS_QUICK_SAVE || result == OPTIONS_QUICK_LOAD) && g_game.champion_count) {
            close_screen();
            advanced.pending_key = result == OPTIONS_QUICK_SAVE ? KEY_QUICK_SAVE : KEY_QUICK_LOAD;
        }
        return;
    }
    if (g_advanced_screen == ADVANCED_SCREEN_SPELLBOOK) {
        SpellbookResult result = spellbook_event(type, x, y);
        if (result != SPELLBOOK_STAY)
            close_screen();
        if (result == SPELLBOOK_CAST)
            game_set_command_stop(magic_cast_entered_runes_failed());
        return;
    }
    if (g_advanced_screen == ADVANCED_SCREEN_CHEATS)
        cheat_event(type, x, y);
    else if (type == INPUT_BUTTON_RIGHT)
        close_screen();
    else if (g_advanced_screen == ADVANCED_SCREEN_AUTOMAP)
        map_event(type, x, y);
}

uint16_t advanced_take_pending_key(void)
{
    uint16_t key = advanced.pending_key;
    advanced.pending_key = 0;
    return key;
}

/* Focus loss is independent of the original pause, sleep, and modal input modes. */
static bool window_focused = true;
static bool focus_paused;
/* Repeat keys held across a focus change must be pressed again after resuming. */
#define FOCUS_KEY_COUNT 256
static bool repeats_blocked;
static bool repeat_armed[FOCUS_KEY_COUNT];

bool advanced_focus_paused(void)
{
    return g_game_mode == GAME_MODE_ADVANCED && focus_paused;
}

void advanced_focus_changed(bool focused)
{
    window_focused = focused;
    if (g_game_mode != GAME_MODE_ADVANCED || focused || !g_game.champion_count || g_main_loop.end || recovery_is_open())
        return;
    focus_paused = true;
    repeats_blocked = true;
    memset(repeat_armed, 0, sizeof repeat_armed);
    advanced.drag.active = false;
    input_cancel_pending();
}

bool advanced_focus_key(uint16_t key, bool repeat)
{
    if (!advanced_focus_paused()) {
        if (g_game_mode == GAME_MODE_ADVANCED && repeats_blocked) {
            if (repeat && !repeat_armed[key & 0xff])
                return true;
            if (!repeat)
                repeat_armed[key & 0xff] = true;
        }
        return false;
    }
    if (window_focused && key == KEY_ENTER && !repeat) {
        input_cancel_pending();
        focus_paused = false;
    }
    return true;
}

void advanced_draw_focus_pause(void)
{
    if (!advanced_focus_paused())
        return;
    text_draw_on_screen(0, TEXT_LINE_HEIGHT, g_ui_colors[UI_WHITE], g_ui_colors[UI_BLACK], "PAUSED - RETURN TO RESUME");
}

/* The small, always visible shortcut occupies the bottom right message line during stable gameplay. */
#define SHORTCUT_TEXT  "F1 OPTIONS/HELP"
#define SHORTCUT_WIDTH ((int)(sizeof SHORTCUT_TEXT - 1) * FONT_ADVANCE)
#define SHORTCUT_LEFT  (SCREEN_W - SHORTCUT_WIDTH)
#define SHORTCUT_TOP   (SCREEN_H - TEXT_LINE_HEIGHT)

/* Whether a convenience overlay may be opened without interrupting a modal operation. */
static bool shortcut_available(void)
{
    enum InputMode mode = input_get_mode();
    return g_game_mode == GAME_MODE_ADVANCED && !g_advanced_screen && !g_ui.candidate && !g_dialog.screen_mode &&
           !g_main_loop.end && !recovery_is_open() &&
           (mode == INPUT_MODE_MOVE || mode == INPUT_MODE_INVENTORY || mode == INPUT_MODE_PAUSE ||
            mode == INPUT_MODE_SLEEP);
}

void advanced_draw_shortcut(void)
{
    if (shortcut_available())
        text_draw_on_screen(SHORTCUT_LEFT, SCREEN_H - 1, g_ui_colors[UI_WHITE], g_ui_colors[UI_BLACK], SHORTCUT_TEXT);
}

bool advanced_shortcut_click(int16_t x, int16_t y, int button)
{
    if (button != INPUT_BUTTON_LEFT || !shortcut_available() || x < SHORTCUT_LEFT || x >= SCREEN_W ||
        y < SHORTCUT_TOP || y >= SCREEN_H)
        return false;
    input_key_down(KEY_OPTIONS);
    return true;
}
