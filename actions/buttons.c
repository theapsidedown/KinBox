/*
 * Buttons: gpio-keys (devicetree) -> Zephyr input subsystem -> message queue.
 * Only key *presses* are queued; releases are ignored.
 */
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#include "buttons.h"

K_MSGQ_DEFINE(button_q, sizeof(enum button), 8, sizeof(enum button));

static void on_input(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY || evt->value != 1) {
        return;
    }

    enum button b;

    switch (evt->code) {
    case INPUT_KEY_UP:    b = BTN_UP;    break;
    case INPUT_KEY_DOWN:  b = BTN_DOWN;  break;
    case INPUT_KEY_LEFT:  b = BTN_LEFT;  break;
    case INPUT_KEY_RIGHT: b = BTN_RIGHT; break;
    case INPUT_KEY_ENTER: b = BTN_A;     break;
    case INPUT_KEY_MENU:  b = BTN_B;     break;
    default:              return;
    }

    /* Never block in the input callback; if the queue is full, drop the press. */
    (void)k_msgq_put(&button_q, &b, K_NO_WAIT);
}

/* NULL device = listen to every input device (our gpio-keys node) */
INPUT_CALLBACK_DEFINE(NULL, on_input, NULL);

int buttons_wait(enum button *btn, k_timeout_t timeout)
{
    return k_msgq_get(&button_q, btn, timeout);
}

void buttons_flush(void)
{
    k_msgq_purge(&button_q);
}