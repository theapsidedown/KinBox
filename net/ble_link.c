/*
 * BLE link layer for KinBox <-> KinBox.
 *
 *  Discovery : connectable advertising + passive scanning, manufacturer data
 *              "KB" + version + flags + species + level + trainer name.
 *  Link      : one GATT service with two characteristics
 *                RX (write)   - the peer writes messages to us
 *                TX (notify)  - we notify messages to the peer
 *              All messages are <= 20 bytes so the default ATT MTU (23) works.
 *  Proximity : average RSSI must beat CONFIG_KINBOX_BLE_RSSI_NEAR for several
 *              sightings in a row before the user is alerted.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include "net.h"
#include "app_event.h"

LOG_MODULE_REGISTER(net, LOG_LEVEL_INF);

#define PROTO_VER      1
#define MAX_PEERS      4
#define NAME_LEN       10
#define PEER_TIMEOUT_MS   30000
#define PEER_HITS_NEEDED  3
#define CONNECT_TIMEOUT_S 10

/* ---- UUIDs (random, project specific) ---- */
#define KB_UUID_SVC  BT_UUID_128_ENCODE(0x6b696e62, 0x6f78, 0x4b42, 0x8001, 0x000000000001ULL)
#define KB_UUID_RX   BT_UUID_128_ENCODE(0x6b696e62, 0x6f78, 0x4b42, 0x8001, 0x000000000002ULL)
#define KB_UUID_TX   BT_UUID_128_ENCODE(0x6b696e62, 0x6f78, 0x4b42, 0x8001, 0x000000000003ULL)

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(KB_UUID_SVC);
static struct bt_uuid_128 rx_uuid  = BT_UUID_INIT_128(KB_UUID_RX);
static struct bt_uuid_128 tx_uuid  = BT_UUID_INIT_128(KB_UUID_TX);

/* ---- State ---- */
enum net_state { NS_OFF, NS_IDLE, NS_CONNECTING, NS_DISCOVER, NS_LINKED };

struct peer {
    bool used;
    bt_addr_le_t addr;
    char trainer[NAME_LEN + 1];
    uint8_t species, level, flags;
    int8_t rssi;
    uint8_t hits;
    int64_t last_ms;
    int64_t ignore_until;
    bool alerted;
};

static K_MUTEX_DEFINE(lock);
static struct peer peers[MAX_PEERS];
static enum net_state state = NS_OFF;
static bool visible;
static bool bt_ready;

static struct bt_conn *conn_ref;
static bool is_central;
static bool link_announced;
static uint16_t rx_handle, tx_handle;      /* peer's characteristics (central role) */

/* Our advertised profile */
static uint8_t adv_mfg[2 + 2 + 1 + 1 + 1 + 1 + NAME_LEN];

/* Dynamic Bluetooth name: "Ash's KinBox", etc. */
static char adv_name[NAME_LEN + sizeof("'s KinBox")] = "Unknown's KinBox";

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_mfg, sizeof(adv_mfg)),
};

/* Device name is sent in the scan response. */
static struct bt_data sd[] = {
    {
        .type = BT_DATA_NAME_COMPLETE,
        .data_len = sizeof("Unknown's KinBox") - 1,
        .data = (const uint8_t *)adv_name,
    },
};

static bool adv_running, scan_running;

static void link_timeout_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(link_timeout, link_timeout_fn);

/* ------------------------------------------------------------------ */
/* Advertising + scanning                                              */
/* ------------------------------------------------------------------ */

static void adv_fill_defaults(void)
{
    adv_mfg[0] = 0xFF; adv_mfg[1] = 0xFF;      /* company id: 0xFFFF = test/internal */
    adv_mfg[2] = 'K';  adv_mfg[3] = 'B';
    adv_mfg[4] = PROTO_VER;
}

