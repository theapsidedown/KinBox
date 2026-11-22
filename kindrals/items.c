#include <errno.h>
#include "items.h"

static const item_def table[ITEM_COUNT] = {
    [ITEM_SUNBERRY] = {
        .name = "Sunberry", .desc = "Restores 30 HP.",
        .kind = ITEM_K_HEAL_HP, .flags = ITEM_F_BATTLE | ITEM_F_FIELD, .value = 30, .rarity = 0,
        .give_friend = 1, .give_happy = 2, .recv_friend = 1, .recv_happy = 5,
    },
    [ITEM_MOON_TONIC] = {
        .name = "Moon Tonic", .desc = "Restores 30 EP.",
        .kind = ITEM_K_HEAL_EP, .flags = ITEM_F_BATTLE | ITEM_F_FIELD, .value = 30, .rarity = 0,
        .give_friend = 1, .give_happy = 2, .recv_friend = 1, .recv_happy = 4,
    },
    [ITEM_POWER_SEED] = {
        .name = "Power Seed", .desc = "Battle: raises Attack one stage.",
        .kind = ITEM_K_BOOST, .flags = ITEM_F_BATTLE, .stat = BSTAT_ATK, .value = 1, .rarity = 1,
        .give_friend = 1, .give_happy = 2, .recv_friend = 2, .recv_happy = 5,
    },
    [ITEM_GUARD_PEBBLE] = {
        .name = "Guard Pebble", .desc = "Battle: raises Defense one stage.",
        .kind = ITEM_K_BOOST, .flags = ITEM_F_BATTLE, .stat = BSTAT_DEF, .value = 1, .rarity = 1,
        .give_friend = 1, .give_happy = 2, .recv_friend = 2, .recv_happy = 5,
    },
    [ITEM_SWIFT_FEATHER] = {
        .name = "Swift Feather", .desc = "Battle: raises Speed one stage.",
        .kind = ITEM_K_BOOST, .flags = ITEM_F_BATTLE, .stat = BSTAT_SPD, .value = 1, .rarity = 1,
        .give_friend = 1, .give_happy = 2, .recv_friend = 2, .recv_happy = 5,
    },
    [ITEM_HONEY_BUN] = {
        .name = "Honey Bun", .desc = "A sweet snack. Eases hunger.",
        .kind = ITEM_K_FOOD, .flags = ITEM_F_FIELD, .value = 20, .rarity = 0,
        .give_friend = 2, .give_happy = 5, .recv_friend = 3, .recv_happy = 15,
    },
    [ITEM_PLUSH_TOY] = {
        .name = "Plush Toy", .desc = "A cuddly gift.",
        .kind = ITEM_K_GIFT, .rarity = 1,
        .give_friend = 2, .give_happy = 5, .recv_friend = 5, .recv_happy = 10,
    },
    [ITEM_SHINY_GEM] = {
        .name = "Shiny Gem", .desc = "Sparkly! A wonderful gift.",
        .kind = ITEM_K_GIFT, .rarity = 2,
        .give_friend = 4, .give_happy = 10, .recv_friend = 10, .recv_happy = 25,
    },
    [ITEM_ELIXIR] = {
        .name = "Elixir", .desc = "Fully restores HP and EP.",
        .kind = ITEM_K_ELIXIR, .flags = ITEM_F_BATTLE | ITEM_F_FIELD, .rarity = 2,
        .give_friend = 2, .give_happy = 4, .recv_friend = 4, .recv_happy = 8,
    },
    [ITEM_GROWTH_CANDY] = {
        .name = "Growth Candy", .desc = "Gives 60 exp.",
        .kind = ITEM_K_EXP, .flags = ITEM_F_FIELD, .value = 60, .rarity = 3,
        .give_friend = 3, .give_happy = 5, .recv_friend = 6, .recv_happy = 10,
    },
    [ITEM_STALE_CRUST] = {
        .name = "Stale Crust", .desc = "Rock hard. A rude gift.",
        .kind = ITEM_K_GIFT, .rarity = 0,
        .give_friend = 0, .give_happy = -10, .recv_friend = 0, .recv_happy = 0,
    },
    [ITEM_OLD_BOOT] = {
        .name = "Old Boot", .desc = "Smells awful. A rude gift.",
        .kind = ITEM_K_GIFT, .rarity = 0,
        .give_friend = 0, .give_happy = -15, .recv_friend = 0, .recv_happy = 0,
    },
};

const item_def *items_get(uint8_t id)
{
    if (id == ITEM_NONE || id >= ITEM_COUNT || table[id].name == NULL) {
        return NULL;
    }
    return &table[id];
}

int items_use_field(kindral *kin, uint8_t id)
{
    const item_def *it = items_get(id);

    if (kin == NULL || it == NULL) {
        return -EINVAL;
    }
    if (!(it->flags & ITEM_F_FIELD)) {
        return -ENOTSUP;
    }

    switch (it->kind) {
    case ITEM_K_HEAL_HP:
        if (kin->stats.health_points >= kin->stats.max_hp) {
            return -EALREADY;
        }
        kindrals_update_health(kin, it->value);
        break;
    case ITEM_K_HEAL_EP:
        if (kin->stats.energy_points >= kin->stats.max_ep) {
            return -EALREADY;
        }
        kindrals_update_energy(kin, it->value);
        break;
    case ITEM_K_ELIXIR:
        if (kin->stats.health_points >= kin->stats.max_hp &&
            kin->stats.energy_points >= kin->stats.max_ep) {
            return -EALREADY;
        }
        kindrals_update_health(kin, kin->stats.max_hp);
        kindrals_update_energy(kin, kin->stats.max_ep);
        break;
    case ITEM_K_FOOD:
        if (kin->walker.hunger == 0) {
            return -EALREADY;
        }
        kindrals_update_hunger(kin, -it->value);
        kindrals_update_happiness(kin, 5);
        break;
    case ITEM_K_EXP:
        kindrals_add_exp(kin, it->value);
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}

void items_apply_gift_giver(kindral *kin, uint8_t id)
{
    const item_def *it = items_get(id);

    if (kin == NULL || it == NULL) {
        return;
    }
    kindrals_update_friendship(kin, it->give_friend);
    kindrals_update_happiness(kin, it->give_happy);
}

void items_apply_gift_receiver(kindral *kin, uint8_t id)
{
    const item_def *it = items_get(id);

    if (kin == NULL || it == NULL) {
        return;
    }
    kindrals_update_friendship(kin, it->recv_friend);
    kindrals_update_happiness(kin, it->recv_happy);
}

uint8_t items_random_of_rarity(int rarity, uint32_t rnd)
{
    uint8_t pool[ITEM_COUNT];
    int n = 0;

    for (int i = ITEM_NONE + 1; i < ITEM_COUNT; i++) {
        if (table[i].name != NULL && table[i].rarity == rarity) {
            pool[n++] = (uint8_t)i;
        }
    }
    if (n == 0) {
        return ITEM_SUNBERRY;
    }
    return pool[rnd % n];
}
