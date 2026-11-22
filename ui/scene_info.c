/* Stats, moves, items, settings, and the learn-a-move prompt. */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ui.h"
#include "game.h"
#include "kindrals.h"
#include "moves.h"
#include "items.h"
#include "traits.h"
#include "abilities.h"
#include "storage.h"
#include "net.h"
#include "steps.h"

/* =================================================================== */
/* Stats (2 pages: LEFT/RIGHT)                                         */
/* =================================================================== */

static lv_obj_t *page_box;
static int page;

static void stats_build(void)
{
    const kindral *k = &g_game.kin;
    const kin_species *sp = kindrals_species(k->id.species);
    char buf[96];

    lv_obj_clean(page_box);
    if (page == 0) {
        ui_image(page_box, 4, 14, spr_front(k->id.species));
        ui_image(page_box, 58, 16, spr_badge(sp->type));
        ui_label(page_box, 78, 20, COL_FG, kindrals_type_name(sp->type));
        ui_label(page_box, 58, 38, COL_DIM, "Trait");
        ui_label(page_box, 58, 48, COL_FG, kindrals_trait_name(sp->trait));
        ui_label(page_box, 58, 62, COL_DIM, sp->species_name);

        uint32_t next = kindrals_exp_for_level(k->stats.level + 1);

        snprintf(buf, sizeof(buf),
                 "HP  %d/%d\nEP  %d/%d\nATK %d  DEF %d\nSPD %d  FR  %d\nEXP %d/%u",
                 k->stats.health_points, k->stats.max_hp, k->stats.energy_points, k->stats.max_ep,
                 k->stats.attack, k->stats.defense, k->stats.speed, k->stats.friendship,
                 k->stats.exp_points, k->stats.level >= KIN_MAX_LEVEL ? 0u : next);
        lv_obj_t *t = ui_text(page_box, 4, 72, 120, COL_FG, buf);

        lv_obj_set_style_text_line_space(t, 4, 0);
    } else {
        const ability_def *ab = abilities_get(k->id.ability);
        int64_t age = kindrals_age_seconds(k, g_game.last_unix);

        snprintf(buf, sizeof(buf), "Record\n W %u  L %u  D %u\n Streak %u (best %u)",
                 (unsigned)k->record.wins, (unsigned)k->record.losses, (unsigned)k->record.draws,
                 (unsigned)k->record.win_streak, (unsigned)k->record.best_streak);
        lv_obj_t *t = ui_text(page_box, 4, 16, 120, COL_FG, buf);

        lv_obj_set_style_text_line_space(t, 3, 0);
        ui_label(page_box, 4, 56, COL_ACCENT, ab->name);
        ui_text(page_box, 4, 66, 120, COL_DIM, ab->desc);
        snprintf(buf, sizeof(buf), "Trainer\n %s\nSteps %u\nAge %dd %dh", g_game.trainer,
                 (unsigned)g_game.steps_total, (int)(age / 86400), (int)((age % 86400) / 3600));
        t = ui_text(page_box, 4, 92, 120, COL_FG, buf);
        lv_obj_set_style_text_line_space(t, 3, 0);
    }
}

static void stats_enter(void *arg)
{
    ARG_UNUSED(arg);
    page = 0;
    ui_label(ui_screen(), 2, 2, COL_ACCENT, g_game.kin.id.name);
    lv_label_set_text_fmt(ui_label(ui_screen(), 88, 2, COL_ACCENT, ""), "Lv%d", g_game.kin.stats.level);
    page_box = ui_rect(ui_screen(), 0, 0, 128, 148, COL_BG);
    ui_label(ui_screen(), 2, 150, COL_DIM, "<  >  page   B:back");
    stats_build();
}

static void stats_btn(enum button b)
{
    if (b == BTN_B) {
        ui_pop();
    } else if (b == BTN_LEFT || b == BTN_RIGHT) {
        page ^= 1;
        stats_build();
    }
}

const ui_scene_t scene_stats = { .name = "stats", .enter = stats_enter, .on_btn = stats_btn };

/* =================================================================== */
/* Moves                                                               */
/* =================================================================== */

