#ifndef BATTLE_SESSION_H
#define BATTLE_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include "battle.h"

/*
 * One battle between two linked KinBoxes. Both sides send a snapshot of their
 * kindral; the BLE central also sends the RNG seed. After that only each
 * turn's chosen action (plus a state hash) crosses the link; both devices run
 * the same engine and get identical results.
 */
void bsess_begin(void);
void bsess_abort(void);                    /* link lost / cancelled: nothing is committed */
bool bsess_rx(const uint8_t *d, size_t n); /* true if the message was ours */

bool bsess_ready(void);                    /* snapshots + seed exchanged */
bool bsess_active(void);
const battle_t *bsess_battle(void);
int  bsess_my_side(void);                  /* 0 = central, 1 = peripheral */

int  bsess_submit(battle_action a);        /* 0 ok; -EINVAL/-ENOENT/-EALREADY */
bool bsess_waiting_for_peer(void);         /* I've chosen, they haven't */
bool bsess_try_resolve(battle_log *log);   /* true when a turn was resolved */
bool bsess_desync(void);

/* Battle over: applies record/exp/HP/EP, saves. Fills a one-line summary. */
void bsess_finish(char *summary, size_t n);

#endif
