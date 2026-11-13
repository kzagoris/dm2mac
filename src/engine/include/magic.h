/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_ENGINE_MAGIC_H
#define DM2_ENGINE_MAGIC_H
/* Prefixes: magic_ */

/* Magic: the spell table, runes, spell casting, and the magic light. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "champion.h"
#include "timeline.h"

/* A spell of the spell table. */
typedef struct {
    uint32_t runes; /* power rune in bits 24-31 (0: any power), then up to 3 runes, first rune highest */
    uint8_t difficulty;
    uint8_t skill;   /* the skill that casts it and that gains the experience */
    uint16_t effect; /* kind in bits 0-3, type in bits 4-9, action delay factor in bits 10-15 */
} Spell;

/* The result of a cast: success, or why it failed. */
typedef enum { CAST_OK, CAST_FAILED, CAST_UNKNOWN_SPELL, CAST_NEEDS_FLASK } CastResult;

/* Kinds of magic light (magic_add_light). */
#define LIGHT_DARKNESS 0x06
#define LIGHT_TORCH    0x26
#define LIGHT_LIGHT    0x27

/* The power of the MON rune, the strongest power rune. */
#define POWER_MON 6

/* Player-recorded combinations per champion; the last attempted cast follows the four favorites. */
#define MAGIC_FAVORITE_COUNT 4
#define MAGIC_FAVORITE_LAST  MAGIC_FAVORITE_COUNT
#define MAGIC_RECORD_COUNT   (MAGIC_FAVORITE_COUNT + 1)

/* Clears recorded combinations and the known spells for a new adventure or an old save. */
void magic_reset_favorites(void);
/* Records the currently entered runes in a favorite slot; false for invalid/incomplete input. */
bool magic_record_favorite(int16_t caster, int16_t slot);
/* Recorded glyph at step, or 0 for an absent/invalid entry. */
uint8_t magic_favorite_rune(int16_t caster, int16_t slot, int step);
/* Total rune-entry mana at the chosen power; -1 for no combination or invalid power. */
int magic_favorite_mana(int16_t caster, int16_t slot, int power);
/* Writes the versioned optional FAVS save extension and the KNOW block of the known spells after it. */
bool magic_write_favorites(FILE *f);
/* Reads an optional FAVS extension, alone or with the KNOW block after it, at EOF; rejects malformed/unknown data and
   clears old entries. */
bool magic_read_favorites(FILE *f);

/* Spell lookup */
/* The spell of the entered runes (a power rune and 1 to 3 runes), NULL when no spell has them. */
const Spell *magic_find_spell(const uint8_t *runes); /* exported for the tests */
/* The spell at index of the spell table, NULL after the last spell. */
const Spell *magic_get_spell(int index);
/* The rune glyph of a spell at step 0 (the power rune; 0 for any power) to 3 (0 after the last rune). */
uint8_t magic_get_spell_rune(const Spell *spell, int step);
/* The port name of a spell, for example "FIREBALL"; the game files have no spell names. */
const char *magic_get_spell_name(const Spell *spell);
/* The lower-case name of a rune (its glyph - SPELL_RUNE_GLYPH_FIRST: the power runes, then the three rune sets), for
   example "ful"; NULL for a value outside the runes. */
const char *magic_get_rune_name(int rune);

/* Known spells */
/* Whether a cast of entered runes found spell in the advanced mode of this adventure, also when the cast then failed. */
bool magic_is_spell_known(const Spell *spell);
/* Makes spell known in the advanced mode; exported for the tests. */
void magic_learn_spell(const Spell *spell);
/* The mana that the power rune of power 1 to 6 and the runes of spell cost; -1 for another power. */
int magic_spell_mana(const Spell *spell, int power);

/* Light and auras */
/* Adds magic light (or darkness) of a kind and power; it fades through timeline events. */
void magic_add_light(int kind, int power);
/* A magic light event: the light fades one level. */
void magic_event_light(TimelineEvent *e);
/* Gives the living champions of champion_mask aura type with value for duration game ticks. As in the original, the loop
   over the champions runs over all CHAMPION_MAX slots, also those that hold no champion. */
void magic_add_party_aura(int champion_mask, int type, int value, uint16_t duration);
/* A party aura from champion c; pay: it costs 4 mana (less mana halves it). Returns false when it was not paid in full. */
bool magic_cast_party_aura(Champion *c, int type, int power, bool pay);

/* Casting */
/* Champion champion_index shoots the missile thing with power for cost mana; returns false without the mana. */
bool magic_cast_projectile_spell(int16_t champion_index, uint16_t thing, int power, int cost);
/* Champion champion_index casts spell with power 1 to 6; returns the cast result. */
CastResult magic_cast_spell(int16_t champion_index, const Spell *spell, int power); /* exported for the tests */
/* Champion champion_index casts spell from the cheat screen at the power of its power rune (POWER_MON for any power), without
   the failure roll and without experience; a potion still needs an empty flask. Returns the cast result. */
CastResult magic_cast_cheat_spell(int16_t champion_index, const Spell *spell);
/* The champion of the open spell panel casts the entered runes; returns true when the cast failed. */
bool magic_cast_entered_runes_failed(void);

/* Runes */
/* The mana that rune (0 to 5) of the next rune set costs champion champion_index now. */
uint16_t magic_rune_mana_cost(int16_t champion_index, int rune);
/* Adds rune 0 to 5 of the next rune set to the runes of the open spell panel when the mana suffices. */
void magic_add_rune(int rune);
/* The champion who casts takes back the last rune. */
void magic_remove_last_rune(void);

/* Whether the caster has an empty flask in either hand. */
bool magic_has_empty_flask(int16_t caster);
/* Whether the recorded spell needs a flask at its recorded power. */
bool magic_favorite_needs_flask(int16_t caster, int16_t slot);
/* Enters recorded runes through magic_add_rune; rejects dead/busy casters, existing runes, and insufficient mana. */
bool magic_prepare_favorite(int16_t caster, int16_t slot, int power);
/* Whether spell makes a potion and so needs an empty flask in a hand of its caster. */
bool magic_spell_needs_flask(const Spell *spell);
/* Enters the power rune of power and the runes of spell through magic_add_rune, as magic_prepare_favorite. */
bool magic_prepare_spell(int16_t caster, const Spell *spell, int power);

#endif /* DM2_ENGINE_MAGIC_H */
