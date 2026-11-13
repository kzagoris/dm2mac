/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "options.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "advanced.h"
#include "filter.h"
#include "game_text.h"
#include "ids.h"
#include "move.h"
#include "text.h"
#include "ui.h"
#include "video.h"

/* Option rows; the title item closes the overlay. */
#define OPTIONS_ROW_HELP      0
#define OPTIONS_ROW_BOOK      1
#define OPTIONS_ROW_FILTER    2
#define OPTIONS_ROW_ASPECT    3
#define OPTIONS_ROW_SCANLINES 4
#define OPTIONS_ROW_STEPPING  5
#define OPTIONS_ROW_SAVE      6
#define OPTIONS_ROW_LOAD      7
#define OPTIONS_ROW_CHEATS    8
#define OPTIONS_ROW_ROWS      9
#define OPTIONS_ROW_TEXT_SIZE 40

static bool help;
static int16_t selected;
static bool blocked;

/* Full-screen options avoid mixing help text with the dungeon controls. */
#define PANEL_LEFT 18
#define PANEL_VALUE_LEFT 108
#define PANEL_TITLE_Y 20
#define PANEL_FIRST_Y 46
#define PANEL_ROW_HEIGHT 12
#define PANEL_BACK_LEFT 260
#define PANEL_FOOTER_Y 186

static void panel_text(int x, int y, int color, const char *text)
{
    text_draw_on_screen((int16_t)x, (int16_t)y, g_ui_colors[color], g_ui_colors[UI_BLACK], text);
}

/* The header action is shared by Help and Options. */
static bool panel_back_click(int16_t x, int16_t y)
{
    return x >= PANEL_BACK_LEFT && x < SCREEN_W - PANEL_LEFT &&
           y >= PANEL_TITLE_Y - TEXT_LINE_HEIGHT && y <= PANEL_TITLE_Y + FONT_BASELINE;
}

static void show_options(void)
{
    memset(g_video_fb, g_ui_colors[UI_BLACK], sizeof g_video_fb);
    panel_text(PANEL_LEFT, PANEL_TITLE_Y, UI_CYAN, help ? "CONTROLS" : "OPTIONS / HELP");
    panel_text(PANEL_BACK_LEFT, PANEL_TITLE_Y, UI_YELLOW, help ? "BACK" : "CLOSE");
    if (help) {
        static const char *const keys[] = {
            "WASD", "Q / E", "ARROWS", "TAB / M", "1 - 4", "F5 / F9", "B", "F1", "ESC", "CTRL+S", "CTRL+R"};
        static const char *const actions[] = {
            "MOVE", "TURN LEFT / RIGHT", "ORIGINAL MOVEMENT", "EXPLORED MAP", "CHAMPION INVENTORY",
            "QUICK SAVE / LOAD", "SPELL FAVORITES", "OPTIONS / HELP", "OPTIONS / RESUME", "DISK MENU", "REST (ZZZ)"};
        panel_text(PANEL_LEFT, 36, UI_CYAN, "MOVEMENT AND SHORTCUTS");
        for (int i = 0; i < 11; i++) {
            panel_text(PANEL_LEFT, 46 + i * 9, UI_YELLOW, keys[i]);
            panel_text(PANEL_VALUE_LEFT, 46 + i * 9, UI_WHITE, actions[i]);
        }
        panel_text(PANEL_LEFT, 152, UI_CYAN, "SPELLS");
        panel_text(PANEL_LEFT, 164, UI_WHITE, "CHAMPION ICON > POWER > RUNES > CAST");
        panel_text(PANEL_LEFT, 174, UI_WHITE, "B RECORDS RUNES; NORMAL MANA RULES APPLY");
    } else {
        char filter[OPTIONS_ROW_TEXT_SIZE];
        snprintf(filter, sizeof filter, "SCREEN FILTER: %s", settings_filter_name(upscale_get_filter()));
        for (char *ch = filter; *ch; ch++)
            *ch = (char)toupper((unsigned char)*ch);
        const char *lines[OPTIONS_ROW_ROWS] = {
            "CONTROLS",
            "SPELL FAVORITES (B)",
            filter,
            upscale_has_aspect() ? "4:3 ASPECT: ON" : "4:3 ASPECT: OFF",
            upscale_has_scanlines() ? "SCANLINES: ON" : "SCANLINES: OFF",
            move_has_classic_steps() ? "SMOOTH STEPPING: OFF" : "SMOOTH STEPPING: ON",
            "QUICK SAVE (F5)",
            "QUICK LOAD (F9)",
            "CHEATS (OPTIONAL)"};
        for (int i = 0; i < OPTIONS_ROW_ROWS; i++) {
            panel_text(PANEL_LEFT, PANEL_FIRST_Y + i * PANEL_ROW_HEIGHT,
                       i == selected ? UI_YELLOW : UI_WHITE, lines[i]);
        }
        if (blocked)
            panel_text(PANEL_LEFT, 164, UI_YELLOW, "RETURN TO DUNGEON TO SAVE OR CAST");
        else
            panel_text(PANEL_LEFT, 164, UI_CYAN, "CHEATS ARE SEPARATE AND OPTIONAL");
    }
    panel_text(PANEL_LEFT, PANEL_FOOTER_Y, UI_WHITE,
               help ? "ENTER / ESC / RIGHT CLICK: BACK" : "UP/DOWN: SELECT  ENTER: OPEN  ESC: CLOSE");
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
    if (selected != OPTIONS_ROW_HELP && selected != OPTIONS_ROW_FILTER && selected != OPTIONS_ROW_ASPECT &&
        selected != OPTIONS_ROW_STEPPING && selected != OPTIONS_ROW_SCANLINES && selected != OPTIONS_ROW_ROWS &&
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
    case OPTIONS_ROW_ASPECT:
        upscale_toggle_aspect();
        break;
    case OPTIONS_ROW_SCANLINES:
        upscale_toggle_scanlines();
        break;
    case OPTIONS_ROW_STEPPING:
        upscale_toggle_stepping();
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
        if (back || (type == INPUT_KEY && x == KEY_ENTER) ||
            (type == INPUT_BUTTON_LEFT && panel_back_click(x, y))) {
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
        int16_t hit = -1;
        if (panel_back_click(x, y))
            hit = OPTIONS_ROW_ROWS;
        else if (x >= PANEL_LEFT && x < SCREEN_W - PANEL_LEFT &&
                 y >= PANEL_FIRST_Y - TEXT_LINE_HEIGHT &&
                 y < PANEL_FIRST_Y - TEXT_LINE_HEIGHT + OPTIONS_ROW_ROWS * PANEL_ROW_HEIGHT)
            hit = (int16_t)((y - PANEL_FIRST_Y + TEXT_LINE_HEIGHT) / PANEL_ROW_HEIGHT);
        if (hit >= 0) {
            selected = hit;
            return run_option();
        }
    }
    return OPTIONS_STAY;
}
