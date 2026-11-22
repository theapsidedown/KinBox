#ifndef PROTO_H
#define PROTO_H

/*
 * KinBox <-> KinBox application messages. Each message is <= 20 bytes:
 * byte 0 = type, then the fields below (little endian).
 */
enum msg_type {
    MSG_REQ = 1,        /* [trainer x11]                 central asks to connect      */
    MSG_ACCEPT,         /* [trainer x11]                                              */
    MSG_DECLINE,        /* -                                                          */
    MSG_PROPOSE,        /* [action]  PROP_*                                           */
    MSG_GREET,          /* -                                                          */
    MSG_GIFT,           /* [item]                                                     */
    MSG_ACK,            /* [of_type][result 0=ok]                                     */
    MSG_SNAP_A,         /* [species][ability][level][name x11][moves x4]              */
    MSG_SNAP_B,         /* [hp u16][max_hp u16][ep u16][max_ep u16][atk u16][def u16][spd u16] */
    MSG_SEED,           /* [seed u32]                    battle RNG seed (central)    */
    MSG_ACTION,         /* [turn u16][kind][arg][state_hash u16]                      */
    MSG_OFFER,          /* [item]   0 = nothing                                       */
    MSG_CONFIRM,        /* -                                                          */
    MSG_TRADE_CANCEL,   /* -                                                          */
};

enum proposal { PROP_NONE, PROP_BATTLE, PROP_TRADE };

#endif
