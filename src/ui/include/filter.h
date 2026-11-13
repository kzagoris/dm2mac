/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_FILTER_H
#define DM2_UI_FILTER_H
/* Prefixes: upscale_ */

/* The screen filter of the window (not in the original game): the filter at the start, the filter in use, and the
   change from the SCREEN FILTER row of the cheat screen. The 4:3 picture and the scanlines of the window are here
   too. A change shows at once and goes to SETTINGS.TXT in the save folder. The prefix upscale_ is the module
   prefix: the first word filter_ is in use in engine/. */

#include <stdbool.h>
#include "settings.h"

/* The platform sets it: shows the filter in the window; false when it cannot. NULL: no window. */
extern bool (*g_upscale_filter_hook)(enum SettingsFilter filter);
/* The platform sets it: shows the 4:3 picture and the scanlines in the window; false when it cannot. NULL: no
   window. */
extern bool (*g_upscale_picture_hook)(bool aspect, bool scanlines);

/* Setup */
/* Starts the filter state at the beginning of the program, after the game mode is chosen. filter is the value
   that --filter or DM2_FILTER gave, or SETTINGS_FILTER_UNSET for none given: the advanced mode then takes the
   filter of SETTINGS.TXT. The advanced mode takes the 4:3 picture and the scanlines of SETTINGS.TXT. */
void upscale_start(enum SettingsFilter filter);

/* Queries */
/* The filter in use. */
enum SettingsFilter upscale_get_filter(void);
/* True when the picture is 4:3 instead of square pixels. */
bool upscale_has_aspect(void);
/* True when the window shows scanlines. */
bool upscale_has_scanlines(void);

/* Commands */
/* Changes to the next filter (NONE, SCALE2X, SCALE3X, XBRZ2X, XBRZ3X, XBRZ4X, then NONE), shows it in the window, and writes it to
   SETTINGS.TXT. When the window cannot show the filter, the filter does not change; a failed write says
   UNABLE TO SAVE OPTIONS once per failure run and keeps the filter. */
void upscale_cycle_filter(void);
/* Turns the 4:3 picture on or off, shows it in the window, and writes it to SETTINGS.TXT, as upscale_cycle_filter. */
void upscale_toggle_aspect(void);
/* Turns the scanlines on or off, shows them in the window, and writes them to SETTINGS.TXT, as
   upscale_cycle_filter. */
void upscale_toggle_scanlines(void);
/* Toggles smooth walking and saves the preference. */
void upscale_toggle_stepping(void);

#endif /* DM2_UI_FILTER_H */
