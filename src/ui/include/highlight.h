/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_HIGHLIGHT_H
#define DM2_UI_HIGHLIGHT_H
/* Prefixes: highlight_ */

/* The touch highlight of the advanced mode: while Ctrl is held, outlines mark what a hand click in the view reaches. */

#include <stdbool.h>

/* The platform tells whether Ctrl is held; only the advanced mode shows the highlight. */
void highlight_set_held(bool held);
/* True when the shown highlight differs from the held key while the view of the party shows, so that the viewport
   needs a new present. */
bool highlight_is_due(void);
/* Draws the outlines on the screen after the dungeon view has been copied there, when Ctrl is held. */
void highlight_draw(void);

#endif /* DM2_UI_HIGHLIGHT_H */