void net_set_profile(const char *trainer, uint8_t species, uint8_t level)
{
    const char *safe_trainer =
        (trainer && trainer[0]) ? trainer : "Unknown";

    adv_fill_defaults();

    /* Generate the Bluetooth device name */
    snprintf(adv_name, sizeof(adv_name),
             "%.*s's KinBox",
             NAME_LEN,
             safe_trainer);

    /* Update the actual scan response length */
    sd[0].data_len = (uint8_t)strlen(adv_name);

    /* Manufacturer data */
    adv_mfg[5] = 0;       /* flags */
    adv_mfg[6] = species;
    adv_mfg[7] = level;

    memset(&adv_mfg[8], 0, NAME_LEN);
    strncpy((char *)&adv_mfg[8], safe_trainer, NAME_LEN);

    /* Update advertising if it is already running */
    if (adv_running) {
        int err = bt_le_adv_update_data(
            ad, ARRAY_SIZE(ad),
            sd, ARRAY_SIZE(sd)
        );

        if (err) {
            LOG_ERR("Advertising update failed (%d)", err);
        }
    }

    LOG_INF("Bluetooth name: %s", adv_name);
}

static void adv_start(void)
{
    if (!bt_ready || !visible || adv_running || state != NS_IDLE) {
        return;
    }
    static const struct bt_le_adv_param p = BT_LE_ADV_PARAM_INIT(
    BT_LE_ADV_OPT_CONN,
    0x0320, 0x0640, NULL);                  /* 500 ms .. 1 s */
    int err = bt_le_adv_start(&p, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY) {
        LOG_ERR("adv start failed (%d)", err);
        return;
    }
    adv_running = true;
}

static void adv_stop(void)
{
    if (adv_running) {
        bt_le_adv_stop();
        adv_running = false;
    }
}

struct ad_ctx { bool is_kb; uint8_t ver, flags, species, level; char name[NAME_LEN + 1]; };

static bool ad_parse_cb(struct bt_data *data, void *user_data)
{
    struct ad_ctx *c = user_data;

    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len >= sizeof(adv_mfg) &&
        data->data[0] == 0xFF && data->data[1] == 0xFF &&
        data->data[2] == 'K' && data->data[3] == 'B') {
        c->is_kb = true;
        c->ver = data->data[4];
        c->flags = data->data[5];
        c->species = data->data[6];
        c->level = data->data[7];
        memcpy(c->name, &data->data[8], NAME_LEN);
        c->name[NAME_LEN] = '\0';
        return false;           /* stop parsing */
    }
    return true;
}

static void scan_recv(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
                      struct net_buf_simple *buf)
{
    struct ad_ctx c = {0};

    if (adv_type != BT_GAP_ADV_TYPE_ADV_IND) {
        return;
    }
    bt_data_parse(buf, ad_parse_cb, &c);
    if (!c.is_kb || c.ver != PROTO_VER) {
        return;
    }

    int64_t now = k_uptime_get();
    struct peer *p = NULL, *free_slot = NULL, *oldest = &peers[0];
    app_event ev = {0};
    bool alert = false;

    k_mutex_lock(&lock, K_FOREVER);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].used && bt_addr_le_eq(&peers[i].addr, addr)) {
            p = &peers[i];
        } else if (!peers[i].used && !free_slot) {
            free_slot = &peers[i];
        }
        if (peers[i].last_ms < oldest->last_ms) {
            oldest = &peers[i];
        }
    }
    if (!p) {
        p = free_slot ? free_slot : oldest;
        memset(p, 0, sizeof(*p));
        p->used = true;
        bt_addr_le_copy(&p->addr, addr);
        p->rssi = rssi;
    }

    p->rssi = (int8_t)((p->rssi * 3 + rssi) / 4);          /* smooth */
    p->hits = (now - p->last_ms < 6000) ? (uint8_t)MIN(p->hits + 1, 250) : 1;
    p->last_ms = now;
    memcpy(p->trainer, c.name, sizeof(p->trainer));
    p->species = c.species;
    p->level = c.level;
    p->flags = c.flags;

    if (!p->alerted && p->hits >= PEER_HITS_NEEDED && p->rssi >= CONFIG_KINBOX_BLE_RSSI_NEAR &&
        now >= p->ignore_until && state == NS_IDLE) {
        p->alerted = true;
        alert = true;
        ev.type = EVT_NET_PEER_NEAR;
        ev.peer.idx = (uint8_t)(p - peers);
        ev.peer.rssi = p->rssi;
        ev.peer.species = p->species;
        ev.peer.level = p->level;
        memcpy(ev.peer.trainer, p->trainer, sizeof(ev.peer.trainer) - 1);
    }
    k_mutex_unlock(&lock);

    if (alert) {
        app_post(&ev);
    }
}

