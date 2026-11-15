/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Dialogs: messages, questions, progress, and the volume settings. */
#include "dialog.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "draw.h"
#include "game.h"
#include "game_text.h"
#include "gdat.h"
#include "ids.h"
#include "input.h"
#include "interface_state.h"
#include "play.h"
#include "save.h"
#include "sound.h"
#include "text.h"
#include "ui.h"
#include "view.h"
#include "zone.h"

/* The buttons of the dialogs. */
#define DIALOG_BUTTONS_MAX    4
#define DIALOG_VOLUME_BUTTONS 7
#define BUTTON_KEY            7 /* transparent color of a pressed button picture */
#define BUTTON_PRESS_VBLANKS  20

/* The layout of the message texts and of the progress bar. */
#define DIALOG_LINE_GAP    1 /* pixels between two lines */
#define DIALOG_MESSAGE_TOP 6 /* the message area starts this far into its zone */
#define PERMILLE           1000
#define MESSAGE_TEXTS      2 /* the dialog message and the error message under it */
#define WRAP_MARGIN_SHIFT  3 /* a text wider than 7/8 of the zone wraps */
#define WRAP_SHRINK_SHIFT  2 /* to 3/4 of its width, when it is not wider than 5/4 of the zone */
#define BUTTON_TEXT_ZONES  10

/* The volume settings 0 to 7 of the volume dialog. */
#define VOLUME_LEVELS 8

/* The cancel of a dialog without a button for a right click. */
#define DIALOG_NO_CANCEL (-1)

/* The advanced save menu: the button of text entry 3 shows LOAD. */
#define SAVE_MENU_LOAD_ENTRY 3

/* The choice of the close button of the advanced save menu: no button has it. */
#define DIALOG_CLOSE_CHOICE 0x7f

/* The close button: the exit button picture of the inventory without its black outline, in the stone bar above the
   message panel, in the corner right of the panel (its distance from the top and the right edge). */
#define CLOSE_BUTTON_OUTLINE 1
#define CLOSE_BUTTON_TOP     0
#define CLOSE_BUTTON_RIGHT   0

/* The zones of the button texts (table of SKULL.EXE), and the first zone of the texts by button count; a dialog with
   more than DIALOG_BUTTONS_MAX buttons starts at the first zone. */
static const uint16_t button_text_zones[BUTTON_TEXT_ZONES] = {
    ZONE_BUTTON_TEXT_BOTTOM,                                                                   /* 1 button */
    ZONE_BUTTON_TEXT_TOP,         ZONE_BUTTON_TEXT_BOTTOM,                                     /* 2 buttons */
    ZONE_BUTTON_TEXT_TOP,         ZONE_BUTTON_TEXT_BOTTOM_LEFT, ZONE_BUTTON_TEXT_BOTTOM_RIGHT, /* 3 buttons */
    ZONE_BUTTON_TEXT_TOP_LEFT,    ZONE_BUTTON_TEXT_TOP_RIGHT,   ZONE_BUTTON_TEXT_BOTTOM_LEFT,  /* 4 buttons */
    ZONE_BUTTON_TEXT_BOTTOM_RIGHT};
static const uint8_t button_zone_first[DIALOG_BUTTONS_MAX + 1] = {0, 0, 1, 3, 6};

DialogState g_dialog = {.screen_mode = 1};

/* Whether dialog id is the save menu of the advanced mode, with LOAD and the close button. */
static bool is_advanced_save_menu(int id)
{
    return id == DIALOG_SAVE_MENU && g_game_mode == GAME_MODE_ADVANCED;
}

/* Drawing */

/* The screen position of the top left corner of the dialog viewport. */
static ScreenPoint viewport_origin(void)
{
    int16_t r[RECT_SIZE];
    ScreenSize size = {0, 0};
    if (!g_dialog.screen_mode)
        return zone_get_viewport_rect(r) ? (ScreenPoint){r[RECT_X], r[RECT_Y]} : (ScreenPoint){0, 0};
    if (!zone_compute_rect(&g_view_viewport, r, ZONE_DIALOG_SCREEN, &size))
        return (ScreenPoint){0, 0};
    return (ScreenPoint){(int16_t)(r[RECT_X] - size.w), (int16_t)(r[RECT_Y] - size.h)};
}

