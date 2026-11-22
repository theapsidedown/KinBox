#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include "buttons.h"

/* Max bytes in one KinBox link message (kept <= 20 so the default ATT MTU is enough). */
#define NET_MSG_MAX  20

enum app_evt_type {
    EVT_BTN,               /* .btn                                           */
    EVT_POWER_OFF,         /* power button long-press                        */
    EVT_SCREEN_TOGGLE,     /* power button short-press                       */
    EVT_NET_PEER_NEAR,     /* .peer  another KinBox is close                 */
    EVT_NET_LINK_UP,       /* .link  connection + service ready              */
    EVT_NET_LINK_DOWN,
    EVT_NET_CONNECT_FAIL,
    EVT_NET_RX,            /* .rx    one message from the other KinBox       */
};

typedef struct {
    uint8_t type;
    union {
        uint8_t btn;
        struct {
            uint8_t idx;           /* slot in the net peer table */
            int8_t rssi;
            uint8_t species;
            uint8_t level;
            char trainer[12];
        } peer;
        struct {
            uint8_t is_central;    /* 1 = we initiated (central), 0 = we accepted (peripheral) */
        } link;
        struct {
            uint8_t len;
            uint8_t data[NET_MSG_MAX];
        } rx;
    };
} app_event;

/* Thread-safe, never blocks. Returns 0 or -ENOMEM if the queue is full. */
int app_post(const app_event *evt);

/* Main loop side. */
int app_wait(app_event *evt, k_timeout_t timeout);

#endif
