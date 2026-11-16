#include <stdio.h>
#include <string.h>
#include <errno.h>

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