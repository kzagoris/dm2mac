/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_DATA_SETTINGS_H
#define DM2_DATA_SETTINGS_H
/* Prefixes: settings_ */

/* The settings file of the advanced mode (SETTINGS.TXT in the saves folder): the upscale filter as the text line
   filter=none|scale2x|scale3x|xbrz2x|xbrz3x|xbrz4x, the 4:3 picture as aspect=on|off, the scanlines as scanlines=on|off,
   and the spells of the spell list as spells=known|all. A player preference, not game state: the save file does not hold it. It reads and writes the file; it does not apply the
   values. */

#include <stdbool.h>
#include <stddef.h>

/* The settings file. */
#define SETTINGS_FILE     "SETTINGS.TXT"
#define SETTINGS_MAX_SIZE 1024 /* a larger file gives the defaults */

/* The upscale filters; settings_filter_factor gives their output scale. SETTINGS_FILTER_UNSET is not a
   filter: no filter given, or an unknown name. */
enum SettingsFilter {
    SETTINGS_FILTER_UNSET = 0,
    SETTINGS_FILTER_NONE = 1,
    SETTINGS_FILTER_SCALE2X = 2,
    SETTINGS_FILTER_SCALE3X = 3,
    SETTINGS_FILTER_XBRZ2X = 4,
    SETTINGS_FILTER_XBRZ3X = 5,
    SETTINGS_FILTER_XBRZ4X = 6
};

/* The values of the settings file. */
typedef struct {
    enum SettingsFilter filter;
    bool aspect;    /* the picture is 4:3, as on a VGA monitor, instead of square pixels */
    bool scanlines; /* a dark line below every line of the game screen */
    bool all_spells; /* the spell list and the rune hints show every spell, not only the known spells */
} Settings;

/* Queries */
/* The lower-case name of filter in SETTINGS.TXT, NULL for a value that is not a filter. */
const char *settings_filter_name(enum SettingsFilter filter);
/* The output pixels per input pixel on each axis, or 0 for an unknown filter. */
int settings_filter_factor(enum SettingsFilter filter);
/* The filter of a lower-case name, or SETTINGS_FILTER_UNSET for an unknown name. */
enum SettingsFilter settings_filter_parse(const char *name);
/* The values of the settings text: a missing line or a bad value gives the default (no filter, off, the known spells), an unknown key and a
   line of 64 characters or more are skipped, a text above SETTINGS_MAX_SIZE gives all defaults;
   exported for the tests. */
Settings settings_parse(const char *file_text, size_t length);
/* The values of the settings file in folder, as settings_parse; a missing file gives the defaults. */
Settings settings_load(const char *folder);

/* Commands */
/* Writes the values to the settings file in folder through SETTINGS.TMP. False when the write fails; the old file then stays. */
bool settings_save(const char *folder, const Settings *settings);

#endif /* DM2_DATA_SETTINGS_H */