/* Draws the close button of the advanced save menu, released or pressed, and keeps its screen rect for the clicks. */
static void render_close_button(bool pressed)
{
    int entry = pressed ? GFX_PANEL_BUTTON_EXIT + 1 : GFX_PANEL_BUTTON_EXIT;
    const Bitmap *image = draw_gdat_image(CAT_PANEL, 0, entry);
    int16_t r[RECT_SIZE];
    if (!image || image->width <= 2 * CLOSE_BUTTON_OUTLINE || image->height <= 2 * CLOSE_BUTTON_OUTLINE ||
        !zone_get_rect(ZONE_DIALOG_BACK, r))
        return;
    int16_t w = (int16_t)(image->width - 2 * CLOSE_BUTTON_OUTLINE);
    int16_t h = (int16_t)(image->height - 2 * CLOSE_BUTTON_OUTLINE);
    r[RECT_X] = (int16_t)(r[RECT_X] + r[RECT_W] - w - CLOSE_BUTTON_RIGHT);
    r[RECT_Y] = (int16_t)(r[RECT_Y] + CLOSE_BUTTON_TOP);
    r[RECT_W] = w;
    r[RECT_H] = h;
    Blit blit = {.src = image->pixels,
                 .dst = g_view_viewport.pixels,
                 .src_x = CLOSE_BUTTON_OUTLINE,
                 .src_y = CLOSE_BUTTON_OUTLINE,
                 .src_width = image->width,
                 .dst_width = g_view_viewport.width,
                 .src_bpp = image->bpp,
                 .dst_bpp = IMAGE_8BPP,
                 .transparent = GFX_OPAQUE,
                 .mirror = MIRROR_NONE,
                 .colormap = draw_gdat_image_palette(CAT_PANEL, 0, entry)};
    gfx_blit(&blit, r);
    ScreenPoint origin = viewport_origin();
    r[RECT_X] = (int16_t)(r[RECT_X] + origin.x);
    r[RECT_Y] = (int16_t)(r[RECT_Y] + origin.y);
    memcpy(g_dialog.close_rect, r, sizeof r);
}

void dialog_screen_mode(bool begin)
{
    if (g_dialog.screen_mode && begin)
        ui_fill_zone_screen(ZONE_SCREEN, 0);
}

void dialog_present(void)
{
    if (!g_dialog.screen_mode) {
        play_present_viewport();
        return;
    }
    int16_t r[RECT_SIZE];
    ScreenSize size = {0, 0};
    if (zone_compute_rect(&g_view_viewport, r, ZONE_DIALOG_SCREEN, &size))
        ui_blit_bitmap(&(BlitSource){.bitmap = &g_view_viewport, .x = size.w, .y = size.h, .transparent = GFX_OPAQUE},
                       &g_draw_screen, r);
}

