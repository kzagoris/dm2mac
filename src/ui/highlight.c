/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The touch highlight: outlines around the pictures of the click zones of the view that a hand click reaches. */
#include "highlight.h"
#include <stdlib.h>
#include "advanced.h"
#include "champion.h"
#include "dialog.h"
#include "draw.h"
#include "game.h"
#include "game_text.h"
#include "input.h"
#include "move.h"
#include "num.h"
#include "view.h"
#include "zone.h"

/* The longest arm of a bracket, and the part of the zone side that an arm takes at most. */
#define BRACKET_ARM  6
#define BRACKET_PART 3

/* The distance of the outline ring from the picture, in pixels. */
#define OUTLINE_RING 1

static struct {
    bool held;  /* Ctrl is held in the advanced mode */
    bool shown; /* the screen shows the highlight */
} highlight;

/* The view of the standing party shows on the screen with no screen over it. */
static bool is_party_view_shown(void)
{
    return g_game_mode == GAME_MODE_ADVANCED && g_main_loop.running && !g_main_loop.end && !g_advanced_screen &&
           !g_main_loop.view_hidden && !g_ui.inventory && !g_ui.candidate && !g_dialog.screen_mode &&
           !g_party_sleeping && !g_party.move_steps && g_current_map.map == g_game.party_map && view_shows_party();
}

void highlight_set_held(bool held)
{
    highlight.held = held && g_game_mode == GAME_MODE_ADVANCED;
}

bool highlight_is_due(void)
{
    return highlight.held != highlight.shown && is_party_view_shown();
}

/* The UI color of a touch kind: white takes, cyan an empty hand presses, yellow wants an item. */
static uint8_t touch_color(enum TouchKind touch)
{
    switch (touch) {
    case TOUCH_TAKE:
        return g_ui_colors[UI_WHITE];
    case TOUCH_HAND:
        return g_ui_colors[UI_CYAN];
    case TOUCH_ITEM:
        return g_ui_colors[UI_YELLOW];
    case TOUCH_NONE:
        break;
    }
    return g_ui_colors[UI_BLACK];
}

/* One pixel at (x, y) of the viewport, clipped to it. */
static void put_pixel(const int16_t *viewport, int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || y < 0 || x >= viewport[RECT_W] || y >= viewport[RECT_H])
        return;
    g_draw_screen.pixels[(viewport[RECT_Y] + y) * g_draw_screen.width + viewport[RECT_X] + x] = color;
}

/* The four corners of rect r (viewport pixels) in color, each arm with a black edge outside it. */
static void mark_zone(const int16_t *viewport, const int16_t *r, uint8_t color)
{
    int left = r[RECT_X];
    int top = r[RECT_Y];
    int right = r[RECT_X] + r[RECT_W] - 1;
    int bottom = r[RECT_Y] + r[RECT_H] - 1;
    int arm_x = clamp_int(1, r[RECT_W] / BRACKET_PART, BRACKET_ARM);
    int arm_y = clamp_int(1, r[RECT_H] / BRACKET_PART, BRACKET_ARM);
    uint8_t edge = g_ui_colors[UI_BLACK];
    for (int i = -1; i <= arm_x; i++) {
        put_pixel(viewport, left + i, top - 1, edge);
        put_pixel(viewport, right - i, top - 1, edge);
        put_pixel(viewport, left + i, bottom + 1, edge);
        put_pixel(viewport, right - i, bottom + 1, edge);
    }
    for (int i = -1; i <= arm_y; i++) {
        put_pixel(viewport, left - 1, top + i, edge);
        put_pixel(viewport, right + 1, top + i, edge);
        put_pixel(viewport, left - 1, bottom - i, edge);
        put_pixel(viewport, right + 1, bottom - i, edge);
    }
    for (int i = 0; i < arm_x; i++) {
        put_pixel(viewport, left + i, top, color);
        put_pixel(viewport, right - i, top, color);
        put_pixel(viewport, left + i, bottom, color);
        put_pixel(viewport, right - i, bottom, color);
    }
    for (int i = 0; i < arm_y; i++) {
        put_pixel(viewport, left, top + i, color);
        put_pixel(viewport, right, top + i, color);
        put_pixel(viewport, left, bottom - i, color);
        put_pixel(viewport, right, bottom - i, color);
    }
}

/* The outlined click zone of the picture pixel at (x, y) of the viewport, or -1. */
static int touch_zone_at(const bool *outlined, int16_t x, int16_t y)
{
    int zone = view_click_zone_at(x, y);
    return zone >= 0 && outlined[zone] ? zone : -1;
}

/* An outlined zone at distance (in pixels, also diagonal) from (x, y) of the viewport, or -1. */
static int touch_zone_near(const bool *outlined, int16_t x, int16_t y, int distance)
{
    for (int dy = -distance; dy <= distance; dy++)
        for (int dx = -distance; dx <= distance; dx++) {
            if (max_int(abs(dx), abs(dy)) != distance)
                continue;
            int zone = touch_zone_at(outlined, (int16_t)(x + dx), (int16_t)(y + dy));
            if (zone >= 0)
                return zone;
        }
    return -1;
}

/* Outlines the pictures of the touch zones: a ring in the touch color around the picture pixels, only on pixels
   outside a touch picture. */
static void outline_pictures(const int16_t *viewport, const bool *outlined, const uint8_t *colors)
{
    for (int16_t y = 0; y < viewport[RECT_H]; y++)
        for (int16_t x = 0; x < viewport[RECT_W]; x++) {
            if (touch_zone_at(outlined, x, y) >= 0)
                continue;
            int zone = touch_zone_near(outlined, x, y, OUTLINE_RING);
            if (zone >= 0)
                put_pixel(viewport, x, y, colors[zone]);
        }
}

/* Sets drawn[i] for each click zone i of which the last view drew a picture pixel. */
static void find_drawn_zones(bool *drawn)
{
    for (int16_t y = 0; y < VIEWPORT_HEIGHT; y++)
        for (int16_t x = 0; x < VIEWPORT_WIDTH; x++) {
            int zone = view_click_zone_at(x, y);
            if (zone >= 0)
                drawn[zone] = true;
        }
}

void highlight_draw(void)
{
    highlight.shown = false;
    int16_t viewport[RECT_SIZE];
    if (!highlight.held || !is_party_view_shown() || !zone_get_viewport_rect(viewport))
        return;
    bool outlined[VIEW_CLICK_ZONE_MAX] = {false}; /* the touch zones with a picture */
    uint8_t colors[VIEW_CLICK_ZONE_MAX] = {0};
    bool drawn[VIEW_CLICK_ZONE_MAX] = {false};
    find_drawn_zones(drawn);
    for (int i = 0; i < g_view_click_zone_count; i++) {
        const ViewClickZone *z = &g_view_click_zones[i];
        if (z->kind == CLICK_NONE || z->rect[RECT_W] <= 0 || z->rect[RECT_H] <= 0)
            continue;
        enum TouchKind touch = input_zone_touch(i);
        if (touch == TOUCH_NONE)
            continue;
        colors[i] = touch_color(touch);
        if (drawn[i])
            outlined[i] = true;
        else
            mark_zone(viewport, z->rect, touch_color(touch)); /* a zone without a picture of its own */
    }
    outline_pictures(viewport, outlined, colors);
    highlight.shown = true;
}
