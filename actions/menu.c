/*
 * Text menu. D-pad Up/Down moves the cursor, A selects, B closes.
 * The main loop does not tick while we are in here, so the game is paused.
 */
#include <errno.h>
#include <zephyr/sys/printk.h>

#include "menu.h"
#include "buttons.h"

enum menu_item {
    MENU_EAT,
    MENU_DRINK,
    MENU_SLEEP,
    MENU_PLAY,
    MENU_STATS,
    MENU_CLOSE,
    MENU_COUNT
};

static const char *const item_names[MENU_COUNT] = {
    [MENU_EAT]   = "Eat",
    [MENU_DRINK] = "Drink",
    [MENU_SLEEP] = "Sleep",
    [MENU_PLAY]  = "Play",
    [MENU_STATS] = "Stats",
    [MENU_CLOSE] = "Back",
};

static void menu_draw(int cursor)
{
    printk("\n===== MENU (paused) =====\n");
    for (int i = 0; i < MENU_COUNT; i++) {
        printk("  %s %s\n", (i == cursor) ? ">" : " ", item_names[i]);
    }
    printk("[Up/Down] move   [A] select   [B] close\n");
}

static void run_action(kindral *kin, int64_t now, enum menu_item item)
{
    const char *name = kin->id.name;
    int ret;

    switch (item) {
    case MENU_EAT:
        ret = kindrals_eat(kin);
        if (ret == -EALREADY) {
            printk("%s isn't hungry right now.\n", name);
        } else {
            printk("%s munches happily. Yum!\n", name);
        }
        break;

    case MENU_DRINK:
        ret = kindrals_drink(kin);
        if (ret == -EALREADY) {
            printk("%s isn't thirsty right now.\n", name);
        } else {
            printk("%s gulps down some water. Ahh!\n", name);
        }
        break;

    case MENU_SLEEP:
        ret = kindrals_sleep(kin);
        if (ret == -EALREADY) {
            printk("%s isn't sleepy right now.\n", name);
        } else {
            printk("%s curls up and sleeps... zzz\n", name);
        }
        break;

    case MENU_PLAY:
        ret = kindrals_play(kin);
        if (ret == -EBUSY) {
            printk("%s is too tired to play.\n", name);
        } else {
            printk("%s plays and has a great time!\n", name);
        }
        break;

    case MENU_STATS:
        kindrals_display_stats(kin, now);
        break;

    default:
        break;
    }
}

void menu_run(kindral *kin, int64_t now)
{
    int cursor = 0;

    buttons_flush();    /* ignore presses made before the menu opened */
    menu_draw(cursor);

    while (1) {
        enum button b;

        if (buttons_wait(&b, K_FOREVER) != 0) {
            continue;
        }

        switch (b) {
        case BTN_UP:
            cursor = (cursor + MENU_COUNT - 1) % MENU_COUNT;
            menu_draw(cursor);
            break;

        case BTN_DOWN:
            cursor = (cursor + 1) % MENU_COUNT;
            menu_draw(cursor);
            break;

        case BTN_A:
            if (cursor != MENU_CLOSE) {
                run_action(kin, now, (enum menu_item)cursor);
            }
            printk("===== resuming =====\n");
            return;

        case BTN_B:
            printk("===== resuming =====\n");
            return;

        default:    /* left/right unused */
            break;
        }
    }
}