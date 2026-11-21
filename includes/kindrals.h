#ifndef KINDRALS_H
#define KINDRALS_H

#include <stdint.h>
#include <zephyr/random/random.h>

#define KIN_NAME_LEN 20
#define KIN_MAX_ABILITIES 4
#define KIN_COUNT 10

enum kindral_types {
    KIN_TYPE_IGNIS,
    KIN_TYPE_AQUA,
    KIN_TYPE_TERRA,
    KIN_TYPE_AERO,
    KIN_TYPE_VITA,
    KIN_TYPE_LUME,
    KIN_TYPE_NOX
};

enum kindral_traits {
    KIN_TRAIT_BEAST,
    KIN_TRAIT_AERIAL,
    KIN_TRAIT_AQUATIC,
    KIN_TRAIT_INSECT,
    KIN_TRAIT_SPECTRAL,
    KIN_TRAIT_MYTHICAL,
    KIN_TRAIT_CONSTRUCT
};

enum kindral_id {
    KIN_001,
    KIN_002,
    KIN_003,
    KIN_004,
    KIN_005,
    KIN_006,
    KIN_007,
    KIN_008,
    KIN_009,
    KIN_010
};

enum kindral_ability {
    KIN_ABILITY_001,
    KIN_ABILITY_002,
    KIN_ABILITY_003,
    KIN_ABILITY_004,
    KIN_ABILITY_005,
    KIN_ABILITY_006,
    KIN_ABILITY_007,
    KIN_ABILITY_008,
    KIN_ABILITY_009,
    KIN_ABILITY_010
};

typedef struct {
    int64_t birth_time;            // Unix epoch seconds (UTC), set once at init
    enum kindral_id species;       // renamed from 'id'
    enum kindral_ability ability;
    char name[KIN_NAME_LEN];
} kin_id;

typedef struct {
    const char *species_name;
    enum kindral_types type;
    enum kindral_traits trait;
    uint8_t ability_count;
    enum kindral_ability abilities[KIN_MAX_ABILITIES];
} kin_species;

const kin_species *kindrals_species(enum kindral_id id);

typedef struct {
    int health_points;
    int exp_points;
    int attack;
    int defense;
    int speed;
    int energy_points;
    int friendship;
} kin_stats;

typedef struct {
    int happiness;
    int hunger;
    int thirst;
    int sleep;
} kin_walker;

typedef struct {
    kin_id id;
    kin_stats stats;
    kin_walker walker;
} kindral;

int kindrals_init(kindral *kin, enum kindral_id id, int64_t birth_time);
int kindrals_set_name(kindral *kin, const char *name);
int64_t kindrals_age_seconds(const kindral *kin, int64_t now);

int kindrals_update_health(kindral *kin, int delta);
int kindrals_update_exp(kindral *kin, int delta);
int kindrals_update_attack(kindral *kin, int delta);
int kindrals_update_defense(kindral *kin, int delta);
int kindrals_update_speed(kindral *kin, int delta);
int kindrals_update_energy(kindral *kin, int delta);
int kindrals_update_friendship(kindral *kin, int delta);
int kindrals_update_happiness(kindral *kin, int delta);
int kindrals_update_hunger(kindral *kin, int delta);
int kindrals_update_thirst(kindral *kin, int delta);
int kindrals_update_sleep(kindral *kin, int delta);


/* ---- Care / game logic ---- */

#define KIN_METER_MAX 100       /* hunger, thirst, sleep, happiness, energy, hp */

/* Display names for the enums */
const char *kindrals_type_name(enum kindral_types type);
const char *kindrals_trait_name(enum kindral_traits trait);

/*
 * Care actions. Return 0 on success, or:
 *   -EALREADY  the kindral doesn't need it (eat/drink/sleep)
 *   -EBUSY     too tired (play)
 * Nothing is changed when an error is returned.
 */
int kindrals_eat(kindral *kin);
int kindrals_drink(kindral *kin);
int kindrals_sleep(kindral *kin);
int kindrals_play(kindral *kin);

/* One step of time passing: needs build up, mood and health react. */
void kindrals_tick(kindral *kin);

/* Print the full stat sheet (text). 'now' is unix seconds, used for age. */
void kindrals_display_stats(const kindral *kin, int64_t now);

#endif