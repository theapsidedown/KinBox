#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>

#include "kindrals.h"

#define SECONDS_PER_DAY 86400

static const kin_species species_table[KIN_COUNT] = {
    [KIN_001] = {
        .species_name = "Ignia",
        .type = KIN_TYPE_IGNIS,
        .trait = KIN_TRAIT_BEAST,
        .ability_count = 3,
        .abilities = {KIN_ABILITY_001, KIN_ABILITY_003, KIN_ABILITY_004},
    },
    [KIN_002] = {
        .species_name = "Aquis",
        .type = KIN_TYPE_AQUA,
        .trait = KIN_TRAIT_AQUATIC,
        .ability_count = 2,
        .abilities = {KIN_ABILITY_002, KIN_ABILITY_005},
    },
    [KIN_003] = {
        .species_name = "Vitara",
        .type = KIN_TYPE_VITA,
        .trait = KIN_TRAIT_BEAST,
        .ability_count = 3,
        .abilities = {KIN_ABILITY_001, KIN_ABILITY_007, KIN_ABILITY_008},
    },
};

const kin_species *kindrals_species(enum kindral_id id)
{
    if ((unsigned)id >= KIN_COUNT) {
        return NULL;
    }
    return &species_table[id];
}

int kindrals_init(kindral *kin, enum kindral_id id, int64_t birth_time)
{
    const kin_species *sp = kindrals_species(id);

    if (kin == NULL || sp == NULL || sp->ability_count == 0) {
        return -EINVAL;
    }

    *kin = (kindral){0};

    kin->id.species = id;
    kin->id.birth_time = birth_time;

    // Default name = species name
    snprintf(kin->id.name, sizeof(kin->id.name), "%s", sp->species_name);

    uint32_t roll = sys_rand32_get() % sp->ability_count;
    kin->id.ability = sp->abilities[roll];

    // Stats
    kin->stats.health_points = 100;
    kin->stats.exp_points = 0;
    kin->stats.attack = 10;
    kin->stats.defense = 10;
    kin->stats.speed = 10;
    kin->stats.energy_points = 100;
    kin->stats.friendship = 0;

    // Walker
    kin->walker.happiness = 100;
    kin->walker.hunger = 0;
    kin->walker.thirst = 0;
    kin->walker.sleep = 100;

    return 0;
}

int kindrals_set_name(kindral *kin, const char *name)
{
    if (kin == NULL) {
        return -EINVAL;
    }
    const kin_species *sp = kindrals_species(kin->id.species);
    if (sp == NULL) {
        return -EINVAL;
    }

    if (name == NULL || name[0] == '\0') {
        name = sp->species_name;
    }

    snprintf(kin->id.name, sizeof(kin->id.name), "%s", name);
    return 0;
}

int64_t kindrals_age_seconds(const kindral *kin, int64_t now)
{
    // Guard against a clock that's behind the birth time
    return (now > kin->id.birth_time) ? (now - kin->id.birth_time) : 0;
}

/* ------------------------------------------------------------------ */
/* Display names (order must match the enums in kindrals.h)           */
/* ------------------------------------------------------------------ */

static const char *const type_names[] = {
    "Ignis", "Aqua", "Terra", "Aero", "Vita", "Lume", "Nox"
};

static const char *const trait_names[] = {
    "Beast", "Aerial", "Aquatic", "Insect", "Spectral", "Mythical", "Construct"
};

const char *kindrals_type_name(enum kindral_types type)
{
    return ((unsigned)type < ARRAY_SIZE(type_names)) ? type_names[type] : "?";
}

const char *kindrals_trait_name(enum kindral_traits trait)
{
    return ((unsigned)trait < ARRAY_SIZE(trait_names)) ? trait_names[trait] : "?";
}

/* ------------------------------------------------------------------ */
/* Stat updaters: add delta, clamp to range, return the new value      */
/* ------------------------------------------------------------------ */

static int apply_delta(int *field, int delta, int lo, int hi)
{
    int64_t v = (int64_t)*field + delta;

    if (v < lo) {
        v = lo;
    } else if (v > hi) {
        v = hi;
    }
    *field = (int)v;
    return *field;
}

#define DEFINE_UPDATER(fn, field, lo, hi)                \
    int fn(kindral *kin, int delta)                      \
    {                                                    \
        if (kin == NULL) {                               \
            return -EINVAL;                              \
        }                                                \
        return apply_delta(&(field), delta, lo, hi);     \
    }

#define FRIENDSHIP_MAX 255

DEFINE_UPDATER(kindrals_update_health,     kin->stats.health_points,  0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_exp,        kin->stats.exp_points,     0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_attack,     kin->stats.attack,         0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_defense,    kin->stats.defense,        0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_speed,      kin->stats.speed,          0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_energy,     kin->stats.energy_points,  0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_friendship, kin->stats.friendship,     0, FRIENDSHIP_MAX)
DEFINE_UPDATER(kindrals_update_happiness,  kin->walker.happiness,     0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_hunger,     kin->walker.hunger,        0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_thirst,     kin->walker.thirst,        0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_sleep,      kin->walker.sleep,         0, KIN_METER_MAX)

/* ------------------------------------------------------------------ */
/* Care actions                                                        */
/*                                                                     */
/* Meaning of the meters:                                              */
/*   hunger / thirst : 0 = full,    100 = starving / parched           */
/*   sleep           : 100 = rested, 0 = exhausted                     */
/*   happiness       : 100 = delighted                                 */
/* ------------------------------------------------------------------ */

