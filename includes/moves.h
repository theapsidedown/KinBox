#ifndef MOVES_H
#define MOVES_H

#include <stdint.h>
#include <stdbool.h>
#include "kindrals.h"

enum move_cat {
    MOVE_CAT_DAMAGE,     /* hits the foe's HP (power = base power)              */
    MOVE_CAT_EP_DRAIN,   /* special: drains the foe's EP (power = EP drained)   */
    MOVE_CAT_HEAL,       /* heals the user (power = % of max HP)                */
    MOVE_CAT_STAT,       /* raises/lowers a battle stat stage                   */
};

enum battle_stat { BSTAT_ATK, BSTAT_DEF, BSTAT_SPD, BSTAT_COUNT };

typedef enum {
    MOVE_NONE = 0,
    MOVE_SCRATCH, MOVE_BODY_SLAM,
    MOVE_EMBER, MOVE_FLAME_BURST, MOVE_INFERNO_RUSH,
    MOVE_BUBBLE_BEAM, MOVE_AQUA_JET, MOVE_TIDAL_CRASH,
    MOVE_PEBBLE_TOSS, MOVE_EARTHQUAKE,
    MOVE_GUST, MOVE_WING_SLASH,
    MOVE_VINE_LASH, MOVE_LIFE_SIPHON, MOVE_BLOOM_BURST,
    MOVE_GLINT, MOVE_RADIANT_BEAM,
    MOVE_SHADE_CLAW, MOVE_DUSK_PULSE,
    MOVE_MIND_SIPHON, MOVE_DAZZLE, MOVE_STATIC_SAP,
    MOVE_MEND, MOVE_SOOTHING_MIST,
    MOVE_HARDEN, MOVE_RALLY, MOVE_SWIFTSTEP, MOVE_INTIMIDATE, MOVE_SAP_ARMOR,
    MOVE_STRUGGLE,          /* fallback when nothing is affordable */
    MOVE_COUNT
} move_id;

typedef struct {
    const char *name;        /* <= 12 chars so it fits the battle menu */
    uint8_t type;            /* enum kindral_types (KIN_TYPE_NONE = typeless) */
    uint8_t cat;             /* enum move_cat */
    uint8_t power;
    uint8_t accuracy;        /* 1..100 */
    uint8_t ep_cost;         /* fixed Energy Points consumed per use */
    int8_t  priority;        /* +1 acts before normal moves */
    uint8_t drain_pct;       /* % of damage dealt that heals the user */
    uint8_t stat;            /* enum battle_stat (STAT moves) */
    int8_t  stages;          /* +/- stages (STAT moves) */
    bool    on_self;         /* STAT moves: true = user, false = foe */
    bool    recoil;          /* user takes 1/8 max HP (Struggle) */
} move_def;

const move_def *moves_get(uint8_t id);   /* NULL if invalid */
const char *moves_cat_name(uint8_t cat);

#endif
