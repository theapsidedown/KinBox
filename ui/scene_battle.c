/*
 * Battle screen (portrait 128x160). Both devices show both kindrals:
 *   foe   : top-right, front sprite (48x48), name + HP/EP at top-left
 *   you   : bottom-left, back sprite (48x48), name + HP/EP at right
 *   bottom: text box = action menu / move list / item list / battle log
 * "You" and "foe" are mapped from the fixed battle sides (0 = BLE central).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ui.h"
#include "game.h"
#include "social.h"
#include "battle.h"
#include "battle_session.h"
#include "items.h"
#include "moves.h"

enum { M_LINKING, M_MENU, M_MOVES, M_ITEMS, M_WAIT, M_PLAY, M_END };

static int mode;
static lv_obj_t *foe_name, *foe_hp, *foe_ep, *foe_spr;
static lv_obj_t *my_name, *my_hp, *my_ep, *my_hp_txt, *my_ep_txt, *my_spr;
static lv_obj_t *box_txt;
static ui_menu bm;
static const char *bm_lbl[KIN_MAX_MOVES + 1];
static char bm_buf[KIN_MAX_MOVES + 1][24];
static uint8_t bi_ids[INV_SLOTS];
static int bi_n;

static battle_log log_;
static int log_pos;
static lv_timer_t *play_timer;
static char summary[48];
static bool finished;

static int me_side(void) { return bsess_my_side(); }

static void set_bars(int hp_me, int ep_me, int hp_foe, int ep_foe)
{
    const battle_t *b = bsess_battle();
    const battle_mon *me = &b->mon[me_side()], *foe = &b->mon[me_side() ^ 1];

    ui_bar_set(my_hp, hp_me, me->max_hp);
    ui_bar_color(my_hp, hp_me * 4 < me->max_hp ? COL_HP_LOW : COL_HP);
    ui_bar_set(my_ep, ep_me, me->max_ep);
    ui_bar_set(foe_hp, hp_foe, foe->max_hp);
    ui_bar_color(foe_hp, hp_foe * 4 < foe->max_hp ? COL_HP_LOW : COL_HP);
    ui_bar_set(foe_ep, ep_foe, foe->max_ep);
    lv_label_set_text_fmt(my_hp_txt, "HP %d/%d", hp_me, me->max_hp);
    lv_label_set_text_fmt(my_ep_txt, "EP %d/%d", ep_me, me->max_ep);
}

static void set_bars_now(void)
{
    const battle_t *b = bsess_battle();
    int m = me_side();

    set_bars(b->mon[m].hp, b->mon[m].ep, b->mon[m ^ 1].hp, b->mon[m ^ 1].ep);
}

static void say(const char *s)
{
    lv_label_set_text(box_txt, s);
}

static void show_menu_box(bool on)
{
    for (int i = 0; i < bm.vis; i++) {
        if (on) {
            lv_obj_remove_flag(bm.row[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(bm.row[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (on) {
        lv_obj_add_flag(box_txt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(box_txt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void goto_menu(void)
{
    static const char *const top[3] = { "Fight", "Item", "Forfeit" };

    mode = M_MENU;
    show_menu_box(true);
    bm.cursor = 0;
    bm.first = 0;
    bm.disabled = 0;
    ui_menu_set(&bm, top, 3);
}

static void goto_moves(void)
{
    const battle_t *b = bsess_battle();
    const battle_mon *me = &b->mon[me_side()];

    mode = M_MOVES;
    bm.disabled = 0;
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        const move_def *m = moves_get(me->moves[i]);

        snprintf(bm_buf[i], sizeof(bm_buf[i]), "%-11.11s %2dEP", m ? m->name : "-", m ? m->ep_cost : 0);
        bm_lbl[i] = bm_buf[i];
        if (!m || !battle_move_usable(me, i)) {
            bm.disabled |= 1u << i;
        }
    }
    bm.cursor = 0;
    bm.first = 0;
    ui_menu_set(&bm, bm_lbl, KIN_MAX_MOVES);
}

static void goto_items(void)
{
    uint8_t all[INV_SLOTS];
    int n = inv_list(&g_game.inv, all, INV_SLOTS);

    bi_n = 0;
    for (int i = 0; i < n; i++) {
        const item_def *d = items_get(all[i]);

        if (d && (d->flags & ITEM_F_BATTLE) && bi_n < KIN_MAX_MOVES + 1) {
            bi_ids[bi_n] = all[i];
            snprintf(bm_buf[bi_n], sizeof(bm_buf[bi_n]), "%-11.11s x%d", d->name, inv_count(&g_game.inv, all[i]));
            bm_lbl[bi_n] = bm_buf[bi_n];
            bi_n++;
        }
    }
    if (!bi_n) {
        ui_toast("No battle items.");
        return;
    }
    mode = M_ITEMS;
    bm.disabled = 0;
    bm.cursor = 0;
    bm.first = 0;
    ui_menu_set(&bm, bm_lbl, bi_n);
}

/* ---- playback of a resolved turn, one event at a time ---- */

static void play_step(lv_timer_t *t)
{
    ARG_UNUSED(t);
    const battle_t *b = bsess_battle();

    if (log_pos < log_.n) {
        const battle_event *e = &log_.ev[log_pos++];
        char line[64];
        int m = me_side();

        battle_event_text(b, e, line, sizeof(line));
        say(line);
        set_bars(e->hp[m], e->ep[m], e->hp[m ^ 1], e->ep[m ^ 1]);
        return;
    }
    lv_timer_pause(play_timer);

    if (b->result != BATTLE_ONGOING) {
        if (!finished) {
            finished = true;
            bsess_finish(summary, sizeof(summary));
        }
        mode = M_END;
        say(summary);
        set_bars_now();
        ui_flush_pending();          /* level-ups from the exp reward, shown after this screen */
    } else {
        set_bars_now();
        goto_menu();
    }
}

