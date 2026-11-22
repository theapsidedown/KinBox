/* Proximity alert, connect handshake, link menu, gifts and trading. */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ui.h"
#include "game.h"
#include "social.h"
#include "proto.h"
#include "net.h"
#include "items.h"
#include "kindrals.h"

/* =================================================================== */
/* "A KinBox is nearby!" alert                                         */
/* =================================================================== */

static ui_peer_args peer;

void ui_peer_near(const app_event *e)
{
    const ui_scene_t *t = ui_top();
    bool busy = !g_game.setup_done || g_game.in_event || social_state() != SOC_IDLE ||
                t == &scene_peer_near || t == &scene_keyboard || t == &scene_hatch ||
                t == &scene_battle || t == &scene_learn;

    if (busy) {
        net_ignore_peer(e->peer.idx, 60);      /* ask again in a minute */
        return;
    }
    peer.idx = e->peer.idx;
    peer.species = e->peer.species;
    peer.level = e->peer.level;
    snprintf(peer.trainer, sizeof(peer.trainer), "%s", e->peer.trainer);
    ui_screen_power(true);
    ui_push(&scene_peer_near, &peer);
}

static void pn_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    ui_rect(s, 0, 0, 128, 14, COL_ACCENT);
    lv_obj_set_style_text_color(ui_label(s, 8, 3, COL_BG, "KinBox nearby!"), lv_color_hex(COL_BG), 0);
    ui_image(s, 40, 24, spr_front(peer.species));
    ui_label(s, 4, 78, COL_FG, peer.trainer);
    const kin_species *sp = kindrals_species(peer.species);

    lv_label_set_text_fmt(ui_label(s, 4, 90, COL_DIM, ""), "%s Lv%d",
                          sp && sp->species_name ? sp->species_name : "???", peer.level);
    ui_text(s, 4, 110, 120, COL_FG, "Connect to say hi, trade, or battle?");
    ui_label(s, 4, 148, COL_ACCENT, "A:Connect B:Ignore");
}

static void pn_btn(enum button b)
{
    if (b == BTN_A) {
        if (social_request_connect(peer.idx) == 0) {
            ui_replace(&scene_link, NULL);
        } else {
            ui_pop();
            ui_toast("They moved out of range.");
        }
    } else if (b == BTN_B) {
        net_ignore_peer(peer.idx, 300);        /* don't nag for 5 minutes */
        ui_pop();
    }
}

const ui_scene_t scene_peer_near = { .name = "peer_near", .enter = pn_enter, .on_btn = pn_btn };

/* =================================================================== */
/* Connecting / incoming request                                       */
/* =================================================================== */

static lv_obj_t *lk_text, *lk_hint;

static void link_refresh(void)
{
    if (social_state() == SOC_INCOMING) {
        lv_label_set_text_fmt(lk_text, "%s wants to connect!", g_peer_trainer[0] ? g_peer_trainer : "Someone");
        lv_label_set_text(lk_hint, "A:Accept B:Decline");
    } else {
        lv_label_set_text(lk_text, "Connecting...\nHold the KinBoxes close.");
        lv_label_set_text(lk_hint, "B: Cancel");
    }
}

static void link_event(const app_event *e)
{
    ARG_UNUSED(e);
    link_refresh();
}

static void link_enter(void *arg)
{
    ARG_UNUSED(arg);
    lk_text = ui_text(ui_screen(), 6, 50, 116, COL_FG, "");
    lk_hint = ui_label(ui_screen(), 6, 130, COL_ACCENT, "");
    link_refresh();
}

static void link_btn(enum button b)
{
    if (social_state() == SOC_INCOMING) {
        if (b == BTN_A) {
            social_accept(true);                   /* NOTE_LINKED swaps this scene for the menu */
        } else if (b == BTN_B) {
            social_accept(false);
            ui_pop();
        }
    } else if (b == BTN_B) {
        social_cancel_connect();
        ui_pop();
    }
}

const ui_scene_t scene_link = {
    .name = "link", .needs_link = true, .enter = link_enter, .on_btn = link_btn,
    .on_event = link_event,
};

/* =================================================================== */
/* Link menu: Battle / Trade / Greet / Gift / Disconnect               */
/* =================================================================== */

static ui_menu sm;
static const char *sm_lbl[5];
static char sm_battle[24], sm_trade[24];
static lv_obj_t *sm_status;

static const char *prop_name(uint8_t p)
{
    return p == PROP_BATTLE ? "battle" : p == PROP_TRADE ? "trade" : "";
}

