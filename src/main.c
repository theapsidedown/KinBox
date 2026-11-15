
/*
 * KinBox main.c
 * Created by: Aps
 */

#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#include "kindrals.h"

int main(void)
{
    LOG_INF("KinBox is running!");
    LOG_INF("Meet your first kindral!");

    // Actually create a Kindral object in memory
    kindral newKin;

    // Pass its address to the initialization function
    kindrals_init(
        &newKin,
        KIN_001,
        0,
        "Lors",
        KIN_ABILITY_001
    );

    LOG_INF("Kindral initialized!");

    LOG_INF("Kindral name: %s", newKin.id.name);
    LOG_INF("Kindral age: %d", newKin.id.age);
    LOG_INF("Kindral ability: %d", newKin.id.ability);
    LOG_INF("Kindral health points: %d", newKin.stats.health_points);
    LOG_INF("Kindral exp points: %d", newKin.stats.exp_points);
    LOG_INF("Kindral attack: %d", newKin.stats.attack);
    LOG_INF("Kindral walker happiness: %d", newKin.walker.happiness);

    return 0;
}
