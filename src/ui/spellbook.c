/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "spellbook.h"
#include <stdio.h>
#include "action.h"
#include "advanced.h"
#include "champion.h"
#include "magic.h"
#include "rows.h"

/* Selectable rows and informational rows of the spellbook. */
#define BOOK_CASTER    0
#define BOOK_POWER     1
#define BOOK_FIRST     2
#define BOOK_MANA      (BOOK_FIRST + MAGIC_RECORD_COUNT)
#define BOOK_RECORD    (BOOK_MANA + 1)
#define BOOK_CAST      (BOOK_RECORD + 1)
#define BOOK_STATUS    (BOOK_CAST + 1)
#define BOOK_ROWS      (BOOK_STATUS + 1)
#define BOOK_SUBTITLES ((1u << BOOK_MANA) | (1u << BOOK_STATUS))
#define BOOK_TEXT_SIZE 48

static int16_t caster, selected, power, row;
static const char *status;

/* Draws only the player's recorded combinations and the selected combination's costs. */
static void show_book(void)
{
    char texts[BOOK_ROWS][BOOK_TEXT_SIZE];
    const char *lines[BOOK_ROWS];
    for (int i = 0; i < BOOK_ROWS; i++)
        lines[i] = texts[i];
    const Champion *c = caster >= 0 ? champion(caster) : NULL;
    snprintf(texts[BOOK_CASTER], BOOK_TEXT_SIZE, "CASTER %d %.7s - CHANGE", caster + 1, c ? c->name : "NONE");
    snprintf(texts[BOOK_POWER], BOOK_TEXT_SIZE, "SPELL POWER %d/6 - CHANGE", power);
    for (int slot = 0; slot < MAGIC_RECORD_COUNT; slot++) {
        char runes[CHAMPION_RUNES_SIZE] = "---";
        for (int step = 1; step < CHAMPION_RUNES_SIZE; step++) {
            uint8_t glyph = magic_favorite_rune(caster, slot, step);
            runes[step - 1] = glyph ? (char)('1' + (glyph - SPELL_RUNE_GLYPH_FIRST) % SPELL_RUNES_PER_SET) : '-';
        }
        char label[24];
        if (slot == MAGIC_FAVORITE_LAST)
            snprintf(label, sizeof(label), "LAST ATTEMPT");
        else
            snprintf(label, sizeof(label), "FAVORITE %d", slot + 1);
        snprintf(texts[BOOK_FIRST + slot], BOOK_TEXT_SIZE, "%c %s: %s", selected == slot ? '>' : ' ',
                 label, magic_favorite_rune(caster, slot, 1) ? runes : "EMPTY");
    }
    int mana = magic_favorite_mana(caster, selected, power);
    if (mana < 0)
        snprintf(texts[BOOK_MANA], BOOK_TEXT_SIZE, "ENTER RUNES IN SPELL PANEL FIRST");
    else
        snprintf(texts[BOOK_MANA], BOOK_TEXT_SIZE, "MANA %d/%d - %s", mana, c ? c->mana : 0,
                 magic_favorite_needs_flask(caster, selected)
                     ? (magic_has_empty_flask(caster) ? "FLASK READY" : "FLASK MISSING")
                     : "NO FLASK NEEDED");
    if (selected == MAGIC_FAVORITE_LAST)
        snprintf(texts[BOOK_RECORD], BOOK_TEXT_SIZE, "TO SAVE: SELECT FAVORITE 1-4");
    else
        snprintf(texts[BOOK_RECORD], BOOK_TEXT_SIZE, "SAVE ENTERED RUNES TO FAVORITE %d", selected + 1);
    snprintf(texts[BOOK_CAST], BOOK_TEXT_SIZE, "CAST SELECTED SPELL");
    snprintf(texts[BOOK_STATUS], BOOK_TEXT_SIZE, "%s", status ? status :
             mana < 0 ? "CLOSE, ENTER RUNES, THEN PRESS B" : "SELECT SPELL, CHOOSE POWER, CAST");
    rows_draw(&(RowsScreen){.title = "FAVORITES",
                            .title_item = "CLOSE",
                            .rows = lines,
                            .row_count = BOOK_ROWS,
                            .subtitles = BOOK_SUBTITLES},
              row);
}

