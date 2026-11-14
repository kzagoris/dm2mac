/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "spellbook.h"
#include <ctype.h>
#include <stdio.h>
#include "action.h"
#include "advanced.h"
#include "champion.h"
#include "filter.h"
#include "game.h"
#include "game_text.h"
#include "magic.h"
#include "rows.h"
#include "text.h"

/* Selectable rows and informational rows of the spell list page. */
#define LIST_CASTER    0
#define LIST_POWER     1
#define LIST_FIRST     2
#define LIST_SPELLS    6 /* the spells of a page */
#define LIST_NEXT      (LIST_FIRST + LIST_SPELLS)
#define LIST_FAVORITES (LIST_NEXT + 1)
#define LIST_STATUS    (LIST_FAVORITES + 1)
#define LIST_ROWS      (LIST_STATUS + 1)
_Static_assert(LIST_ROWS <= ROWS_MAX_COUNT, "the spell list fits below the title");

/* Selectable rows and informational rows of the favorites page. */
#define BOOK_CASTER    0
#define BOOK_POWER     1
#define BOOK_FIRST     2
#define BOOK_MANA      (BOOK_FIRST + MAGIC_RECORD_COUNT)
#define BOOK_RECORD    (BOOK_MANA + 1)
#define BOOK_CAST      (BOOK_RECORD + 1)
#define BOOK_STATUS    (BOOK_CAST + 1)
#define BOOK_ROWS      (BOOK_STATUS + 1)
#define BOOK_SUBTITLES ((1u << BOOK_MANA) | (1u << BOOK_STATUS))

/* The texts of a row. */
#define BOOK_TEXT_SIZE  48
#define RUNE_NAMES_SIZE 16 /* three rune names, the spaces between them, and the end */
#define NAME_COLUMNS    17 /* the longest spell name */
#define SLOT_LABEL_SIZE 24 /* LAST ATTEMPT or FAVORITE and its number */

static struct {
    bool favorites;     /* the favorites page shows, else the spell list */
    int16_t caster;     /* the champion who casts, -1 for none */
    int16_t power;      /* the spell power 1 to 6 */
    int16_t row;        /* the selected row of the shown page; its row count for the title item */
    int16_t page;       /* the shown page of the spell list, from 0 */
    int16_t spell;      /* the marked spell: its place among the listed spells, -1 for none */
    int16_t slot;       /* the marked favorite */
    const char *status; /* the message of the last action, NULL for the costs of the marked spell */
} book;

/* Whether the spell list and the rune hints show spell: every spell, or only the known spells. */
static bool is_listed(const Spell *spell)
{
    return upscale_has_all_spells() || magic_is_spell_known(spell);
}

static int16_t count_listed_spells(void)
{
    int16_t count = 0;
    for (int i = 0; magic_get_spell(i); i++)
        count = (int16_t)(count + is_listed(magic_get_spell(i)));
    return count;
}

/* The listed spell at place in spell table order, NULL after the last. */
static const Spell *find_listed_spell(int place)
{
    for (int i = 0; magic_get_spell(i) && place >= 0; i++) {
        const Spell *spell = magic_get_spell(i);
        if (is_listed(spell) && !place--)
            return spell;
    }
    return NULL;
}

static int16_t count_pages(void)
{
    int16_t pages = (int16_t)((count_listed_spells() + LIST_SPELLS - 1) / LIST_SPELLS);
    return pages ? pages : 1;
}

/* The listed spell of a row of the shown spell list page, NULL for another row and for a row without a spell. */
static const Spell *find_row_spell(int16_t row)
{
    if (book.favorites || row < LIST_FIRST || row >= LIST_NEXT)
        return NULL;
    return find_listed_spell(book.page * LIST_SPELLS + row - LIST_FIRST);
}

/* The rows and the title item of the shown page, without their texts: the rows without a spell are not selectable. */
static RowsScreen find_layout(void)
{
    if (book.favorites)
        return (RowsScreen){.title_item = "BACK", .row_count = BOOK_ROWS, .subtitles = BOOK_SUBTITLES};
    uint16_t subtitles = 1u << LIST_STATUS;
    for (int16_t row = LIST_FIRST; row < LIST_NEXT; row++)
        if (!find_row_spell(row))
            subtitles |= (uint16_t)(1u << row);
    if (count_pages() == 1)
        subtitles |= 1u << LIST_NEXT;
    return (RowsScreen){.title_item = "CLOSE", .row_count = LIST_ROWS, .subtitles = subtitles};
}

