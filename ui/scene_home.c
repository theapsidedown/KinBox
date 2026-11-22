#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ui.h"
#include "game.h"
#include "steps.h"
#include "storage.h"
#include "social.h"
#include "net.h"
#include "traits.h"

/* =================================================================== */
/* Home screen                                                         */
/* =================================================================== */

static lv_obj_t *l_title, *l_status, *spr;
static lv_obj_t *bar_hp, *bar_ep, *bar_hap, *bar_hun, *bar_thi, *bar_rst;
static bool bob;

static lv_obj_t *meter(lv_obj_t *s, int y, const char *name, uint32_t col)
{
    ui_label(s, 2, y - 1, COL_DIM, name);
    return ui_bar(s, 30, y, 94, 6, col);
}

static void home_refresh(void)
{
    const kindral *k = &g_game.kin;

    lv_label_set_text_fmt(l_title, "%s Lv%d", k->id.name, k->stats.level);
    lv_label_set_text(l_status, social_state() == SOC_LINKED ? "LINK" :
                                (g_game.visible ? "BT" : "--"));
    ui_bar_set(bar_hp, k->stats.health_points, k->stats.max_hp);
    ui_bar_color(bar_hp, k->stats.health_points * 4 < k->stats.max_hp ? COL_HP_LOW : COL_HP);
    ui_bar_set(bar_ep, k->stats.energy_points, k->stats.max_ep);
    ui_bar_set(bar_hap, k->walker.happiness, KIN_METER_MAX);
    ui_bar_set(bar_hun, KIN_METER_MAX - k->walker.hunger, KIN_METER_MAX);   /* full bar = fed   */
    ui_bar_set(bar_thi, KIN_METER_MAX - k->walker.thirst, KIN_METER_MAX);   /* full bar = quenched */
    ui_bar_set(bar_rst, k->walker.sleep, KIN_METER_MAX);
}

static void home_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();
    const kindral *k = &g_game.kin;

    l_title = ui_label(s, 2, 2, COL_ACCENT, "");
    l_status = ui_label(s, 104, 2, COL_EP, "");
    ui_label(s, 2, 12, COL_DIM, g_game.trainer);
    spr = ui_image(s, 40, 26, spr_front(k->id.species));

    bar_hp  = meter(s, 88,  "HP",  COL_HP);
    bar_ep  = meter(s, 97,  "EP",  COL_EP);
    bar_hap = meter(s, 108, "HAP", COL_MOOD);
    bar_hun = meter(s, 117, "FOD", 0xFFA24D);
    bar_thi = meter(s, 126, "H2O", 0x4DD2FF);
    bar_rst = meter(s, 135, "RST", 0xB48CFF);
    ui_label(s, 2, 148, COL_DIM, "B: menu");
    home_refresh();
}

static void home_second(void)
{
    bob = !bob;
    lv_obj_set_y(spr, bob ? 28 : 26);
    home_refresh();
}

static void home_btn(enum button b)
{
    if (b == BTN_A || b == BTN_B) {
        ui_push(&scene_menu, NULL);
    }
}

const ui_scene_t scene_home = {
    .name = "home", .enter = home_enter, .resume = home_refresh,
    .on_btn = home_btn, .on_second = home_second,
};

/* =================================================================== */
/* Main menu                                                           */
/* =================================================================== */

enum act { A_EAT, A_DRINK, A_SLEEP, A_PLAY, A_WALK, A_STATS, A_MOVES, A_ITEMS, A_LINK, A_SETTINGS, A_BACK };

static ui_menu menu;
static const char *labels[12];
static uint8_t acts[12];
static char walk_label[16], link_label[20];
static int n_items;
static bool menu_first = true;

static void build_items(void)
{
    n_items = 0;
#define ADD(a, text) do { labels[n_items] = (text); acts[n_items++] = (a); } while (0)
    ADD(A_EAT, "Eat");
    ADD(A_DRINK, "Drink");
    ADD(A_SLEEP, "Sleep");
    ADD(A_PLAY, "Play");
#if CONFIG_KINBOX_STEPS_SIM
    snprintf(walk_label, sizeof(walk_label), "Walk: %s", steps_sim_walking() ? "ON" : "off");
    ADD(A_WALK, walk_label);
#endif
    ADD(A_STATS, "Stats");
    ADD(A_MOVES, "Moves");
    ADD(A_ITEMS, "Items");
    if (social_state() == SOC_LINKED) {
        snprintf(link_label, sizeof(link_label), "> %.10s", g_peer_trainer);
        ADD(A_LINK, link_label);
    }
    ADD(A_SETTINGS, "Settings");
    ADD(A_BACK, "Back");
#undef ADD
    ui_menu_set(&menu, labels, n_items);
}

static void menu_enter(void *arg)
{
    ARG_UNUSED(arg);
    ui_label(ui_screen(), 4, 3, COL_ACCENT, "MENU");
    ui_menu_create(&menu, ui_screen(), 0, 16, 128, 13, 10);
    if (menu_first) {
        menu.cursor = 0;
        menu.first = 0;
    }
    build_items();
}

static void care_result(int rc, const char *ok, const char *no_need, const char *busy)
{
    if (rc == 0) {
        storage_mark_dirty(SAVE_KIN);
        ui_flush_pending();          /* level-up popups sit below the result toast */
        ui_toast("%s", ok);
    } else if (rc == -EBUSY) {
        ui_toast("%s", busy);
    } else {
        ui_toast("%s", no_need);
    }
}

static void menu_select(void)
{
    kindral *k = &g_game.kin;
    const char *n = k->id.name;
    char buf[48];

    switch (acts[menu.cursor]) {
    case A_EAT:
        snprintf(buf, sizeof(buf), "%s ate happily!", n);
        care_result(kindrals_eat(k), buf, "Not hungry.", "");
        break;
    case A_DRINK:
        snprintf(buf, sizeof(buf), "%s had a drink.", n);
        care_result(kindrals_drink(k), buf, "Not thirsty.", "");
        break;
    case A_SLEEP:
        snprintf(buf, sizeof(buf), "%s had a nap. Zzz...", n);
        care_result(kindrals_sleep(k), buf, "Not sleepy.", "");
        break;
    case A_PLAY:
        snprintf(buf, sizeof(buf), "%s loved playing! (+5 exp)", n);
        care_result(kindrals_play(k), buf, "", "Too tired to play.");
        break;
    case A_WALK:
        steps_sim_toggle();
        build_items();
        break;
    case A_STATS:    ui_push(&scene_stats, NULL); break;
    case A_MOVES:    ui_push(&scene_moves, NULL); break;
    case A_ITEMS:    ui_push(&scene_items, NULL); break;
    case A_LINK:     ui_push(&scene_social, NULL); break;
    case A_SETTINGS: ui_push(&scene_settings, NULL); break;
    case A_BACK:     ui_pop(); break;
    }
}

static void menu_btn(enum button b)
{
    switch (b) {
    case BTN_UP:   ui_menu_move(&menu, -1); break;
    case BTN_DOWN: ui_menu_move(&menu, +1); break;
    case BTN_A:    menu_first = false; menu_select(); break;
    case BTN_B:    menu_first = true; ui_pop(); break;
    default: break;
    }
}

static void menu_resume(void)
{
    build_items();
}

const ui_scene_t scene_menu = {
    .name = "menu", .enter = menu_enter, .resume = menu_resume, .on_btn = menu_btn,
};
