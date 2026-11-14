/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The automap of the advanced mode: the map picture. */
#include "automap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "advanced.h"
#include "aspect.h"
#include "dungeon.h"
#include "game.h"
#include "game_text.h"
#include "item.h"
#include "minion.h"
#include "minion_map.h"
#include "num.h"
#include "rows.h"
#include "save.h"
#include "seen.h"
#include "sensor.h"
#include "zone.h"

/* The gap left of and above each square icon. */
#define ICON_GAP 1

/* The pressure pad mark: a square this size in the middle of the icon. */
#define PAD_MARK_SIZE   3
#define PAD_MARK_OFFSET ((MAP_ICON_W - PAD_MARK_SIZE) / 2)

/* The squares around a map that the view draws as its edge walls (dungeon_square_edge). */
#define MAP_MARGIN 1

/* A plus in the upper right of any square containing loose items. */
#define ITEM_MARK_W     3
#define ITEM_MARK_H     5
#define ITEM_MARK_LEFT  (MAP_ICON_W - ITEM_MARK_W)

/* The item list: a count/page subtitle, eight names, then mouse paging controls. */
#define ITEM_ROW_SUMMARY  0
#define ITEM_ROW_FIRST    1
#define ITEMS_PER_PAGE    8
#define ITEM_ROW_PREVIOUS (ITEM_ROW_FIRST + ITEMS_PER_PAGE)
#define ITEM_ROW_NEXT     (ITEM_ROW_PREVIOUS + 1)
#define ITEM_ROW_COUNT    (ITEM_ROW_NEXT + 1)
#define ITEM_LINE_SIZE    (ROWS_COLUMNS + 1)
_Static_assert(ITEM_ROW_COUNT <= ROWS_MAX_COUNT, "the automap item page fits in the viewport");

/* The first and the last level column (or row) that the automap draws. */
typedef struct {
    int first, last;
} Span;

/* A group of loose items with the same displayed name. Containers are listed without their contents. */
typedef struct {
    char name[TEXT_SIZE];
    int quantity;
} ItemSummary;

/* The text storage and mouse layout of one item page. */
typedef struct {
    char title[TEXT_SIZE];
    char texts[ITEM_ROW_COUNT][ITEM_LINE_SIZE];
    const char *lines[ITEM_ROW_COUNT];
    RowsScreen screen;
    int page_count;
} ItemPage;

static int scroll_x, scroll_y; /* squares between the party and the middle of the drawn map */

/* Selection is a level position, so it follows the same owner and visibility rules as the map picture. */
static struct {
    bool open;
    Location square;
    int page;
} inspection;

/* Three pixels per row: the same plus for any number of items. */
static const char item_mark[] = "000010111010000";

/* The maps of the party level, ended by LEVEL_LIST_END. */
static const uint8_t *party_level_maps(void)
{
    return dungeon_level_maps(dungeon_map_level(g_game.party_map));
}

/* The level position of the party square. */
static Location party_position(void)
{
    int map = g_game.party_map;
    return (Location){dungeon_map_offset_x(map) + g_game.party_x, dungeon_map_offset_y(map) + g_game.party_y};
}

/* The level position at the middle of the drawn map. */
static Location middle_square(void)
{
    Location party = party_position();
    return (Location){party.x + scroll_x, party.y + scroll_y};
}

/* The smallest rect of level positions (level x, y, width, height) that holds all maps of the party level with their
   margins. */
