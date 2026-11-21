#ifndef BUTTONS_H
#define BUTTONS_H

#include <zephyr/kernel.h>

enum button {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,      /* select           */
    BTN_B,      /* open menu / back */
};

/* Wait for the next button press. Returns 0 and fills *btn, or -EAGAIN on timeout. */
int buttons_wait(enum button *btn, k_timeout_t timeout);

/* Drop any presses still sitting in the queue. */
void buttons_flush(void);

#endif /* BUTTONS_H */