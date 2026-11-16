/*
 * KinBox main.c
 * Created by: Aps
 */
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/console/console.h>
#include <zephyr/sys/printk.h>

#include "kindrals.h"

#define MIN_VALID_TIME 1735689600   // 2025-01-01, anything earlier means the clock isn't set

// Display names for the enums (indexed by enum value, so keep the order in sync with kindrals.h)
static const char *const type_names[] = {
    "Ignis", "Aqua", "Terra", "Aero", "Vita", "Lume", "Nox"
};

static const char *const trait_names[] = {
    "Beast", "Aerial", "Aquatic", "Insect", "Spectral", "Mythical", "Construct"
};

int main(void)
{
    // int64_t now = time(NULL);           // or read from your RTC driver
    // if (now < MIN_VALID_TIME) {
    //     LOG_ERR("Clock not set, cannot hatch");
    //     return -ETIME;
    // }

    int64_t now = 1767225600;  // placeholder for testing, replace with RTC time

    LOG_INF("KinBox is running!");
    LOG_INF("Meet your first kindral!");

    // 1. Hatch: creates the kindral, rolls its ability, sets default name + stats
    kindral kin;
    int ret = kindrals_init(&kin, KIN_001, now);
    if (ret != 0) {
        LOG_ERR("kindrals_init failed: %d", ret);
        return ret;
    }

    // 2. Look up the species data (lives in flash, not in the kindral)
    const kin_species *sp = kindrals_species(kin.id.species);
    if (sp == NULL) {
        LOG_ERR("Unknown species: %d", (int)kin.id.species);
        return -EINVAL;
    }

    // 3. Show stats + birth time on the display (logged for now)
    LOG_INF("A kindral hatched!");
    LOG_INF("Species:  %s", sp->species_name);
    LOG_INF("Type:     %s", type_names[sp->type]);
    LOG_INF("Trait:    %s", trait_names[sp->trait]);
    LOG_INF("Ability:  %d", (int)kin.id.ability);
    LOG_INF("Born:     %lld (unix time)", (long long)kin.id.birth_time);

    // 4. Prompt for a name (may come back empty -> species name)
    console_getline_init();
    printk("\nName your %s (max %d chars, Enter for default): ",
           sp->species_name, KIN_NAME_LEN - 1);

    char *user_input = console_getline();   // blocks until Enter is pressed

    if (user_input != NULL && strlen(user_input) >= KIN_NAME_LEN) {
        printk("\nName too long, it will be cut to %d characters.\n", KIN_NAME_LEN - 1);
    }

    ret = kindrals_set_name(&kin, user_input);
    if (ret != 0) {
        LOG_ERR("kindrals_set_name failed: %d", ret);
        return ret;
    }

    LOG_INF("Kindral initialized!");
    LOG_INF("Name:     %s", kin.id.name);
    LOG_INF("Age:      %lld s", (long long)kindrals_age_seconds(&kin, now));

    LOG_INF("HP:       %d", kin.stats.health_points);
    LOG_INF("EXP:      %d", kin.stats.exp_points);
    LOG_INF("Attack:   %d", kin.stats.attack);
    LOG_INF("Defense:  %d", kin.stats.defense);
    LOG_INF("Speed:    %d", kin.stats.speed);
    LOG_INF("Energy:   %d", kin.stats.energy_points);
    LOG_INF("Friend:   %d", kin.stats.friendship);

    LOG_INF("Happiness: %d", kin.walker.happiness);
    LOG_INF("Hunger:    %d", kin.walker.hunger);
    LOG_INF("Thirst:    %d", kin.walker.thirst);
    LOG_INF("Sleep:     %d", kin.walker.sleep);

    // 5. Next step: save_kindral(&kin) to NVS, only after naming

    return 0;
}