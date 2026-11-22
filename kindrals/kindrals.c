#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

#include "kindrals.h"
#include "moves.h"
#include "traits.h"
#include "abilities.h"

/* ------------------------------------------------------------------ */
/* Species table                                                       */
/* Empty slots (species_name == NULL) are "not designed yet" and are   */
/* never rolled when hatching.                                         */
/* ------------------------------------------------------------------ */

static const kin_species species_table[KIN_COUNT] = {
    [KIN_001] = {
        .species_name = "Ignia",
        .base_stats = { .hp = 40, .attack = 10, .defense = 10, .speed = 10, .energy = 60 },
        .type = KIN_TYPE_IGNIS,
        .trait = KIN_TRAIT_BEAST,
        .ability_count = 3,
        .abilities = {KIN_ABILITY_001, KIN_ABILITY_003, KIN_ABILITY_004},
        .egg_count = 3,
        .egg_moves = {MOVE_EMBER, MOVE_SCRATCH, MOVE_HARDEN},
        .learn_count = 4,
        .learnset = { {4, MOVE_FLAME_BURST}, {7, MOVE_SWIFTSTEP},
                      {15, MOVE_INFERNO_RUSH}, {24, MOVE_EARTHQUAKE} },
    },
    [KIN_002] = {
        .species_name = "Aquis",
        .base_stats = { .hp = 40, .attack = 10, .defense = 10, .speed = 10, .energy = 60 },
        .type = KIN_TYPE_AQUA,
        .trait = KIN_TRAIT_VITAE,
        .ability_count = 2,
        .abilities = {KIN_ABILITY_002, KIN_ABILITY_005},
        .egg_count = 3,
        .egg_moves = {MOVE_BUBBLE_BEAM, MOVE_SCRATCH, MOVE_GLINT},
        .learn_count = 4,
        .learnset = { {4, MOVE_AQUA_JET}, {8, MOVE_INTIMIDATE},
                      {17, MOVE_TIDAL_CRASH}, {21, MOVE_MIND_SIPHON} },
    },
    [KIN_003] = {
        .species_name = "Vitara",
        .base_stats = { .hp = 40, .attack = 10, .defense = 10, .speed = 10, .energy = 60 },
        .type = KIN_TYPE_FLORA,
        .trait = KIN_TRAIT_VITAE,
        .ability_count = 3,
        .abilities = {KIN_ABILITY_001, KIN_ABILITY_007, KIN_ABILITY_008},
        .egg_count = 3,
        .egg_moves = {MOVE_VINE_LASH, MOVE_SCRATCH, MOVE_HARDEN},
        .learn_count = 4,
        .learnset = { {4, MOVE_LIFE_SIPHON}, {9, MOVE_MEND},
                      {16, MOVE_BLOOM_BURST}, {22, MOVE_GLINT} },
    },
};

const kin_species *kindrals_species(enum kindral_id id)
{
    if ((unsigned)id >= KIN_COUNT) {
        return NULL;
    }
    return &species_table[id];
}

bool kindrals_species_defined(enum kindral_id id)
{
    return (unsigned)id < KIN_COUNT && species_table[id].species_name != NULL &&
           species_table[id].ability_count > 0;
}

enum kindral_id kindrals_random_species(void)
{
    enum kindral_id pool[KIN_COUNT];
    int n = 0;

    for (int i = 0; i < KIN_COUNT; i++) {
        if (kindrals_species_defined((enum kindral_id)i)) {
            pool[n++] = (enum kindral_id)i;
        }
    }
    return pool[sys_rand32_get() % n];
}

/* ------------------------------------------------------------------ */
/* Hatching                                                            */
/* ------------------------------------------------------------------ */

/* Fallback level-0 stats for species without an explicit base profile. */
#define START_HP      40
#define START_EP      60
#define START_ATK     10
#define START_DEF     10
#define START_SPD     10
#define GAIN_HP       8
#define GAIN_EP       4
#define GAIN_ATK      3
#define GAIN_DEF      3
#define GAIN_SPD      3

