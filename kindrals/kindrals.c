
#include "kindrals.h"

#include <string.h>

void kindrals_init(
    kindral *kin,
    enum kindral_id id,
    int age,
    const char *name,
    enum kindral_ability ability
) {
    if (kin == NULL || name == NULL) {
        return;
    }

    // Prevent buffer overflow
    if (strlen(name) >= sizeof(kin->id.name)) {
        return;
    }

    // Clear all existing data
    *kin = (kindral){0};

    // ID
    kin->id.kin_id = id;
    kin->id.age = age;
    strcpy(kin->id.name, name);
    kin->id.ability = ability;

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
}