static void level_rect(int16_t *rect)
{
    int map = g_game.party_map;
    int x0 = dungeon_map_offset_x(map) - MAP_MARGIN;
    int y0 = dungeon_map_offset_y(map) - MAP_MARGIN;
    int x1 = x0 + dungeon_map_width(map) + 2 * MAP_MARGIN;
    int y1 = y0 + dungeon_map_height(map) + 2 * MAP_MARGIN;
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++) {
        int left = dungeon_map_offset_x(*m) - MAP_MARGIN;
        int top = dungeon_map_offset_y(*m) - MAP_MARGIN;
        int right = left + dungeon_map_width(*m) + 2 * MAP_MARGIN;
        int bottom = top + dungeon_map_height(*m) + 2 * MAP_MARGIN;
        x0 = min_int(left, x0);
        y0 = min_int(top, y0);
        x1 = max_int(right, x1);
        y1 = max_int(bottom, y1);
    }
    rect[RECT_X] = (int16_t)x0;
    rect[RECT_Y] = (int16_t)y0;
    rect[RECT_W] = (int16_t)(x1 - x0);
    rect[RECT_H] = (int16_t)(y1 - y0);
}

/* Queries */

bool automap_is_imaginary_wall(uint8_t square)
{
    return square_type(square) == SQUARE_FAKE_WALL && square_flag(square, SQUARE_FLAG_0) &&
           !square_flag(square, SQUARE_FAKE_WALL_OPEN);
}

bool automap_is_pressure_pad(Position at)
{
    if (square_type(dungeon_square(at.map, at.x, at.y)) != SQUARE_FLOOR)
        return false;
    ViewSquare view;
    aspect_get(&view, at.map, DIR_NORTH, at.x, at.y);
    if ((view.side[SIDE_FLOOR_ORNAMENT] & ORNAMENT_INDEX_MASK) == ORNAMENT_NONE)
        return false; /* the view draws no pad: a hidden trigger stays hidden */
    for (uint16_t t = dungeon_square_first_thing(at.map, at.x, at.y);
         t != THING_ENDOFLIST && thing_type(t) <= THING_SENSOR; t = dungeon_next_thing(t)) {
        if (thing_type(t) != THING_SENSOR || sensor_ornament_index(t) < 0)
            continue;
        switch (sensor_type(t)) {
        case FLOOR_ANY:
        case FLOOR_PARTY_OR_CREATURE:
        case FLOOR_PARTY:
        case FLOOR_CREATURE:
        case FLOOR_PARTY_POSSESSES:
        case FLOOR_ITEM:
            return true;
        }
    }
    return false;
}

/* True when map covers the level position level_at; *position is the square of map there. */
static bool covers(int16_t map, Location level_at, Position *position)
{
    *position = (Position){map, level_at.x - dungeon_map_offset_x(map), level_at.y - dungeon_map_offset_y(map)};
    return position->x >= 0 && position->x < dungeon_map_width(map) && position->y >= 0 &&
           position->y < dungeon_map_height(map);
}

/* True when the level position level_at is in the margin of map; *position is the margin square of map there. */
static bool in_margin(int16_t map, Location level_at, Position *position)
{
    if (covers(map, level_at, position))
        return false;
    return position->x >= -MAP_MARGIN && position->x < dungeon_map_width(map) + MAP_MARGIN &&
           position->y >= -MAP_MARGIN && position->y < dungeon_map_height(map) + MAP_MARGIN;
}

/* True when map covers the level position level_at with a square that is not SQUARE_UNKNOWN. */
static bool covers_known(int16_t map, Location level_at, Position *position)
{
    return covers(map, level_at, position) &&
           square_type(dungeon_square(map, position->x, position->y)) != SQUARE_UNKNOWN;
}

/* True when a map of the party level covers the level position level_at with a square that is not SQUARE_UNKNOWN. */
static bool covered(Location level_at)
{
    Position position;
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (covers_known(*m, level_at, &position))
            return true;
    return false;
}