/* Writes the rune names of spell after its power rune in upper case, for example FUL IR. */
static void get_rune_names(const Spell *spell, char *text, size_t size)
{
    size_t length = 0;
    for (int step = 1; step < CHAMPION_RUNES_SIZE; step++) {
        const char *name = magic_get_rune_name(magic_get_spell_rune(spell, step) - SPELL_RUNE_GLYPH_FIRST);
        if (!name)
            break; /* the spell has fewer runes */
        if (length && length + 1 < size)
            text[length++] = ' ';
        for (; *name && length + 1 < size; name++)
            text[length++] = (char)toupper((unsigned char)*name);
    }
    text[length] = '\0';
}

/* Writes the mana of a cast and of the caster, and what the cast still needs. */
static void get_cost_text(char *text, int mana, bool needs_flask)
{
    const Champion *c = book.caster >= 0 ? champion(book.caster) : NULL;
    snprintf(text, BOOK_TEXT_SIZE, "MANA %d/%d - %s", mana, c ? c->mana : 0,
             !needs_flask                         ? "NO FLASK NEEDED"
             : magic_has_empty_flask(book.caster) ? "FLASK READY"
                                                  : "FLASK MISSING");
}

/* Writes the caster row and the power row, the first two rows of both pages. */
static void get_caster_rows(char *caster_text, char *power_text)
{
    const Champion *c = book.caster >= 0 ? champion(book.caster) : NULL;
    snprintf(caster_text, BOOK_TEXT_SIZE, "CASTER %d %.7s - CHANGE", book.caster + 1, c ? c->name : "NONE");
    snprintf(power_text, BOOK_TEXT_SIZE, "SPELL POWER %d/6 - CHANGE", book.power);
}

/* Draws the spell list: the listed spells of the shown page by name with their runes, and the costs of the marked
   spell. */
static void show_list(void)
{
    char texts[LIST_ROWS][BOOK_TEXT_SIZE];
    const char *lines[LIST_ROWS];
    for (int i = 0; i < LIST_ROWS; i++)
        lines[i] = texts[i];
    get_caster_rows(texts[LIST_CASTER], texts[LIST_POWER]);
    for (int16_t row = LIST_FIRST; row < LIST_NEXT; row++) {
        const Spell *spell = find_row_spell(row);
        char runes[RUNE_NAMES_SIZE] = "";
        if (spell)
            get_rune_names(spell, runes, sizeof runes);
        bool marked = book.spell == book.page * LIST_SPELLS + row - LIST_FIRST;
        snprintf(texts[row], BOOK_TEXT_SIZE, "%s%-*s %s",
                 !spell   ? ""
                 : marked ? "> "
                          : "  ",
                 NAME_COLUMNS, spell ? magic_get_spell_name(spell) : "", runes);
    }
    snprintf(texts[LIST_NEXT], BOOK_TEXT_SIZE, "MORE SPELLS - PAGE %d/%d", book.page + 1, count_pages());
    if (count_pages() == 1)
        texts[LIST_NEXT][0] = '\0';
    snprintf(texts[LIST_FAVORITES], BOOK_TEXT_SIZE, "FAVORITES AND LAST ATTEMPT");
    const Spell *marked = find_listed_spell(book.spell);
    if (book.status)
        snprintf(texts[LIST_STATUS], BOOK_TEXT_SIZE, "%s", book.status);
    else if (marked)
        get_cost_text(texts[LIST_STATUS], magic_spell_mana(marked, book.power), magic_spell_needs_flask(marked));
    else
        snprintf(texts[LIST_STATUS], BOOK_TEXT_SIZE, "NO KNOWN SPELLS YET - ENTER RUNES");
    RowsScreen screen = find_layout();
    screen.title = upscale_has_all_spells() ? "ALL SPELLS" : "KNOWN SPELLS";
    screen.rows = lines;
    rows_draw(&screen, book.row);
}

