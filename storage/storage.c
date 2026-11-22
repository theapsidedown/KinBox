#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include "storage.h"
#include "kclock.h"
#include "moves.h"
#include "items.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

BUILD_ASSERT(FIXED_PARTITION_EXISTS(storage_partition),
             "A 'storage_partition' flash partition is required (see boards/*.overlay)");

#define ID_META  1
#define ID_KIN   2
#define ID_INV   3

#define SAVE_MAGIC    0x3158424Bu   /* "KBX1" */
#define SAVE_VERSION  1

/* ---- On-disk layouts: fixed width + packed, versioned. Never reorder; add a
 * new version instead. ---- */
struct __packed rec_meta {
    uint32_t magic;
    uint16_t version;
    uint8_t  visible;
    uint8_t  reserved;
    char     trainer[KIN_NAME_LEN];
};

struct __packed rec_kin {
    int64_t  birth_time;
    int64_t  last_unix;
    uint32_t steps_total;
    char     name[KIN_NAME_LEN];
    uint8_t  species, ability, level;
    uint8_t  moves[KIN_MAX_MOVES];
    int32_t  exp, hp, max_hp, ep, max_ep, atk, def, spd, friendship;
    uint8_t  happiness, hunger, thirst, sleep;
    uint32_t wins, losses, draws, win_streak, best_streak;
};

static struct nvs_fs fs;
static bool mounted;
static uint32_t dirty;
static int64_t last_flush_ms;
static bool deferred;

int storage_init(void)
{
    struct flash_pages_info info;

    fs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Flash device not ready");
        return -ENODEV;
    }
    fs.offset = FIXED_PARTITION_OFFSET(storage_partition);
    int rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);

    if (rc) {
        return rc;
    }
    fs.sector_size = info.size;
    fs.sector_count = FIXED_PARTITION_SIZE(storage_partition) / info.size;

    rc = nvs_mount(&fs);
    if (rc) {
        LOG_ERR("nvs_mount failed (%d)", rc);
        return rc;
    }
    mounted = true;
    LOG_INF("NVS mounted: %u sectors x %u B", fs.sector_count, fs.sector_size);
    return 0;
}

/* ------------------------------------------------------------------ */

static void kin_to_rec(const game_t *g, struct rec_kin *r)
{
    const kindral *k = &g->kin;

    memset(r, 0, sizeof(*r));
    r->birth_time = k->id.birth_time;
    r->last_unix = g->last_unix;
    r->steps_total = g->steps_total;
    memcpy(r->name, k->id.name, sizeof(r->name));
    r->species = (uint8_t)k->id.species;
    r->ability = (uint8_t)k->id.ability;
    r->level = (uint8_t)k->stats.level;
    memcpy(r->moves, k->moves.slot, KIN_MAX_MOVES);
    r->exp = k->stats.exp_points;
    r->hp = k->stats.health_points;
    r->max_hp = k->stats.max_hp;
    r->ep = k->stats.energy_points;
    r->max_ep = k->stats.max_ep;
    r->atk = k->stats.attack;
    r->def = k->stats.defense;
    r->spd = k->stats.speed;
    r->friendship = k->stats.friendship;
    r->happiness = (uint8_t)k->walker.happiness;
    r->hunger = (uint8_t)k->walker.hunger;
    r->thirst = (uint8_t)k->walker.thirst;
    r->sleep = (uint8_t)k->walker.sleep;
    r->wins = k->record.wins;
    r->losses = k->record.losses;
    r->draws = k->record.draws;
    r->win_streak = k->record.win_streak;
    r->best_streak = k->record.best_streak;
}

/* Validate before trusting anything read from flash. */
static int rec_to_kin(const struct rec_kin *r, game_t *g)
{
    if (r->species >= KIN_COUNT || !kindrals_species_defined((enum kindral_id)r->species) ||
        r->ability >= KIN_ABILITY_COUNT || r->level > KIN_MAX_LEVEL ||
        r->max_hp < 1 || r->max_ep < 1 || r->hp < 0 || r->hp > r->max_hp ||
        r->ep < 0 || r->ep > r->max_ep || r->name[KIN_NAME_LEN - 1] != '\0') {
        return -EBADMSG;
    }
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        if (r->moves[i] != MOVE_NONE && moves_get(r->moves[i]) == NULL) {
            return -EBADMSG;
        }
    }
    kindral *k = &g->kin;

    memset(k, 0, sizeof(*k));
    k->id.birth_time = r->birth_time;
    k->id.species = (enum kindral_id)r->species;
    k->id.ability = (enum kindral_ability)r->ability;
    memcpy(k->id.name, r->name, sizeof(k->id.name));
    k->stats.level = r->level;
    memcpy(k->moves.slot, r->moves, KIN_MAX_MOVES);
    k->stats.exp_points = r->exp;
    k->stats.health_points = r->hp;
    k->stats.max_hp = r->max_hp;
    k->stats.energy_points = r->ep;
    k->stats.max_ep = r->max_ep;
    k->stats.attack = r->atk;
    k->stats.defense = r->def;
    k->stats.speed = r->spd;
    k->stats.friendship = r->friendship;
    k->walker.happiness = MIN(r->happiness, KIN_METER_MAX);
    k->walker.hunger = MIN(r->hunger, KIN_METER_MAX);
    k->walker.thirst = MIN(r->thirst, KIN_METER_MAX);
    k->walker.sleep = MIN(r->sleep, KIN_METER_MAX);
    k->record.wins = r->wins;
    k->record.losses = r->losses;
    k->record.draws = r->draws;
    k->record.win_streak = r->win_streak;
    k->record.best_streak = r->best_streak;
    g->last_unix = r->last_unix;
    g->steps_total = r->steps_total;
    return 0;
}

