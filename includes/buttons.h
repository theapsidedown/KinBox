#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

enum button {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,      /* select / confirm */
    BTN_B,      /* back / menu      */
};

/* Reads the A and B pins directly. True if both are held right now (boot-time
 * factory-reset combo). Safe to call before the UI is up. */
bool buttons_a_and_b_held(void);

#endif /* BUTTONS_H */
