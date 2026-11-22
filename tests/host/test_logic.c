/* Host-side tests for the pure game logic. Build/run: ./run.sh */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kindrals.h"
#include "battle.h"
#include "items.h"
#include "inventory.h"
#include "traits.h"
#include "types.h"
#include "abilities.h"
#include "walk.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void test_charts(void)
{
    for (int t = 0; t < KIN_TYPE_COUNT; t++) {
        int so = 0, no = 0, si = 0, ni = 0;
        for (int d = 0; d < KIN_TYPE_COUNT; d++) {
            int o = types_effectiveness_pct(t, d), i = types_effectiveness_pct(d, t);
            so += o == 150; no += o == 70; si += i == 150; ni += i == 70;
        }
        CHECK(so == 2 && no == 2 && si == 2 && ni == 2);
    }
    for (int t = 0; t < KIN_TRAIT_COUNT; t++) {
        const trait_def *td = traits_get(t);
        CHECK(td->hp + td->atk + td->def + td->spd + td->ep == 500);
        int up = 0, dn = 0;
        for (int d = 0; d < KIN_TRAIT_COUNT; d++) {
            int m = traits_matchup_pct(t, d);
            up += m == 115; dn += m == 85;
            /* symmetric: if A beats B, B must be at a disadvantage vs A */
            CHECK(traits_matchup_pct(d, t) == (m == 115 ? 85 : m == 85 ? 115 : 100));
        }
        CHECK(up == 2 && dn == 2);
        for (int i = 0; i < td->learn_count; i++) CHECK(moves_get(td->learnset[i].move) != NULL);
    }
    CHECK(types_effectiveness_pct(KIN_TYPE_NONE, KIN_TYPE_NOX) == 100);
}

static void test_data_refs(void)
{
    for (int s = 0; s < KIN_COUNT; s++) {
        if (!kindrals_species_defined(s)) continue;
        const kin_species *sp = kindrals_species(s);
        CHECK(sp->egg_count >= 2);
        for (int i = 0; i < sp->egg_count; i++) CHECK(moves_get(sp->egg_moves[i]));
        for (int i = 0; i < sp->learn_count; i++) CHECK(moves_get(sp->learnset[i].move));
        for (int i = 0; i < sp->ability_count; i++) CHECK(abilities_get(sp->abilities[i])->name[0] != '-');
    }
    for (int m = 1; m < MOVE_COUNT; m++) {
        const move_def *mv = moves_get(m);
        CHECK(mv != NULL);
        if (mv) { CHECK(strlen(mv->name) <= 12); CHECK(mv->accuracy >= 1 && mv->accuracy <= 100); }
    }
    for (int i = 1; i < ITEM_COUNT; i++) CHECK(items_get(i) && strlen(items_get(i)->name) <= 14);
}

static void test_hatch_and_levels(void)
{
    for (int s = 0; s < KIN_COUNT; s++) {
        if (!kindrals_species_defined(s)) continue;
        for (int rep = 0; rep < 50; rep++) {
            kindral k;
            CHECK(kindrals_init(&k, s, 1000) == 0);
            CHECK(kindrals_move_count(&k) == 2);
            CHECK(k.moves.slot[0] != k.moves.slot[1]);
            CHECK(k.stats.level == 0);
        }
        kindral k; kindrals_init(&k, s, 0);
        int lv = kindrals_add_exp(&k, kindrals_exp_for_level(10));
        CHECK(lv == 10 && k.stats.level == 10);
        CHECK(kindrals_move_count(&k) == 4 || k.pending.need_slot_count > 0 || k.pending.auto_learned_count > 0);
        printf("  species %d @L10: hp %d ep %d atk %d def %d spd %d, moves %d, pending-slot %d\n", s,
               k.stats.max_hp, k.stats.max_ep, k.stats.attack, k.stats.defense, k.stats.speed,
               kindrals_move_count(&k), k.pending.need_slot_count);
    }
    kindral k; kindrals_init(&k, KIN_001, 0);
    kindrals_add_exp(&k, 999999);
    CHECK(k.stats.level == KIN_MAX_LEVEL);
    CHECK(kindrals_add_exp(&k, 100) == 0);
}

