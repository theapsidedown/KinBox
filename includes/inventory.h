#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>
#include <stdbool.h>

#define INV_SLOTS      16
#define INV_MAX_COUNT  99

typedef struct {
    uint8_t id;      /* item_id, 0 = empty slot */
    uint8_t count;
} inv_slot;

typedef struct {
    inv_slot slot[INV_SLOTS];
} inventory;

void inv_clear(inventory *inv);
void inv_give_starter_items(inventory *inv);

int  inv_count(const inventory *inv, uint8_t id);
bool inv_can_add(const inventory *inv, uint8_t id, int n);
int  inv_add(inventory *inv, uint8_t id, int n);      /* 0 / -ENOSPC / -EINVAL */
int  inv_remove(inventory *inv, uint8_t id, int n);   /* 0 / -ENOENT           */

/* Fill ids[] (up to max) with the item ids held, packed. Returns how many. */
int  inv_list(const inventory *inv, uint8_t *ids, int max);

#endif
