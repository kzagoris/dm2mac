/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_PLATFORM_SC55_H
#define DM2_PLATFORM_SC55_H
/* Prefixes: sc55_ */

/* The Roland SC-55 music driver: runs the SC-55 emulator Nuked-SC55, a CLAP plugin of DOSBox Staging, on its own
   thread and gives its sound to the music player. It plays the MIDI messages it gets and knows no song. */

#include "music.h"

/* Where the driver looks for the plugin file and the ROM folder; NULL looks in the folders of DOSBox Staging. */
typedef struct {
    const char *plugin;
    const char *roms;
} Sc55Paths;

/* Setup */
/* Loads the plugin with the first SC-55 model that its ROMs allow and starts its render thread. Returns NULL with a
   message when the plugin, the ROMs or the audio stream is missing. */
const MusicDriver *sc55_open(Sc55Paths paths);
/* Stops the render thread and unloads the plugin; nothing without an open driver. */
void sc55_close(void);

#endif /* DM2_PLATFORM_SC55_H */