/* Draws only the player's recorded combinations and the selected combination's costs. */
static void show_favorites(void)
{
    char texts[BOOK_ROWS][BOOK_TEXT_SIZE];
    const char *lines[BOOK_ROWS];
    for (int i = 0; i < BOOK_ROWS; i++)
        lines[i] = texts[i];
    get_caster_rows(texts[BOOK_CASTER], texts[BOOK_POWER]);
    for (int16_t slot = 0; slot < MAGIC_RECORD_COUNT; slot++) {
        char runes[CHAMPION_RUNES_SIZE] = "---";
        for (int step = 1; step < CHAMPION_RUNES_SIZE; step++) {
            uint8_t glyph = magic_favorite_rune(book.caster, slot, step);
            runes[step - 1] = glyph ? (char)('1' + (glyph - SPELL_RUNE_GLYPH_FIRST) % SPELL_RUNES_PER_SET) : '-';
        }
        char label[SLOT_LABEL_SIZE];
        if (slot == MAGIC_FAVORITE_LAST)
            snprintf(label, sizeof(label), "LAST ATTEMPT");
        else
            snprintf(label, sizeof(label), "FAVORITE %d", slot + 1);
        snprintf(texts[BOOK_FIRST + slot], BOOK_TEXT_SIZE, "%c %s: %s", book.slot == slot ? '>' : ' ', label,
                 magic_favorite_rune(book.caster, slot, 1) ? runes : "EMPTY");
    }
    int mana = magic_favorite_mana(book.caster, book.slot, book.power);
    if (mana < 0)
        snprintf(texts[BOOK_MANA], BOOK_TEXT_SIZE, "ENTER RUNES IN SPELL PANEL FIRST");
    else
        get_cost_text(texts[BOOK_MANA], mana, magic_favorite_needs_flask(book.caster, book.slot));
    if (book.slot == MAGIC_FAVORITE_LAST)
        snprintf(texts[BOOK_RECORD], BOOK_TEXT_SIZE, "TO SAVE: SELECT FAVORITE 1-4");
    else
        snprintf(texts[BOOK_RECORD], BOOK_TEXT_SIZE, "SAVE ENTERED RUNES TO FAVORITE %d", book.slot + 1);
    snprintf(texts[BOOK_CAST], BOOK_TEXT_SIZE, "CAST SELECTED SPELL");
    snprintf(texts[BOOK_STATUS], BOOK_TEXT_SIZE, "%s",
             book.status ? book.status
             : mana < 0  ? "CLOSE, ENTER RUNES, THEN PRESS B"
                         : "SELECT SPELL, CHOOSE POWER, CAST");
    RowsScreen screen = find_layout();
    screen.title = "FAVORITES";
    screen.rows = lines;
    rows_draw(&screen, book.row);
}

static void show_book(void)
{
    if (book.favorites)
        show_favorites();
    else
        show_list();
}

/* Shows the spell list on the page of the marked spell with that spell selected; without listed spells the caster
   row. */
static void open_list(void)
{
    int16_t count = count_listed_spells();
    if (book.spell >= count)
        book.spell = (int16_t)(count - 1);
    if (book.spell < 0 && count)
        book.spell = 0;
    book.favorites = false;
    book.page = (int16_t)(book.spell < 0 ? 0 : book.spell / LIST_SPELLS);
    book.row = (int16_t)(book.spell < 0 ? LIST_CASTER : LIST_FIRST + book.spell % LIST_SPELLS);
    book.status = NULL;
    show_book();
}

/* Queries */

/* Whether the listed spell begins with the entered runes of champion c after the power rune; a spell with a power
   rune needs that power rune. */
static bool has_entered_runes(const Spell *spell, const Champion *c)
{
    uint8_t power_rune = magic_get_spell_rune(spell, 0);
    if (power_rune && power_rune != (uint8_t)c->runes[0])
        return false;
    for (int step = 1; step < c->rune_count; step++)
        if (magic_get_spell_rune(spell, step) != (uint8_t)c->runes[step])
            return false;
    return true;
}

/* The champion of the open spell panel in the advanced mode with a power rune entered, else NULL: no hints. */
static const Champion *find_hint_caster(void)
{
    if (g_game_mode != GAME_MODE_ADVANCED || g_action.champion <= 0 || g_action.hand != ACTION_HAND_SPELL)
        return NULL;
    const Champion *c = champion(g_action.champion - 1);
    return c->rune_count >= 1 && c->rune_count <= CHAMPION_RUNES_SIZE ? c : NULL;
}

