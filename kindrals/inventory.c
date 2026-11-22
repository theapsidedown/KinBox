#include <errno.h>
#include <string.h>
#include "inventory.h"
#include "items.h"

void inv_clear(inventory *inv)
{
    memset(inv, 0, sizeof(*inv));
}

void inv_give_starter_items(inventory *inv)
{
    inv_clear(inv);
    inv_add(inv, ITEM_SUNBERRY, 3);
    inv_add(inv, ITEM_MOON_TONIC, 2);
    inv_add(inv, ITEM_POWER_SEED, 1);
    inv_add(inv, ITEM_GUARD_PEBBLE, 1);
    inv_add(inv, ITEM_HONEY_BUN, 2);
}

static inv_slot *find(inventory *inv, uint8_t id)
{
    for (int i = 0; i < INV_SLOTS; i++) {
        if (inv->slot[i].id == id && inv->slot[i].count > 0) {
            return &inv->slot[i];
        }
    }
    return NULL;
}

int inv_count(const inventory *inv, uint8_t id)
{
    for (int i = 0; i < INV_SLOTS; i++) {
        if (inv->slot[i].id == id) {
            return inv->slot[i].count;
        }
    }
    return 0;
}

bool inv_can_add(const inventory *inv, uint8_t id, int n)
{
    if (items_get(id) == NULL || n <= 0) {
        return false;
    }
    int have = inv_count(inv, id);

    if (have > 0) {
        return have + n <= INV_MAX_COUNT;
    }
    for (int i = 0; i < INV_SLOTS; i++) {
        if (inv->slot[i].id == 0) {
            return n <= INV_MAX_COUNT;
        }
    }
    return false;
}

int inv_add(inventory *inv, uint8_t id, int n)
{
    if (items_get(id) == NULL || n <= 0) {
        return -EINVAL;
    }
    if (!inv_can_add(inv, id, n)) {
        return -ENOSPC;
    }
    inv_slot *s = find(inv, id);

    if (s == NULL) {
        for (int i = 0; i < INV_SLOTS; i++) {
            if (inv->slot[i].id == 0) {
                s = &inv->slot[i];
                s->id = id;
                s->count = 0;
                break;
            }
        }
    }
    s->count = (uint8_t)(s->count + n);
    return 0;
}

int inv_remove(inventory *inv, uint8_t id, int n)
{
    inv_slot *s = find(inv, id);

    if (s == NULL || s->count < n || n <= 0) {
        return -ENOENT;
    }
    s->count = (uint8_t)(s->count - n);
    if (s->count == 0) {
        s->id = 0;
    }
    return 0;
}

int inv_list(const inventory *inv, uint8_t *ids, int max)
{
    int n = 0;

    for (int i = 0; i < INV_SLOTS && n < max; i++) {
        if (inv->slot[i].id != 0 && inv->slot[i].count > 0) {
            ids[n++] = inv->slot[i].id;
        }
    }
    return n;
}
