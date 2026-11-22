#ifndef TRAITS_H
#define TRAITS_H

#include <stdint.h>
#include "kindrals.h"

/*
 * A trait does three things:
 *  1. Growth weights: per-level stat gains are base_gain * weight / 100.
 *     Every trait's five weights add up to 500, so no trait is strictly better.
 *  2. Trait learnset: extra moves every kindral of that trait learns, on top
 *     of its species learnset.
 *  3. Trait matchup: +/-15% damage between traits (see traits.c).
 */
typedef struct {
    const char *name;
    uint8_t hp, atk, def, spd, ep;      /* growth weights, percent */
    uint8_t learn_count;
    kin_learn learnset[6];
} trait_def;

const trait_def *traits_get(enum kindral_traits trait);

/* 115 = attacker trait has the edge, 85 = defender trait has the edge, 100 = even */
int traits_matchup_pct(enum kindral_traits attacker, enum kindral_traits defender);

#endif