static int scan_fail_logged;

/*
 * The ESP32 controller rejected the first scan parameters we tried on some builds
 * (HCI "LE Set Scan Enable" status 0x12, invalid parameters), so try a short ladder:
 * configured duty cycle -> 100 ms continuous -> Zephyr's default fast scan.
 */
static void scan_start(void)
{
    static const struct { uint16_t itv, win; } fallback[] = {
        { 0x00A0, 0x00A0 },      /* 100 ms / 100 ms */
        { BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_WINDOW },
    };
    struct bt_le_scan_param sp = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = (CONFIG_KINBOX_BLE_SCAN_INTERVAL_MS * 8 / 5),   /* 0.625 ms units */
        .window = (CONFIG_KINBOX_BLE_SCAN_WINDOW_MS * 8 / 5),
    };
    int err;

    if (!bt_ready || !visible || scan_running || state != NS_IDLE) {
        return;
    }
    err = bt_le_scan_start(&sp, scan_recv);
    for (size_t i = 0; err && err != -EALREADY && i < ARRAY_SIZE(fallback); i++) {
        sp.interval = fallback[i].itv;
        sp.window = fallback[i].win;
        err = bt_le_scan_start(&sp, scan_recv);
        if (!err) {
            LOG_WRN("scan running with fallback params (itv %u, win %u)", sp.interval, sp.window);
        }
    }
    if (err && err != -EALREADY) {
        if (!scan_fail_logged++) {
            LOG_ERR("scan start failed (%d); will keep retrying", err);
        }
        return;
    }
    scan_fail_logged = 0;
    scan_running = true;
}

static void scan_stop(void)
{
    if (scan_running) {
        bt_le_scan_stop();
        scan_running = false;
    }
}

static void go_idle(void)
{
    state = NS_IDLE;
    link_announced = false;
    scan_start();
    adv_start();
    scan_start();
}

/* ------------------------------------------------------------------ */
/* GATT server (peripheral side, and the RX path for both roles)       */
/* ------------------------------------------------------------------ */

static ssize_t rx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                        uint16_t len, uint16_t offset, uint8_t flags)
{
    ARG_UNUSED(attr); ARG_UNUSED(flags);

    if (offset != 0 || len == 0 || len > NET_MSG_MAX) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    if (conn != conn_ref) {
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }
    app_event e = { .type = EVT_NET_RX };

    e.rx.len = (uint8_t)len;
    memcpy(e.rx.data, buf, len);
    app_post(&e);
    return len;
}

static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);

    if (value == BT_GATT_CCC_NOTIFY && conn_ref && !is_central && !link_announced) {
        link_announced = true;
        state = NS_LINKED;
        k_work_cancel_delayable(&link_timeout);
        app_event e = { .type = EVT_NET_LINK_UP, .link.is_central = 0 };

        app_post(&e);
    }
}