static int mv_sel;
static lv_obj_t *mv_hl, *mv_detail;
static lv_obj_t *mv_name[KIN_MAX_MOVES], *mv_cost[KIN_MAX_MOVES];

static void moves_draw(void)
{
    const kindral *k = &g_game.kin;

    lv_obj_set_y(mv_hl, 14 + mv_sel * 20);
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        const move_def *m = moves_get(k->moves.slot[i]);
        bool sel = (i == mv_sel);

        lv_label_set_text(mv_name[i], m ? m->name : "----");
        lv_label_set_text_fmt(mv_cost[i], "%s", m ? "" : "");
        if (m) {
            lv_label_set_text_fmt(mv_cost[i], "%dEP", m->ep_cost);
        }
        lv_obj_set_style_text_color(mv_name[i], lv_color_hex(sel ? COL_BG : COL_FG), 0);
        lv_obj_set_style_text_color(mv_cost[i], lv_color_hex(sel ? COL_BG : COL_EP), 0);
    }
    const move_def *m = moves_get(k->moves.slot[mv_sel]);

    if (m) {
        char buf[96];

        snprintf(buf, sizeof(buf), "%s  %s\nPow %d  Acc %d%%\nCost %d EP%s",
                 m->type < KIN_TYPE_COUNT ? kindrals_type_name(m->type) : "Normal",
                 moves_cat_name(m->cat), m->power, m->accuracy, m->ep_cost,
                 m->priority > 0 ? "\nGoes first!" : "");
        lv_label_set_text(mv_detail, buf);
    } else {
        lv_label_set_text(mv_detail, "Empty slot.");
    }
}

static void moves_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();
    const kindral *k = &g_game.kin;

    mv_sel = 0;
    ui_label(s, 2, 2, COL_ACCENT, "MOVES");
    mv_hl = ui_rect(s, 0, 14, 128, 18, COL_ACCENT);
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        const move_def *m = moves_get(k->moves.slot[i]);

        if (m && m->type < KIN_TYPE_COUNT) {
            ui_image(s, 2, 15 + i * 20, spr_badge(m->type));
        }
        mv_name[i] = ui_label(s, 22, 19 + i * 20, COL_FG, "");
        mv_cost[i] = ui_label(s, 98, 19 + i * 20, COL_EP, "");
    }
    mv_detail = ui_text(s, 4, 100, 120, COL_FG, "");
    lv_obj_set_style_text_line_space(mv_detail, 3, 0);
    ui_label(s, 2, 150, COL_DIM, "B: back");
    moves_draw();
}

static void moves_btn(enum button b)
{
    if (b == BTN_B) {
        ui_pop();
    } else if (b == BTN_UP) {
        mv_sel = (mv_sel + KIN_MAX_MOVES - 1) % KIN_MAX_MOVES;
        moves_draw();
    } else if (b == BTN_DOWN) {
        mv_sel = (mv_sel + 1) % KIN_MAX_MOVES;
        moves_draw();
    }
}

const ui_scene_t scene_moves = { .name = "moves", .enter = moves_enter, .on_btn = moves_btn };

/* =================================================================== */
/* Items                                                               */
/* =================================================================== */

static ui_menu it_menu;
static uint8_t it_ids[INV_SLOTS];
static char it_buf[INV_SLOTS][24];
static const char *it_lbl[INV_SLOTS];
static int it_n;
static lv_obj_t *it_icon, *it_desc;

