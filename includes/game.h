#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "kindrals.h"
#include "inventory.h"

/* Everything that makes up "this KinBox's save", plus a few runtime flags. */
typedef struct {
    kindral kin;
    char trainer[KIN_NAME_LEN];
    inventory inv;
    uint32_t steps_total;
    int64_t last_unix;        /* wall-clock time of the last save/boot */
    bool visible;             /* BLE social visibility (Settings)      */

    /* runtime only */
    bool setup_done;          /* trainer + nickname chosen -> saving allowed */
    bool in_event;            /* battle / trade running: care ticks pause    */
} game_t;

extern game_t g_game;

#endif
