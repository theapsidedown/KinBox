/*
 * Social layer: connection handshake, the link menu actions (greet, gift),
 * trade, and routing of battle messages. Pure state machine: it never draws.
 * The UI polls state and drains notes (social_next_note).
 */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "social.h"
#include "net.h"
#include "proto.h"
#include "game.h"
#include "items.h"
#include "storage.h"
#include "battle_session.h"

LOG_MODULE_REGISTER(social, LOG_LEVEL_INF);

char g_peer_trainer[12];

static soc_state state;
static uint8_t my_prop, peer_prop;

#define NOTE_Q 8
static soc_note notes[NOTE_Q];
static int note_head, note_tail;

static int64_t greet_ready_ms;
static uint8_t pending_gift;

/* trade */
static uint8_t t_mine, t_theirs;
static bool t_my_ok, t_peer_ok, t_done, t_active;

#define GREET_COOLDOWN_MS (10 * 60 * 1000)

static void note(enum soc_note_kind kind, const char *fmt, ...)
{
    soc_note *n = &notes[note_head];
    va_list ap;

    n->kind = (uint8_t)kind;
    n->text[0] = '\0';
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(n->text, sizeof(n->text), fmt, ap);
        va_end(ap);
    }
    note_head = (note_head + 1) % NOTE_Q;
    if (note_head == note_tail) {
        note_tail = (note_tail + 1) % NOTE_Q;       /* drop the oldest */
    }
}

bool social_next_note(soc_note *n)
{
    if (note_tail == note_head) {
        return false;
    }
    *n = notes[note_tail];
    note_tail = (note_tail + 1) % NOTE_Q;
    return true;
}

void social_init(void)
{
    state = SOC_IDLE;
    my_prop = peer_prop = PROP_NONE;
}

soc_state social_state(void)
{
    return state;
}

static void send(const uint8_t *d, size_t n)
{
    int rc = net_send(d, n);

    if (rc) {
        LOG_WRN("send failed (%d)", rc);
    }
}

static void send_name(uint8_t type)
{
    uint8_t m[12] = { type };

    strncpy((char *)&m[1], g_game.trainer, 11);
    send(m, sizeof(m));
}

static void reset_link_state(void)
{
    my_prop = peer_prop = PROP_NONE;
    pending_gift = 0;
    t_mine = t_theirs = 0;
    t_my_ok = t_peer_ok = t_done = t_active = false;
}

/* ------------------------------------------------------------------ */
/* User actions                                                        */
/* ------------------------------------------------------------------ */

int social_request_connect(uint8_t idx)
{
    if (state != SOC_IDLE) {
        return -EBUSY;
    }
    int rc = net_connect(idx);

    if (rc == 0) {
        state = SOC_CONNECTING;
    }
    return rc;
}

void social_cancel_connect(void)
{
    net_disconnect();
    state = SOC_IDLE;
}

void social_accept(bool yes)
{
    if (state != SOC_INCOMING) {
        return;
    }
    if (yes) {
        send_name(MSG_ACCEPT);
        state = SOC_LINKED;
        reset_link_state();
        note(NOTE_LINKED, NULL);
    } else {
        uint8_t m[1] = { MSG_DECLINE };

        send(m, 1);
        k_sleep(K_MSEC(150));
        net_disconnect();
        state = SOC_IDLE;
    }
}

void social_disconnect(void)
{
    net_disconnect();
}

int social_propose(uint8_t action)
{
    if (state != SOC_LINKED) {
        return -ENOTCONN;
    }
    my_prop = action;
    uint8_t m[2] = { MSG_PROPOSE, action };

    send(m, 2);

    if (action != PROP_NONE && peer_prop == action) {
        goto launch;
    }
    return 0;

launch:
    if (action == PROP_BATTLE) {
        my_prop = peer_prop = PROP_NONE;
        bsess_begin();
        note(NOTE_LAUNCH_BATTLE, NULL);
    } else {
        my_prop = peer_prop = PROP_NONE;
        social_trade_begin();
        note(NOTE_LAUNCH_TRADE, NULL);
    }
    return 0;
}

uint8_t social_my_proposal(void)   { return my_prop; }
uint8_t social_peer_proposal(void) { return peer_prop; }

