#ifndef KCLOCK_H
#define KCLOCK_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Wall-clock time (unix seconds, UTC).
 *  - CONFIG_KINBOX_USE_DS3231=y : read from the DS3231 RTC over I2C.
 *  - otherwise                  : temporary clock = last saved time + uptime,
 *                                 plus CONFIG_KINBOX_FALLBACK_OFFLINE_SECS at boot.
 */
void kclock_init(int64_t last_saved_unix);
int64_t kclock_now(void);
bool kclock_is_rtc(void);

#endif
