#ifndef POWER_H
#define POWER_H

#include <stdbool.h>

/* Power key: long-press = power off, short-press = screen on/off.
 * Wake from "off" is the same key (deep-sleep wake). */
void power_init(void);
void power_button_event(bool pressed);      /* called from the input callback */

/* Save, stop BLE, blank the screen and enter deep sleep (does not return).
 * With CONFIG_KINBOX_DEEP_SLEEP=n it returns after a "soft off" and main()
 * reboots on the next key press. */
void power_off(void);

#endif
