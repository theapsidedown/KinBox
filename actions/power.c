/*
 * Soft power button.
 *
 *  - Hold the power key (CONFIG_KINBOX_LONG_PRESS_MS) -> save + deep sleep ("off").
 *  - Tap it -> screen on/off.
 *  - While "off", a press of the same key wakes the ESP32-S3 (EXT1 wake on an
 *    RTC-capable pin). A wake is a normal reset, so the game just boots again.
 *
 * NOTE (hardware): deep sleep on a DevKitC still burns a few mA in the USB
 * bridge and the 3.3 V regulator. For a battery product, use a custom board or
 * add a battery switch; the software side below is what you'd keep.
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "power.h"
#include "app_event.h"
#include "storage.h"
#include "net.h"
#include "ui.h"

#if CONFIG_KINBOX_DEEP_SLEEP
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#endif

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define PWR_NODE  DT_NODELABEL(btn_power)
static const struct gpio_dt_spec pwr_pin = GPIO_DT_SPEC_GET(PWR_NODE, gpios);

static void long_press_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(long_press_work, long_press_fn);

static bool armed;              /* ignore the press that woke us from deep sleep */
static bool long_fired;

static void long_press_fn(struct k_work *w)
{
    ARG_UNUSED(w);
    long_fired = true;
    app_event e = { .type = EVT_POWER_OFF };

    (void)app_post(&e);
}

void power_init(void)
{
    /* If the key is still down from the wake-up press, wait for its release. */
    armed = !(gpio_is_ready_dt(&pwr_pin) && gpio_pin_get_dt(&pwr_pin) > 0);
}

void power_button_event(bool pressed)
{
    if (!armed) {
        if (!pressed) {
            armed = true;
        }
        return;
    }
    if (pressed) {
        long_fired = false;
        k_work_reschedule(&long_press_work, K_MSEC(CONFIG_KINBOX_LONG_PRESS_MS));
    } else {
        k_work_cancel_delayable(&long_press_work);
        if (!long_fired) {
            app_event e = { .type = EVT_SCREEN_TOGGLE };

            (void)app_post(&e);
        }
    }
}

void power_off(void)
{
    LOG_INF("Powering off");

    ui_show_goodbye();              /* "Bye!" for a moment */
    k_sleep(K_MSEC(700));

    storage_save_now(SAVE_ALL);     /* no-op until the first-time setup is done */
    net_stop();
    ui_screen_power(false);

#if CONFIG_KINBOX_DEEP_SLEEP
    /* Wake on the power key going low (it is wired key -> GND with a pull-up). */
    const int gpio_num = DT_GPIO_PIN(PWR_NODE, gpios);

    /* Wait for release so we don't wake immediately. */
    while (gpio_pin_get_dt(&pwr_pin) > 0) {
        k_sleep(K_MSEC(20));
    }
    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(gpio_num);
    rtc_gpio_pulldown_dis(gpio_num);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_ext1_wakeup_io(1ULL << gpio_num, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
#else
    /* "Soft off": screen dark, BLE stopped. main() turns the next key press into a reboot. */
    LOG_WRN("Soft power-off (deep sleep disabled)");
#endif
}
