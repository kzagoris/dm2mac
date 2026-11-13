/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_CHEATS_H
#define DM2_UI_CHEATS_H
/* Prefixes: cheats_ */

/* Optional rule-changing cheats and the explicitly free-cast spell catalog. Ordinary preferences and save rows
   live in options.h. advanced.h owns opening/closing and the F5/F9 keyboard shortcuts. */

#include <stddef.h>
#include <stdint.h>
#include "magic.h"

/* The cheat page; the spell pages count from 1. */
#define CHEATS_PAGE 0

/* What a row or a click on the cheat screen asks for. */
enum CheatsResult {
    CHEATS_STAY,       /* the screen stays open */
    CHEATS_CLOSE,      /* the screen closes */
    CHEATS_QUICK_SAVE, /* a quick save; the screen stays open */
    CHEATS_QUICK_LOAD  /* the screen closes, then a quick load */
};

/* The shown page and its selected row. */
typedef struct {
    int16_t page, row;
} CheatsCursor;

/* Queries */
/* The text line of a spell: the runes as names, as numbers (X for any power), then the spell name, for example
   "FUL IR (X,4,4) FIREBALL"; a line wider than a row has no rune names. */
void cheats_get_spell_line(const Spell *spell, char *line, size_t size); /* exported for the tests */
/* The shown page (CHEATS_PAGE or a spell page) and its selected row. */
CheatsCursor cheats_get_cursor(void); /* exported for the tests */

/* Commands */
/* Shows the cheat page in the viewport with the first row selected. */
void cheats_open(void);
/* Selects the row step rows below (step 1) or above (step -1); stops at the ends, with CLOSE last. */
void cheats_move_selection(int16_t step);
/* Runs the selected row: toggles a cheat, navigates spell pages, or free-casts a spell and closes the screen. */
enum CheatsResult cheats_run_selected_row(void);
/* A left click at the screen position (x, y): a click on a row selects and runs it. */
enum CheatsResult cheats_process_click(int16_t x, int16_t y);
/* A right click: a spell page goes back to the cheat page with SPELLS selected; the cheat page closes. */
enum CheatsResult cheats_process_right_click(void);

#endif /* DM2_UI_CHEATS_H */