BT_GATT_SERVICE_DEFINE(kb_svc,
    BT_GATT_PRIMARY_SERVICE(&svc_uuid),
    BT_GATT_CHARACTERISTIC(&rx_uuid.uuid,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE, NULL, rx_write, NULL),
    BT_GATT_CHARACTERISTIC(&tx_uuid.uuid, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(tx_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

/* ------------------------------------------------------------------ */
/* GATT client (central side): find the peer's characteristics, subscribe */
/* ------------------------------------------------------------------ */

static struct bt_gatt_discover_params disc;
static struct bt_gatt_subscribe_params sub;
static uint8_t disc_stage;

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    ARG_UNUSED(conn); ARG_UNUSED(params);

    if (data == NULL) {
        return BT_GATT_ITER_STOP;
    }
    if (length > 0 && length <= NET_MSG_MAX) {
        app_event e = { .type = EVT_NET_RX };

        e.rx.len = (uint8_t)length;
        memcpy(e.rx.data, data, length);
        app_post(&e);
    }
    return BT_GATT_ITER_CONTINUE;
}

static void subscribed_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params)
{
    ARG_UNUSED(conn); ARG_UNUSED(params);

    if (err) {
        LOG_ERR("subscribe failed (%u)", err);
        net_disconnect();
        return;
    }
    link_announced = true;
    state = NS_LINKED;
    k_work_cancel_delayable(&link_timeout);
    app_event e = { .type = EVT_NET_LINK_UP, .link.is_central = 1 };

    app_post(&e);
}

static uint8_t disc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                       struct bt_gatt_discover_params *params)
{
    if (attr == NULL) {
        LOG_ERR("KinBox service not found on peer");
        net_disconnect();
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *chrc = attr->user_data;

    if (disc_stage == 0) {
        rx_handle = chrc->value_handle;
        disc_stage = 1;
        params->uuid = &tx_uuid.uuid;
        params->start_handle = attr->handle + 1;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        if (bt_gatt_discover(conn, params)) {
            net_disconnect();
        }
        return BT_GATT_ITER_STOP;
    }

    tx_handle = chrc->value_handle;
    sub.notify = notify_cb;
    sub.subscribe = subscribed_cb;
    sub.value = BT_GATT_CCC_NOTIFY;
    sub.value_handle = tx_handle;
    sub.ccc_handle = tx_handle + 1;     /* our service puts the CCC right after the value */
    if (bt_gatt_subscribe(conn, &sub)) {
        net_disconnect();
    }
    return BT_GATT_ITER_STOP;
}

static void start_discovery(struct bt_conn *conn)
{
    memset(&disc, 0, sizeof(disc));
    disc_stage = 0;
    disc.uuid = &rx_uuid.uuid;
    disc.func = disc_cb;
    disc.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    disc.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    disc.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    state = NS_DISCOVER;
    if (bt_gatt_discover(conn, &disc)) {
        net_disconnect();
    }
}

/* ------------------------------------------------------------------ */
/* Connection callbacks                                                */
/* ------------------------------------------------------------------ */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    struct bt_conn_info info;

    if (err) {
        LOG_WRN("connect failed (0x%02x)", err);
        if (conn_ref) {
            bt_conn_unref(conn_ref);
            conn_ref = NULL;
        }
        app_event e = { .type = EVT_NET_CONNECT_FAIL };

        app_post(&e);
        go_idle();
        return;
    }
    bt_conn_get_info(conn, &info);
    scan_stop();
    adv_stop();

    if (info.role == BT_CONN_ROLE_CENTRAL) {
        is_central = true;                  /* conn_ref was set by net_connect() */
        start_discovery(conn);
    } else {
        if (conn_ref) {                     /* we were also dialing out: yield */
            bt_conn_disconnect(conn_ref, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            bt_conn_unref(conn_ref);
        }
        conn_ref = bt_conn_ref(conn);
        is_central = false;
        state = NS_DISCOVER;               /* waiting for the peer to subscribe */
    }
    k_work_reschedule(&link_timeout, K_SECONDS(CONNECT_TIMEOUT_S));
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(reason);
    if (conn != conn_ref) {
        return;
    }
    bool was_linked = link_announced;

    bt_conn_unref(conn_ref);
    conn_ref = NULL;
    k_work_cancel_delayable(&link_timeout);

    app_event e = { .type = was_linked ? EVT_NET_LINK_DOWN : EVT_NET_CONNECT_FAIL };

    app_post(&e);
    go_idle();
}

BT_CONN_CB_DEFINE(conn_cbs) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static void link_timeout_fn(struct k_work *w)
{
    ARG_UNUSED(w);
    if (state == NS_CONNECTING || state == NS_DISCOVER) {
        LOG_WRN("link setup timed out");
        net_disconnect();
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int net_init(void)
{
    int err = bt_enable(NULL);

    if (err) {
        LOG_ERR("bt_enable failed (%d)", err);
        return err;
    }
    bt_ready = true;
    adv_fill_defaults();
    state = NS_IDLE;
    return 0;
}

int net_set_visible(bool on)
{
    visible = on;
    if (!bt_ready) {
        return -ENODEV;
    }
    if (on) {
        scan_start();
        adv_start();
    } else {
        adv_stop();
        scan_stop();
        k_mutex_lock(&lock, K_FOREVER);
        memset(peers, 0, sizeof(peers));
        k_mutex_unlock(&lock);
    }
    return 0;
}

void net_stop(void)
{
    net_disconnect();
    visible = false;
    adv_stop();
    scan_stop();
}

int net_connect(uint8_t idx)
{
    bt_addr_le_t addr;

    if (!bt_ready || state != NS_IDLE || idx >= MAX_PEERS) {
        return -EBUSY;
    }
    k_mutex_lock(&lock, K_FOREVER);
    bool ok = peers[idx].used;

    if (ok) {
        bt_addr_le_copy(&addr, &peers[idx].addr);
    }
    k_mutex_unlock(&lock);
    if (!ok) {
        return -ENOENT;
    }

    scan_stop();
    adv_stop();
    state = NS_CONNECTING;
    link_announced = false;

    struct bt_conn_le_create_param cp = {
        .options = BT_CONN_LE_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
        .timeout = CONNECT_TIMEOUT_S * 100,
    };
    int err = bt_conn_le_create(&addr, &cp, BT_LE_CONN_PARAM_DEFAULT, &conn_ref);

    if (err) {
        LOG_ERR("bt_conn_le_create failed (%d)", err);
        go_idle();
        return err;
    }
    k_work_reschedule(&link_timeout, K_SECONDS(CONNECT_TIMEOUT_S));
    return 0;
}

void net_disconnect(void)
{
    if (conn_ref) {
        bt_conn_disconnect(conn_ref, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        /* on_disconnected() cleans up and restarts discovery */
    } else if (state != NS_IDLE && state != NS_OFF) {
        go_idle();
    }
}

void net_ignore_peer(uint8_t idx, int seconds)
{
    if (idx >= MAX_PEERS) {
        return;
    }
    k_mutex_lock(&lock, K_FOREVER);
    peers[idx].ignore_until = k_uptime_get() + (int64_t)seconds * 1000;
    k_mutex_unlock(&lock);
}

int net_send(const uint8_t *data, size_t len)
{
    if (!conn_ref || state != NS_LINKED || len == 0 || len > NET_MSG_MAX) {
        return -ENOTCONN;
    }
    if (is_central) {
        return bt_gatt_write_without_response(conn_ref, rx_handle, data, len, false);
    }
    return bt_gatt_notify_uuid(conn_ref, &tx_uuid.uuid, kb_svc.attrs, data, len);
}

bool net_link_up(void)
{
    return state == NS_LINKED;
}

bool net_is_central(void)
{
    return is_central;
}

void net_housekeeping(void)
{
    int64_t now = k_uptime_get();

    k_mutex_lock(&lock, K_FOREVER);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].used && now - peers[i].last_ms > PEER_TIMEOUT_MS) {
            peers[i].used = false;          /* walked away: forget, can alert again later */
        }
    }
    k_mutex_unlock(&lock);
}
