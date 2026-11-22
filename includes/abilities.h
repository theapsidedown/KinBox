#ifndef ABILITIES_H
#define ABILITIES_H

#include <stdint.h>
#include <stdbool.h>
#include "kindrals.h"
#include "moves.h"

struct battle_mon;

/*
 * Battle abilities. Add new ones by adding a row in abilities.c: fill in any of
 * the simple data fields, or write a small hook function for custom logic.
 * Hooks return a damage multiplier in percent (100 = unchanged).
 */
typedef struct {
    const char *name;
    const char *desc;

    int (*out_dmg_pct)(const struct battle_mon *self, const move_def *mv); /* damage it deals   */
    int (*in_dmg_pct)(const struct battle_mon *self, const move_def *mv);  /* damage it takes   */
    int (*speed_pct)(const struct battle_mon *self);                       /* turn-order speed  */

    int8_t  accuracy_bonus;      /* added to the accuracy of its own moves          */
    uint8_t end_turn_heal_pct;   /* % of max HP regained at the end of each turn    */
    uint8_t end_turn_ep;         /* EP regained at the end of each turn             */
    uint8_t reflect_pct;         /* % of damage taken that hits the attacker        */
    uint8_t drain_ep_on_hit;     /* EP the attacker loses when it damages this mon  */
    bool    survive_ko_once;     /* survives one lethal hit with 1 HP               */
} ability_def;

const ability_def *abilities_get(enum kindral_ability id);

#endif