int kindrals_init(kindral *kin, enum kindral_id id, int64_t birth_time)
{
    const kin_species *sp = kindrals_species(id);

    if (kin == NULL || !kindrals_species_defined(id)) {
        return -EINVAL;
    }

    *kin = (kindral){0};

    kin->id.species = id;
    kin->id.birth_time = birth_time;
    snprintf(kin->id.name, sizeof(kin->id.name), "%s", sp->species_name);

    uint32_t roll = sys_rand32_get() % sp->ability_count;
    kin->id.ability = sp->abilities[roll];

    kin->stats.level = 0;
    const kin_base_stats *base = &sp->base_stats;
    kin->stats.max_hp = base->hp ? base->hp : START_HP;
    kin->stats.max_ep = base->energy ? base->energy : START_EP;
    kin->stats.health_points = kin->stats.max_hp;
    kin->stats.energy_points = kin->stats.max_ep;
    kin->stats.exp_points = 0;
    kin->stats.attack = base->attack ? base->attack : START_ATK;
    kin->stats.defense = base->defense ? base->defense : START_DEF;
    kin->stats.speed = base->speed ? base->speed : START_SPD;
    kin->stats.friendship = 0;

    kin->walker.happiness = 100;
    kin->walker.hunger = 0;
    kin->walker.thirst = 0;
    kin->walker.sleep = 100;

    /* "Egg moves": 2 distinct moves drawn at random from the species pool. */
    uint8_t pool[KIN_MAX_EGG];
    int n = MIN(sp->egg_count, KIN_MAX_EGG);

    memcpy(pool, sp->egg_moves, n);
    for (int slot = 0; slot < 2 && n > 0; slot++) {
        int pick = sys_rand32_get() % n;

        kin->moves.slot[slot] = pool[pick];
        pool[pick] = pool[--n];
    }
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
    return (now > kin->id.birth_time) ? (now - kin->id.birth_time) : 0;
}

/* ------------------------------------------------------------------ */
/* Names                                                               */
/* ------------------------------------------------------------------ */

static const char *const type_names[] = {
    "Ignis", "Aqua", "Terra", "Aero", "Flora", "Lume", "Nox"
};

const char *kindrals_type_name(enum kindral_types type)
{
    return ((unsigned)type < ARRAY_SIZE(type_names)) ? type_names[type] : "-";
}

const char *kindrals_trait_name(enum kindral_traits trait)
{
    return traits_get(trait)->name;
}

/* ------------------------------------------------------------------ */
/* Stat updaters                                                       */
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

DEFINE_UPDATER(kindrals_update_health,     kin->stats.health_points,  0, kin->stats.max_hp)
DEFINE_UPDATER(kindrals_update_exp,        kin->stats.exp_points,     0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_attack,     kin->stats.attack,         0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_defense,    kin->stats.defense,        0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_speed,      kin->stats.speed,          0, INT32_MAX)
DEFINE_UPDATER(kindrals_update_energy,     kin->stats.energy_points,  0, kin->stats.max_ep)
DEFINE_UPDATER(kindrals_update_friendship, kin->stats.friendship,     0, FRIENDSHIP_MAX)
DEFINE_UPDATER(kindrals_update_happiness,  kin->walker.happiness,     0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_hunger,     kin->walker.hunger,        0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_thirst,     kin->walker.thirst,        0, KIN_METER_MAX)
DEFINE_UPDATER(kindrals_update_sleep,      kin->walker.sleep,         0, KIN_METER_MAX)

/* ------------------------------------------------------------------ */
/* Levels & moves                                                      */
/* ------------------------------------------------------------------ */

uint32_t kindrals_exp_for_level(int level)
{
    return (level <= 0) ? 0 : (uint32_t)(8 * level * level);
}

bool kindrals_knows(const kindral *kin, uint8_t move)
{
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        if (kin->moves.slot[i] == move) {
            return true;
        }
    }
    return false;
}

int kindrals_move_count(const kindral *kin)
{
    int n = 0;

    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        n += (kin->moves.slot[i] != MOVE_NONE);
    }
    return n;
}

int kindrals_learn_move(kindral *kin, uint8_t move)
{
    if (kin == NULL || moves_get(move) == NULL) {
        return -EINVAL;
    }
    if (kindrals_knows(kin, move)) {
        return -EEXIST;
    }
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        if (kin->moves.slot[i] == MOVE_NONE) {
            kin->moves.slot[i] = move;
            return 0;
        }
    }
    return -ENOSPC;
}

int kindrals_replace_move(kindral *kin, int slot, uint8_t move)
{
    if (kin == NULL || slot < 0 || slot >= KIN_MAX_MOVES || moves_get(move) == NULL ||
        kindrals_knows(kin, move)) {
        return -EINVAL;
    }
    kin->moves.slot[slot] = move;
    return 0;
}

void kindrals_clear_pending(kindral *kin)
{
    memset(&kin->pending, 0, sizeof(kin->pending));
}

static void offer_move(kindral *kin, uint8_t move)
{
    int r = kindrals_learn_move(kin, move);

    if (r == 0) {
        if (kin->pending.auto_learned_count < KIN_MAX_MOVES) {
            kin->pending.auto_learned[kin->pending.auto_learned_count++] = move;
        }
    } else if (r == -ENOSPC && kin->pending.need_slot_count < KIN_MAX_MOVES) {
        kin->pending.need_slot[kin->pending.need_slot_count++] = move;
    }
    /* -EEXIST: already known, nothing to do */
}

