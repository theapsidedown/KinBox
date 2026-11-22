#ifndef TRAITS_H
#define TRAITS_H

#include <stdint.h>
#include "kindrals.h"

/* A trait defines one primary per-level growth bonus and an extra learnset.
 * Spectral has no growth bonus; its passive is implemented by the battle engine.
 */
enum kin_growth_stat {
    KIN_GROW_NONE,
    KIN_GROW_HP,
    KIN_GROW_ATK,
    KIN_GROW_DEF,
    KIN_GROW_SPD,
    KIN_GROW_EP
};

typedef struct {
    const char *name;
    enum kin_growth_stat primary_stat;
    uint8_t learn_count;
    kin_learn learnset[6];
} trait_def;

const trait_def *traits_get(enum kindral_traits trait);

#endif