/* Writes the hint for the entered runes of the open spell panel in the advanced mode as a message: the runes and the
   name of the listed spell that they are, or that no spell has them when every spell is listed and none begins with
   them. False for no hint. */
static bool get_hint(char *text, size_t size)
{
    const Champion *c = find_hint_caster();
    bool begun = false;
    if (!c || c->rune_count < 2)
        return false; /* a power rune alone begins every spell */
    for (int i = 0; magic_get_spell(i); i++) {
        const Spell *spell = magic_get_spell(i);
        if (!is_listed(spell) || !has_entered_runes(spell, c))
            continue;
        if (c->rune_count == CHAMPION_RUNES_SIZE || !magic_get_spell_rune(spell, c->rune_count)) {
            char runes[RUNE_NAMES_SIZE];
            get_rune_names(spell, runes, sizeof runes);
            snprintf(text, size, "\n%s: %s", runes, magic_get_spell_name(spell));
            return true;
        }
        begun = true;
    }
    if (begun || !upscale_has_all_spells())
        return false;
    snprintf(text, size, "\nNO SPELL HAS THESE RUNES");
    return true;
}

bool spellbook_is_next_rune(int rune)
{
    const Champion *c = find_hint_caster();
    if (!c || c->rune_count == CHAMPION_RUNES_SIZE)
        return false;
    uint8_t glyph = (uint8_t)(SPELL_RUNE_GLYPH_FIRST + c->rune_count * SPELL_RUNES_PER_SET + rune);
    for (int i = 0; magic_get_spell(i); i++) {
        const Spell *spell = magic_get_spell(i);
        if (is_listed(spell) && has_entered_runes(spell, c) && magic_get_spell_rune(spell, c->rune_count) == glyph)
            return true;
    }
    return false;
}

/* Commands */

void spellbook_add_rune(int rune)
{
    const Champion *c = champion(g_action.champion - 1);
    uint8_t entered = c->rune_count;
    char hint[BOOK_TEXT_SIZE];
    magic_add_rune(rune);
    if (c->rune_count != entered && get_hint(hint, sizeof hint))
        text_print_message(UI_CYAN, hint);
}

void spellbook_open(void)
{
    if (g_action.champion > 0 && g_action.hand == ACTION_HAND_SPELL)
        book.caster = (int16_t)(g_action.champion - 1);
    if (book.caster < 0 || book.caster >= g_game.champion_count || !champion(book.caster)->health) {
        book.caster = -1;
        for (int16_t i = 0; i < g_game.champion_count; i++)
            if (champion(i)->health) {
                book.caster = i;
                break;
            }
    }
    if (book.power < 1)
        book.power = 1;
    book.slot = 0;
    open_list();
}

/* Why the caster cannot enter the runes of a spell now. */
static const char *find_refusal(void)
{
    const Champion *c = book.caster >= 0 ? champion(book.caster) : NULL;
    return !c || !c->health                ? "CHOOSE A LIVING CASTER"
           : c->rune_count                 ? "CLOSE AND CAST OR REMOVE YOUR RUNES"
           : c->action_delay[ACTION_SPELL] ? "CASTER BUSY - WAIT AND TRY AGAIN"
                                           : "NOT ENOUGH MANA - LOWER SPELL POWER";
}

/* Runs the selected row of the favorites page. Recording does not charge mana or remove entered runes. */
static SpellbookResult run_favorites_row(void)
{
    if (book.row >= BOOK_FIRST && book.row < BOOK_MANA)
        book.slot = (int16_t)(book.row - BOOK_FIRST);
    else if (book.row == BOOK_RECORD)
        book.status = book.slot == MAGIC_FAVORITE_LAST                ? "SELECT FAVORITE 1-4 TO SAVE"
                      : magic_record_favorite(book.caster, book.slot) ? "SAVED. CLOSE AND CAST ENTERED RUNES"
                                                                      : "CLOSE, ENTER RUNES, THEN PRESS B";
    else if (book.row == BOOK_CAST) {
        if (magic_prepare_favorite(book.caster, book.slot, book.power))
            return SPELLBOOK_CAST;
        book.status = magic_favorite_mana(book.caster, book.slot, book.power) < 0 ? "EMPTY SLOT - SAVE A SPELL FIRST"
                                                                                  : find_refusal();
    }
    return SPELLBOOK_STAY;
}

