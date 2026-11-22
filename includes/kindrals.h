#ifndef KINDRALS_H
#define KINDRALS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define KIN_NAME_LEN        20   /* buffer size (nickname / trainer name)        */
#define KIN_NAME_MAX_CHARS  10   /* what the on-screen keyboard allows           */
#define KIN_MAX_ABILITIES   4
#define KIN_COUNT           10
#define KIN_MAX_MOVES       4
#define KIN_MAX_LEVEL       50
#define KIN_METER_MAX       100  /* hunger, thirst, sleep, happiness             */

enum kindral_types {
    KIN_TYPE_IGNIS,
    KIN_TYPE_AQUA,
    KIN_TYPE_TERRA,
    KIN_TYPE_AERO,
    KIN_TYPE_FLORA,
    KIN_TYPE_LUME,
    KIN_TYPE_NOX,
    KIN_TYPE_COUNT,
    KIN_TYPE_NONE = KIN_TYPE_COUNT      /* typeless moves (Scratch, Struggle..) */
};

enum kindral_traits {
    KIN_TRAIT_BEAST,
    KIN_TRAIT_AERIAL,
    KIN_TRAIT_INSECT,
    KIN_TRAIT_VITAE,
    KIN_TRAIT_CONSTRUCT,
    KIN_TRAIT_SPECTRAL,
    KIN_TRAIT_COUNT
};

enum kindral_id {
    KIN_001, KIN_002, KIN_003, KIN_004, KIN_005,
    KIN_006, KIN_007, KIN_008, KIN_009, KIN_010
};

enum kindral_ability {
    KIN_ABILITY_001, KIN_ABILITY_002, KIN_ABILITY_003, KIN_ABILITY_004,
    KIN_ABILITY_005, KIN_ABILITY_006, KIN_ABILITY_007, KIN_ABILITY_008,
    KIN_ABILITY_009, KIN_ABILITY_010,
    KIN_ABILITY_COUNT
};

typedef struct {
    int64_t birth_time;            /* Unix epoch seconds (UTC), set once at hatch */
    enum kindral_id species;
    enum kindral_ability ability;
    char name[KIN_NAME_LEN];
} kin_id;

/* A move the species (or its trait) learns when reaching 'level'. */
typedef struct {
    uint8_t level;
    uint8_t move;                  /* move_id from moves.h */
} kin_learn;

typedef struct {
    uint16_t hp;
    uint16_t attack;
    uint16_t defense;
    uint16_t speed;
    uint16_t energy;
} kin_base_stats;

#define KIN_MAX_EGG    4
#define KIN_MAX_LEARN  8

typedef struct {
    const char *species_name;      /* NULL = species slot not defined yet */
    enum kindral_types type;
    enum kindral_traits trait;
    uint8_t ability_count;
    enum kindral_ability abilities[KIN_MAX_ABILITIES];
    uint8_t egg_count;             /* level-0 pool: 2 are picked at random */
    uint8_t egg_moves[KIN_MAX_EGG];
    uint8_t learn_count;           /* level-up learnset */
    kin_learn learnset[KIN_MAX_LEARN];
    kin_base_stats base_stats;
} kin_species;

const kin_species *kindrals_species(enum kindral_id id);
bool kindrals_species_defined(enum kindral_id id);
enum kindral_id kindrals_random_species(void);

typedef struct {
    int health_points;
    int exp_points;
    int attack;
    int defense;
    int speed;
    int energy_points;
    int friendship;
    int level;
    int max_hp;
    int max_ep;
} kin_stats;

typedef struct {
    int happiness;
    int hunger;
    int thirst;
    int sleep;
} kin_walker;

typedef struct {
    uint8_t slot[KIN_MAX_MOVES];   /* move_id, 0 = empty */
} kin_moves;

typedef struct {
    uint32_t wins;
    uint32_t losses;
    uint32_t draws;
    uint32_t win_streak;
    uint32_t best_streak;
} kin_record;

/* RAM-only notices produced by level-ups; the UI drains these. */
typedef struct {
    uint8_t levelup_to;            /* 0 = none                                  */
    uint8_t auto_learned[KIN_MAX_MOVES];  /* learned straight into an empty slot */
    uint8_t auto_learned_count;
    uint8_t need_slot[KIN_MAX_MOVES];     /* moves waiting for a slot choice     */
    uint8_t need_slot_count;
} kin_pending;

typedef struct {
    kin_id id;
    kin_stats stats;
    kin_walker walker;
    kin_moves moves;
    kin_record record;
    kin_pending pending;           /* not saved */
} kindral;

/* ---- Lifecycle ---- */
int kindrals_init(kindral *kin, enum kindral_id id, int64_t birth_time);
int kindrals_set_name(kindral *kin, const char *name);
int64_t kindrals_age_seconds(const kindral *kin, int64_t now);

/* ---- Stat updaters (add delta, clamp, return new value) ---- */
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

/* ---- Levels & moves ---- */
uint32_t kindrals_exp_for_level(int level);   /* total exp needed to BE this level */
int kindrals_add_exp(kindral *kin, int amount);   /* returns levels gained        */
bool kindrals_knows(const kindral *kin, uint8_t move);
int kindrals_learn_move(kindral *kin, uint8_t move);              /* 0/-EEXIST/-ENOSPC */
int kindrals_replace_move(kindral *kin, int slot, uint8_t move);  /* 0/-EINVAL         */
int kindrals_move_count(const kindral *kin);
void kindrals_clear_pending(kindral *kin);

/* ---- Care / game logic ---- */
const char *kindrals_type_name(enum kindral_types type);
const char *kindrals_trait_name(enum kindral_traits trait);

/*
 * Care actions. Return 0 on success, or:
 *   -EALREADY  the kindral doesn't need it (eat/drink/sleep)
 *   -EBUSY     too tired (play)
 */
int kindrals_eat(kindral *kin);
int kindrals_drink(kindral *kin);
int kindrals_sleep(kindral *kin);
int kindrals_play(kindral *kin);

/* One step of time passing: needs build up, mood and health react. */
void kindrals_tick(kindral *kin);

/* Apply time spent switched off: 1 tick per 'secs_per_tick', at most 'max_ticks'. */
void kindrals_catch_up(kindral *kin, int64_t offline_secs, int secs_per_tick, int max_ticks);

/* Post-battle bookkeeping (record, streak). */
void kindrals_record_win(kindral *kin);
void kindrals_record_loss(kindral *kin);
void kindrals_record_draw(kindral *kin);

/* Print the stat sheet to the console (debug). */
void kindrals_display_stats(const kindral *kin, int64_t now);

#endif /* KINDRALS_H */
