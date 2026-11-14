/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_COMBAT_FEEDBACK_H
#define DM2_UI_COMBAT_FEEDBACK_H
/* Prefixes: combat_ */

/* Temporary damage stars over creatures, drawn after the dungeon without changing its targeting pixels. */

#include "dungeon.h"

/* Clears the hit feedback when a game starts or loads. */
void combat_reset_feedback(void);
/* Records a resolved melee or projectile hit on the current map, including a zero-damage contact. */
void combat_show_hit(uint16_t group, Location at, int damage);
/* Draws active damage stars on the screen after the dungeon view has been copied there. */
void combat_draw_feedback(void);

#endif /* DM2_UI_COMBAT_FEEDBACK_H */
