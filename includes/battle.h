#ifndef BATTLE_H
#define BATTLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "kindrals.h"
#include "moves.h"

/*
 * Turn-based battle engine. Pure logic: no hardware, no Zephyr calls.
 *
 * Both KinBoxes run this same engine with the same seed and the same pair of
 * actions, so they always arrive at the same result without sending the full
 * outcome over the air. Sides are fixed: side 0 = BLE central, side 1 =
 * peripheral. The UI maps "me / foe" onto those.
 */

typedef struct battle_mon {
    char name[KIN_NAME_LEN];
    uint8_t species, ability, level, type, trait;
    int16_t hp, max_hp, ep, max_ep;
    int16_t atk, def, spd;
    uint8_t moves[KIN_MAX_MOVES];
    int8_t stage[BSTAT_COUNT];
    uint8_t turns;               /* turns fought so far */
    bool survived;               /* Last Stand-style ability already used */
    int8_t spectral_reduction[BSTAT_COUNT]; /* temporary percentage reduction */
    uint8_t spectral_turns[BSTAT_COUNT];    /* remaining active battle turns */
} battle_mon;

enum battle_result { BATTLE_ONGOING, BATTLE_WIN_0, BATTLE_WIN_1, BATTLE_DRAW };

enum battle_action_kind { BACT_MOVE, BACT_ITEM, BACT_FORFEIT };

typedef struct {
    uint8_t kind;                /* enum battle_action_kind */
    uint8_t arg;                 /* move slot 0..3, or item_id */
} battle_action;

enum battle_event_kind {
    BEV_USED, BEV_MISS, BEV_NO_EP, BEV_DAMAGE, BEV_SUPER, BEV_RESIST,
    BEV_HEAL, BEV_EP_DRAIN, BEV_EP_GAIN, BEV_STAT, BEV_ITEM, BEV_ABILITY,
    BEV_RECOIL, BEV_FAINT, BEV_FORFEIT, BEV_SURVIVE, BEV_TIMEOUT,
    BEV_SPECTRAL, BEV_SPECTRAL_END,
};

typedef struct {
    uint8_t kind;                /* enum battle_event_kind */
    uint8_t side;                /* who the event is about  */
    uint8_t id;                  /* move / item / stat / ability id */
    int16_t val;
    int16_t hp[2], ep[2];        /* snapshot after the event (for animating bars) */
} battle_event;

#define BATTLE_MAX_EVENTS  48
#define BATTLE_MAX_TURNS   60
#define BATTLE_EP_REGEN    3     /* passive EP regained per turn */

typedef struct {
    battle_event ev[BATTLE_MAX_EVENTS];
    uint8_t n;
} battle_log;

typedef struct {
    battle_mon mon[2];
    uint32_t rng;
    uint16_t turn;
    uint8_t result;              /* enum battle_result */
} battle_t;

void battle_mon_from_kindral(battle_mon *m, const kindral *kin);
void battle_init(battle_t *b, const battle_mon *side0, const battle_mon *side1, uint32_t seed);

/* A move slot is usable if it holds a move and the mon can afford its EP. */
bool battle_move_usable(const battle_mon *m, int slot);
bool battle_can_afford_any(const battle_mon *m);

/* Resolve one full turn. Both actions are applied; events are appended to log. */
void battle_resolve_turn(battle_t *b, const battle_action act[2], battle_log *log);

/* Small checksum of the shared state, used to detect desyncs between devices. */
uint16_t battle_hash(const battle_t *b);

/* Human-readable line for an event (e.g. "Ignia used Ember!"). */
int battle_event_text(const battle_t *b, const battle_event *e, char *buf, size_t n);

/* Rewards. */
int battle_exp_reward(const battle_mon *winner, const battle_mon *loser, bool won);

#endif /* BATTLE_H */
