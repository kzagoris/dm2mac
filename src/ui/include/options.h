/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_OPTIONS_H
#define DM2_UI_OPTIONS_H
/* Prefixes: options_ */

/* Ordinary convenience options and reopenable help, separate from cheats. */

#include <stdint.h>

typedef enum {
    OPTIONS_STAY,
    OPTIONS_CLOSE,
    OPTIONS_QUICK_SAVE,
    OPTIONS_QUICK_LOAD,
    OPTIONS_CHEATS,
    OPTIONS_SPELLBOOK
} OptionsResult;

/* Opens the options list with Help selected. */
void options_open(void);
/* Handles an overlay event; returns the requested action to the overlay owner. */
OptionsResult options_event(int type, int16_t x, int16_t y);

#endif /* DM2_UI_OPTIONS_H */
