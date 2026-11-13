/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_SPELLBOOK_H
#define DM2_UI_SPELLBOOK_H
/* Prefixes: spellbook_ */

/* Player-recorded rune combinations; never exposes the cheat spell catalog. */

#include <stdint.h>

typedef enum { SPELLBOOK_STAY, SPELLBOOK_CLOSE, SPELLBOOK_CAST } SpellbookResult;

/* Opens the current caster's favorites, or the first living champion's. */
void spellbook_open(void);
/* Handles keyboard and mouse navigation; CAST means run the normal entered-rune cast after closing the overlay. */
SpellbookResult spellbook_event(int type, int16_t x, int16_t y);

#endif /* DM2_UI_SPELLBOOK_H */
