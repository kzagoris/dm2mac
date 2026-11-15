/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_UI_MUSIC_H
#define DM2_UI_MUSIC_H
/* Prefixes: music_ */

/* Game music: the song of each map from SONGLIST.DAT, played by the HMP player on a music driver (the FM driver
   unless another one is selected). */

#include <stdbool.h>
#include <stdint.h>
#include "gdat.h"
#include "hmp.h"

/* The output rate and channels of the music, and the size of SONGLIST.DAT. */
#define MUSIC_SAMPLE_RATE 44100
#define MUSIC_CHANNELS    2  /* stereo */
#define SONGLIST_MAX      63 /* the game reads at most 63 bytes of SONGLIST.DAT */

/* The song number of no song. */
#define MUSIC_NO_SONG 0xff

/* A music driver: it plays the MIDI messages of a song and renders their sound. */
typedef struct {
    enum HmpDevice device;                    /* the tracks of a song that the driver gets */
    HmpSend send;                             /* plays a 3-byte MIDI channel message */
    void (*render)(int16_t *out, int frames); /* renders frames of stereo samples at MUSIC_SAMPLE_RATE */
} MusicDriver;

typedef struct {
    bool enabled; /* the music driver can play: the FM driver has its banks */
    bool songlist_loaded;
    uint8_t songlist[SONGLIST_MAX]; /* song number of each map */
    int songlist_size;
    int16_t fade; /* game steps left of the fade-out before the next song; 0: no fade */
    bool paused;
    bool playing;
    bool started;
    int16_t requested_song;          /* last song asked for, -1 at start */
    int16_t current_song, next_song; /* SONGLIST.DAT values; MUSIC_NO_SONG at start */
    int16_t volume;                  /* music volume setting, 0 to 7 */
} Music;

/* The state of the music player. */
extern Music g_music; /* exported for the tests */

/* Setup */
/* Stops the song and selects the driver of the next songs; NULL selects the FM driver. Call it before music_init. */
void music_set_driver(const MusicDriver *driver);
/* Loads SONGLIST.DAT from data_dir and, for the FM driver, the FM banks; takes songs from g, and sets volume 5.
   Returns false when the FM driver plays and MELODIC.BNK or DRUM.BNK is missing or bad; music stays off. */
bool music_init(const Gdat *g, const char *data_dir);
/* Stops the song and frees the FM banks. */
void music_quit(void);

/* Songs */
/* Starts song (the HMP item of CAT_MUSIC with index song); song 0 starts nothing. */
void music_play_song(int16_t song); /* exported for the tests */
/* Pauses the song until the next music_update_for_map. */
void music_pause(void);
/* Once per game step: resumes a paused song, steps the fade, and changes to the song of map. */
void music_update_for_map(int16_t map);
/* Music volume 0 to 7; 0 stops the song. */
void music_set_volume(int level);

/* Output */
/* Renders frames of stereo samples at MUSIC_SAMPLE_RATE and runs the song timer. */
void music_render(int16_t *out, int frames);

#endif /* DM2_UI_MUSIC_H */
