#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>

#include "battle_session.h"
#include "net.h"
#include "proto.h"
#include "game.h"
#include "items.h"
#include "storage.h"

LOG_MODULE_REGISTER(bsess, LOG_LEVEL_INF);

static bool active;
static battle_mon mine, theirs;
static bool got_a, got_b, got_seed;
static uint32_t seed;
static battle_t bt;
static bool built;

static bool have_my_act, have_peer_act;
static battle_action my_act, peer_act;
static uint16_t peer_hash;
static bool desync;
static uint8_t consumed[ITEM_COUNT];

static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static void send(const uint8_t *d, size_t n)
{
    if (net_send(d, n)) {
        LOG_WRN("battle send failed");
    }
}

static int my_side(void) { return net_is_central() ? 0 : 1; }

static void try_build(void)
{
    if (built || !got_a || !got_b || !got_seed) {
        return;
    }
    if (my_side() == 0) {
        battle_init(&bt, &mine, &theirs, seed);
    } else {
        battle_init(&bt, &theirs, &mine, seed);
    }
    built = true;
}

void bsess_begin(void)
{
    memset(&theirs, 0, sizeof(theirs));
    memset(consumed, 0, sizeof(consumed));
    got_a = got_b = got_seed = built = false;
    have_my_act = have_peer_act = desync = false;
    active = true;
    g_game.in_event = true;
    storage_defer(true);

    battle_mon_from_kindral(&mine, &g_game.kin);

    uint8_t a[19] = { MSG_SNAP_A, mine.species, mine.ability, mine.level };

    strncpy((char *)&a[4], mine.name, 11);
    memcpy(&a[15], mine.moves, 4);
    send(a, sizeof(a));

    uint8_t b[15] = { MSG_SNAP_B };

    put16(&b[1], mine.hp);   put16(&b[3], mine.max_hp);
    put16(&b[5], mine.ep);   put16(&b[7], mine.max_ep);
    put16(&b[9], mine.atk);  put16(&b[11], mine.def);
    put16(&b[13], mine.spd);
    send(b, sizeof(b));

    if (my_side() == 0) {
        seed = sys_rand32_get() | 1u;
        got_seed = true;
        uint8_t s[5] = { MSG_SEED, seed & 0xFF, (seed >> 8) & 0xFF,
                         (seed >> 16) & 0xFF, (seed >> 24) & 0xFF };

        send(s, sizeof(s));
    }
    try_build();
}

static void restore_items(void)
{
    for (int i = 1; i < ITEM_COUNT; i++) {
        if (consumed[i]) {
            inv_add(&g_game.inv, (uint8_t)i, consumed[i]);
        }
    }
    memset(consumed, 0, sizeof(consumed));
}

void bsess_abort(void)
{
    if (!active) {
        return;
    }
    restore_items();
    active = false;
    built = false;
    g_game.in_event = false;
    storage_defer(false);
}

static bool valid_snapshot(void)
{
    if (!kindrals_species_defined((enum kindral_id)theirs.species) ||
        theirs.ability >= KIN_ABILITY_COUNT || theirs.level > KIN_MAX_LEVEL ||
        theirs.max_hp < 1 || theirs.max_hp > 2000 || theirs.max_ep < 1 || theirs.max_ep > 1000 ||
        theirs.hp < 1 || theirs.hp > theirs.max_hp || theirs.ep < 0 || theirs.ep > theirs.max_ep ||
        theirs.atk < 1 || theirs.atk > 2000 || theirs.def < 1 || theirs.def > 2000 ||
        theirs.spd < 1 || theirs.spd > 2000) {
        return false;
    }
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        if (theirs.moves[i] != MOVE_NONE && moves_get(theirs.moves[i]) == NULL) {
            return false;
        }
    }
    return true;
}