static void level_up_once(kindral *kin)
{
    const kin_species *sp = kindrals_species(kin->id.species);
    const trait_def *tr = traits_get(sp->trait);
    int hp = GAIN_HP;
    int ep = GAIN_EP;
    int atk = GAIN_ATK;
    int def = GAIN_DEF;
    int spd = GAIN_SPD;

    /* Every Kindral receives the same baseline growth. Its trait adds
     * exactly one point to one primary stat; Spectral adds no stat growth.
     */
    switch (tr->primary_stat) {
    case KIN_GROW_HP:  hp++; break;
    case KIN_GROW_ATK: atk++; break;
    case KIN_GROW_DEF: def++; break;
    case KIN_GROW_SPD: spd++; break;
    case KIN_GROW_EP:  ep++; break;
    case KIN_GROW_NONE:
    default: break;
    }

    kin->stats.level++;
    kin->stats.max_hp += hp;
    kin->stats.max_ep += ep;
    kin->stats.health_points += hp; /* level-up is not a full heal */
    kin->stats.energy_points += ep;
    kin->stats.attack += atk;
    kin->stats.defense += def;
    kin->stats.speed += spd;
    kin->pending.levelup_to = (uint8_t)kin->stats.level;

    /* Species learnset first, then the trait learnset. */
    for (int i = 0; i < sp->learn_count; i++) {
        if (sp->learnset[i].level == kin->stats.level) {
            offer_move(kin, sp->learnset[i].move);
        }
    }
    for (int i = 0; i < tr->learn_count; i++) {
        if (tr->learnset[i].level == kin->stats.level) {
            offer_move(kin, tr->learnset[i].move);
        }
    }
}

int kindrals_add_exp(kindral *kin, int amount)
{
    int gained = 0;

    if (kin == NULL || amount <= 0) {
        return 0;
    }
    kindrals_update_exp(kin, amount);

    while (kin->stats.level < KIN_MAX_LEVEL &&
           (uint32_t)kin->stats.exp_points >= kindrals_exp_for_level(kin->stats.level + 1)) {
        level_up_once(kin);
        gained++;
    }
    return gained;
}

/* ------------------------------------------------------------------ */
/* Care actions                                                        */
/*                                                                     */
/*   hunger / thirst : 0 = full,    100 = starving / parched           */
/*   sleep           : 100 = rested, 0 = exhausted                     */
/*   happiness       : 100 = delighted                                 */
/* ------------------------------------------------------------------ */

#define FEED_AMOUNT        30
#define DRINK_AMOUNT       30
#define SLEEP_AMOUNT       40
#define SLEEP_NOT_NEEDED   90
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
    kindrals_update_hunger(kin, 5);
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
    kindrals_add_exp(kin, 5);
    kindrals_update_friendship(kin, 1);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Time passing                                                        */
/* ------------------------------------------------------------------ */

#define NEED_WARN_LEVEL   70
#define SLEEP_WARN_LEVEL  30

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
        /* Health suffers, but never drops below 1 (no death state). */
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

void kindrals_catch_up(kindral *kin, int64_t offline_secs, int secs_per_tick, int max_ticks)
{
    if (kin == NULL || offline_secs <= 0 || secs_per_tick <= 0) {
        return;
    }
    int64_t ticks = offline_secs / secs_per_tick;

    if (ticks > max_ticks) {
        ticks = max_ticks;
    }
    for (int64_t i = 0; i < ticks; i++) {
        kindrals_tick(kin);
    }
}

/* ------------------------------------------------------------------ */
/* Battle record                                                       */
/* ------------------------------------------------------------------ */

void kindrals_record_win(kindral *kin)
{
    kin->record.wins++;
    kin->record.win_streak++;
    if (kin->record.win_streak > kin->record.best_streak) {
        kin->record.best_streak = kin->record.win_streak;
    }
}

void kindrals_record_loss(kindral *kin)
{
    kin->record.losses++;
    kin->record.win_streak = 0;
}

void kindrals_record_draw(kindral *kin)
{
    kin->record.draws++;        /* a draw neither extends nor breaks the streak */
}

/* ------------------------------------------------------------------ */
/* Console stat sheet (debug aid; the LCD uses scene_info.c)           */
/* ------------------------------------------------------------------ */

void kindrals_display_stats(const kindral *kin, int64_t now)
{
    if (kin == NULL) {
        return;
    }
    const kin_species *sp = kindrals_species(kin->id.species);

    printk("== %s (%s) Lv%d  age %lld s ==\n", kin->id.name,
           sp ? sp->species_name : "?", kin->stats.level,
           (long long)kindrals_age_seconds(kin, now));
    printk("HP %d/%d  EP %d/%d  ATK %d DEF %d SPD %d  EXP %d  FR %d\n",
           kin->stats.health_points, kin->stats.max_hp,
           kin->stats.energy_points, kin->stats.max_ep,
           kin->stats.attack, kin->stats.defense, kin->stats.speed,
           kin->stats.exp_points, kin->stats.friendship);
    printk("Hap %d Hun %d Thi %d Rest %d  W/L/D %u/%u/%u streak %u\n",
           kin->walker.happiness, kin->walker.hunger, kin->walker.thirst,
           kin->walker.sleep, (unsigned)kin->record.wins, (unsigned)kin->record.losses,
           (unsigned)kin->record.draws, (unsigned)kin->record.win_streak);
}