static void social_refresh(void)
{
    uint8_t mine = social_my_proposal(), theirs = social_peer_proposal();

    snprintf(sm_battle, sizeof(sm_battle), "Battle%s",
             mine == PROP_BATTLE ? " (waiting)" : (theirs == PROP_BATTLE ? " (!)" : ""));
    snprintf(sm_trade, sizeof(sm_trade), "Trade%s",
             mine == PROP_TRADE ? " (waiting)" : (theirs == PROP_TRADE ? " (!)" : ""));
    sm_lbl[0] = sm_battle;
    sm_lbl[1] = sm_trade;
    sm_lbl[2] = "Greet";
    sm_lbl[3] = "Gift";
    sm_lbl[4] = "Disconnect";
    ui_menu_set(&sm, sm_lbl, 5);

    if (mine != PROP_NONE) {
        lv_label_set_text_fmt(sm_status, "Waiting for %.8s\nto pick %s too...", g_peer_trainer, prop_name(mine));
    } else if (theirs != PROP_NONE) {
        lv_label_set_text_fmt(sm_status, "%.8s wants to\n%s! Pick it too.", g_peer_trainer, prop_name(theirs));
    } else {
        lv_label_set_text(sm_status, "Battle & trade need\nboth of you to pick.");
    }
}

static void social_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    lv_label_set_text_fmt(ui_label(s, 2, 2, COL_ACCENT, ""), "Linked: %.8s", g_peer_trainer);
    ui_menu_create(&sm, s, 0, 16, 128, 14, 5);
    sm_status = ui_text(s, 4, 100, 120, COL_DIM, "");
    ui_label(s, 2, 150, COL_DIM, "A:select B:back");
    social_refresh();
}

static void disconnect_cb(bool yes, void *ud)
{
    ARG_UNUSED(ud);
    if (yes) {
        social_disconnect();
    }
}

static void social_btn(enum button b)
{
    uint8_t mine = social_my_proposal();

    switch (b) {
    case BTN_UP:   ui_menu_move(&sm, -1); break;
    case BTN_DOWN: ui_menu_move(&sm, +1); break;
    case BTN_B:    ui_pop(); return;       /* widgets are gone after the pop */
    case BTN_A:
        switch (sm.cursor) {
        case 0:
            social_propose(mine == PROP_BATTLE ? PROP_NONE : PROP_BATTLE);
            break;
        case 1:
            social_propose(mine == PROP_TRADE ? PROP_NONE : PROP_TRADE);
            break;
        case 2:
            if (social_greet() == -EBUSY) {
                ui_toast("You greeted recently. Give it a few minutes!");
            } else {
                ui_toast("You wave at %s's kindral...", g_peer_trainer);
            }
            break;
        case 3:
            ui_push(&scene_gift, NULL);
            break;
        case 4:
            ui_confirm("Disconnect?", disconnect_cb, NULL);
            break;
        }
        break;
    default: break;
    }
    social_refresh();
}

static void social_event(const app_event *e)
{
    ARG_UNUSED(e);
    social_refresh();
}

const ui_scene_t scene_social = {
    .name = "social", .needs_link = true, .enter = social_enter, .resume = social_refresh,
    .on_btn = social_btn, .on_event = social_event,
};

/* =================================================================== */
/* Gift picker                                                         */
/* =================================================================== */

static ui_menu gm;
static uint8_t g_ids[INV_SLOTS];
static char g_buf[INV_SLOTS][24];
static const char *g_lbl[INV_SLOTS];
static int g_n;
static lv_obj_t *g_icon, *g_desc;

static void gift_refresh(void)
{
    if (g_n) {
        lv_image_set_src(g_icon, spr_item(g_ids[gm.cursor]));
        lv_label_set_text(g_desc, items_get(g_ids[gm.cursor])->desc);
    }
}

static void gift_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    g_n = inv_list(&g_game.inv, g_ids, INV_SLOTS);
    lv_label_set_text_fmt(ui_label(s, 2, 2, COL_ACCENT, ""), "Gift to %.8s", g_peer_trainer);
    ui_menu_create(&gm, s, 0, 14, 128, 12, 7);
    g_icon = ui_image(s, 4, 104, spr_item(1));
    g_desc = ui_text(s, 40, 106, 86, COL_FG, "");
    for (int i = 0; i < g_n; i++) {
        snprintf(g_buf[i], sizeof(g_buf[i]), "%-13.13s x%d", items_get(g_ids[i])->name,
                 inv_count(&g_game.inv, g_ids[i]));
        g_lbl[i] = g_buf[i];
    }
    if (!g_n) {
        g_lbl[0] = "(nothing to give)";
        lv_obj_add_flag(g_icon, LV_OBJ_FLAG_HIDDEN);
    }
    ui_menu_set(&gm, g_lbl, g_n ? g_n : 1);
    ui_label(s, 2, 150, COL_DIM, "A:give B:back");
    gift_refresh();
}

static void gift_btn(enum button b)
{
    if (b == BTN_B) {
        ui_pop();
    } else if (b == BTN_UP) {
        ui_menu_move(&gm, -1);
        gift_refresh();
    } else if (b == BTN_DOWN) {
        ui_menu_move(&gm, +1);
        gift_refresh();
    } else if (b == BTN_A && g_n) {
        if (social_gift(g_ids[gm.cursor]) == 0) {
            ui_pop();
            ui_toast("Gift sent to %s's kindral!", g_peer_trainer);
        }
    }
}

const ui_scene_t scene_gift = {
    .name = "gift", .needs_link = true, .enter = gift_enter, .on_btn = gift_btn,
};