bool bsess_rx(const uint8_t *d, size_t n)
{
    if (n < 1) {
        return false;
    }
    switch (d[0]) {
    case MSG_SNAP_A:
        if (n < 19 || !active) {
            return true;
        }
        theirs.species = d[1];
        theirs.ability = d[2];
        theirs.level = d[3];
        memcpy(theirs.name, &d[4], 11);
        theirs.name[11] = '\0';
        memcpy(theirs.moves, &d[15], 4);
        got_a = true;
        break;

    case MSG_SNAP_B:
        if (n < 15 || !active) {
            return true;
        }
        theirs.hp = (int16_t)get16(&d[1]);   theirs.max_hp = (int16_t)get16(&d[3]);
        theirs.ep = (int16_t)get16(&d[5]);   theirs.max_ep = (int16_t)get16(&d[7]);
        theirs.atk = (int16_t)get16(&d[9]);  theirs.def = (int16_t)get16(&d[11]);
        theirs.spd = (int16_t)get16(&d[13]);
        got_b = true;
        break;

    case MSG_SEED:
        if (n < 5 || !active) {
            return true;
        }
        seed = d[1] | (d[2] << 8) | (d[3] << 16) | ((uint32_t)d[4] << 24);
        got_seed = true;
        break;

    case MSG_ACTION:
        if (n < 7 || !built) {
            return true;
        }
        if (get16(&d[1]) != bt.turn || d[2] > BACT_FORFEIT) {
            desync = true;
            return true;
        }
        peer_act.kind = d[2];
        peer_act.arg = d[3];
        peer_hash = get16(&d[5]);
        have_peer_act = true;
        return true;

    default:
        return false;
    }

    if (got_a && got_b) {
        const kin_species *sp = kindrals_species((enum kindral_id)theirs.species);

        if (!sp || !valid_snapshot()) {
            LOG_ERR("peer snapshot rejected");
            desync = true;
            return true;
        }
        theirs.type = (uint8_t)sp->type;
        theirs.trait = (uint8_t)sp->trait;
    }
    try_build();
    return true;
}

bool bsess_ready(void)            { return built; }
bool bsess_active(void)           { return active; }
const battle_t *bsess_battle(void){ return &bt; }
int bsess_my_side(void)           { return my_side(); }
bool bsess_desync(void)           { return desync; }

int bsess_submit(battle_action a)
{
    if (!built || have_my_act) {
        return -EALREADY;
    }
    if (a.kind == BACT_MOVE) {
        const battle_mon *m = &bt.mon[my_side()];

        if (a.arg >= KIN_MAX_MOVES || m->moves[a.arg] == MOVE_NONE) {
            return -EINVAL;
        }
        /* Not affordable -> engine falls back to Struggle, so allow it only when
         * nothing is affordable. */
        if (!battle_move_usable(m, a.arg) && battle_can_afford_any(m)) {
            return -EINVAL;
        }
    } else if (a.kind == BACT_ITEM) {
        const item_def *it = items_get(a.arg);

        if (!it || !(it->flags & ITEM_F_BATTLE) || inv_count(&g_game.inv, a.arg) < 1) {
            return -ENOENT;
        }
    }
    my_act = a;
    have_my_act = true;

    uint8_t m[7] = { MSG_ACTION };

    put16(&m[1], bt.turn);
    m[3] = a.kind;
    m[4] = a.arg;
    put16(&m[5], battle_hash(&bt));
    send(m, sizeof(m));
    return 0;
}

bool bsess_waiting_for_peer(void)
{
    return have_my_act && !have_peer_act;
}

bool bsess_try_resolve(battle_log *log)
{
    if (!built || !have_my_act || !have_peer_act) {
        return false;
    }
    if (peer_hash != battle_hash(&bt)) {
        LOG_ERR("state hash mismatch");
        desync = true;
        return false;
    }
    battle_action act[2];

    act[my_side()] = my_act;
    act[my_side() ^ 1] = peer_act;

    if (my_act.kind == BACT_ITEM && inv_remove(&g_game.inv, my_act.arg, 1) == 0) {
        consumed[my_act.arg]++;
    }
    battle_resolve_turn(&bt, act, log);
    have_my_act = have_peer_act = false;
    return true;
}

void bsess_finish(char *summary, size_t n)
{
    int me = my_side();
    battle_mon *m = &bt.mon[me];
    battle_mon *foe = &bt.mon[me ^ 1];
    kindral *k = &g_game.kin;
    int exp;

    k->stats.health_points = m->hp < 1 ? 1 : m->hp;     /* no death state */
    k->stats.energy_points = m->ep;

    if (bt.result == BATTLE_DRAW) {
        kindrals_record_draw(k);
        exp = battle_exp_reward(m, foe, false);
        snprintf(summary, n, "Draw! +%d exp", exp);
    } else if ((bt.result == BATTLE_WIN_0) == (me == 0)) {
        kindrals_record_win(k);
        exp = battle_exp_reward(m, foe, true);
        kindrals_update_friendship(k, 1);
        snprintf(summary, n, "You won! +%d exp (streak %u)", exp, (unsigned)k->record.win_streak);
    } else {
        kindrals_record_loss(k);
        exp = battle_exp_reward(m, foe, false);
        snprintf(summary, n, "You lost... +%d exp", exp);
    }
    kindrals_add_exp(k, exp);

    memset(consumed, 0, sizeof(consumed));     /* items are really spent now */
    active = false;
    built = false;
    g_game.in_event = false;
    storage_defer(false);
    storage_save_now(SAVE_ALL);
}
