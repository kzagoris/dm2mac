/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "os.h"

/* The text of the settings file. */
#define LINE_SIZE      64  /* longer lines are unreadable */
#define FILE_TEXT_SIZE 128 /* the lines that settings_save writes */
#define TEMPORARY_FILE "SETTINGS.TMP"
#define SWITCH_ON      "on"
#define SWITCH_OFF     "off"
#define SPELLS_ALL     "all"
#define SPELLS_KNOWN   "known"

/* Output factors above double resolution. */
#define UPSCALE_3X 3
#define UPSCALE_4X 4

/* The switches of the settings file besides the filter. */
#define SWITCH_COUNT 5

/* A switch of the settings file: its key, the field of Settings that it sets, and its value for on and for off. */
typedef struct {
    const char *key;
    size_t field; /* offsetof the bool in Settings */
    const char *on, *off;
} SettingsSwitch;

static const char *const filter_names[] = {"none",   "scale2x", "scale3x",
                                           "xbrz2x", "xbrz3x",  "xbrz4x"}; /* from SETTINGS_FILTER_NONE */

/* Output pixels per source pixel, in the same order as filter_names. */
static const int filter_factors[] = {1, 2, UPSCALE_3X, 2, UPSCALE_3X, UPSCALE_4X};

/* The switches in the order of the lines that settings_save writes after the filter line. */
static const SettingsSwitch switches[SWITCH_COUNT] = {
    {"aspect",        offsetof(Settings, aspect),        SWITCH_ON,  SWITCH_OFF  },
    {"scanlines",     offsetof(Settings, scanlines),     SWITCH_ON,  SWITCH_OFF  },
    {"classic_steps", offsetof(Settings, classic_steps), SWITCH_ON,  SWITCH_OFF  },
    {"spells",        offsetof(Settings, all_spells),    SPELLS_ALL, SPELLS_KNOWN},
    {"fullscreen",    offsetof(Settings, fullscreen),    SWITCH_ON,  SWITCH_OFF  },
};

/* The field of settings that the switch at index sets. */
static bool *switch_field(Settings *settings, size_t index)
{
    return (bool *)((char *)settings + switches[index].field);
}

/* The value of the switch at index in settings. */
static bool switch_value(const Settings *settings, size_t index)
{
    return *(const bool *)((const char *)settings + switches[index].field);
}

/* Sets the value of one line key=value; an unknown key or a bad value changes nothing. */
static void parse_line(char *line, Settings *settings)
{
    char *value = strchr(line, '=');
    if (!value)
        return;
    *value++ = '\0';
    for (size_t i = 0; i < SWITCH_COUNT; i++)
        if (!strcmp(line, switches[i].key) && (!strcmp(value, switches[i].on) || !strcmp(value, switches[i].off)))
            *switch_field(settings, i) = !strcmp(value, switches[i].on);
    if (strcmp(line, "filter"))
        return;
    enum SettingsFilter filter = settings_filter_parse(value);
    if (filter != SETTINGS_FILTER_UNSET)
        settings->filter = filter;
}

/* Queries */

const char *settings_filter_name(enum SettingsFilter filter)
{
    if (filter < SETTINGS_FILTER_NONE || filter > SETTINGS_FILTER_XBRZ4X)
        return NULL;
    return filter_names[filter - SETTINGS_FILTER_NONE];
}

int settings_filter_factor(enum SettingsFilter filter)
{
    if (!settings_filter_name(filter))
        return 0;
    return filter_factors[filter - SETTINGS_FILTER_NONE];
}

bool settings_equal(const Settings *a, const Settings *b)
{
    if (a->filter != b->filter)
        return false;
    for (size_t i = 0; i < SWITCH_COUNT; i++)
        if (switch_value(a, i) != switch_value(b, i))
            return false;
    return true;
}

enum SettingsFilter settings_filter_parse(const char *name)
{
    for (enum SettingsFilter filter = SETTINGS_FILTER_NONE; filter <= SETTINGS_FILTER_XBRZ4X; filter++)
        if (!strcmp(name, filter_names[filter - SETTINGS_FILTER_NONE]))
            return filter;
    return SETTINGS_FILTER_UNSET;
}

Settings settings_parse(const char *file_text, size_t length)
{
    Settings settings = {.filter = SETTINGS_FILTER_NONE};
    if (length > SETTINGS_MAX_SIZE)
        return settings;
    char line[LINE_SIZE];
    size_t used = 0;
    bool too_long = false;
    for (size_t i = 0; i <= length; i++) {
        char c = i < length ? file_text[i] : '\n'; /* the last line needs no final newline */
        if (c != '\n') {
            too_long |= used + 1 >= sizeof line;
            if (!too_long && c != '\r')
                line[used++] = c;
            continue;
        }
        line[used] = '\0';
        if (!too_long)
            parse_line(line, &settings);
        used = 0;
        too_long = false;
    }
    return settings;
}

Settings settings_load(const char *folder)
{
    char path[OS_PATH_SIZE], text[SETTINGS_MAX_SIZE + 1];
    size_t length = 0;
    if (snprintf(path, sizeof path, "%s/%s", folder, SETTINGS_FILE) < (int)sizeof path) {
        FILE *file = fopen(path, "rb");
        if (file) {
            length = fread(text, 1, sizeof text, file); /* one byte above the limit tells a file that is too large */
            fclose(file);
        }
    }
    return settings_parse(text, length);
}

/* Commands */

bool settings_save(const char *folder, const Settings *settings)
{
    char path[OS_PATH_SIZE], temporary[OS_PATH_SIZE], text[FILE_TEXT_SIZE];
    if (snprintf(path, sizeof path, "%s/%s", folder, SETTINGS_FILE) >= (int)sizeof path ||
        snprintf(temporary, sizeof temporary, "%s/%s", folder, TEMPORARY_FILE) >= (int)sizeof temporary ||
        !settings_filter_name(settings->filter))
        return false;
    size_t length = (size_t)snprintf(text, sizeof text, "filter=%s\n", settings_filter_name(settings->filter));
    for (size_t i = 0; i < SWITCH_COUNT && length < sizeof text; i++)
        length += (size_t)snprintf(text + length, sizeof text - length, "%s=%s\n", switches[i].key,
                                   switch_value(settings, i) ? switches[i].on : switches[i].off);
    if (length >= sizeof text)
        return false;
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return false;
    bool ok = fwrite(text, 1, length, file) == length;
    ok = fclose(file) == 0 && ok;
    if (ok && rename(temporary, path)) {
        remove(path); /* Windows does not replace a file by a rename */
        ok = !rename(temporary, path);
    }
    if (!ok)
        remove(temporary);
    return ok;
}
