#ifndef ITEMS_H
#define ITEMS_H

#include <stdint.h>
#include <stdbool.h>
#include "kindrals.h"
#include "moves.h"

typedef enum {
    ITEM_NONE = 0,
    ITEM_SUNBERRY,      /* +30 HP                         */
    ITEM_MOON_TONIC,    /* +30 EP                         */
    ITEM_POWER_SEED,    /* battle: attack +1 stage        */
    ITEM_GUARD_PEBBLE,  /* battle: defense +1 stage       */
    ITEM_SWIFT_FEATHER, /* battle: speed +1 stage         */
    ITEM_HONEY_BUN,     /* snack: hunger down, great gift */
    ITEM_PLUSH_TOY,     /* gift                           */
    ITEM_SHINY_GEM,     /* rare gift / trade bait         */
    ITEM_ELIXIR,        /* full HP + EP                   */
    ITEM_GROWTH_CANDY,  /* +60 exp (field only)           */
    ITEM_STALE_CRUST,   /* bad gift                       */
    ITEM_OLD_BOOT,      /* bad gift                       */
    ITEM_COUNT
} item_id;

enum item_kind {
    ITEM_K_HEAL_HP,
    ITEM_K_HEAL_EP,
    ITEM_K_BOOST,       /* battle stat stage (value = stages, stat = enum battle_stat) */
    ITEM_K_ELIXIR,
    ITEM_K_FOOD,        /* value = hunger removed */
    ITEM_K_EXP,
    ITEM_K_GIFT,        /* no use effect, only gifting/trading */
};

#define ITEM_F_BATTLE  0x01
#define ITEM_F_FIELD   0x02

typedef struct {
    const char *name;           /* <= 14 chars */
    const char *desc;           /* short, wraps on a 128px screen */
    uint8_t kind;
    uint8_t flags;
    uint8_t stat;
    uint8_t rarity;             /* 0 common .. 3 rare (walk drops) */
    int16_t value;
    /* Gift effects. Receiver gets recv_*, giver gets give_*. Bad gifts have
     * a negative give_happy (the GIVER feels bad about it). */
    int8_t give_friend, give_happy;
    int8_t recv_friend, recv_happy;
} item_def;

const item_def *items_get(uint8_t id);

/* Outside battle. 0 = used, -EALREADY = would do nothing, -ENOTSUP = not a field item. */
int items_use_field(kindral *kin, uint8_t id);

void items_apply_gift_giver(kindral *kin, uint8_t id);
void items_apply_gift_receiver(kindral *kin, uint8_t id);

/* Random item of the given rarity (0..3). */
uint8_t items_random_of_rarity(int rarity, uint32_t rnd);

#endif
