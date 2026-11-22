#ifndef SOCIAL_H
#define SOCIAL_H

#include <stdint.h>
#include <stdbool.h>
#include "app_event.h"

typedef enum {
    SOC_IDLE,
    SOC_CONNECTING,     /* we pressed Connect, BLE link is being set up       */
    SOC_WAIT_REPLY,     /* linked as central, waiting for Accept/Decline      */
    SOC_INCOMING,       /* linked as peripheral, the user must Accept/Decline */
    SOC_LINKED,         /* menu: battle / trade / greet / gift                */
} soc_state;

enum soc_note_kind {
    NOTE_TOAST,         /* show .text                                         */
    NOTE_INCOMING,      /* someone wants to connect: open the prompt          */
    NOTE_LINKED,        /* link is ready: open the social menu                */
    NOTE_LINK_LOST,     /* link ended: drop link scenes, show .text           */
    NOTE_LAUNCH_BATTLE,
    NOTE_LAUNCH_TRADE,
    NOTE_TRADE_CLOSED,  /* trade finished or cancelled by the other side      */
};

typedef struct {
    uint8_t kind;
    char text[48];
} soc_note;

extern char g_peer_trainer[12];

void social_init(void);
soc_state social_state(void);
bool social_next_note(soc_note *n);

/* Called from the main loop for every app event (net events only matter). */
void social_handle_event(const app_event *e);

/* User actions */
int  social_request_connect(uint8_t peer_idx);
void social_cancel_connect(void);
void social_accept(bool yes);
void social_disconnect(void);

int  social_propose(uint8_t action);       /* PROP_* */
uint8_t social_my_proposal(void);
uint8_t social_peer_proposal(void);

int  social_greet(void);                   /* -EBUSY while on cooldown */
int  social_gift(uint8_t item);

/* Trade */
void    social_trade_begin(void);
int     social_trade_offer(uint8_t item);
int     social_trade_confirm(void);        /* -ENOSPC if your inventory could not take theirs */
void    social_trade_cancel(void);
uint8_t social_trade_my_offer(void);
uint8_t social_trade_peer_offer(void);
bool    social_trade_my_confirmed(void);
bool    social_trade_peer_confirmed(void);
bool    social_trade_done(void);
bool    social_trade_active(void);

#endif
