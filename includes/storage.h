#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "game.h"

/*
 * Persistence on Zephyr NVS (circular log across several 4 KB flash sectors).
 *
 * Wear strategy:
 *  - 3 small records (meta, kindral, inventory), each rewritten only when saved.
 *  - NVS skips a write if the bytes are identical to the stored copy.
 *  - Routine changes are only marked dirty and flushed every
 *    CONFIG_KINBOX_SAVE_INTERVAL_S seconds; important events (naming, battle end,
 *    trade, gift, power off) flush immediately.
 *  - Nothing is written until the first-time setup (trainer + nickname) is done.
 *  - Periodic flushes are deferred while a battle/trade is running.
 *
 * Flush order is kindral -> inventory -> meta, so a meta record only exists if a
 * complete save was written; a power cut mid-save leaves the previous one intact.
 */

#define SAVE_KIN   0x01
#define SAVE_INV   0x02
#define SAVE_META  0x04
#define SAVE_ALL   (SAVE_KIN | SAVE_INV | SAVE_META)

int storage_init(void);

/* 0 = loaded into *g;  -ENOENT = nothing saved yet (first boot);
 * other negative = unreadable/corrupt (treated as first boot by the caller). */
int storage_load(game_t *g);

void storage_mark_dirty(uint32_t mask);
int  storage_save_now(uint32_t mask);          /* no-op until g_game.setup_done */
void storage_poll(int64_t now_ms);             /* call ~1/s */
void storage_defer(bool hold);                 /* hold periodic flushes */

/* Wipes the whole partition and reboots. Does not return. */
void storage_factory_reset(void);

#endif
