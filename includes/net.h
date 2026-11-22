#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * BLE transport between two KinBoxes. Every box advertises (connectable) and
 * scans at the same time. Discovery results come back as EVT_NET_PEER_NEAR,
 * link state as EVT_NET_LINK_UP/DOWN/CONNECT_FAIL, and incoming messages as
 * EVT_NET_RX (see app_event.h).
 *
 * Roles: whoever presses "Connect" becomes the BLE central; the other side is
 * the peripheral. Central = battle side 0.
 */
int  net_init(void);
void net_stop(void);                              /* before power-off */
int  net_set_visible(bool on);                    /* advertise+scan on/off */
void net_set_profile(const char *trainer, uint8_t species, uint8_t level);

int  net_connect(uint8_t peer_idx);
void net_disconnect(void);
void net_ignore_peer(uint8_t peer_idx, int seconds);

int  net_send(const uint8_t *data, size_t len);   /* len <= NET_MSG_MAX */
bool net_link_up(void);
bool net_is_central(void);

void net_housekeeping(void);                      /* ~1 Hz */

#endif
