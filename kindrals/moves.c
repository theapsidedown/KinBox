#include "moves.h"
#include "kindrals.h"

#define N KIN_TYPE_NONE
#define IG KIN_TYPE_IGNIS
#define AQ KIN_TYPE_AQUA
#define TE KIN_TYPE_TERRA
#define AE KIN_TYPE_AERO
#define FL KIN_TYPE_FLORA
#define LU KIN_TYPE_LUME
#define NX KIN_TYPE_NOX

#define DMG(nm, ty, pw, ac, ep, ...) \
    { .name = nm, .type = ty, .cat = MOVE_CAT_DAMAGE, .power = pw, .accuracy = ac, .ep_cost = ep, __VA_ARGS__ }
#define DRAIN(nm, ty, amt, ac, ep) \
    { .name = nm, .type = ty, .cat = MOVE_CAT_EP_DRAIN, .power = amt, .accuracy = ac, .ep_cost = ep }
#define HEAL(nm, ty, pct, ep) \
    { .name = nm, .type = ty, .cat = MOVE_CAT_HEAL, .power = pct, .accuracy = 100, .ep_cost = ep }
#define STAT(nm, ty, st, stg, self, ac, ep) \
    { .name = nm, .type = ty, .cat = MOVE_CAT_STAT, .accuracy = ac, .ep_cost = ep, \
      .stat = st, .stages = stg, .on_self = self }

static const move_def table[MOVE_COUNT] = {
    [MOVE_SCRATCH]      = DMG("Scratch",      N,  30, 100,  3),
    [MOVE_BODY_SLAM]    = DMG("Body Slam",    N,  55,  95, 10),

    [MOVE_EMBER]        = DMG("Ember",        IG, 35, 100,  6),
    [MOVE_FLAME_BURST]  = DMG("Flame Burst",  IG, 50,  90, 10),
    [MOVE_INFERNO_RUSH] = DMG("Inferno Rush", IG, 80,  85, 18),

    [MOVE_BUBBLE_BEAM]  = DMG("Bubble Beam",  AQ, 35, 100,  6),
    [MOVE_AQUA_JET]     = DMG("Aqua Jet",     AQ, 40,  95,  7, .priority = 1),
    [MOVE_TIDAL_CRASH]  = DMG("Tidal Crash",  AQ, 85,  85, 18),

    [MOVE_PEBBLE_TOSS]  = DMG("Pebble Toss",  TE, 30, 100,  5),
    [MOVE_EARTHQUAKE]   = DMG("Earthquake",   TE, 100, 85, 24),

    [MOVE_GUST]         = DMG("Gust",         AE, 30, 100,  5),
    [MOVE_WING_SLASH]   = DMG("Wing Slash",   AE, 55,  95, 10),

    [MOVE_VINE_LASH]    = DMG("Vine Lash",    FL, 40, 100,  7),
    [MOVE_LIFE_SIPHON]  = DMG("Life Siphon",  FL, 45, 100,  9, .drain_pct = 50),
    [MOVE_BLOOM_BURST]  = DMG("Bloom Burst",  FL, 75,  90, 16),

    [MOVE_GLINT]        = DMG("Glint",        LU, 35, 100,  6),
    [MOVE_RADIANT_BEAM] = DMG("Radiant Beam", LU, 70,  90, 15),

    [MOVE_SHADE_CLAW]   = DMG("Shade Claw",   NX, 40, 100,  7),
    [MOVE_DUSK_PULSE]   = DMG("Dusk Pulse",   NX, 65,  95, 13),

    /* Special EP moves: they hit EP, not HP */
    [MOVE_MIND_SIPHON]  = DRAIN("Mind Siphon", NX, 25, 95, 8),
    [MOVE_DAZZLE]       = DRAIN("Dazzle",      LU, 20, 100, 6),
    [MOVE_STATIC_SAP]   = DRAIN("Static Sap",  AE, 18, 100, 5),

    [MOVE_MEND]         = HEAL("Mend",          FL, 30, 12),
    [MOVE_SOOTHING_MIST]= HEAL("Calm Mist", AQ, 25, 10),

    [MOVE_HARDEN]       = STAT("Harden",     N,  BSTAT_DEF, +1, true,  100, 5),
    [MOVE_RALLY]        = STAT("Rally",      N,  BSTAT_ATK, +1, true,  100, 5),
    [MOVE_SWIFTSTEP]    = STAT("Swiftstep",  N,  BSTAT_SPD, +1, true,  100, 5),
    [MOVE_INTIMIDATE]   = STAT("Intimidate", N,  BSTAT_ATK, -1, false, 95,  5),
    [MOVE_SAP_ARMOR]    = STAT("Sap Armor",  TE, BSTAT_DEF, -1, false, 95,  6),

    [MOVE_STRUGGLE]     = DMG("Struggle",     N,  25, 100, 0, .recoil = true),
};

const move_def *moves_get(uint8_t id)
{
    if (id == MOVE_NONE || id >= MOVE_COUNT || table[id].name == NULL) {
        return NULL;
    }
    return &table[id];
}

const char *moves_cat_name(uint8_t cat)
{
    switch (cat) {
    case MOVE_CAT_DAMAGE:   return "Damage";
    case MOVE_CAT_EP_DRAIN: return "EP Drain";
    case MOVE_CAT_HEAL:     return "Heal";
    case MOVE_CAT_STAT:     return "Status";
    default:                return "?";
    }
}