static void items_refresh(void)
{
    it_n = inv_list(&g_game.inv, it_ids, INV_SLOTS);
    for (int i = 0; i < it_n; i++) {
        snprintf(it_buf[i], sizeof(it_buf[i]), "%-13.13s x%d", items_get(it_ids[i])->name,
                 inv_count(&g_game.inv, it_ids[i]));
        it_lbl[i] = it_buf[i];
    }
    if (it_n == 0) {
        it_lbl[0] = "(empty)";
    }
    ui_menu_set(&it_menu, it_lbl, it_n ? it_n : 1);
    if (it_n) {
        const item_def *d = items_get(it_ids[it_menu.cursor]);

        lv_image_set_src(it_icon, spr_item(it_ids[it_menu.cursor]));
        lv_label_set_text(it_desc, d->desc);
    } else {
        lv_obj_add_flag(it_icon, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(it_desc, "Walk with your kindral to find items!");
    }
}

static void items_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    ui_label(s, 2, 2, COL_ACCENT, "ITEMS");
    ui_menu_create(&it_menu, s, 0, 14, 128, 12, 7);
    it_icon = ui_image(s, 4, 104, spr_item(1));
    it_desc = ui_text(s, 40, 106, 86, COL_FG, "");
    ui_label(s, 2, 150, COL_DIM, "A:use  B:back");
    items_refresh();
}

static void items_btn(enum button b)
{
    if (b == BTN_B) {
        ui_pop();
    } else if (b == BTN_UP) {
        ui_menu_move(&it_menu, -1);
        items_refresh();
    } else if (b == BTN_DOWN) {
        ui_menu_move(&it_menu, +1);
        items_refresh();
    } else if (b == BTN_A && it_n) {
        uint8_t id = it_ids[it_menu.cursor];
        const item_def *d = items_get(id);
        int rc = items_use_field(&g_game.kin, id);

        if (rc == 0) {
            inv_remove(&g_game.inv, id, 1);
            storage_save_now(SAVE_INV | SAVE_KIN);
            items_refresh();
            ui_flush_pending();
            ui_toast("Used %s on %s.", d->name, g_game.kin.id.name);
        } else if (rc == -EALREADY) {
            ui_toast("It wouldn't help right now.");
        } else {
            ui_toast("%s can't be used here. Try gifting it or use it in battle.", d->name);
        }
    }
}

static void items_resume(void)
{
    items_refresh();
}

const ui_scene_t scene_items = {
    .name = "items", .enter = items_enter, .resume = items_resume, .on_btn = items_btn,
};

/* =================================================================== */
/* Settings                                                            */
/* =================================================================== */

static ui_menu st_menu;
static const char *st_lbl[5];
static char st_vis[20], st_trn[20];

static void settings_build(void)
{
    snprintf(st_vis, sizeof(st_vis), "Visible: %s", g_game.visible ? "ON" : "off");
    snprintf(st_trn, sizeof(st_trn), "You: %.10s", g_game.trainer);
    st_lbl[0] = st_vis;
    st_lbl[1] = st_trn;
    st_lbl[2] = "Factory reset";
    st_lbl[3] = "About";
    st_lbl[4] = "Back";
    st_menu.disabled = 1u << 1;
    ui_menu_set(&st_menu, st_lbl, 5);
}

static void reset_confirm2(bool yes, void *ud)
{
    ARG_UNUSED(ud);
    if (yes) {
        storage_factory_reset();
    }
}

static void reset_confirm1(bool yes, void *ud)
{
    ARG_UNUSED(ud);
    if (yes) {
        ui_confirm("LAST CHANCE. Really delete everything?", reset_confirm2, NULL);
    }
}

static void settings_enter(void *arg)
{
    ARG_UNUSED(arg);
    ui_label(ui_screen(), 2, 2, COL_ACCENT, "SETTINGS");
    ui_menu_create(&st_menu, ui_screen(), 0, 16, 128, 14, 8);
    ui_label(ui_screen(), 2, 150, COL_DIM, "A:select B:back");
    settings_build();
}

static void settings_btn(enum button b)
{
    switch (b) {
    case BTN_UP:   ui_menu_move(&st_menu, -1); break;
    case BTN_DOWN: ui_menu_move(&st_menu, +1); break;
    case BTN_B:    ui_pop(); break;
    case BTN_A:
        switch (st_menu.cursor) {
        case 0:
            g_game.visible = !g_game.visible;
            net_set_visible(g_game.visible);
            storage_save_now(SAVE_META);
            settings_build();
            break;
        case 2:
            ui_confirm("Erase your kindral, items and records?", reset_confirm1, NULL);
            break;
        case 3:
            ui_toast("KinBox\nSteps: %u\nTotal wins: %u", (unsigned)g_game.steps_total,
                     (unsigned)g_game.kin.record.wins);
            break;
        case 4: ui_pop(); break;
        }
        break;
    default: break;
    }
}