int storage_load(game_t *g)
{
    struct rec_meta meta;
    struct rec_kin rk;
    inventory inv;
    int rc;

    if (!mounted) {
        return -ENODEV;
    }
    rc = nvs_read(&fs, ID_META, &meta, sizeof(meta));
    if (rc == -ENOENT) {
        return -ENOENT;
    }
    if (rc != sizeof(meta) || meta.magic != SAVE_MAGIC || meta.version != SAVE_VERSION) {
        LOG_ERR("Bad meta record (%d)", rc);
        return -EBADMSG;
    }
    rc = nvs_read(&fs, ID_KIN, &rk, sizeof(rk));
    if (rc != sizeof(rk) || rec_to_kin(&rk, g) != 0) {
        LOG_ERR("Bad kindral record (%d)", rc);
        return -EBADMSG;
    }
    rc = nvs_read(&fs, ID_INV, &inv, sizeof(inv));
    if (rc != sizeof(inv)) {
        LOG_ERR("Bad inventory record (%d)", rc);
        return -EBADMSG;
    }
    for (int i = 0; i < INV_SLOTS; i++) {
        if (inv.slot[i].id != 0 && items_get(inv.slot[i].id) == NULL) {
            inv.slot[i] = (inv_slot){0};
        }
    }
    g->inv = inv;
    memcpy(g->trainer, meta.trainer, sizeof(g->trainer));
    g->trainer[KIN_NAME_LEN - 1] = '\0';
    g->visible = meta.visible != 0;
    g->setup_done = true;
    return 0;
}

/* ------------------------------------------------------------------ */

int storage_save_now(uint32_t mask)
{
    int rc = 0;

    mask |= dirty;
    if (!mounted || !g_game.setup_done) {
        return -EAGAIN;                 /* never write before setup is finished */
    }

    if (mask & SAVE_KIN) {
        struct rec_kin rk;

        g_game.last_unix = kclock_now();
        kin_to_rec(&g_game, &rk);
        rc = nvs_write(&fs, ID_KIN, &rk, sizeof(rk));
        if (rc < 0) {
            goto fail;
        }
    }
    if (mask & SAVE_INV) {
        rc = nvs_write(&fs, ID_INV, &g_game.inv, sizeof(g_game.inv));
        if (rc < 0) {
            goto fail;
        }
    }
    /* Meta last: its presence means a full save exists. Rewrite it with the
     * first save and whenever settings changed (NVS skips identical data). */
    {
        struct rec_meta meta = { .magic = SAVE_MAGIC, .version = SAVE_VERSION,
                                 .visible = g_game.visible };

        memcpy(meta.trainer, g_game.trainer, sizeof(meta.trainer));
        rc = nvs_write(&fs, ID_META, &meta, sizeof(meta));
        if (rc < 0) {
            goto fail;
        }
    }
    dirty = 0;
    last_flush_ms = k_uptime_get();
    return 0;

fail:
    LOG_ERR("NVS write failed (%d)", rc);
    return rc;
}

void storage_mark_dirty(uint32_t mask)
{
    dirty |= mask;
}

void storage_defer(bool hold)
{
    deferred = hold;
}

void storage_poll(int64_t now_ms)
{
    if (!g_game.setup_done || deferred || dirty == 0) {
        return;
    }
    if (now_ms - last_flush_ms >= (int64_t)CONFIG_KINBOX_SAVE_INTERVAL_S * 1000) {
        storage_save_now(dirty);
    }
}

void storage_factory_reset(void)
{
    LOG_WRN("FACTORY RESET");
    if (mounted) {
        nvs_clear(&fs);
    }
    sys_reboot(SYS_REBOOT_COLD);
    for (;;) {
    }
}