bool automap_level_square(Location level_at, Position *position)
{
    int16_t party_map = g_game.party_map;
    Location party = party_position();
    if (level_at.x == party.x && level_at.y == party.y) {
        *position = (Position){party_map, g_game.party_x, g_game.party_y};
        return true;
    }
    int next = 0;
    do {
        int16_t x = level_at.x - dungeon_map_offset_x(party_map);
        int16_t y = level_at.y - dungeon_map_offset_y(party_map);
        int map = dungeon_location_on_map_from((Position){party_map, x, y}, 0, &next).map;
        if (map >= 0 && covers(map, level_at, position))
            return true; /* a found square in the margin of its map does not count */
    } while (next != 0);
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (covers_known(*m, level_at, position))
            return true;
    /* no map has a square there: the edge wall that the view draws in the margin of a map */
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (in_margin(*m, level_at, position) &&
            square_type(dungeon_square_edge(*m, position->x, position->y)) == SQUARE_WALL)
            return true;
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (covers(*m, level_at, position))
            return true;
    return false;
}

/* True when map has seen a square of its own next to its margin square position. */
static bool margin_seen(const Position *position)
{
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if (seen_is_seen((Position){position->map, (int16_t)(position->x + dx), (int16_t)(position->y + dy)}))
                return true;
    return false;
}

bool automap_level_seen(Location level_at)
{
    Position position;
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (covers(*m, level_at, &position) && seen_is_seen(position))
            return true;
    if (covered(level_at))
        return false;
    /* an edge wall counts as seen with a seen square next to it: the view shows it from there */
    for (const uint8_t *m = party_level_maps(); *m != LEVEL_LIST_END; m++)
        if (in_margin(*m, level_at, &position) && margin_seen(&position))
            return true;
    return false;
}

int automap_items(Location level_at, uint16_t *items, int capacity)
{
    Position at;
    if (g_game.party_map >= dungeon_map_count() || !(game_has_cheat(CHEAT_MAP) || automap_level_seen(level_at)) ||
        !automap_level_square(level_at, &at) || !covers(at.map, level_at, &at) ||
        square_type(dungeon_square(at.map, at.x, at.y)) == SQUARE_UNKNOWN)
        return 0;
    int count = 0;
    for (uint16_t t = dungeon_square_first_thing(at.map, at.x, at.y); t != THING_ENDOFLIST; t = dungeon_next_thing(t)) {
        if (!thing_is_item(t))
            continue;
        if (items && count < capacity)
            items[count] = t;
        count++;
    }
    return count;
}

bool automap_items_open(void)
{
    return inspection.open;
}

/* Drawing */

