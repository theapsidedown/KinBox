#include "abilities.h"
#include "battle.h"

static bool low_hp(const battle_mon *m)
{
    return m->hp * 3 <= m->max_hp;
}

static int tidal_surge_out(const battle_mon *self, const move_def *mv)
{
    return (mv->type == KIN_TYPE_AQUA && low_hp(self)) ? 150 : 100;
}

static int ember_soul_out(const battle_mon *self, const move_def *mv)
{
    return (mv->type == KIN_TYPE_IGNIS && low_hp(self)) ? 150 : 100;
}

static int steadfast_in(const battle_mon *self, const move_def *mv)
{
    (void)self; (void)mv;
    return 90;
}

static int quick_start_speed(const battle_mon *self)
{
    return (self->turns == 0) ? 150 : 100;
}

static const ability_def table[KIN_ABILITY_COUNT] = {
    [KIN_ABILITY_001] = { .name = "Steadfast",     .desc = "Takes 10% less damage.",
                          .in_dmg_pct = steadfast_in },
    [KIN_ABILITY_002] = { .name = "Tidal Surge",   .desc = "Aqua moves +50% when HP is low.",
                          .out_dmg_pct = tidal_surge_out },
    [KIN_ABILITY_003] = { .name = "Ember Soul",    .desc = "Ignis moves +50% when HP is low.",
                          .out_dmg_pct = ember_soul_out },
    [KIN_ABILITY_004] = { .name = "Quick Start",   .desc = "Much faster on the first turn.",
                          .speed_pct = quick_start_speed },
    [KIN_ABILITY_005] = { .name = "Soothing Aura", .desc = "Heals 4% HP every turn.",
                          .end_turn_heal_pct = 4 },
    [KIN_ABILITY_006] = { .name = "Static Field",  .desc = "Attackers lose 3 EP on hit.",
                          .drain_ep_on_hit = 3 },
    [KIN_ABILITY_007] = { .name = "Verdant Bloom", .desc = "Regains 4 EP every turn.",
                          .end_turn_ep = 4 },
    [KIN_ABILITY_008] = { .name = "Thorn Coat",    .desc = "Reflects 12% of damage taken.",
                          .reflect_pct = 12 },
    [KIN_ABILITY_009] = { .name = "Last Stand",    .desc = "Survives one KO with 1 HP.",
                          .survive_ko_once = true },
    [KIN_ABILITY_010] = { .name = "Sharp Focus",   .desc = "Moves are 10% more accurate.",
                          .accuracy_bonus = 10 },
};

const ability_def *abilities_get(enum kindral_ability id)
{
    static const ability_def none = { .name = "-", .desc = "No ability." };

    if ((unsigned)id >= KIN_ABILITY_COUNT || table[id].name == NULL) {
        return &none;
    }
    return &table[id];
}
