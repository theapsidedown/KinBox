
#ifndef KINDRALS_H
#define KINDRALS_H

#include <stdint.h>

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
    enum kindral_id kin_id;
    int age;
    char name[20];
    enum kindral_ability ability;
} kin_id;

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

// Initialization
void kindrals_init(
    kindral *kin,
    enum kindral_id id,
    int age,
    const char *name,
    enum kindral_ability ability
);

#endif
