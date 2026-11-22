#include <string.h>
#include "walk.h"
#include "items.h"

void walk_init(walk_state *w)
{
    memset(w, 0, sizeof(*w));
}

int walk_find_chance_pct(uint32_t session_steps)
{
    int p = 3 + (int)(session_steps / 100);

    return p > 20 ? 20 : p;
}

int walk_pick_rarity(uint32_t session_steps, uint32_t rnd)
{
    /* cumulative weights for rarity 0,1,2,3 */
    static const uint8_t w[4][4] = {
        {90, 100, 100, 100},    /* < 200 steps  */
        {60,  90, 100, 100},    /* < 600        */
        {35,  70,  95, 100},    /* < 1500       */
        {20,  50,  85, 100},    /* long walks   */
    };
    int tier = session_steps < 200 ? 0 : session_steps < 600 ? 1 : session_steps < 1500 ? 2 : 3;
    int roll = (int)(rnd % 100);

    for (int r = 0; r < 4; r++) {
        if (roll < w[tier][r]) {
            return r;
        }
    }
    return 0;
}

uint8_t walk_feed(walk_state *w, uint32_t new_steps, int64_t now_ms, uint32_t (*rnd)(void))
{
    uint8_t found = ITEM_NONE;

    if (new_steps == 0) {
        if (w->session_steps && now_ms - w->last_step_ms > WALK_GAP_MS) {
            w->session_steps = 0;       /* walk ended */
            w->since_roll = 0;
        }
        return ITEM_NONE;
    }
    if (now_ms - w->last_step_ms > WALK_GAP_MS) {
        w->session_steps = 0;           /* new walk */
        w->since_roll = 0;
    }
    w->last_step_ms = now_ms;
    w->session_steps += new_steps;
    w->since_roll += new_steps;

    while (w->since_roll >= WALK_ROLL_STEPS) {
        w->since_roll -= WALK_ROLL_STEPS;
        if ((int)(rnd() % 100) < walk_find_chance_pct(w->session_steps) && found == ITEM_NONE) {
            found = items_random_of_rarity(walk_pick_rarity(w->session_steps, rnd()), rnd());
        }
    }
    return found;
}