static void test_inventory(void)
{
    inventory inv; inv_give_starter_items(&inv);
    CHECK(inv_count(&inv, ITEM_SUNBERRY) == 3);
    CHECK(inv_remove(&inv, ITEM_SUNBERRY, 3) == 0);
    CHECK(inv_count(&inv, ITEM_SUNBERRY) == 0);
    CHECK(inv_remove(&inv, ITEM_SUNBERRY, 1) == -ENOENT);
    CHECK(inv_add(&inv, ITEM_SHINY_GEM, 98) == 0 && inv_add(&inv, ITEM_SHINY_GEM, 2) == -ENOSPC);
    for (int i = ITEM_SUNBERRY; i < ITEM_COUNT; i++) inv_add(&inv, i, 1);
    CHECK(inv_count(&inv, ITEM_OLD_BOOT) == 1);
    kindral k; kindrals_init(&k, KIN_001, 0);
    CHECK(items_use_field(&k, ITEM_SUNBERRY) == -EALREADY);
    k.stats.health_points = 10;
    CHECK(items_use_field(&k, ITEM_SUNBERRY) == 0 && k.stats.health_points == 40);
    CHECK(items_use_field(&k, ITEM_POWER_SEED) == -ENOTSUP);
    int h0 = k.walker.happiness = 50;
    items_apply_gift_giver(&k, ITEM_OLD_BOOT);
    CHECK(k.walker.happiness < h0);
    items_apply_gift_receiver(&k, ITEM_SHINY_GEM);
    CHECK(k.walker.happiness > h0 - 20);
}

static battle_action pick(const battle_t *b, int s, unsigned *r)
{
    battle_action a = { BACT_MOVE, 0 };
    int usable[4], n = 0;
    for (int i = 0; i < 4; i++) if (battle_move_usable(&b->mon[s], i)) usable[n++] = i;
    *r = *r * 1664525u + 1013904223u;
    a.arg = n ? usable[(*r >> 16) % n] : 0;   /* 0 slot may be unaffordable -> Struggle fallback */
    return a;
}

static int run_battle(battle_t *b, unsigned seed, int *turns)
{
    unsigned r = seed;
    battle_log log;
    while (b->result == BATTLE_ONGOING) {
        battle_action act[2] = { pick(b, 0, &r), pick(b, 1, &r) };
        battle_resolve_turn(b, act, &log);
        CHECK(log.n > 0);
    }
    *turns = b->turn;
    return b->result;
}

static void make(kindral *k, int species, int level)
{
    kindrals_init(k, species, 0);
    kindrals_add_exp(k, kindrals_exp_for_level(level));
    /* give a full moveset from the learnset for the test */
    const kin_species *sp = kindrals_species(species);
    for (int i = 0; i < sp->learn_count && kindrals_move_count(k) < 4; i++) kindrals_learn_move(k, sp->learnset[i].move);
    k->stats.health_points = k->stats.max_hp; k->stats.energy_points = k->stats.max_ep;
}

static void test_battle(void)
{
    /* determinism: same seed + same actions => identical outcome and hash */
    kindral a, b; make(&a, KIN_001, 8); make(&b, KIN_002, 8);
    battle_mon ma, mb; battle_mon_from_kindral(&ma, &a); battle_mon_from_kindral(&mb, &b);
    battle_t x, y; battle_init(&x, &ma, &mb, 12345); battle_init(&y, &ma, &mb, 12345);
    int tx, ty; run_battle(&x, 7, &tx); run_battle(&y, 7, &ty);
    CHECK(tx == ty && battle_hash(&x) == battle_hash(&y) && x.result == y.result);

    /* balance: all species pairs at equal level */
    for (int lvl = 0; lvl <= 30; lvl += 10) {
        printf("  level %d matchups (side0 wins/side1 wins/draws, avg turns) over 400 battles:\n", lvl);
        for (int s0 = 0; s0 < KIN_COUNT; s0++) for (int s1 = s0; s1 < KIN_COUNT; s1++) {
            if (!kindrals_species_defined(s0) || !kindrals_species_defined(s1)) continue;
            int w[4] = {0}; long tt = 0;
            for (int i = 0; i < 400; i++) {
                kindral p, q; make(&p, s0, lvl); make(&q, s1, lvl);
                battle_mon mp, mq; battle_mon_from_kindral(&mp, &p); battle_mon_from_kindral(&mq, &q);
                battle_t bt; battle_init(&bt, &mp, &mq, 1000 + i);
                int t; int res = run_battle(&bt, 55 + i, &t);
                w[res]++; tt += t;
            }
            printf("    %s vs %s: %3d / %3d / %3d   avg %.1f turns\n", kindrals_species(s0)->species_name,
                   kindrals_species(s1)->species_name, w[BATTLE_WIN_0], w[BATTLE_WIN_1], w[BATTLE_DRAW], tt / 400.0);
        }
    }
}