/* The message texts of a dialog, centered in rect: each text wraps to a width near the zone width. */
static void render_message(const int16_t *rect, char *texts[MESSAGE_TEXTS], const int colors[MESSAGE_TEXTS])
{
    int16_t widths[MESSAGE_TEXTS] = {0};
    int16_t heights[MESSAGE_TEXTS] = {0};
    int total = 0;
    char line[TEXT_SIZE];
    for (int i = 0; i < MESSAGE_TEXTS; i++) {
        if (!texts[i])
            continue;
        ScreenSize size = text_measure(texts[i]);
        widths[i] = size.w;
        heights[i] = size.h;
        int w = rect[RECT_W];
        if (widths[i] > w - (w >> WRAP_MARGIN_SHIFT))
            widths[i] = widths[i] < w + (w >> WRAP_SHRINK_SHIFT)
                            ? (int16_t)(widths[i] - (widths[i] >> WRAP_SHRINK_SHIFT))
                            : (int16_t)w;
        for (int16_t pos = 0; texts[i][pos];) {
            text_wrap_line(texts[i], line, &pos, widths[i]);
            total += 2 * DIALOG_LINE_GAP + heights[i] - FONT_BASELINE + 1;
            if (texts[i][pos] == '\n')
                pos++;
        }
    }
    if (!total)
        return;
    int offset = DIALOG_MESSAGE_TOP + ((rect[RECT_H] - (total - 2 * DIALOG_LINE_GAP)) >> 1) - 1;
    int y = rect[RECT_Y] + (offset > 0 ? offset : 0);
    for (int i = 0; i < MESSAGE_TEXTS; i++) {
        if (!texts[i])
            continue;
        for (int16_t pos = 0; texts[i][pos];) {
            int w = text_wrap_line(texts[i], line, &pos, widths[i]);
            text_draw_in_viewport((int16_t)(VIEWPORT_WIDTH - w) >> 1, (int16_t)y, colors[i], line);
            y += TEXT_LINE_HEIGHT + DIALOG_LINE_GAP;
            if (texts[i][pos] == '\n')
                pos++;
        }
    }
}

int dialog_draw(int id, int message)
{
    const Gdat *g = gdat_current();
    bool present = !((id == DIALOG_LOADING || id == DIALOG_VOLUME) && message);
    if (message && id != DIALOG_ERROR && id != DIALOG_LOADING &&
        gdat_exists(g, CAT_DIALOG, DIALOG_ERROR, GDAT_TYPE_IMAGE, GFX_DIALOG_PICTURE)) {
        dialog_run(dialog_draw(DIALOG_ERROR, message));
        message = MESSAGE_NONE;
    }
    g_dialog.id = id;
    if (present)
        dialog_screen_mode(true);
    char texts[DIALOG_TEXT_COUNT][TEXT_SIZE];
    int count = 0;
    for (int e = 0; e < DIALOG_TEXT_COUNT; e++)
        if (*gtext_gdat(CAT_DIALOG, id, e, texts[count]))
            count++;
    int16_t r[RECT_SIZE];
    if (zone_get_rect(ZONE_DIALOG_BACK, r))
        gfx_blit_to_bitmap(draw_gdat_image(CAT_DIALOG, id, GFX_DIALOG_PICTURE), &g_view_viewport, r, GFX_OPAQUE,
                           draw_gdat_image_palette(CAT_DIALOG, id, GFX_DIALOG_PICTURE));
    if (is_advanced_save_menu(id)) {
        render_close_button(false);
        if (count > SAVE_MENU_LOAD_ENTRY)
            snprintf(texts[SAVE_MENU_LOAD_ENTRY], TEXT_SIZE, "%s", DIALOG_LOAD_TEXT);
    } else {
        text_draw_in_zone(ZONE_DIALOG_VERSION, g_ui_colors[UI_DARKEST_GRAY], DIALOG_VERSION_TEXT);
    }
    int first = count <= DIALOG_BUTTONS_MAX ? button_zone_first[count] : 0;
    /* fix: the texts stop at the end of the zone table; the original reads past it with more than 10 texts */
    for (int i = 0; i < count && first + i < BUTTON_TEXT_ZONES; i++)
        text_draw_in_zone(button_text_zones[first + i], g_ui_colors[UI_YELLOW], texts[i]);
    if (id != DIALOG_VOLUME && zone_get_rect(count < 2 ? ZONE_DIALOG_MESSAGE_WIDE : ZONE_DIALOG_MESSAGE, r)) {
        char message_texts[MESSAGE_TEXTS][TEXT_SIZE];
        char *shown[MESSAGE_TEXTS] = {NULL, NULL};
        int colors[MESSAGE_TEXTS];
        if (message && id == DIALOG_ERROR) {
            colors[0] = g_ui_colors[UI_WHITE];
            shown[0] = gtext_gdat(CAT_DIALOG, DIALOG_ERROR, message, message_texts[0]);
        } else {
            colors[0] = g_ui_colors[UI_YELLOW];
            shown[0] = gtext_gdat(CAT_DIALOG, id, GFX_DIALOG_MESSAGE, message_texts[0]);
            if (message) {
                colors[1] = g_ui_colors[UI_WHITE];
                shown[1] = gtext_gdat(CAT_DIALOG, DIALOG_ERROR, message, message_texts[1]);
            }
        }
        render_message(r, shown, colors);
    }
    if (present) {
        dialog_present();
        dialog_screen_mode(false);
    }
    game_set_screen_refresh(true);
    return id;
}

