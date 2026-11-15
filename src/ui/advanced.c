/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The advanced mode: the movement letter keys, the quick save and quick load, the automap, and the cheat screen. */
#include "advanced.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "action_area.h"
#include "automap.h"
#include "cheats.h"
#include "draw.h"
#include "dungeon.h"
#include "game.h"
#include "game_text.h"
#include "ids.h"
#include "interface_state.h"
#include "magic.h"
#include "music.h"
#include "options.h"
#include "os.h"
#include "recovery.h"
#include "save.h"
#include "seen.h"
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
#define READOUT_NOTICE    2

/* A click tolerates small mouse jitter; a larger movement is a drag even below one whole map square. */
#define MAP_CLICK_SLOP 3

/* The readout of a note of the player before its text. */
#define NOTE_PREFIX "NOTE: "

enum AdvancedScreen g_advanced_screen;

/* Options may cover an inventory, sleep or pause picture that will not redraw on its own. */
static uint8_t options_picture[SCREEN_H][SCREEN_W];

/* The state of the viewport screens and of the quick load. */
static struct {
    struct {
        bool active;
        bool moved;
        int x, y;             /* the mouse position that matches the current scroll */
        int start_x, start_y; /* the press position, before any scrolling */
    } drag;
    struct {
        bool valid;     /* a click on the map waits for a second click */
        uint32_t ticks; /* the timer tick of the click */
        int16_t x, y;   /* its screen place */
    } click;
    struct {
        bool editing;              /* the player types the note of square */
        bool shown;                /* the readout shows the note of square */
        Position square;           /* the map square of the note */
        char text[SEEN_NOTE_SIZE]; /* the typed text */
    } note;
    const char *map_notice; /* the readout line after a level key that found no level, NULL for none */
    bool lost;              /* a quick load lost the game: there is no running game to keep */
    uint16_t pending_key;   /* the key that the closed screen leaves for the game (F9 of QUICK LOAD), 0 for none */
} advanced;

static const char *const direction_names[DIR_COUNT] = {"NORTH", "EAST", "SOUTH", "WEST"};

