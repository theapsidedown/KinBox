/*
 * KinBox main.c
 * Created by: Aps
 */
#include <stdint.h>
#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/console/console.h>
#include <zephyr/sys/printk.h>

#include "kindrals.h"
#include "buttons.h"
#include "menu.h"

/* How often the kindral's needs change. */
#define TICK_SECONDS  5
#define TICK_MS       (TICK_SECONDS * 1000)

/* Placeholder "birth clock" until an RTC is hooked up (2026-01-01 00:00:00 UTC). */
#define GAME_EPOCH    1767225600LL

/*
 * The game clock only advances on ticks, so it stands still while the menu is
 * open (the menu blocks the loop below). Swap for RTC time later if you like.
 */
static int64_t game_ticks;

static int64_t game_now(void)
{
    return GAME_EPOCH + game_ticks * TICK_SECONDS;
}

static void print_tick_line(const kindral *kin)
{
    printk("[%s] Hap:%d Hun:%d Thi:%d Rest:%d HP:%d  (B = menu)\n",
           kin->id.name, kin->walker.happiness, kin->walker.hunger,
           kin->walker.thirst, kin->walker.sleep, kin->stats.health_points);
}

int main(void)
{
    LOG_INF("KinBox is running!");
    LOG_INF("Meet your first kindral!");

    /* 1. Hatch: creates the kindral, rolls its ability, sets default name + stats */
    kindral kin;
    int ret = kindrals_init(&kin, KIN_001, game_now());
    if (ret != 0) {
        LOG_ERR("kindrals_init failed: %d", ret);
        return ret;
    }

    const kin_species *sp = kindrals_species(kin.id.species);
    if (sp == NULL) {
        LOG_ERR("Unknown species: %d", (int)kin.id.species);
        return -EINVAL;
    }

    /* 2. Prompt for a name over the console (empty -> species name) */
    console_getline_init();
    printk("\nName your %s (max %d chars, Enter for default): ",
           sp->species_name, KIN_NAME_LEN - 1);

    char *user_input = console_getline();   /* blocks until Enter is pressed */

    if (user_input != NULL && strlen(user_input) >= KIN_NAME_LEN) {
        printk("\nName too long, it will be cut to %d characters.\n", KIN_NAME_LEN - 1);
    }

    ret = kindrals_set_name(&kin, user_input);
    if (ret != 0) {
        LOG_ERR("kindrals_set_name failed: %d", ret);
        return ret;
    }

    kindrals_display_stats(&kin, game_now());

    /* 3. Game loop: tick on a timer, open the menu when B is pressed */
    printk("\nPress B to open the menu. Up/Down + A to choose.\n");
    buttons_flush();    /* drop anything pressed during naming */

    int64_t next_tick = k_uptime_get() + TICK_MS;

    while (1) {
        int64_t remaining = next_tick - k_uptime_get();

        if (remaining <= 0) {
            game_ticks++;
            kindrals_tick(&kin);
            print_tick_line(&kin);
            next_tick += TICK_MS;
            continue;
        }

        enum button b;

        if (buttons_wait(&b, K_MSEC(remaining)) == 0 && b == BTN_B) {
            /* Game is paused while this runs; the action is applied inside. */
            menu_run(&kin, game_now());

            buttons_flush();
            next_tick = k_uptime_get() + TICK_MS;   /* fresh interval after the pause */
        }
    }

    return 0;
}