void dialog_draw_button(bool highlighted, GdatKey picture)
{
    if (!highlighted)
        return;
    const Bitmap *image = draw_gdat_image(picture.category, picture.index, picture.entry);
    if (!image)
        return;
    int16_t r[RECT_SIZE];
    rect_center_in(r, g_input_match.rect, image->width, image->height);
    if (g_input_match.origin != -1) {
        ScreenPoint origin = zone_get_position(g_input_match.origin);
        r[RECT_X] = (int16_t)(r[RECT_X] - origin.x);
        r[RECT_Y] = (int16_t)(r[RECT_Y] - origin.y);
    }
    gfx_blit_to_bitmap(image, &g_view_viewport, r, BUTTON_KEY,
                       draw_gdat_image_palette(picture.category, picture.index, picture.entry));
    play_wait_frames(BUTTON_PRESS_VBLANKS);
}

void dialog_draw_progress_bar(int permille)
{
    int16_t r[RECT_SIZE];
    if (!g_dialog.progress_enabled || !zone_get_rect(ZONE_PROGRESS_BAR, r))
        return;
    r[RECT_W] = (int16_t)(r[RECT_W] * permille / PERMILLE);
    if (r[RECT_W] <= 0 || r[RECT_W] == g_dialog.progress_width)
        return;
    g_dialog.progress_width = r[RECT_W];
    gfx_fill_rect(g_view_viewport.pixels, r, g_ui_colors[UI_GOLD], g_view_viewport.width, IMAGE_8BPP);
    dialog_present();
}

/* Running */

int dialog_run(int id)
{
    return dialog_run_cancel(id, DIALOG_NO_CANCEL);
}

int dialog_run_cancel(int id, int cancel)
{
    const Gdat *g = gdat_current();
    uint8_t values[DIALOG_TEXT_COUNT];
    int count = 0;
    char text[TEXT_SIZE];
    for (int e = 0; e < DIALOG_TEXT_COUNT; e++) {
        if (!*gdat_text(g, (GdatKey){CAT_DIALOG, id, e}, text, sizeof text))
            continue;
        /* the high byte (a default button of the original) only feeds a check that is always 0 */
        uint8_t value = (uint8_t)gdat_entry_index(g, CAT_DIALOG, id, GDAT_TYPE_VALUE, e);
        values[count++] = value ? value : (uint8_t)e;
    }
    int right_choice = g_dialog.right_choice;
    bool close_button = g_dialog.close_button;
    g_dialog.close_button = is_advanced_save_menu(id);
    g_dialog.right_choice = g_dialog.close_button ? DIALOG_CLOSE_CHOICE : 0;
    for (int i = 0; i < count && !g_dialog.close_button; i++)
        if (values[i] == cancel)
            g_dialog.right_choice = i + 1;
    g_dialog.button_count = id == DIALOG_VOLUME ? DIALOG_VOLUME_BUTTONS : count;
    input_push_mode(INPUT_MODE_DIALOG);
    g_input_match.choice = DIALOG_NO_CHOICE;
    do {
        command_process_queue();
        play_wait_frames(1);
        /* the keys that come before the input reads them: Return chooses the first button, Esc cancels */
        uint16_t key = g_input_match.choice == DIALOG_NO_CHOICE ? input_read_key() : 0;
        if (key == KEY_ENTER)
            input_simulate_command_click(CMD_DIALOG_BUTTON_FIRST);
        else if (key)
            input_cancels_dialog(INPUT_KEY, (int16_t)key, 0);
    } while (g_input_match.choice == DIALOG_NO_CHOICE && !g_main_loop.quit);
    int choice = g_input_match.choice;
    if (choice == DIALOG_CLOSE_CHOICE) {
        /* the pressed close button, as a pressed button of the inventory */
        render_close_button(true);
        dialog_present();
        play_wait_frames(BUTTON_PRESS_VBLANKS);
    }
    int result = id == DIALOG_VOLUME              ? g_input_match.value
                 : choice == DIALOG_CLOSE_CHOICE  ? DIALOG_CLOSED
                 : choice >= 1 && choice <= count ? values[choice - 1]
                                                  : 0;
    game_set_screen_refresh(true);
    input_pop_mode();
    g_dialog.right_choice = right_choice;
    g_dialog.close_button = close_button;
    return result;
}