#define FEED_AMOUNT        30
#define DRINK_AMOUNT       30
#define SLEEP_AMOUNT       40
#define SLEEP_NOT_NEEDED   90   /* refuses to sleep when sleep >= this */
#define PLAY_ENERGY_COST   15

int kindrals_eat(kindral *kin)
{
    if (kin == NULL) {
        return -EINVAL;
    }
    if (kin->walker.hunger == 0) {
        return -EALREADY;
    }

    kindrals_update_hunger(kin, -FEED_AMOUNT);
    kindrals_update_happiness(kin, 3);
    kindrals_update_energy(kin, 5);
    kindrals_update_friendship(kin, 1);
    return 0;
}

int kindrals_drink(kindral *kin)
{
    if (kin == NULL) {
        return -EINVAL;
    }
    if (kin->walker.thirst == 0) {
        return -EALREADY;
    }

    kindrals_update_thirst(kin, -DRINK_AMOUNT);
    kindrals_update_happiness(kin, 2);
    return 0;
}

int kindrals_sleep(kindral *kin)
{
    if (kin == NULL) {
        return -EINVAL;
    }
    if (kin->walker.sleep >= SLEEP_NOT_NEEDED) {
        return -EALREADY;
    }

    kindrals_update_sleep(kin, SLEEP_AMOUNT);
    kindrals_update_energy(kin, 30);
    kindrals_update_hunger(kin, 5);     /* waking up peckish */
    kindrals_update_thirst(kin, 5);
    return 0;
}

int kindrals_play(kindral *kin)
{
    if (kin == NULL) {
        return -EINVAL;
    }
    if (kin->stats.energy_points < PLAY_ENERGY_COST) {
        return -EBUSY;
    }

    kindrals_update_energy(kin, -PLAY_ENERGY_COST);
    kindrals_update_happiness(kin, 15);
    kindrals_update_hunger(kin, 8);
    kindrals_update_thirst(kin, 8);
    kindrals_update_sleep(kin, -5);
    kindrals_update_exp(kin, 5);
    kindrals_update_friendship(kin, 1);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Time passing                                                        */
/* ------------------------------------------------------------------ */

#define NEED_WARN_LEVEL   70    /* hunger/thirst above this = unhappy */
#define SLEEP_WARN_LEVEL  30    /* sleep below this = unhappy         */

void kindrals_tick(kindral *kin)
{
    if (kin == NULL) {
        return;
    }

    kindrals_update_hunger(kin, 1);
    kindrals_update_thirst(kin, 1);
    kindrals_update_sleep(kin, -1);
    kindrals_update_energy(kin, 1);

    bool neglected = kin->walker.hunger >= NEED_WARN_LEVEL ||
                     kin->walker.thirst >= NEED_WARN_LEVEL ||
                     kin->walker.sleep  <= SLEEP_WARN_LEVEL;
    bool critical  = kin->walker.hunger >= KIN_METER_MAX ||
                     kin->walker.thirst >= KIN_METER_MAX ||
                     kin->walker.sleep  == 0;

    if (neglected) {
        kindrals_update_happiness(kin, -2);
    }

    if (critical) {
        /* Health suffers, but never drops below 1 (no death state yet) */
        if (kin->stats.health_points > 1) {
            kindrals_update_health(kin, -2);
            if (kin->stats.health_points < 1) {
                kin->stats.health_points = 1;
            }
        }
    } else if (!neglected) {
        kindrals_update_health(kin, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Stats display                                                       */
/* ------------------------------------------------------------------ */

static void print_bar(const char *label, int value)
{
    char bar[11];
    int filled = (value * 10) / KIN_METER_MAX;

    for (int i = 0; i < 10; i++) {
        bar[i] = (i < filled) ? '#' : '-';
    }
    bar[10] = '\0';

    printk("  %s [%s] %d/%d\n", label, bar, value, KIN_METER_MAX);
}

void kindrals_display_stats(const kindral *kin, int64_t now)
{
    if (kin == NULL) {
        return;
    }

    const kin_species *sp = kindrals_species(kin->id.species);
    uint32_t age = (uint32_t)kindrals_age_seconds(kin, now);

    printk("\n========== %s ==========\n", kin->id.name);
    if (sp != NULL) {
        printk("Species: %s   Type: %s   Trait: %s\n",
               sp->species_name, kindrals_type_name(sp->type),
               kindrals_trait_name(sp->trait));
    }
    printk("Ability: %d   Age: %ud %uh %um %us\n", (int)kin->id.ability,
           (unsigned)(age / 86400), (unsigned)((age % 86400) / 3600),
           (unsigned)((age % 3600) / 60), (unsigned)(age % 60));

    printk("-- Care --\n");
    print_bar("Happiness", kin->walker.happiness);
    print_bar("Hunger   ", kin->walker.hunger);
    print_bar("Thirst   ", kin->walker.thirst);
    print_bar("Rest     ", kin->walker.sleep);

    printk("-- Stats --\n");
    print_bar("HP       ", kin->stats.health_points);
    print_bar("Energy   ", kin->stats.energy_points);
    printk("  EXP: %d   ATK: %d   DEF: %d   SPD: %d   Friendship: %d\n",
           kin->stats.exp_points, kin->stats.attack, kin->stats.defense,
           kin->stats.speed, kin->stats.friendship);
    printk("=============================\n");
}