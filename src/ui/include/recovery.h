/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_RECOVERY_H
#define DM2_UI_RECOVERY_H
/* Prefixes: recovery_ */

/* Explicit Advanced startup and death recovery. Never writes a save or backs up an absent game. */

#include <stdbool.h>
#include <stdint.h>
#include "advanced.h"

/* Chooser letter keys. */
#define KEY_L 0x26
#define KEY_N 0x31

typedef enum { RECOVERY_WAIT, RECOVERY_QUICK, RECOVERY_SLOTS, RECOVERY_EXIT, RECOVERY_NEW } RecoveryChoice;

/* Opens the chooser; death omits the new game choice. */
void recovery_open(bool death); /* exported for the tests */
/* Whether the chooser owns input. */
bool recovery_is_open(void);
/* The requested action, or RECOVERY_WAIT. */
RecoveryChoice recovery_choice(void); /* exported for the tests */
/* Handles a key or click on the chooser. */
void recovery_event(int type, int16_t x, int16_t y);
/* Loads QUICK.DAT without backing up a running game; only for startup or death. */
enum QuickLoadResult recovery_load_quick(void); /* exported for the tests */
/* Runs the chooser and loads a game; false for exit. */
bool recovery_run(bool death);

#endif /* DM2_UI_RECOVERY_H */