bool dialog_close_button_contains(int16_t x, int16_t y)
{
    return g_dialog.close_button && rect_contains_point(g_dialog.close_rect, x, y);
}

/* Volume */

/* The buttons of the settings of one volume, the chosen one lit, and their texts (0: "off"). */
static void render_volume_buttons(int setting, int button_zone, int text_zone, int off_entry)
{
    char text[TEXT_SIZE];
    for (int level = VOLUME_LEVELS - 1; level >= 0; level--) {
        ui_draw_viewport_image(CAT_DIALOG, DIALOG_VOLUME, level == setting ? GFX_VOLUME_BUTTON_ON : GFX_VOLUME_BUTTON,
                               button_zone + level, GFX_OPAQUE);
        if (level) {
            text[0] = (char)('0' + level);
            text[1] = '\0';
        } else {
            gtext_gdat(CAT_DIALOG, DIALOG_VOLUME, off_entry, text);
        }
        text_draw_in_zone(text_zone + level, g_ui_colors[UI_YELLOW], text);
    }
}

void dialog_volume(void)
{
    char text[TEXT_SIZE];
    if (!gdat_exists(gdat_current(), CAT_DIALOG, DIALOG_VOLUME, GDAT_TYPE_IMAGE, GFX_DIALOG_PICTURE))
        return;
    dialog_draw(DIALOG_VOLUME, MESSAGE_NONE);
    text_draw_in_zone(ZONE_SOUND_LABEL, g_ui_colors[UI_YELLOW],
                      gtext_gdat(CAT_DIALOG, DIALOG_VOLUME, GFX_VOLUME_SOUND_LABEL, text));
    text_draw_in_zone(ZONE_MUSIC_LABEL, g_ui_colors[UI_YELLOW],
                      gtext_gdat(CAT_DIALOG, DIALOG_VOLUME, GFX_VOLUME_MUSIC_LABEL, text));
    for (;;) {
        render_volume_buttons(g_game.sound_volume, ZONE_SOUND_BUTTON, ZONE_SOUND_TEXT, GFX_VOLUME_SOUND_OFF);
        render_volume_buttons(g_game.music_volume, ZONE_MUSIC_BUTTON, ZONE_MUSIC_TEXT, GFX_VOLUME_MUSIC_OFF);
        dialog_present();
        int command = dialog_run(DIALOG_VOLUME);
        if (command == CMD_DIALOG_BUTTON_FIRST || g_main_loop.quit)
            return;
        if (command >= CMD_MUSIC_VOLUME_FIRST && command <= CMD_MUSIC_VOLUME_LAST)
            sound_set_volume_and_test(command - CMD_MUSIC_VOLUME_FIRST, DEVICE_MUSIC);
        else
            sound_set_volume_and_test(command - CMD_SOUND_VOLUME_FIRST, DEVICE_SOUND);
    }
}