void spellbook_open(void)
{
    caster = g_action.champion > 0 && g_action.hand == ACTION_HAND_SPELL ? g_action.champion - 1 : 0;
    if (caster >= g_game.champion_count || !champion(caster)->health) {
        caster = -1;
        for (int16_t i = 0; i < g_game.champion_count; i++)
            if (champion(i)->health) {
                caster = i;
                break;
            }
    }
    selected = 0;
    power = 1;
    row = BOOK_FIRST;
    status = NULL;
    show_book();
}

/* Runs one selected row. Recording does not charge mana or remove entered runes. */
static SpellbookResult run_row(void)
{
    if (row == BOOK_ROWS)
        return SPELLBOOK_CLOSE;
    status = NULL;
    if (row == BOOK_CASTER && g_game.champion_count)
        caster = (int16_t)((caster + 1) % g_game.champion_count);
    else if (row == BOOK_POWER)
        power = (int16_t)(power % POWER_MON + 1);
    else if (row >= BOOK_FIRST && row < BOOK_MANA)
        selected = (int16_t)(row - BOOK_FIRST);
    else if (row == BOOK_RECORD)
        status = selected == MAGIC_FAVORITE_LAST ? "SELECT FAVORITE 1-4 TO SAVE"
                 : magic_record_favorite(caster, selected) ? "SAVED. CLOSE AND CAST ENTERED RUNES"
                                                         : "CLOSE, ENTER RUNES, THEN PRESS B";
    else if (row == BOOK_CAST) {
        if (magic_prepare_favorite(caster, selected, power))
            return SPELLBOOK_CAST;
        const Champion *c = caster >= 0 ? champion(caster) : NULL;
        status = magic_favorite_mana(caster, selected, power) < 0 ? "EMPTY SLOT - SAVE A SPELL FIRST"
                 : !c || !c->health ? "CHOOSE A LIVING CASTER"
                 : c->rune_count ? "CLOSE AND CAST OR REMOVE YOUR RUNES"
                 : c->action_delay[ACTION_SPELL] ? "CASTER BUSY - WAIT AND TRY AGAIN"
                                               : "NOT ENOUGH MANA - LOWER SPELL POWER";
    }
    show_book();
    return SPELLBOOK_STAY;
}

SpellbookResult spellbook_event(int type, int16_t x, int16_t y)
{
    if (type == INPUT_BUTTON_RIGHT || (type == INPUT_KEY && (x == SCAN_ESCAPE || x == KEY_SPELLBOOK)))
        return SPELLBOOK_CLOSE;
    if (type == INPUT_KEY) {
        int step = x == KEYPAD_FORWARD || x == SCAN_W ? -1 : x == KEYPAD_BACK || x == SCAN_S ? 1 : 0;
        if (step) {
            row = (int16_t)(row + step);
            while (row >= 0 && row < BOOK_ROWS && (BOOK_SUBTITLES >> row & 1))
                row = (int16_t)(row + step);
            if (row < 0)
                row = 0;
            if (row > BOOK_ROWS)
                row = BOOK_ROWS;
            show_book();
        } else if (x == KEY_ENTER)
            return run_row();
    } else if (type == INPUT_BUTTON_LEFT) {
        RowsScreen layout = {
            .title_item = "CLOSE", .row_count = BOOK_ROWS, .subtitles = BOOK_SUBTITLES};
        int16_t hit = rows_find_row(&layout, x, y);
        if (hit >= 0) {
            row = hit;
            return run_row();
        }
    }
    return SPELLBOOK_STAY;
}
