/*
 * KinBox - Tamagotchi x Pokemon handheld (ESP32-S3 / Zephyr)
 *
 * One thread owns the UI and all game state. Everything else (buttons, BLE,
 * power key) only posts events to the queue (see app_event.h).
 */
#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/reboot.h>

#include "game.h"
#include "app_event.h"
#include "buttons.h"
#include "kclock.h"
#include "net.h"
#include "power.h"
#include "social.h"
#include "steps.h"
#include "storage.h"
#include "ui.h"
#include "walk.h"
#include "items.h"

LOG_MODULE_REGISTER(kinbox, LOG_LEVEL_INF);

game_t g_game;

static walk_state walk;
static bool soft_off;
static uint8_t advertised_level = 0xFF;

static uint32_t rnd(void)
{
    return sys_rand32_get();
}

static void sync_profile(void)
{
    if (g_game.setup_done && advertised_level != g_game.kin.stats.level) {
        advertised_level = (uint8_t)g_game.kin.stats.level;
        net_set_profile(g_game.trainer, (uint8_t)g_game.kin.id.species, advertised_level);
    }
}

static void every_second(int64_t now_ms)
{
    static int tick_s;

    net_housekeeping();

    if (g_game.setup_done && !g_game.in_event) {
        /* care tick */
        if (++tick_s >= CONFIG_KINBOX_TICK_SECONDS) {
            tick_s = 0;
            kindrals_tick(&g_game.kin);
            storage_mark_dirty(SAVE_KIN);
        }

        /* steps -> walk sessions -> item finds */
        uint32_t steps = steps_take_new();

        g_game.steps_total += steps;
        uint8_t found = walk_feed(&walk, steps, now_ms, rnd);

        if (found != ITEM_NONE) {
            const item_def *it = items_get(found);

            if (inv_add(&g_game.inv, found, 1) == 0) {
                storage_mark_dirty(SAVE_INV);
                ui_toast("%s found a %s while walking!", g_game.kin.id.name, it->name);
            } else {
                ui_toast("%s found a %s but your bag is full.", g_game.kin.id.name, it->name);
            }
        }
        if (steps) {
            storage_mark_dirty(SAVE_KIN);
        }
    }

    sync_profile();
    storage_poll(now_ms);
    ui_second();
}

static void dispatch(const app_event *e)
{
    if (soft_off) {                         /* only used when deep sleep is disabled */
        if (e->type == EVT_SCREEN_TOGGLE || e->type == EVT_POWER_OFF) {
            sys_reboot(SYS_REBOOT_COLD);
        }
        return;
    }

    switch (e->type) {
    case EVT_BTN:
        if (!ui_screen_is_on()) {           /* first press only wakes the screen */
            ui_screen_power(true);
            ui_activity();
            break;
        }
        ui_activity();
        ui_handle_btn((enum button)e->btn);
        break;

    case EVT_POWER_OFF:
        power_off();
        soft_off = true;                    /* only reached without deep sleep */
        break;

    case EVT_SCREEN_TOGGLE:
        ui_screen_power(!ui_screen_is_on());
        ui_activity();
        break;

    case EVT_NET_PEER_NEAR:
        ui_peer_near(e);
        break;

    case EVT_NET_LINK_UP:
    case EVT_NET_LINK_DOWN:
    case EVT_NET_CONNECT_FAIL:
    case EVT_NET_RX:
        social_handle_event(e);             /* 1. update state            */
        ui_handle_event(e);                 /* 2. let the active scene react */
        ui_process_social();                /* 3. toasts, launches, link loss */
        break;
    }
}

int main(void)
{
    int rc;

    LOG_INF("KinBox starting");

    if (storage_init() != 0) {
        LOG_ERR("Storage unavailable - running without saving!");
    }

    /* Factory-reset combo: hold A + B while powering on for 3 seconds. */
    if (buttons_a_and_b_held()) {
        k_sleep(K_SECONDS(3));
        if (buttons_a_and_b_held()) {
            storage_factory_reset();
        }
    }

    g_game.visible = true;
    rc = storage_load(&g_game);
    kclock_init(rc == 0 ? g_game.last_unix : 0);

    power_init();
    ui_init();
    steps_init();
    walk_init(&walk);
    social_init();
    if (net_init() != 0) {
        LOG_ERR("BLE unavailable - social features disabled");
    }

    if (rc == 0) {
        int64_t gap = kclock_now() - g_game.last_unix;

        kindrals_catch_up(&g_game.kin, gap, CONFIG_KINBOX_OFFLINE_SECS_PER_TICK,
                          CONFIG_KINBOX_OFFLINE_MAX_TICKS);
        LOG_INF("Loaded %s Lv%d (offline %lld s)", g_game.kin.id.name, g_game.kin.stats.level,
                (long long)gap);
        net_set_profile(g_game.trainer, (uint8_t)g_game.kin.id.species,
                        (uint8_t)g_game.kin.stats.level);
        advertised_level = (uint8_t)g_game.kin.stats.level;
        net_set_visible(g_game.visible);
        ui_start_home();
    } else {
        if (rc != -ENOENT) {
            LOG_WRN("Save unreadable (%d): starting fresh", rc);
        }
        memset(&g_game.kin, 0, sizeof(g_game.kin));
        g_game.setup_done = false;      /* nothing is saved until setup finishes */
        net_set_profile("", 0, 0);
        ui_start_setup();
    }

    int64_t next_second = k_uptime_get() + 1000;

    for (;;) {
        app_event e;

        if (app_wait(&e, K_MSEC(20)) == 0) {
            dispatch(&e);
        }
        int64_t now = k_uptime_get();

        if (now >= next_second) {
            next_second += 1000;
            if (!soft_off) {
                every_second(now);
            }
        }
        ui_run();
    }
    return 0;
}