const ui_scene_t scene_settings = {
    .name = "settings", .enter = settings_enter, .on_btn = settings_btn,
};

/* =================================================================== */
/* Learn a move (replace one of 4 slots, or skip)                      */
/* =================================================================== */

typedef struct {
    uint8_t moves[KIN_MAX_MOVES];
    int n, idx;
} learn_queue;

static learn_queue lq;
static ui_menu ln_menu;
static const char *ln_lbl[KIN_MAX_MOVES + 1];
static char ln_buf[KIN_MAX_MOVES][24];
static lv_obj_t *ln_head, *ln_info;

static void learn_show(void)
{
    const move_def *nm = moves_get(lq.moves[lq.idx]);
    const kindral *k = &g_game.kin;

    lv_label_set_text_fmt(ln_head, "%s wants to learn\n%s!\nForget a move?", k->id.name, nm->name);
    lv_label_set_text_fmt(ln_info, "%s Pow%d %dEP", nm->type < KIN_TYPE_COUNT ?
                          kindrals_type_name(nm->type) : "Normal", nm->power, nm->ep_cost);
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        const move_def *m = moves_get(k->moves.slot[i]);

        snprintf(ln_buf[i], sizeof(ln_buf[i]), "%-12.12s %2dEP", m ? m->name : "-", m ? m->ep_cost : 0);
        ln_lbl[i] = ln_buf[i];
    }
    ln_lbl[KIN_MAX_MOVES] = "Skip (keep all)";
    ui_menu_set(&ln_menu, ln_lbl, KIN_MAX_MOVES + 1);
}

static void learn_next(void)
{
    if (++lq.idx >= lq.n) {
        storage_mark_dirty(SAVE_KIN);
        ui_pop();
    } else {
        ln_menu.cursor = 0;
        learn_show();
    }
}

static void learn_enter(void *arg)
{
    ARG_UNUSED(arg);
    lq.idx = 0;
    ln_head = ui_text(ui_screen(), 4, 4, 120, COL_ACCENT, "");
    ln_info = ui_label(ui_screen(), 4, 40, COL_EP, "");
    ui_menu_create(&ln_menu, ui_screen(), 0, 56, 128, 14, 6);
    learn_show();
}

static void learn_btn(enum button b)
{
    if (b == BTN_UP) {
        ui_menu_move(&ln_menu, -1);
    } else if (b == BTN_DOWN) {
        ui_menu_move(&ln_menu, +1);
    } else if (b == BTN_B) {
        learn_next();                         /* B = skip this move */
    } else if (b == BTN_A) {
        if (ln_menu.cursor < KIN_MAX_MOVES) {
            kindrals_replace_move(&g_game.kin, ln_menu.cursor, lq.moves[lq.idx]);
        }
        learn_next();
    }
}

const ui_scene_t scene_learn = { .name = "learn", .enter = learn_enter, .on_btn = learn_btn };

/* =================================================================== */
/* Pending level-up notices                                            */
/* =================================================================== */

void ui_flush_pending(void)
{
    kindral *k = &g_game.kin;
    kin_pending p = k->pending;

    kindrals_clear_pending(k);
    if (p.levelup_to == 0 && p.auto_learned_count == 0 && p.need_slot_count == 0) {
        return;
    }
    storage_mark_dirty(SAVE_KIN);

    /* Scenes stack up, so push the last-shown first. Order on screen:
     * level up -> learned -> replace prompt. */
    if (p.need_slot_count) {
        memcpy(lq.moves, p.need_slot, sizeof(lq.moves));
        lq.n = p.need_slot_count;
        ui_push(&scene_learn, NULL);
    }
    for (int i = p.auto_learned_count - 1; i >= 0; i--) {
        ui_toast("%s learned %s!", k->id.name, moves_get(p.auto_learned[i])->name);
    }
    if (p.levelup_to) {
        ui_toast("%s grew to level %d!", k->id.name, p.levelup_to);
    }
}