static void start_playback(void)
{
    mode = M_PLAY;
    show_menu_box(false);
    log_pos = 0;
    lv_timer_resume(play_timer);
    lv_timer_reset(play_timer);
}

static void try_resolve(void)
{
    if (bsess_desync()) {
        bsess_abort();
        social_disconnect();
        ui_pop();
        ui_toast("The battle got out of sync and was cancelled.");
        return;
    }
    if (bsess_try_resolve(&log_)) {
        start_playback();
    }
}

static void submit(battle_action a)
{
    if (bsess_submit(a) != 0) {
        ui_toast("You can't do that right now.");
        return;
    }
    mode = M_WAIT;
    show_menu_box(false);
    say("Waiting for the\nother trainer...");
    try_resolve();
}

/* ---- scene ---- */

static void init_panels(void)
{
    const battle_t *b = bsess_battle();
    const battle_mon *me = &b->mon[me_side()], *foe = &b->mon[me_side() ^ 1];

    lv_label_set_text_fmt(foe_name, "%.9s L%d", foe->name, foe->level);
    lv_label_set_text_fmt(my_name, "%.9s", me->name);
    lv_image_set_src(foe_spr, spr_front(foe->species));
    lv_image_set_src(my_spr, spr_back(me->species));
    lv_obj_remove_flag(foe_spr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(my_spr, LV_OBJ_FLAG_HIDDEN);
    set_bars_now();
}

static void battle_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    finished = false;
    foe_name = ui_label(s, 2, 2, COL_FG, "");
    foe_hp = ui_bar(s, 2, 14, 66, 5, COL_HP);
    foe_ep = ui_bar(s, 2, 21, 66, 3, COL_EP);
    foe_spr = ui_image(s, 78, 2, &spr_missing);

    my_spr = ui_image(s, 4, 54, &spr_missing);
    my_name = ui_label(s, 58, 62, COL_FG, "");
    my_hp = ui_bar(s, 58, 74, 66, 5, COL_HP);
    my_hp_txt = ui_label(s, 58, 82, COL_DIM, "");
    my_ep = ui_bar(s, 58, 93, 66, 3, COL_EP);
    my_ep_txt = ui_label(s, 58, 98, COL_DIM, "");
    lv_obj_add_flag(foe_spr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(my_spr, LV_OBJ_FLAG_HIDDEN);

    ui_rect(s, 0, 110, 128, 50, COL_PANEL);
    box_txt = ui_text(s, 4, 114, 120, COL_FG, "Linking up...");
    lv_obj_set_style_text_line_space(box_txt, 2, 0);
    ui_menu_create(&bm, s, 0, 112, 128, 11, 4);
    show_menu_box(false);

    play_timer = lv_timer_create(play_step, 900, NULL);
    lv_timer_pause(play_timer);
    mode = M_LINKING;
    if (bsess_ready()) {
        init_panels();
        say("A battle begins!");
        goto_menu();
    }
}

static void battle_leave(void)
{
    if (play_timer) {
        lv_timer_delete(play_timer);
        play_timer = NULL;
    }
    if (bsess_active()) {
        bsess_abort();               /* left early: nothing is committed */
    }
}

static void forfeit_cb(bool yes, void *ud)
{
    ARG_UNUSED(ud);
    if (yes) {
        submit((battle_action){ BACT_FORFEIT, 0 });
    }
}

static void battle_btn(enum button b)
{
    switch (mode) {
    case M_MENU:
        if (b == BTN_UP) {
            ui_menu_move(&bm, -1);
        } else if (b == BTN_DOWN) {
            ui_menu_move(&bm, +1);
        } else if (b == BTN_A) {
            if (bm.cursor == 0) {
                goto_moves();
            } else if (bm.cursor == 1) {
                goto_items();
            } else {
                ui_confirm("Give up and take a loss?", forfeit_cb, NULL);
            }
        }
        break;

    case M_MOVES:
        if (b == BTN_UP) {
            ui_menu_move(&bm, -1);
        } else if (b == BTN_DOWN) {
            ui_menu_move(&bm, +1);
        } else if (b == BTN_B) {
            goto_menu();
        } else if (b == BTN_A) {
            const battle_mon *me = &bsess_battle()->mon[me_side()];

            if (battle_can_afford_any(me) && !battle_move_usable(me, bm.cursor)) {
                ui_toast("Not enough EP for that move.");
            } else {
                /* with nothing affordable the engine uses Struggle */
                submit((battle_action){ BACT_MOVE, (uint8_t)bm.cursor });
            }
        }
        break;

    case M_ITEMS:
        if (b == BTN_UP) {
            ui_menu_move(&bm, -1);
        } else if (b == BTN_DOWN) {
            ui_menu_move(&bm, +1);
        } else if (b == BTN_B) {
            goto_menu();
        } else if (b == BTN_A) {
            submit((battle_action){ BACT_ITEM, bi_ids[bm.cursor] });
        }
        break;

    case M_END:
        if (b == BTN_A || b == BTN_B) {
            ui_pop();
        }
        break;

    default:
        break;
    }
}

static void battle_event_cb(const app_event *e)
{
    if (e->type != EVT_NET_RX) {
        return;
    }
    if (mode == M_LINKING && bsess_ready()) {
        init_panels();
        say("A battle begins!");
        goto_menu();
    } else if (mode == M_WAIT) {
        try_resolve();
    } else if (bsess_desync()) {
        try_resolve();
    }
}

const ui_scene_t scene_battle = {
    .name = "battle", .needs_link = true, .enter = battle_enter, .leave = battle_leave,
    .on_btn = battle_btn, .on_event = battle_event_cb,
};
