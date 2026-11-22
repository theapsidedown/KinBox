#include "traits.h"
#include "moves.h"

static const trait_def table[KIN_TRAIT_COUNT] = {
    [KIN_TRAIT_BEAST] = {
        .name = "Beast",     .hp = 110, .atk = 125, .def = 95,  .spd = 90,  .ep = 80,
        .learn_count = 3,
        .learnset = { {5, MOVE_RALLY}, {12, MOVE_BODY_SLAM}, {20, MOVE_INTIMIDATE} },
    },
    [KIN_TRAIT_AERIAL] = {
        .name = "Aerial",    .hp = 80,  .atk = 95,  .def = 75,  .spd = 150, .ep = 100,
        .learn_count = 3,
        .learnset = { {2, MOVE_GUST}, {4, MOVE_SWIFTSTEP}, {9, MOVE_WING_SLASH} },
    },
    [KIN_TRAIT_AQUATIC] = {
        .name = "Aquatic",   .hp = 115, .atk = 95,  .def = 100, .spd = 90,  .ep = 100,
        .learn_count = 3,
        .learnset = { {6, MOVE_SOOTHING_MIST}, {10, MOVE_BUBBLE_BEAM}, {16, MOVE_HARDEN} },
    },
    [KIN_TRAIT_INSECT] = {
        .name = "Insect",    .hp = 85,  .atk = 115, .def = 100, .spd = 130, .ep = 70,
        .learn_count = 3,
        .learnset = { {4, MOVE_SAP_ARMOR}, {10, MOVE_LIFE_SIPHON}, {18, MOVE_STATIC_SAP} },
    },
    [KIN_TRAIT_SPECTRAL] = {
        .name = "Spectral",  .hp = 75,  .atk = 110, .def = 80,  .spd = 115, .ep = 120,
        .learn_count = 3,
        .learnset = { {5, MOVE_MIND_SIPHON}, {9, MOVE_SHADE_CLAW}, {22, MOVE_DUSK_PULSE} },
    },
    [KIN_TRAIT_MYTHICAL] = {
        .name = "Mythical",  .hp = 100, .atk = 110, .def = 90,  .spd = 95,  .ep = 105,
        .learn_count = 3,
        .learnset = { {3, MOVE_GLINT}, {5, MOVE_DAZZLE}, {20, MOVE_RADIANT_BEAM} },
    },
    [KIN_TRAIT_CONSTRUCT] = {
        .name = "Construct", .hp = 120, .atk = 90,  .def = 150, .spd = 60,  .ep = 80,
        .learn_count = 3,
        .learnset = { {3, MOVE_HARDEN}, {6, MOVE_PEBBLE_TOSS}, {10, MOVE_BODY_SLAM} },
    },
};

/*
 * Trait ring (same scheme as the type chart):
 *   Aerial > Beast > Insect > Construct > Aquatic > Spectral > Mythical > Aerial
 * Each trait is +15% against the next two and -15% against the previous two.
 * Rows = attacker trait, columns = defender trait, enum order:
 *        Beast Aerial Aquatic Insect Spectral Mythical Construct
 */
static const uint8_t matchup[KIN_TRAIT_COUNT][KIN_TRAIT_COUNT] = {
    /* Beast     */ {100,  85, 100, 115, 100,  85, 115},
    /* Aerial    */ {115, 100, 100, 115,  85,  85, 100},
    /* Aquatic   */ {100, 100, 100,  85, 115, 115,  85},
    /* Insect    */ { 85,  85, 115, 100, 100, 100, 115},
    /* Spectral  */ {100, 115,  85, 100, 100, 115,  85},
    /* Mythical  */ {115, 115,  85, 100,  85, 100, 100},
    /* Construct */ { 85, 100, 115,  85, 115, 100, 100},
};

const trait_def *traits_get(enum kindral_traits trait)
{
    return ((unsigned)trait < KIN_TRAIT_COUNT) ? &table[trait] : &table[KIN_TRAIT_BEAST];
}

int traits_matchup_pct(enum kindral_traits a, enum kindral_traits d)
{
    if ((unsigned)a >= KIN_TRAIT_COUNT || (unsigned)d >= KIN_TRAIT_COUNT) {
        return 100;
    }
    return matchup[a][d];
}
