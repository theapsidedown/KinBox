#include "traits.h"
#include "moves.h"

static const trait_def table[KIN_TRAIT_COUNT] = {
    [KIN_TRAIT_BEAST] = {
        .name = "Beast", .primary_stat = KIN_GROW_ATK,
        .learn_count = 3,
        .learnset = { {5, MOVE_RALLY}, {12, MOVE_BODY_SLAM}, {20, MOVE_INTIMIDATE} },
    },
    [KIN_TRAIT_AERIAL] = {
        .name = "Aerial", .primary_stat = KIN_GROW_SPD,
        .learn_count = 3,
        .learnset = { {2, MOVE_GUST}, {4, MOVE_SWIFTSTEP}, {9, MOVE_WING_SLASH} },
    },
    [KIN_TRAIT_INSECT] = {
        .name = "Insect", .primary_stat = KIN_GROW_EP,
        .learn_count = 3,
        .learnset = { {4, MOVE_SAP_ARMOR}, {10, MOVE_LIFE_SIPHON}, {18, MOVE_STATIC_SAP} },
    },
    [KIN_TRAIT_VITAE] = {
        .name = "Vitae", .primary_stat = KIN_GROW_HP,
        .learn_count = 3,
        .learnset = { {6, MOVE_SOOTHING_MIST}, {10, MOVE_BUBBLE_BEAM}, {16, MOVE_HARDEN} },
    },
    [KIN_TRAIT_CONSTRUCT] = {
        .name = "Construct", .primary_stat = KIN_GROW_DEF,
        .learn_count = 3,
        .learnset = { {3, MOVE_HARDEN}, {6, MOVE_PEBBLE_TOSS}, {10, MOVE_BODY_SLAM} },
    },
    [KIN_TRAIT_SPECTRAL] = {
        .name = "Spectral", .primary_stat = KIN_GROW_NONE,
        .learn_count = 3,
        .learnset = { {5, MOVE_MIND_SIPHON}, {9, MOVE_SHADE_CLAW}, {22, MOVE_DUSK_PULSE} },
    },
};

const trait_def *traits_get(enum kindral_traits trait)
{
    return ((unsigned)trait < KIN_TRAIT_COUNT) ? &table[trait] : &table[KIN_TRAIT_BEAST];
}
