/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_SPELLBOOK_H
#define DM2_UI_SPELLBOOK_H
/* Prefixes: spellbook_ */

/* The spell list of the advanced mode: the listed spells by name, cast with the normal rune costs, the player-recorded
   rune combinations, and the hints of the rune panel. The listed spells are the known spells, or every spell when the
   player chose that in the options; it never casts through the cheat spell catalog. */

#include <stdbool.h>
#include <stdint.h>

typedef enum { SPELLBOOK_STAY, SPELLBOOK_CLOSE, SPELLBOOK_CAST } SpellbookResult;

/* Queries */
/* Whether rune 0 to 5 of the next rune set continues the entered runes of the open spell panel to a listed spell;
   false before the power rune and outside the advanced mode. */
bool spellbook_is_next_rune(int rune);

/* Commands */
/* The champion of the open spell panel enters rune 0 to 5 of the next rune set, as magic_add_rune. In the advanced
   mode the message area then names the listed spell that the entered runes are, for example FUL IR: FIREBALL, or
   says that no spell has them when every spell is listed and none begins with them. */
void spellbook_add_rune(int rune);
/* Opens the spell list of the current caster, or of the first living champion, with the last cast spell marked. */
void spellbook_open(void);
/* Handles keyboard and mouse navigation; CAST means run the normal entered-rune cast after closing the overlay. */
SpellbookResult spellbook_event(int type, int16_t x, int16_t y);

#endif /* DM2_UI_SPELLBOOK_H */