/* Fills rect cut to clip. */
static void fill_clipped(Offscreen *o, const int16_t *clip, const int16_t *rect, int color)
{
    int x0 = max_int(rect[RECT_X], clip[RECT_X]);
    int y0 = max_int(rect[RECT_Y], clip[RECT_Y]);
    int x1 = min_int(rect[RECT_X] + rect[RECT_W], clip[RECT_X] + clip[RECT_W]);
    int y1 = min_int(rect[RECT_Y] + rect[RECT_H], clip[RECT_Y] + clip[RECT_H]);
    if (x1 <= x0 || y1 <= y0)
        return;
    int16_t r[RECT_SIZE] = {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
    offscreen_fill_rect(o, r, color);
}

/* Light gray on every pixel of the icon at the screen place (icon_x, icon_y) with an even x + y: the wall icon shows a
   checkerboard. */
static void checker_overlay(Offscreen *o, const int16_t *clip, int16_t icon_x, int16_t icon_y)
{
    for (int py = icon_y; py < icon_y + MAP_ICON_H; py++)
        for (int px = icon_x; px < icon_x + MAP_ICON_W; px++)
            if (!((px + py) & 1))
                fill_clipped(o, clip, (int16_t[RECT_SIZE]){(int16_t)px, (int16_t)py, 1, 1}, g_ui_colors[UI_LIGHT_GRAY]);
}

/* Marks loose items, even those without map graphics, with a clipped yellow plus. */
static void show_item_mark(Offscreen *o, const int16_t *clip, Location level_at, int16_t icon_x, int16_t icon_y)
{
    int count = automap_items(level_at, NULL, 0);
    if (!count)
        return;
    for (int y = 0; y < ITEM_MARK_H; y++)
        for (int x = 0; x < ITEM_MARK_W; x++)
            fill_clipped(o, clip,
                         (int16_t[RECT_SIZE]){(int16_t)(icon_x + ITEM_MARK_LEFT + x), (int16_t)(icon_y + y), 1, 1},
                         g_ui_colors[item_mark[y * ITEM_MARK_W + x] == '1' ? UI_YELLOW : UI_BLACK]);
}

/* The white frame just outside the level rect, whose level position (0, 0) icon is at (left, top). */
static void map_frame(Offscreen *o, const int16_t *clip, int left, int top, const int16_t *rect)
{
    int color = g_ui_colors[UI_WHITE];
    int x0 = left + rect[RECT_X] * MAP_CELL_SIZE - ICON_GAP;
    int y0 = top + rect[RECT_Y] * MAP_CELL_SIZE - ICON_GAP;
    int right = x0 + rect[RECT_W] * MAP_CELL_SIZE;
    int bottom = y0 + rect[RECT_H] * MAP_CELL_SIZE;
    int16_t w = (int16_t)(right - x0 + 1);
    int16_t h = (int16_t)(bottom - y0 + 1);
    fill_clipped(o, clip, (int16_t[RECT_SIZE]){(int16_t)x0, (int16_t)y0, w, 1}, color);
    fill_clipped(o, clip, (int16_t[RECT_SIZE]){(int16_t)x0, (int16_t)bottom, w, 1}, color);
    fill_clipped(o, clip, (int16_t[RECT_SIZE]){(int16_t)x0, (int16_t)y0, 1, h}, color);
    fill_clipped(o, clip, (int16_t[RECT_SIZE]){(int16_t)right, (int16_t)y0, 1, h}, color);
}

/* The level columns (or rows) from start, count long, whose icons can show in the screen span from clip_start,
   clip_len long; origin is the screen place of level column 0. */
static Span visible_span(int origin, int start, int count, int clip_start, int clip_len)
{
    int from = clip_start - (origin + start * MAP_CELL_SIZE);
    int to = from + clip_len - 1;
    Span span = {start + (from > 0 ? from / MAP_CELL_SIZE : 0), start + count - 1};
    if (to < 0)
        span.last = start - 1;
    else if (to / MAP_CELL_SIZE < count)
        span.last = start + to / MAP_CELL_SIZE;
    return span;
}

/* Draws the square that the level position level_at shows, with its icon at the screen place (icon_x, icon_y), when it
   is seen or the reveal map cheat is on. */
static void show_level_square(Offscreen *o, const int16_t *clip, Location level_at, int16_t icon_x, int16_t icon_y)
{
    Position position;
    if (!automap_level_square(level_at, &position) || !(game_has_cheat(CHEAT_MAP) || automap_level_seen(level_at)))
        return;
    bool party = position.map == g_game.party_map && position.x == g_game.party_x && position.y == g_game.party_y;
    dungeon_set_current_map(position.map);
    /* the things of a minion map without options: the static creatures */
    SquareIcon icon = {.sx = position.x,
                       .sy = position.y,
                       .x = icon_x,
                       .y = icon_y,
                       .party_item = party ? mmap_party_icons_item() : -1,
                       .party_dir = g_game.party_dir,
                       .flags = MAP_STATIC_CREATURES};
    mmap_draw_square_icon(o, clip, &icon);
    if (!covers(position.map, level_at, &position))
        return; /* an edge wall has no imaginary wall and no pad */
    if (automap_is_imaginary_wall(dungeon_square(position.map, position.x, position.y)))
        checker_overlay(o, clip, icon_x, icon_y);
    else if (automap_is_pressure_pad(position))
        fill_clipped(o, clip,
                     (int16_t[RECT_SIZE]){(int16_t)(icon_x + PAD_MARK_OFFSET), (int16_t)(icon_y + PAD_MARK_OFFSET),
                                          PAD_MARK_SIZE, PAD_MARK_SIZE},
                     g_ui_colors[UI_GRAY]);
    show_item_mark(o, clip, level_at, icon_x, icon_y);
}

void automap_draw(Offscreen *o, const int16_t *clip)
{
    offscreen_fill_rect(o, clip, g_ui_colors[UI_BLACK]);
    if (g_game.party_map >= dungeon_map_count())
        return;
    Location middle = middle_square();
    int16_t rect[RECT_SIZE];
    level_rect(rect);
    int left = clip[RECT_X] + (clip[RECT_W] - MAP_CELL_SIZE) / 2 + ICON_GAP - middle.x * MAP_CELL_SIZE;
    int top = clip[RECT_Y] + (clip[RECT_H] - MAP_CELL_SIZE) / 2 + ICON_GAP - middle.y * MAP_CELL_SIZE;
    Span xs = visible_span(left, rect[RECT_X], rect[RECT_W], clip[RECT_X], clip[RECT_W]);
    Span ys = visible_span(top, rect[RECT_Y], rect[RECT_H], clip[RECT_Y], clip[RECT_H]);
    int saved = g_current_map.map;
    for (int x = xs.first; x <= xs.last; x++)
        for (int y = ys.first; y <= ys.last; y++)
            show_level_square(o, clip, (Location){x, y}, (int16_t)(left + x * MAP_CELL_SIZE),
                              (int16_t)(top + y * MAP_CELL_SIZE));
    map_frame(o, clip, left, top, rect);
    dungeon_set_current_map(saved);
}

/* Groups the selected square's current items by their full displayed names; the caller frees the result. */
static ItemSummary *summarize_items(int *group_count, int *total)
{
    *group_count = 0;
    *total = automap_items(inspection.square, NULL, 0);
    if (!*total)
        return NULL;
    uint16_t *things = malloc((size_t)*total * sizeof *things);
    ItemSummary *groups = calloc((size_t)*total, sizeof *groups);
    if (!things || !groups) {
        free(things);
        free(groups);
        return NULL;
    }
    automap_items(inspection.square, things, *total);
    for (int i = 0; i < *total; i++) {
        const char *name = item_name(things[i]);
        if (!*name)
            name = "UNKNOWN ITEM";
        int group = 0;
        while (group < *group_count && strcmp(groups[group].name, name))
            group++;
        if (group == *group_count) {
            snprintf(groups[group].name, sizeof groups[group].name, "%s", name);
            (*group_count)++;
        }
        groups[group].quantity++;
    }
    free(things);
    return groups;
}

/* Builds a page without drawing it, so the renderer and mouse hit testing share the same layout. */
static void build_item_page(ItemPage *page)
{
    memset(page, 0, sizeof *page);
    for (int i = 0; i < ITEM_ROW_COUNT; i++)
        page->lines[i] = page->texts[i];
    Position at = {0};
    automap_level_square(inspection.square, &at);
    snprintf(page->title, sizeof page->title, "ITEMS M%d X%d Y%d", at.map, at.x, at.y);
    int count, total;
    ItemSummary *groups = summarize_items(&count, &total);
    page->page_count = max_int(1, (count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    inspection.page = clamp_int(0, inspection.page, page->page_count - 1);
    snprintf(page->texts[ITEM_ROW_SUMMARY], ITEM_LINE_SIZE, "%d ITEMS  PAGE %d/%d", total, inspection.page + 1,
             page->page_count);
    if (!groups)
        snprintf(page->texts[ITEM_ROW_FIRST], ITEM_LINE_SIZE, "%s", total ? "UNABLE TO LIST ITEMS" : "NO ITEMS");
    for (int i = 0; i < ITEMS_PER_PAGE && inspection.page * ITEMS_PER_PAGE + i < count; i++) {
        const ItemSummary *group = &groups[inspection.page * ITEMS_PER_PAGE + i];
        char *line = page->texts[ITEM_ROW_FIRST + i];
        int length = snprintf(line, ITEM_LINE_SIZE, "%d X %s", group->quantity, group->name);
        if (length >= ITEM_LINE_SIZE)
            memcpy(line + ITEM_LINE_SIZE - sizeof "..", "..", sizeof "..");
    }
    free(groups);
    if (inspection.page > 0)
        snprintf(page->texts[ITEM_ROW_PREVIOUS], ITEM_LINE_SIZE, "< PREVIOUS");
    if (inspection.page + 1 < page->page_count)
        snprintf(page->texts[ITEM_ROW_NEXT], ITEM_LINE_SIZE, "NEXT >");
    page->screen = (RowsScreen){.title = page->title,
                                .title_item = "MAP",
                                .rows = page->lines,
                                .row_count = ITEM_ROW_COUNT,
                                .subtitles = 1u << ITEM_ROW_SUMMARY};
}

void automap_draw_items(void)
{
    ItemPage page;
    build_item_page(&page);
    rows_draw(&page.screen, -1);
}

/* Inspection */

/* Divides a screen offset into whole map cells, rounding toward negative infinity on the left/top. */
static int cell_offset(int offset)
{
    return offset / MAP_CELL_SIZE - (offset % MAP_CELL_SIZE < 0);
}

bool automap_select(int16_t x, int16_t y)
{
    int16_t rect[RECT_SIZE];
    if (!zone_get_viewport_rect(rect) || x < rect[RECT_X] || x >= rect[RECT_X] + rect[RECT_W] || y < rect[RECT_Y] ||
        y >= rect[RECT_Y] + rect[RECT_H])
        return false;
    Location middle = middle_square();
    Location square = {middle.x + cell_offset(x - rect[RECT_X] - (rect[RECT_W] - MAP_CELL_SIZE) / 2),
                       middle.y + cell_offset(y - rect[RECT_Y] - (rect[RECT_H] - MAP_CELL_SIZE) / 2)};
    if (!automap_items(square, NULL, 0))
        return false;
    inspection.square = square;
    inspection.page = 0;
    inspection.open = true;
    return true;
}

bool automap_items_event(int type, int16_t x, int16_t y)
{
    if (!inspection.open)
        return false;
    if (type == INPUT_BUTTON_RIGHT || (type == INPUT_KEY && x == SCAN_ESCAPE)) {
        inspection.open = false;
        return true;
    }
    ItemPage page;
    build_item_page(&page);
    int step = 0;
    if (type == INPUT_KEY)
        step = x == KEYPAD_TURN_LEFT || x == SCAN_A ? -1 : x == KEYPAD_TURN_RIGHT || x == SCAN_D ? 1 : 0;
    else if (type == INPUT_BUTTON_LEFT) {
        int16_t row = rows_find_row(&page.screen, x, y);
        if (row == ITEM_ROW_COUNT)
            inspection.open = false;
        else if (row == ITEM_ROW_PREVIOUS)
            step = -1;
        else if (row == ITEM_ROW_NEXT)
            step = 1;
    }
    inspection.page = clamp_int(0, inspection.page + step, page.page_count - 1);
    return true;
}

/* Scrolling */

void automap_scroll(int dx, int dy)
{
    if (g_game.party_map >= dungeon_map_count())
        return;
    Location middle = middle_square();
    int16_t rect[RECT_SIZE];
    level_rect(rect);
    scroll_x += clamp_int(rect[RECT_X], middle.x + dx, rect[RECT_X] + rect[RECT_W] - 1) - middle.x;
    scroll_y += clamp_int(rect[RECT_Y], middle.y + dy, rect[RECT_Y] + rect[RECT_H] - 1) - middle.y;
}

void automap_scroll_reset(void)
{
    scroll_x = 0;
    scroll_y = 0;
    inspection.open = false;
    inspection.page = 0;
}