/* Runs the selected row of the spell list: a spell row casts its spell, which is the marked spell. */
static SpellbookResult run_list_row(void)
{
    const Spell *spell = find_row_spell(book.row);
    if (spell) {
        if (magic_prepare_spell(book.caster, spell, book.power))
            return SPELLBOOK_CAST;
        book.status = find_refusal();
    } else if (book.row == LIST_NEXT) {
        book.page = (int16_t)((book.page + 1) % count_pages());
    } else if (book.row == LIST_FAVORITES) {
        book.favorites = true;
        book.row = BOOK_FIRST;
    }
    return SPELLBOOK_STAY;
}

/* Runs the selected row: the title item closes the spell list, and goes back from the favorites to the spell list. */
static SpellbookResult run_row(void)
{
    SpellbookResult result = SPELLBOOK_STAY;
    book.status = NULL;
    if (book.row == find_layout().row_count) {
        if (!book.favorites)
            return SPELLBOOK_CLOSE;
        open_list();
        return SPELLBOOK_STAY;
    }
    if (book.row == BOOK_CASTER && g_game.champion_count)
        book.caster = (int16_t)((book.caster + 1) % g_game.champion_count);
    else if (book.row == BOOK_POWER)
        book.power = (int16_t)(book.power % POWER_MON + 1);
    else
        result = book.favorites ? run_favorites_row() : run_list_row();
    if (result == SPELLBOOK_STAY)
        show_book();
    return result;
}

/* Selects row; a spell row of the spell list marks its spell and shows its costs. */
static void select_row(int16_t row)
{
    book.row = row;
    if (find_row_spell(row)) {
        book.spell = (int16_t)(book.page * LIST_SPELLS + row - LIST_FIRST);
        book.status = NULL;
    }
    show_book();
}

/* Moves the selection one selectable row up (step -1) or down (step 1); the title item is after the last row. */
static void step_selection(int step)
{
    RowsScreen layout = find_layout();
    int16_t row = (int16_t)(book.row + step);
    while (row >= 0 && row < layout.row_count && (layout.subtitles >> row & 1))
        row = (int16_t)(row + step);
    if (row < 0)
        row = book.row;
    if (row > layout.row_count)
        row = layout.row_count;
    select_row(row);
}

/* Shows the spell list page before (step -1) or after (step 1) the shown page, with its first spell marked. */
static void flip_page(int step)
{
    if (book.favorites)
        return;
    book.page = (int16_t)((book.page + step + count_pages()) % count_pages());
    select_row(find_row_spell(LIST_FIRST) ? LIST_FIRST : LIST_CASTER);
}

SpellbookResult spellbook_event(int type, int16_t x, int16_t y)
{
    if (type == INPUT_KEY && x == KEY_SPELLBOOK)
        return SPELLBOOK_CLOSE;
    if (type == INPUT_BUTTON_RIGHT || (type == INPUT_KEY && x == SCAN_ESCAPE)) {
        if (!book.favorites)
            return SPELLBOOK_CLOSE;
        open_list();
    } else if (type == INPUT_KEY) {
        if (x == KEYPAD_FORWARD || x == SCAN_W)
            step_selection(-1);
        else if (x == KEYPAD_BACK || x == SCAN_S)
            step_selection(1);
        else if (x == KEYPAD_LEFT || x == SCAN_A)
            flip_page(-1);
        else if (x == KEYPAD_RIGHT || x == SCAN_D)
            flip_page(1);
        else if (x == KEY_ENTER || x == SCAN_SPACE)
            return run_row();
    } else if (type == INPUT_BUTTON_LEFT) {
        RowsScreen layout = find_layout();
        int16_t hit = rows_find_row(&layout, x, y);
        bool marks = hit >= 0 && find_row_spell(hit) && book.spell != book.page * LIST_SPELLS + hit - LIST_FIRST;
        if (marks)
            select_row(hit); /* the first click on a spell shows its costs, the second casts it */
        else if (hit >= 0) {
            book.row = hit;
            return run_row();
        }
    }
    return SPELLBOOK_STAY;
}
