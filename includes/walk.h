#ifndef WALK_H
#define WALK_H

#include <stdint.h>

/*
 * Walk sessions & item finds. Pure logic (host-testable).
 *
 * A session is continuous walking (gaps shorter than WALK_GAP_MS). Every
 * WALK_ROLL_STEPS steps there is a chance of finding an item; the chance and the
 * rarity both grow with the length of the session.
 */
#define WALK_GAP_MS      30000
#define WALK_ROLL_STEPS  100

typedef struct {
    uint32_t session_steps;
    uint32_t since_roll;
    int64_t  last_step_ms;
} walk_state;

void walk_init(walk_state *w);

/* Feed new steps; returns the item id found (0 = none). 'rnd' returns random u32. */
uint8_t walk_feed(walk_state *w, uint32_t new_steps, int64_t now_ms, uint32_t (*rnd)(void));

/* Roll helpers exposed for tests. */
int walk_find_chance_pct(uint32_t session_steps);
int walk_pick_rarity(uint32_t session_steps, uint32_t rnd);

#endif