int social_greet(void)
{
    if (state != SOC_LINKED) {
        return -ENOTCONN;
    }
    if (k_uptime_get() < greet_ready_ms) {
        return -EBUSY;
    }
    uint8_t m[1] = { MSG_GREET };

    send(m, 1);
    greet_ready_ms = k_uptime_get() + GREET_COOLDOWN_MS;
    return 0;
}

int social_gift(uint8_t item)
{
    if (state != SOC_LINKED) {
        return -ENOTCONN;
    }
    if (items_get(item) == NULL || inv_count(&g_game.inv, item) < 1) {
        return -ENOENT;
    }
    pending_gift = item;
    uint8_t m[2] = { MSG_GIFT, item };

    send(m, 2);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Trade                                                               */
/* ------------------------------------------------------------------ */

void social_trade_begin(void)
{
    t_mine = t_theirs = 0;
    t_my_ok = t_peer_ok = t_done = false;
    t_active = true;
}

uint8_t social_trade_my_offer(void)        { return t_mine; }
uint8_t social_trade_peer_offer(void)      { return t_theirs; }
bool social_trade_my_confirmed(void)       { return t_my_ok; }
bool social_trade_peer_confirmed(void)     { return t_peer_ok; }
bool social_trade_done(void)               { return t_done; }
bool social_trade_active(void)             { return t_active; }

static bool trade_feasible(void)
{
    inventory tmp = g_game.inv;

    if (inv_remove(&tmp, t_mine, 1) != 0) {
        return false;
    }
    return inv_can_add(&tmp, t_theirs, 1);
}

static void trade_commit(void)
{
    if (!trade_feasible()) {
        note(NOTE_TOAST, "Trade failed: bag full");
        t_active = false;
        note(NOTE_TRADE_CLOSED, NULL);
        return;
    }
    inv_remove(&g_game.inv, t_mine, 1);
    inv_add(&g_game.inv, t_theirs, 1);
    storage_save_now(SAVE_INV);
    note(NOTE_TOAST, "Got %s!", items_get(t_theirs)->name);
    t_done = true;
    t_active = false;
    t_my_ok = t_peer_ok = false;
}

int social_trade_offer(uint8_t item)
{
    if (!t_active) {
        return -EINVAL;
    }
    if (item != 0 && inv_count(&g_game.inv, item) < 1) {
        return -ENOENT;
    }
    t_mine = item;
    t_my_ok = t_peer_ok = false;
    uint8_t m[2] = { MSG_OFFER, item };

    send(m, 2);
    return 0;
}

int social_trade_confirm(void)
{
    if (!t_active || t_mine == 0 || t_theirs == 0) {
        return -EINVAL;
    }
    if (!trade_feasible()) {
        return -ENOSPC;
    }
    t_my_ok = true;
    uint8_t m[1] = { MSG_CONFIRM };

    send(m, 1);
    if (t_peer_ok) {
        trade_commit();
    }
    return 0;
}

void social_trade_cancel(void)
{
    if (t_active) {
        uint8_t m[1] = { MSG_TRADE_CANCEL };

        send(m, 1);
    }
    t_active = false;
    t_mine = t_theirs = 0;
    t_my_ok = t_peer_ok = false;
}

/* ------------------------------------------------------------------ */
/* Incoming                                                            */
/* ------------------------------------------------------------------ */

static void on_rx(const uint8_t *d, size_t n)
{
    if (n < 1) {
        return;
    }
    if (bsess_rx(d, n)) {
        return;
    }

    switch (d[0]) {
    case MSG_REQ:
        if (n >= 12 && state == SOC_INCOMING) {
            memcpy(g_peer_trainer, &d[1], 11);
            g_peer_trainer[11] = '\0';
            note(NOTE_INCOMING, NULL);
        }
        break;

    case MSG_ACCEPT:
        if (n >= 12 && state == SOC_WAIT_REPLY) {
            memcpy(g_peer_trainer, &d[1], 11);
            g_peer_trainer[11] = '\0';
            state = SOC_LINKED;
            reset_link_state();
            note(NOTE_LINKED, NULL);
        }
        break;

    case MSG_DECLINE:
        if (state == SOC_WAIT_REPLY) {
            note(NOTE_TOAST, "Request declined.");
            net_disconnect();
        }
        break;

    case MSG_PROPOSE:
        if (n >= 2 && state == SOC_LINKED && d[1] <= PROP_TRADE) {
            peer_prop = d[1];
            if (peer_prop != PROP_NONE && my_prop == peer_prop) {
                uint8_t what = peer_prop;

                my_prop = peer_prop = PROP_NONE;
                if (what == PROP_BATTLE) {
                    bsess_begin();
                    note(NOTE_LAUNCH_BATTLE, NULL);
                } else {
                    social_trade_begin();
                    note(NOTE_LAUNCH_TRADE, NULL);
                }
            }
        }
        break;

    case MSG_GREET: {
        if (state != SOC_LINKED) {
            break;
        }
        kindrals_update_friendship(&g_game.kin, 2);
        kindrals_update_happiness(&g_game.kin, 5);
        storage_mark_dirty(SAVE_KIN);
        note(NOTE_TOAST, "%s greeted %s!", g_peer_trainer, g_game.kin.id.name);
        uint8_t m[3] = { MSG_ACK, MSG_GREET, 0 };

        send(m, 3);
        break;
    }

    case MSG_GIFT: {
        if (n < 2 || state != SOC_LINKED || items_get(d[1]) == NULL) {
            break;
        }
        items_apply_gift_receiver(&g_game.kin, d[1]);
        storage_mark_dirty(SAVE_KIN);
        note(NOTE_TOAST, "%s gave %s a %s!", g_peer_trainer, g_game.kin.id.name,
             items_get(d[1])->name);
        uint8_t m[3] = { MSG_ACK, MSG_GIFT, 0 };

        send(m, 3);
        break;
    }

    case MSG_ACK:
        if (n < 3 || d[2] != 0) {
            break;
        }
        if (d[1] == MSG_GREET) {
            kindrals_update_friendship(&g_game.kin, 2);
            kindrals_update_happiness(&g_game.kin, 3);
            storage_mark_dirty(SAVE_KIN);
            note(NOTE_TOAST, "%s waved back!", g_peer_trainer);
        } else if (d[1] == MSG_GIFT && pending_gift) {
            const item_def *it = items_get(pending_gift);

            if (inv_remove(&g_game.inv, pending_gift, 1) == 0) {
                items_apply_gift_giver(&g_game.kin, pending_gift);
                storage_save_now(SAVE_INV | SAVE_KIN);
                note(NOTE_TOAST, it->give_happy < 0 ? "Gave %s... that was rude."
                                                    : "Gave %s! Nice.", it->name);
            }
            pending_gift = 0;
        }
        break;

    case MSG_OFFER:
        if (n >= 2 && t_active && (d[1] == 0 || items_get(d[1]))) {
            t_theirs = d[1];
            t_my_ok = t_peer_ok = false;
        }
        break;

    case MSG_CONFIRM:
        if (t_active && t_mine && t_theirs) {
            t_peer_ok = true;
            if (t_my_ok) {
                trade_commit();
            }
        }
        break;

    case MSG_TRADE_CANCEL:
        if (t_active) {
            t_active = false;
            t_mine = t_theirs = 0;
            t_my_ok = t_peer_ok = false;
            note(NOTE_TOAST, "%s cancelled the trade.", g_peer_trainer);
            note(NOTE_TRADE_CLOSED, NULL);
        }
        break;

    default:
        break;
    }
}

void social_handle_event(const app_event *e)
{
    switch (e->type) {
    case EVT_NET_LINK_UP:
        if (e->link.is_central) {
            state = SOC_WAIT_REPLY;
            send_name(MSG_REQ);
        } else {
            state = SOC_INCOMING;       /* the REQ message follows */
        }
        break;

    case EVT_NET_RX:
        on_rx(e->rx.data, e->rx.len);
        break;

    case EVT_NET_CONNECT_FAIL:
        if (state == SOC_CONNECTING || state == SOC_WAIT_REPLY || state == SOC_INCOMING) {
            state = SOC_IDLE;
            note(NOTE_LINK_LOST, "Couldn't connect.");
        }
        break;

    case EVT_NET_LINK_DOWN:
        if (state != SOC_IDLE) {
            bsess_abort();
            state = SOC_IDLE;
            reset_link_state();
            note(NOTE_LINK_LOST, "Link lost.");
        }
        break;

    default:
        break;
    }
}
