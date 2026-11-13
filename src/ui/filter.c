/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter.h"
#include <stdio.h>
#include "game.h"
#include "game_text.h"
#include "text.h"

/* The filters, from SETTINGS_FILTER_NONE. */
#define FILTER_COUNT (SETTINGS_FILTER_SCALE3X - SETTINGS_FILTER_NONE + 1)

bool (*g_upscale_filter_hook)(enum SettingsFilter filter);
bool (*g_upscale_picture_hook)(bool aspect, bool scanlines);

static struct {
    Settings shown;   /* the filter, the 4:3 picture and the scanlines in use */
    Settings saved;   /* the values of SETTINGS.TXT, or in use at the start when the file holds none */
    bool save_failed; /* the last write failed and its message shows */
} upscale = {.shown = {.filter = SETTINGS_FILTER_NONE}, .saved = {.filter = SETTINGS_FILTER_NONE}};

/* The folder of SETTINGS.TXT: the --saves folder, else the DATA folder of the game (as QUICK.DAT). */
static char *find_folder(char *folder, size_t size)
{
    if (g_session.save_dir[0])
        snprintf(folder, size, "%s", g_session.save_dir);
    else
        snprintf(folder, size, "%s/DATA", g_session.data_dir);
    return folder;
}

/* Setup */

void upscale_start(enum SettingsFilter filter)
{
    char dir[OS_DIR_SIZE + sizeof "/DATA"];
    upscale.save_failed = false;
    upscale.shown = (Settings){.filter = SETTINGS_FILTER_NONE};
    if (g_game_mode == GAME_MODE_ADVANCED)
        upscale.shown = settings_load(find_folder(dir, sizeof dir));
    if (filter != SETTINGS_FILTER_UNSET)
        upscale.shown.filter = filter; /* the window opened with it */
    else if (g_upscale_filter_hook && upscale.shown.filter != SETTINGS_FILTER_NONE &&
             !g_upscale_filter_hook(upscale.shown.filter))
        upscale.shown.filter = SETTINGS_FILTER_NONE;
    if (g_upscale_picture_hook && (upscale.shown.aspect || upscale.shown.scanlines) &&
        !g_upscale_picture_hook(upscale.shown.aspect, upscale.shown.scanlines)) {
        upscale.shown.aspect = false;
        upscale.shown.scanlines = false;
    }
    upscale.saved = upscale.shown;
}

/* Queries */

enum SettingsFilter upscale_get_filter(void)
{
    return upscale.shown.filter;
}

bool upscale_has_aspect(void)
{
    return upscale.shown.aspect;
}

bool upscale_has_scanlines(void)
{
    return upscale.shown.scanlines;
}

/* Commands */

/* Writes SETTINGS.TXT when a value differs from the file; a failed write says so once per failure run. */
static void write_change(void)
{
    char dir[OS_DIR_SIZE + sizeof "/DATA"];
    if (upscale.shown.filter == upscale.saved.filter && upscale.shown.aspect == upscale.saved.aspect &&
        upscale.shown.scanlines == upscale.saved.scanlines)
        return;
    if (settings_save(find_folder(dir, sizeof dir), &upscale.shown)) {
        upscale.saved = upscale.shown;
        upscale.save_failed = false;
    } else if (!upscale.save_failed) {
        upscale.save_failed = true;
        text_print_message(UI_WHITE, "\nUNABLE TO SAVE OPTIONS");
    }
}

void upscale_cycle_filter(void)
{
    enum SettingsFilter filter =
        (enum SettingsFilter)((upscale.shown.filter - SETTINGS_FILTER_NONE + 1) % FILTER_COUNT + SETTINGS_FILTER_NONE);
    if (g_upscale_filter_hook && !g_upscale_filter_hook(filter))
        return;
    upscale.shown.filter = filter;
    write_change();
}

/* Shows the 4:3 picture and the scanlines in the window and keeps them; no change when the window cannot. */
static void change_picture(bool aspect, bool scanlines)
{
    if (g_upscale_picture_hook && !g_upscale_picture_hook(aspect, scanlines))
        return;
    upscale.shown.aspect = aspect;
    upscale.shown.scanlines = scanlines;
    write_change();
}

void upscale_toggle_aspect(void)
{
    change_picture(!upscale.shown.aspect, upscale.shown.scanlines);
}

void upscale_toggle_scanlines(void)
{
    change_picture(upscale.shown.aspect, !upscale.shown.scanlines);
}