/* Closes the open viewport screen and ends a drag of the map. */
static void close_screen(void)
{
    if (g_advanced_screen == ADVANCED_SCREEN_OPTIONS)
        memcpy(g_video_fb, options_picture, sizeof options_picture);
    g_advanced_screen = ADVANCED_SCREEN_NONE;
    advanced.drag.active = false;
    advanced.map_notice = NULL;
    advanced.click.valid = false;
    advanced.note.editing = false;
    advanced.note.shown = false;
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

/* <save folder>/QUICK.<extension>. */
static char *quick_path(char *path, const char *extension)
{
    char name[OS_FILE_PART_SIZE];
    snprintf(name, sizeof name, "QUICK.%s", extension);
    return session_make_save_path(path, name);
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

/* Loads the save at path in place of the running game, which waits in QUICK.TMP and comes back when the load fails.
   The session (the slot, the QUIT question) stays as before the load. */
static enum QuickLoadResult load_in_place(const char *path)
{
    char keep[OS_PATH_SIZE];
    /* the running game goes to QUICK.TMP first: a damaged save loads it back (the same path: no backup) */
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
        sound_set_volume(g_game.sound_volume);
        music_set_volume(g_game.music_volume);
    }
    return loaded ? QUICK_LOADED : QUICK_DAMAGED;
}

enum QuickLoadResult advanced_quick_load(void)
{
    char path[OS_PATH_SIZE];
    FILE *f = fopen(quick_path(path, "DAT"), "rb");
    if (!f) {
        print_line("NO QUICK SAVE");
        return QUICK_MISSING;
    }
    fclose(f);
    enum QuickLoadResult result = load_in_place(path);
    if (result == QUICK_LOADED)
        game_clear_save_times(); /* a quick load is not a saved game for the QUIT question */
    if (result == QUICK_LOADED || result == QUICK_DAMAGED)
        print_line(result == QUICK_LOADED ? "GAME LOADED" : "QUICK SAVE DAMAGED");
    return result;
}

enum QuickLoadResult advanced_load_slot(int16_t slot, const char *path)
{
    enum QuickLoadResult result = load_in_place(path);
    if (result == QUICK_LOADED) {
        game_set_save_slot(slot);
        game_note_slot_loaded();
    }
    if (result == QUICK_LOADED || result == QUICK_DAMAGED)
        print_line(result == QUICK_LOADED ? "GAME LOADED" : "SAVE DAMAGED");
    return result;
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

/* The note row of the readout: the typed note with a yellow cursor after it, or the note that a click shows. */
static void render_note(Offscreen *o)
{
    const char *text = advanced.note.editing ? advanced.note.text
                       : advanced.note.shown ? seen_note(advanced.note.square)
                                             : NULL;
    if (!text)
        return;
    char line[TEXT_SIZE];
    snprintf(line, sizeof line, NOTE_PREFIX "%s", text);
    render_readout_line(o, READOUT_NOTICE, line);
    if (!advanced.note.editing)
        return;
    int16_t cursor[RECT_SIZE] = {(int16_t)(o->rect[RECT_X] + READOUT_MARGIN + (int)strlen(line) * FONT_ADVANCE),
                                 (int16_t)(o->rect[RECT_Y] + READOUT_MARGIN + READOUT_NOTICE * TEXT_LINE_HEIGHT),
                                 FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT};
    offscreen_fill_rect(o, cursor, g_ui_colors[UI_YELLOW]);
}

void advanced_draw_automap(void)
{
    if (automap_items_open()) {
        automap_draw_items();
        draw_free_temp();
        return;
    }
    Offscreen o;
    char line[TEXT_SIZE];
    if (!zone_get_viewport_rect(o.rect))
        return;
    offscreen_create(&o, ZONE_NONE, true);
    if (!o.bitmap)
        return;
    automap_draw(&o, o.rect);
    int party_level = dungeon_map_level(g_game.party_map);
    if (automap_level() == party_level)
        snprintf(line, sizeof line, "LEVEL %d  X %d  Y %d", party_level, g_game.party_x, g_game.party_y);
    else
        snprintf(line, sizeof line, "LEVEL %d  PARTY ON %d", automap_level(), party_level);
    render_readout_line(&o, READOUT_POSITION, line);
    render_readout_line(&o, READOUT_DIRECTION, direction_names[g_game.party_dir & DIR_MASK]);
    if (advanced.map_notice)
        render_readout_line(&o, READOUT_NOTICE, advanced.map_notice);
    else
        render_note(&o);
    offscreen_flush(&o, true);
    draw_free_temp();
}

void advanced_automap_drag(int16_t x, int16_t y)
{
    if (!advanced.drag.active)
        return;
    if (x - advanced.drag.start_x > MAP_CLICK_SLOP || advanced.drag.start_x - x > MAP_CLICK_SLOP ||
        y - advanced.drag.start_y > MAP_CLICK_SLOP || advanced.drag.start_y - y > MAP_CLICK_SLOP)
        advanced.drag.moved = true;
    int dx = (x - advanced.drag.x) / automap_cell_size();
    int dy = (y - advanced.drag.y) / automap_cell_size();
    if (!dx && !dy)
        return;
    advanced.drag.x += dx * automap_cell_size();
    advanced.drag.y += dy * automap_cell_size();
    automap_scroll(-dx, -dy);
    advanced_draw_automap();
}

void advanced_automap_wheel(int16_t x, int16_t y, int steps)
{
    if (g_advanced_screen != ADVANCED_SCREEN_AUTOMAP || !automap_zoom(x, y, steps))
        return;
    advanced.drag.active = false;
    advanced_draw_automap();
}

/* Viewport screens */

/* Pans, changes the level, or centers the open automap for key; false for any other key. The arrows and W, A, S, D
   pan one square, Q and E show the visited level above and below (or a notice that there is none), Space shows the
   party. */
static bool map_key(int16_t key)
{
    switch (key) {
    case KEYPAD_FORWARD:
    case SCAN_W:
        automap_scroll(0, -1);
        return true;
    case KEYPAD_BACK:
    case SCAN_S:
        automap_scroll(0, 1);
        return true;
    case KEYPAD_TURN_LEFT:
    case KEYPAD_LEFT:
    case SCAN_A:
        automap_scroll(-1, 0);
        return true;
    case KEYPAD_TURN_RIGHT:
    case KEYPAD_RIGHT:
    case SCAN_D:
        automap_scroll(1, 0);
        return true;
    case SCAN_Q:
        if (!automap_browse_level(-1))
            advanced.map_notice = "NO VISITED LEVEL ABOVE";
        return true;
    case SCAN_E:
        if (!automap_browse_level(1))
            advanced.map_notice = "NO VISITED LEVEL BELOW";
        return true;
    case SCAN_SPACE:
        automap_center_party();
        return true;
    }
    return false;
}

/* A key or a click while the player types a note: Return keeps the note (an empty text removes it), Esc and a right
   click cancel, Backspace deletes the last character, and the key of a note character adds it. */
static void note_event(int type, int16_t key)
{
    if (type == INPUT_BUTTON_RIGHT || (type == INPUT_KEY && key == SCAN_ESCAPE)) {
        advanced.note.editing = false;
        return;
    }
    if (type != INPUT_KEY)
        return;
    size_t length = strlen(advanced.note.text);
    if (key == KEY_ENTER) {
        advanced.note.editing = false;
        if (!seen_set_note(advanced.note.square, advanced.note.text))
            advanced.map_notice = "NO ROOM FOR MORE NOTES";
        advanced.note.shown = seen_note(advanced.note.square) != NULL;
    } else if (key == KEY_BACKSPACE) {
        if (length > 0)
            advanced.note.text[length - 1] = '\0';
    } else if (!(key & (KEY_CONTROL | KEY_ALT)) && length < SEEN_NOTE_MAX) {
        char c = kbd_key_to_ascii((uint16_t)key);
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (seen_is_note_char(c)) {
            advanced.note.text[length] = c;
            advanced.note.text[length + 1] = '\0';
        }
    }
}

/* True when a left press at (x, y) is the second click of a double click on the map. */
static bool is_double_click(int16_t x, int16_t y)
{
    return advanced.click.valid && g_main_loop.timer_ticks - advanced.click.ticks <= DOUBLE_CLICK_TICKS &&
           abs(x - advanced.click.x) <= MAP_CLICK_SLOP && abs(y - advanced.click.y) <= MAP_CLICK_SLOP;
}

/* Starts the note of the map square at the screen place (x, y), with its old text, and closes the item list that the
   first click of the double click opened; false where the automap shows no square that can hold a note. */
static bool start_note(int16_t x, int16_t y)
{
    Location level_at;
    Position square;
    if (!automap_square_at(x, y, &level_at) || !automap_note_square(level_at, &square))
        return false;
    if (automap_items_open())
        automap_items_event(INPUT_BUTTON_RIGHT, x, y);
    const char *text = seen_note(square);
    snprintf(advanced.note.text, sizeof advanced.note.text, "%s", text ? text : "");
    advanced.note.square = square;
    advanced.note.editing = true;
    advanced.note.shown = false;
    advanced.map_notice = NULL;
    return true;
}

/* A short left click at (x, y) of the map: waits for a second click, shows the note of the square in the readout,
   and lists the square's items. */
static void click_map(int16_t x, int16_t y)
{
    advanced.click.valid = true;
    advanced.click.ticks = g_main_loop.timer_ticks;
    advanced.click.x = x;
    advanced.click.y = y;
    Location level_at;
    Position square;
    advanced.note.shown =
        automap_square_at(x, y, &level_at) && automap_note_square(level_at, &square) && seen_note(square);
    if (advanced.note.shown)
        advanced.note.square = square;
    automap_select(x, y);
    advanced_draw_automap();
}

/* An input event on the open automap. */
static void map_event(int type, int16_t x, int16_t y)
{
    int16_t r[RECT_SIZE];
    if (advanced.note.editing) {
        note_event(type, x);
        advanced_draw_automap();
        return;
    }
    if (type == INPUT_KEY && (x == KEY_AUTOMAP || x == KEY_AUTOMAP_M)) {
        close_screen();
        return;
    }
    if (type == INPUT_BUTTON_LEFT && is_double_click(x, y) && start_note(advanced.click.x, advanced.click.y)) {
        advanced.click.valid = false;
        advanced.drag.active = false;
        advanced_draw_automap();
        return;
    }
    if (automap_items_event(type, x, y)) {
        advanced_draw_automap();
        return;
    }
    if (type == INPUT_KEY) {
        if (x == SCAN_ESCAPE)
            close_screen();
        else {
            const char *notice = advanced.map_notice;
            advanced.map_notice = NULL;
            if (map_key(x) || notice)
                advanced_draw_automap();
        }
    } else if (type == INPUT_BUTTON_RIGHT) {
        close_screen();
    } else if (type == INPUT_LEFT_RELEASED) {
        advanced_automap_drag(x, y);
        bool click = advanced.drag.active && !advanced.drag.moved;
        advanced.drag.active = false;
        if (click)
            click_map(x, y);
    } else if (type & INPUT_BUTTON_LEFT && zone_get_viewport_rect(r) && x >= r[RECT_X] && x < r[RECT_X] + r[RECT_W] &&
               y >= r[RECT_Y] && y < r[RECT_Y] + r[RECT_H]) {
        advanced.drag.active = true;
        advanced.drag.moved = false;
        advanced.drag.x = x;
        advanced.drag.y = y;
        advanced.drag.start_x = x;
        advanced.drag.start_y = y;
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
    g_advanced_screen = ADVANCED_SCREEN_OPTIONS;
    area_update_rune_hover(); /* the saved picture must contain rune symbols, not a stale hover label */
    memcpy(options_picture, g_video_fb, sizeof options_picture);
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
    else if (g_advanced_screen == ADVANCED_SCREEN_AUTOMAP)
        map_event(type, x, y);
    else if (type == INPUT_BUTTON_RIGHT)
        close_screen();
}

uint16_t advanced_take_pending_key(void)
{
    uint16_t key = advanced.pending_key;
    advanced.pending_key = 0;
    return key;
}

/* Clear held input across focus changes without pausing the game. */
/* Repeat keys held across a focus change must be pressed again after resuming. */
#define FOCUS_KEY_COUNT 256
static bool repeats_blocked;
static bool repeat_armed[FOCUS_KEY_COUNT];

void advanced_focus_changed(bool focused)
{
    if (g_game_mode != GAME_MODE_ADVANCED || focused || !g_game.champion_count || g_main_loop.end || recovery_is_open())
        return;
    repeats_blocked = true;
    memset(repeat_armed, 0, sizeof repeat_armed);
    advanced.drag.active = false;
    int16_t choice = g_input_match.choice;
    input_cancel_pending();
    g_input_match.choice = choice; /* an open dialog keeps waiting: a choice of 0 is its first button */
}

bool advanced_focus_key(uint16_t key, bool repeat)
{
    if (g_game_mode == GAME_MODE_ADVANCED && repeats_blocked) {
        if (repeat && !repeat_armed[key & 0xff])
            return true;
        if (!repeat)
            repeat_armed[key & 0xff] = true;
    }
    return false;
}