/* =================================================================== */
/* Trade                                                               */
/* =================================================================== */

enum { T_PICK, T_ACTION };

static int t_mode;
static ui_menu tm;
static uint8_t t_ids[INV_SLOTS];
static char t_buf[INV_SLOTS + 1][24];
static const char *t_lbl[INV_SLOTS + 1];
static lv_obj_t *t_mine, *t_theirs, *t_mine_n, *t_theirs_n, *t_status;
static int t_count;
static const char *const t_act_lbl[3] = { "Confirm swap", "Change item", "Cancel trade" };

static void trade_show(void)
{
    uint8_t mi = social_trade_my_offer(), th = social_trade_peer_offer();

    lv_image_set_src(t_mine, mi ? spr_item(mi) : &spr_missing);
    lv_image_set_src(t_theirs, th ? spr_item(th) : &spr_missing);
    lv_label_set_text(t_mine_n, mi ? items_get(mi)->name : "-");
    lv_label_set_text(t_theirs_n, th ? items_get(th)->name : "?");

    if (!mi) {
        lv_label_set_text(t_status, "Pick an item to offer.");
    } else if (!th) {
        lv_label_set_text_fmt(t_status, "Waiting for %.8s...", g_peer_trainer);
    } else if (social_trade_my_confirmed()) {
        lv_label_set_text(t_status, "Waiting for them\nto confirm...");
    } else if (social_trade_peer_confirmed()) {
        lv_label_set_text_fmt(t_status, "%.8s is ready!\nConfirm to swap.", g_peer_trainer);
    } else {
        lv_label_set_text(t_status, "Both offers set.\nConfirm to swap.");
    }
}

static void trade_menu(void)
{
    if (t_mode == T_PICK) {
        t_count = inv_list(&g_game.inv, t_ids, INV_SLOTS);
        for (int i = 0; i < t_count; i++) {
            snprintf(t_buf[i], sizeof(t_buf[i]), "%-13.13s x%d", items_get(t_ids[i])->name,
                     inv_count(&g_game.inv, t_ids[i]));
            t_lbl[i] = t_buf[i];
        }
        t_lbl[t_count] = "Cancel trade";
        tm.cursor = 0;
        tm.first = 0;
        ui_menu_set(&tm, t_lbl, t_count + 1);
    } else {
        tm.cursor = 0;
        tm.first = 0;
        ui_menu_set(&tm, t_act_lbl, 3);
    }
}

static void trade_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    t_mode = T_PICK;
    lv_label_set_text_fmt(ui_label(s, 2, 2, COL_ACCENT, ""), "Trade: %.8s", g_peer_trainer);
    ui_label(s, 20, 14, COL_DIM, "You");
    lv_label_set_text_fmt(ui_label(s, 78, 14, COL_DIM, ""), "%.6s", g_peer_trainer);
    t_mine = ui_image(s, 12, 24, &spr_missing);       /* 32x32 item icons */
    t_theirs = ui_image(s, 84, 24, &spr_missing);
    ui_label(s, 52, 36, COL_ACCENT, "<>");
    t_mine_n = ui_text(s, 2, 58, 60, COL_FG, "");
    t_theirs_n = ui_text(s, 66, 58, 60, COL_FG, "");
    t_status = ui_text(s, 2, 76, 124, COL_DIM, "");
    ui_menu_create(&tm, s, 0, 96, 128, 12, 4);
    trade_menu();
    trade_show();
}

static void trade_check_end(void)
{
    if (social_trade_done() || !social_trade_active()) {
        ui_pop();
    }
}

static void trade_btn(enum button b)
{
    if (b == BTN_UP) {
        ui_menu_move(&tm, -1);
    } else if (b == BTN_DOWN) {
        ui_menu_move(&tm, +1);
    } else if (b == BTN_B) {
        social_trade_cancel();
        ui_pop();
        return;
    } else if (b == BTN_A) {
        if (t_mode == T_PICK) {
            if (tm.cursor >= t_count) {
                social_trade_cancel();
                ui_pop();
                return;
            }
            if (social_trade_offer(t_ids[tm.cursor]) == 0) {
                t_mode = T_ACTION;
                trade_menu();
            }
        } else if (tm.cursor == 0) {
            if (!social_trade_my_offer() || !social_trade_peer_offer()) {
                ui_toast("Both of you need to pick an item first.");
            } else if (social_trade_confirm() == -ENOSPC) {
                ui_toast("Your bag has no room for that item.");
            }
        } else if (tm.cursor == 1) {
            social_trade_offer(0);
            t_mode = T_PICK;
            trade_menu();
        } else {
            social_trade_cancel();
            ui_pop();
            return;
        }
    }
    trade_check_end();
    if (ui_top() == &scene_trade) {
        trade_show();
    }
}

static void trade_event(const app_event *e)
{
    ARG_UNUSED(e);
    trade_check_end();
    if (ui_top() == &scene_trade) {
        trade_show();
    }
}

const ui_scene_t scene_trade = {
    .name = "trade", .needs_link = true, .enter = trade_enter,
    .on_btn = trade_btn, .on_event = trade_event,
};
