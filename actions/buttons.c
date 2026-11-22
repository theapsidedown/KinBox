/*
 * Buttons: gpio-keys (devicetree) -> Zephyr input subsystem -> app event queue.
 * D-pad/A/B go to the UI. The power key goes to power.c (it needs press AND release).
 */
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/drivers/gpio.h>

#include "buttons.h"
#include "app_event.h"
#include "power.h"

K_MSGQ_DEFINE(app_q, sizeof(app_event), 16, 4);

int app_post(const app_event *evt)
{
    return k_msgq_put(&app_q, evt, K_NO_WAIT);
}

int app_wait(app_event *evt, k_timeout_t timeout)
{
    return k_msgq_get(&app_q, evt, timeout);
}

static void on_input(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }
    if (evt->code == INPUT_KEY_POWER) {
        power_button_event(evt->value != 0);
        return;
    }
    if (evt->value != 1) {          /* presses only */
        return;
    }

    app_event e = { .type = EVT_BTN };

    switch (evt->code) {
    case INPUT_KEY_UP:    e.btn = BTN_UP;    break;
    case INPUT_KEY_DOWN:  e.btn = BTN_DOWN;  break;
    case INPUT_KEY_LEFT:  e.btn = BTN_LEFT;  break;
    case INPUT_KEY_RIGHT: e.btn = BTN_RIGHT; break;
    case INPUT_KEY_ENTER: e.btn = BTN_A;     break;
    case INPUT_KEY_MENU:  e.btn = BTN_B;     break;
    default:              return;
    }
    (void)app_post(&e);             /* never block in the input callback */
}

/* NULL device = listen to every input device (our gpio-keys node) */
INPUT_CALLBACK_DEFINE(NULL, on_input, NULL);

static const struct gpio_dt_spec pin_a = GPIO_DT_SPEC_GET(DT_NODELABEL(btn_a), gpios);
static const struct gpio_dt_spec pin_b = GPIO_DT_SPEC_GET(DT_NODELABEL(btn_b), gpios);

bool buttons_a_and_b_held(void)
{
    if (!gpio_is_ready_dt(&pin_a) || !gpio_is_ready_dt(&pin_b)) {
        return false;
    }
    return gpio_pin_get_dt(&pin_a) > 0 && gpio_pin_get_dt(&pin_b) > 0;
}
