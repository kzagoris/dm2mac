/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "combat_feedback.h"
#include <stdio.h>
#include <string.h>
#include "advanced.h"
#include "creature.h"
#include "draw.h"
#include "game.h"
#include "game_text.h"
#include "ids.h"
#include "move.h"
#include "num.h"
#include "text.h"
#include "ui.h"
#include "view.h"
#include "zone.h"

/* Half a second at the game's 60 Hz timer, with room for several visible targets. */
#define HIT_SHOW_TICKS   30
#define HIT_TARGETS      8
#define HIT_DAMAGE_MAX   32767
#define HIT_LABEL_SIZE   6 /* five damage digits and the terminator */
/* The same small damage star proportions as the action area's result artwork. */
#define HIT_STAR_SCALE_X 47
#define HIT_STAR_SCALE_Y 53

typedef struct {
    bool active, anchored;
    uint16_t group;
    int damage;
    uint32_t tick, step;
    int16_t x, y;
} HitFeedback;

static HitFeedback hits[HIT_TARGETS];
static int next_hit;
static int16_t eye_map, eye_dir;
static Location eye;

/* Camera changes end feedback at old screen positions. */
static bool same_eye(void)
{
    return eye_map == g_game.party_map && eye.x == g_game.party_x && eye.y == g_game.party_y &&
           eye_dir == g_game.party_dir;
}

void combat_reset_feedback(void)
{
    memset(hits, 0, sizeof hits);
    next_hit = 0;
    eye_map = (int16_t)g_game.party_map;
    eye_dir = (int16_t)g_game.party_dir;
    eye = (Location){g_game.party_x, g_game.party_y};
}

/* Keeps a killing blow anchored even after the creature's next drawing disappears. */
static void anchor_hit(HitFeedback *hit)
{
    int16_t rect[RECT_SIZE];
    if (view_attack_target_rect(hit->group, rect)) {
        hit->x = (int16_t)(rect[RECT_X] + rect[RECT_W] / 2);
        hit->y = (int16_t)(rect[RECT_Y] + rect[RECT_H] / 2);
        hit->anchored = true;
    }
}

void combat_show_hit(uint16_t group, Location at, int damage)
{
    if (g_game_mode != GAME_MODE_ADVANCED || g_current_map.map != g_game.party_map || damage < 0 ||
        group == THING_NONE || creature_group_info(group)->flags & CREATURE_STATIC)
        return;
    if (!same_eye())
        combat_reset_feedback();
    HitFeedback *hit = NULL;
    for (int i = 0; i < HIT_TARGETS; i++)
        if (hits[i].active && hits[i].group == (group & THING_ID_MASK))
            hit = &hits[i];
    if (!hit) {
        hit = &hits[next_hit];
        next_hit = (next_hit + 1) % HIT_TARGETS;
        memset(hit, 0, sizeof *hit);
    }
    int total = hit->active && hit->step == g_game.game_time ? hit->damage : 0;
    hit->damage = min_int(HIT_DAMAGE_MAX, total + damage);
    hit->active = true;
    hit->group = group & THING_ID_MASK;
    hit->tick = g_main_loop.timer_ticks;
    hit->step = g_game.game_time;
    anchor_hit(hit);
    (void)at; /* group identity follows the current sprite; an old anchor keeps a killing blow visible */
}

/* Uses the existing damage-star picture and cyan digits, clipped inside the viewport. */
static void draw_hit(const HitFeedback *hit, const int16_t *viewport)
{
    DrawCtx ctx;
    draw_ctx_init(&ctx, CAT_INTERFACE, UI_INDEX_ACTION, GFX_ACTION_RESULT_DAMAGE);
    ctx.scale_x = HIT_STAR_SCALE_X;
    ctx.scale_y = HIT_STAR_SCALE_Y;
    draw_ctx_prepare(&ctx);
    if (!ctx.image || ctx.w <= 0 || ctx.h <= 0 || ctx.w > viewport[RECT_W] || ctx.h > viewport[RECT_H])
        return;
    int x = viewport[RECT_X] + hit->x;
    int y = viewport[RECT_Y] + hit->y;
    int left = clamp_int(viewport[RECT_X], x - ctx.w / 2, viewport[RECT_X] + viewport[RECT_W] - ctx.w);
    int top = clamp_int(viewport[RECT_Y], y - ctx.h / 2, viewport[RECT_Y] + viewport[RECT_H] - ctx.h);
    ctx.zone = ZONE_NONE;
    ctx.x = 0;
    ctx.y = 0;
    ctx.src_x = 0;
    ctx.src_y = 0;
    const int16_t rect[RECT_SIZE] = {(int16_t)left, (int16_t)top, ctx.w, ctx.h};
    memcpy(ctx.rect, rect, sizeof rect);
    ctx.transparent = UI_BLACK;
    draw_ctx_draw(&ctx);
    char label[HIT_LABEL_SIZE];
    snprintf(label, sizeof label, "%d", hit->damage);
    ScreenSize size = text_measure(label);
    text_draw(&g_draw_screen, (int16_t)(left + (ctx.w - size.w) / 2),
              (int16_t)(top + ctx.h / 2 + FONT_GLYPH_HEIGHT / 2),
              (TextColors){g_ui_colors[UI_CYAN], g_ui_colors[UI_BLACK] | TEXT_TRANSPARENT}, label);
}

void combat_draw_feedback(void)
{
    if (!same_eye())
        combat_reset_feedback();
    if (g_game_mode != GAME_MODE_ADVANCED || !view_shows_party() || g_party.move_steps || g_ui.inventory ||
        g_main_loop.view_hidden || g_advanced_screen != ADVANCED_SCREEN_NONE)
        return;
    int16_t viewport[RECT_SIZE];
    if (!zone_get_viewport_rect(viewport))
        return;
    for (int i = 0; i < HIT_TARGETS; i++) {
        HitFeedback *hit = &hits[i];
        if (!hit->active)
            continue;
        if (g_main_loop.timer_ticks - hit->tick >= HIT_SHOW_TICKS) {
            hit->active = false;
            continue;
        }
        anchor_hit(hit);
        if (hit->anchored)
            draw_hit(hit, viewport);
    }
    draw_free_temp();
}
