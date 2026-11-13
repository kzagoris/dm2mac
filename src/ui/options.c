/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "options.h"
#include <ctype.h>
#include <stdio.h>
#include "advanced.h"
#include "filter.h"
#include "rows.h"

/* Option rows; the title item closes the overlay. */
#define OPTIONS_ROW_HELP      0
#define OPTIONS_ROW_BOOK      1
#define OPTIONS_ROW_FILTER    2
#define OPTIONS_ROW_SAVE      3
#define OPTIONS_ROW_LOAD      4
#define OPTIONS_ROW_CHEATS    5
#define OPTIONS_ROW_ROWS      6
#define OPTIONS_ROW_TEXT_SIZE 40
#define HELP_ROWS             11

static bool help;
static int16_t selected;
static bool blocked;

/* Draws all essential controls without requiring external documentation. */
static void show_options(void)
{
    static const char *const help_lines[HELP_ROWS] = {
        "ADVANCED ADDS CONVENIENCE",       "CHEATS ARE SEPARATE, DEFAULT OFF",  "WASD MOVE; Q/E TURN; ARROWS TOO",
        "1-4 INVENTORY; TAB CLOSES IT",    "HOLD EYE TO INSPECT HELD ITEM",     "CLICK CHAMPION ICON FOR RUNES",
        "CHOOSE POWER, RUNES, THEN CAST",  "B FAVORITES: RECORD ENTERED RUNES", "F5 QUICK SAVE; F9 QUICK LOAD",
        "DISK ICON: SLOT SAVE; M/TAB MAP", "ESC PAUSE; F1 OPTIONS/HELP"};
    char filter[OPTIONS_ROW_TEXT_SIZE];
    snprintf(filter, sizeof filter, "SCREEN FILTER: %s", settings_filter_name(upscale_get_filter()));
    for (char *ch = filter; *ch; ch++)
        *ch = (char)toupper((unsigned char)*ch);
    const char *lines[OPTIONS_ROW_ROWS] = {"HELP / CONTROLS",
                                           "SPELL FAVORITES (B)",
                                           filter,
                                           blocked ? "SAVE/CAST IN DUNGEON VIEW ONLY" : "QUICK SAVE (F5)",
                                           "QUICK LOAD (F9)",
                                           "CHEATS - OPTIONAL RULE CHANGES"};
    rows_draw(&(RowsScreen){.title = help ? "HELP" : "OPTIONS / HELP",
                            .title_item = help ? "BACK" : "CLOSE",
                            .rows = help ? help_lines : lines,
                            .row_count = help ? HELP_ROWS : OPTIONS_ROW_ROWS,
                            .subtitles = help ? (1u << HELP_ROWS) - 1 : 0},
              help ? HELP_ROWS : selected);
}

void options_open(void)
{
    help = false;
    blocked = false;
    selected = OPTIONS_ROW_HELP;
    show_options();
}

/* Runs the selected ordinary option; cheats have a separately labeled destination. */
static OptionsResult run_option(void)
{
    if (selected != OPTIONS_ROW_HELP && selected != OPTIONS_ROW_FILTER && selected != OPTIONS_ROW_ROWS &&
        input_get_mode() != INPUT_MODE_MOVE) {
        blocked = true;
        show_options();
        return OPTIONS_STAY;
    }
    switch (selected) {
    case OPTIONS_ROW_HELP:
        help = true;
        break;
    case OPTIONS_ROW_BOOK:
        return OPTIONS_SPELLBOOK;
    case OPTIONS_ROW_FILTER:
        upscale_cycle_filter();
        break;
    case OPTIONS_ROW_SAVE:
        return OPTIONS_QUICK_SAVE;
    case OPTIONS_ROW_LOAD:
        return OPTIONS_QUICK_LOAD;
    case OPTIONS_ROW_CHEATS:
        return OPTIONS_CHEATS;
    case OPTIONS_ROW_ROWS:
        return OPTIONS_CLOSE;
    }
    show_options();
    return OPTIONS_STAY;
}

OptionsResult options_event(int type, int16_t x, int16_t y)
{
    bool back = type == INPUT_BUTTON_RIGHT || (type == INPUT_KEY && (x == SCAN_ESCAPE || x == KEY_OPTIONS));
    if (help) {
        RowsScreen layout = {.title_item = "BACK", .row_count = HELP_ROWS, .subtitles = (1u << HELP_ROWS) - 1};
        if (back || (type == INPUT_KEY && x == KEY_ENTER) ||
            (type == INPUT_BUTTON_LEFT && rows_find_row(&layout, x, y) == HELP_ROWS)) {
            help = false;
            show_options();
        }
        return OPTIONS_STAY;
    }
    if (back)
        return OPTIONS_CLOSE;
    if (type == INPUT_KEY) {
        if ((x == KEYPAD_FORWARD || x == SCAN_W) && selected > 0)
            selected--;
        else if ((x == KEYPAD_BACK || x == SCAN_S) && selected < OPTIONS_ROW_ROWS)
            selected++;
        else if (x == KEY_ENTER)
            return run_option();
        show_options();
    } else if (type == INPUT_BUTTON_LEFT) {
        RowsScreen layout = {.title_item = "CLOSE", .row_count = OPTIONS_ROW_ROWS};
        int16_t hit = rows_find_row(&layout, x, y);
        if (hit >= 0) {
            selected = hit;
            return run_option();
        }
    }
    return OPTIONS_STAY;
}
