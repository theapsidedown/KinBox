#ifndef ATTACKS_H
#define ATTACKS_H

#include <stdint.h>
#include <zephyr/random/random.h>

#include "kindrals.h"

const attacks attacks_table[ATTACK_COUNT] = {
    [ATTACK_001] = {
        .attack_name = "Flame Burst",
        .type = ATTACK_TYPE_FIRE,
        .power = 50,
        .accuracy = 90,
    },
    [ATTACK_002] = {
        .attack_name = "Aqua Jet",
        .type = ATTACK_TYPE_WATER,
        .power = 40,
        .accuracy = 95,
    },
    [ATTACK_003] = {
        .attack_name = "Earthquake",
        .type = ATTACK_TYPE_GROUND,
        .power = 100,
        .accuracy = 85,
    },
    [ATTACK_004] = {
        .attack_name = "Gust",
        .type = ATTACK_TYPE_AIR,
        .power = 30,
        .accuracy = 100,
    },
};







#endif /* ATTACKS_H */