static void test_battle_features(void)
{
    kindral a, b; make(&a, KIN_001, 10); make(&b, KIN_003, 10);
    battle_mon ma, mb; battle_mon_from_kindral(&ma, &a); battle_mon_from_kindral(&mb, &b);
    battle_t bt; battle_log log;

    /* item use: heal */
    battle_init(&bt, &ma, &mb, 99);
    bt.mon[0].hp = 5;
    battle_action act[2] = { {BACT_ITEM, ITEM_SUNBERRY}, {BACT_MOVE, 0} };
    battle_resolve_turn(&bt, act, &log);
    CHECK(bt.mon[0].hp > 5 || bt.result != BATTLE_ONGOING);
    CHECK(log.ev[0].kind == BEV_ITEM);                       /* items go first */

    /* EP drain move */
    battle_init(&bt, &ma, &mb, 99);
    bt.mon[0].moves[0] = MOVE_MIND_SIPHON; bt.mon[0].ep = 50; bt.mon[1].ep = 40;
    battle_action a2[2] = { {BACT_MOVE, 0}, {BACT_MOVE, 1} };
    bt.mon[1].moves[1] = MOVE_HARDEN;
    battle_resolve_turn(&bt, a2, &log);
    int drained = 0; for (int i = 0; i < log.n; i++) if (log.ev[i].kind == BEV_EP_DRAIN) drained += log.ev[i].val;
    CHECK(drained > 0 || log.ev[1].kind == BEV_MISS);

    /* out-of-EP falls back to Struggle with recoil */
    battle_init(&bt, &ma, &mb, 5);
    bt.mon[0].ep = 0;
    battle_action a3[2] = { {BACT_MOVE, 0}, {BACT_MOVE, 1} };
    battle_resolve_turn(&bt, a3, &log);
    int struggle = 0; for (int i = 0; i < log.n; i++) if (log.ev[i].kind == BEV_USED && log.ev[i].id == MOVE_STRUGGLE) struggle++;
    CHECK(struggle >= 1);

    /* forfeit */
    battle_init(&bt, &ma, &mb, 5);
    battle_action a4[2] = { {BACT_FORFEIT, 0}, {BACT_MOVE, 0} };
    battle_resolve_turn(&bt, a4, &log);
    CHECK(bt.result == BATTLE_WIN_1);

    /* text formatting */
    char buf[64];
    for (int i = 0; i < log.n; i++) { CHECK(battle_event_text(&bt, &log.ev[i], buf, sizeof buf) > 0); }
    battle_init(&bt, &ma, &mb, 5);
    battle_resolve_turn(&bt, a3, &log);
    for (int i = 0; i < log.n; i++) { battle_event_text(&bt, &log.ev[i], buf, sizeof buf); printf("    %s\n", buf); }
}

static uint32_t trnd(void) { return (uint32_t)rand() * 2654435761u ^ (uint32_t)rand(); }

static void test_walk(void)
{
    /* longer walks should find more, and rarer items */
    long found_short = 0, found_long = 0, rare_short = 0, rare_long = 0;
    for (int trial = 0; trial < 300; trial++) {
        walk_state w; walk_init(&w);
        int64_t t = 100000;
        for (int sec = 0; sec < 1200; sec++) {          /* 20 min @ 2 steps/s = 2400 steps */
            t += 1000;
            uint8_t it = walk_feed(&w, 2, t, trnd);
            if (it) {
                const item_def *d = items_get(it); CHECK(d != NULL);
                if (sec < 100) { found_short++; rare_short += d->rarity >= 2; }
                else { found_long++; rare_long += d->rarity >= 2; }
            }
        }
    }
    printf("  walk: first 100 s: %ld finds (%ld rare+) | next 1100 s: %ld finds (%ld rare+) over 300 walks\n",
           found_short, rare_short, found_long, rare_long);
    CHECK(found_long > found_short);
    CHECK(rare_long * found_short > rare_short * found_long);     /* higher rare ratio when walking longer */

    /* gap ends the session */
    walk_state w; walk_init(&w);
    walk_feed(&w, 100, 1000, trnd);
    walk_feed(&w, 0, 1000 + WALK_GAP_MS + 1, trnd);
    CHECK(w.session_steps == 0);
}

int main(void)
{
    srand(1);
    test_charts(); test_data_refs(); test_hatch_and_levels(); test_inventory();
    test_walk(); test_battle_features(); test_battle();
    printf(fails ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", fails);
    return fails != 0;
}
