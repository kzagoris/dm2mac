/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "recovery.h"
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "mode_screen.h"
#include "music.h"
#include "os.h"
#include "play.h"
#include "session.h"
#include "sound.h"
#include "text.h"
#include "video.h"

/* Full-screen chooser rows, including the error line. */
#define RECOVERY_LEFT      12
#define RECOVERY_TOP       60
#define RECOVERY_STEP      18
#define RECOVERY_LINES     6
#define RECOVERY_QUICK_ROW 1
#define RECOVERY_SLOTS_ROW 2
#define RECOVERY_NEW_ROW   3
#define RECOVERY_EXIT_ROW  4

static bool active, after_death;
static RecoveryChoice choice;
static const char *error;
static uint8_t kept_palette[PALETTE_COLORS][RGB_CHANNELS];

/* Draws the identity of each source and the last error without leaving the chooser. */
static void show_choices(void)
{
    const char *lines[RECOVERY_LINES] = {"ADVANCED - RECOVERY",
                                         "F9  LOAD QUICK.DAT",
                                         "L   LOAD SLOT SAVE",
                                         after_death ? "" : "N   NEW GAME",
                                         "ESC EXIT",
                                         error};
    mode_screen_draw();
    memset(g_video_fb, 0, sizeof g_video_fb);
    for (int row = 0; row < RECOVERY_LINES; row++)
        text_draw_on_screen(RECOVERY_LEFT, RECOVERY_TOP + row * RECOVERY_STEP, 1, 0, lines[row]);
}

void recovery_open(bool death)
{
    memcpy(kept_palette, g_video_palette, sizeof kept_palette);
    active = true;
    after_death = death;
    choice = RECOVERY_WAIT;
    error = "";
    input_flush();
    show_choices();
}

bool recovery_is_open(void)
{
    return active;
}

RecoveryChoice recovery_choice(void)
{
    return choice;
}

void recovery_event(int type, int16_t x, int16_t y)
{
    if (type == INPUT_KEY) {
        if (x == KEY_QUICK_LOAD)
            choice = RECOVERY_QUICK;
        else if (x == KEY_L)
            choice = RECOVERY_SLOTS;
        else if (x == KEY_N && !after_death)
            choice = RECOVERY_NEW;
        else if (x == SCAN_ESCAPE)
            choice = RECOVERY_EXIT;
    } else if (type == INPUT_BUTTON_LEFT && x >= RECOVERY_LEFT && y >= RECOVERY_TOP - TEXT_LINE_HEIGHT) {
        int row = (y - RECOVERY_TOP + TEXT_LINE_HEIGHT) / RECOVERY_STEP;
        if (row == RECOVERY_QUICK_ROW)
            choice = RECOVERY_QUICK;
        else if (row == RECOVERY_SLOTS_ROW)
            choice = RECOVERY_SLOTS;
        else if (row == RECOVERY_NEW_ROW && !after_death)
            choice = RECOVERY_NEW;
        else if (row == RECOVERY_EXIT_ROW)
            choice = RECOVERY_EXIT;
    }
}

enum QuickLoadResult recovery_load_quick(void)
{
    char path[OS_PATH_SIZE];
    if (g_session.save_dir[0])
        snprintf(path, sizeof path, "%s/QUICK.DAT", g_session.save_dir);
    else
        snprintf(path, sizeof path, "%s/DATA/QUICK.DAT", g_session.data_dir);
    FILE *f = fopen(path, "rb");
    if (!f)
        return QUICK_MISSING;
    fclose(f);
    game_set_session_loaded(false);
    game_set_new_game(0);
    if (!session_start_save(path))
        return QUICK_DAMAGED;
    game_clear_save_times();
    sound_set_volume(g_game.sound_volume);
    music_set_volume(g_game.music_volume);
    return QUICK_LOADED;
}

bool recovery_run(bool death)
{
    recovery_open(death);
    while (!g_main_loop.quit) {
        while (choice == RECOVERY_WAIT && !g_main_loop.quit)
            play_wait_frames(1);
        RecoveryChoice requested = choice;
        active = false;
        input_flush();
        memcpy(g_video_palette, kept_palette, sizeof kept_palette);
        if (g_main_loop.quit || requested == RECOVERY_EXIT)
            break;
        if (requested == RECOVERY_QUICK) {
            enum QuickLoadResult result = recovery_load_quick();
            if (result == QUICK_LOADED)
                return true;
            error = result == QUICK_MISSING ? "QUICK.DAT MISSING OR UNREADABLE" : "QUICK.DAT DAMAGED - CHOOSE AGAIN";
        } else if (requested == RECOVERY_NEW) {
            if (session_start_new_game())
                return true;
            error = "UNABLE TO START NEW GAME";
        } else if (requested == RECOVERY_SLOTS) {
            if (session_recover_slot())
                return true;
            error = "SLOT NOT LOADED - CHOOSE AGAIN";
        }
        active = true;
        choice = RECOVERY_WAIT;
        show_choices();
    }
    active = false;
    memcpy(g_video_palette, kept_palette, sizeof kept_palette);
    return false;
}
