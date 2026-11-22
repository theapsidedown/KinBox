#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "kclock.h"

#if CONFIG_KINBOX_USE_DS3231
#include <zephyr/drivers/i2c.h>
#endif

LOG_MODULE_REGISTER(kclock, LOG_LEVEL_INF);

/* 2026-01-01 00:00:00 UTC: used when there is no RTC and nothing was ever saved */
#define FALLBACK_EPOCH  1767225600LL

static int64_t base_unix;
static int64_t base_uptime_ms;
static bool rtc_ok;

/* Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm). */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int *y, unsigned *m, unsigned *d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int yy = (int)yoe + (int)era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;

    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = yy + (*m <= 2);
}

#if CONFIG_KINBOX_USE_DS3231
/* ---- DS3231 over I2C (minimal: time registers + OSF flag) ---- */

#define DS3231_ADDR   0x68
#define DS3231_REG_TIME   0x00
#define DS3231_REG_STATUS 0x0F

static const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static uint8_t bcd2bin(uint8_t v) { return (v & 0x0F) + 10 * (v >> 4); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static int ds3231_read(int64_t *unix_out)
{
    uint8_t r[7];
    int ret = i2c_burst_read(i2c_dev, DS3231_ADDR, DS3231_REG_TIME, r, sizeof(r));

    if (ret) {
        return ret;
    }
    int year = 2000 + bcd2bin(r[6]);
    unsigned mon = bcd2bin(r[5] & 0x1F);
    unsigned day = bcd2bin(r[4]);
    int hh = bcd2bin(r[2] & 0x3F);      /* 24h mode */
    int mm = bcd2bin(r[1]);
    int ss = bcd2bin(r[0]);

    *unix_out = days_from_civil(year, mon, day) * 86400 + hh * 3600 + mm * 60 + ss;
    return 0;
}

static int ds3231_write(int64_t t)
{
    int y; unsigned m, d;
    int64_t days = t / 86400;
    int rem = (int)(t % 86400);

    civil_from_days(days, &y, &m, &d);
    uint8_t r[7] = {
        bin2bcd(rem % 60), bin2bcd((rem / 60) % 60), bin2bcd(rem / 3600),
        (uint8_t)(((days + 4) % 7) + 1),    /* weekday 1..7 */
        bin2bcd((uint8_t)d), bin2bcd((uint8_t)m), bin2bcd((uint8_t)(y - 2000)),
    };
    int ret = i2c_burst_write(i2c_dev, DS3231_ADDR, DS3231_REG_TIME, r, sizeof(r));

    if (ret == 0) {         /* clear the Oscillator-Stop flag */
        uint8_t st;

        if (i2c_reg_read_byte(i2c_dev, DS3231_ADDR, DS3231_REG_STATUS, &st) == 0) {
            i2c_reg_write_byte(i2c_dev, DS3231_ADDR, DS3231_REG_STATUS, st & ~0x80);
        }
    }
    return ret;
}

/* Build timestamp (local time of the build machine) used if the RTC lost power. */
static int64_t build_time_unix(void)
{
    static const char *const mons = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *date = __DATE__;      /* "Mmm dd yyyy" */
    const char *time = __TIME__;      /* "hh:mm:ss"    */
    unsigned mon = 1;

    for (unsigned i = 0; i < 12; i++) {
        if (strncmp(date, mons + 3 * i, 3) == 0) {
            mon = i + 1;
        }
    }
    unsigned day = (unsigned)atoi(date + 4);
    int year = atoi(date + 7);

    return days_from_civil(year, mon, day) * 86400 + atoi(time) * 3600 +
           atoi(time + 3) * 60 + atoi(time + 6);
}
#endif /* CONFIG_KINBOX_USE_DS3231 */

void kclock_init(int64_t last_saved_unix)
{
#if CONFIG_KINBOX_USE_DS3231
    uint8_t st = 0;

    if (device_is_ready(i2c_dev) &&
        i2c_reg_read_byte(i2c_dev, DS3231_ADDR, DS3231_REG_STATUS, &st) == 0) {
        if (st & 0x80) {
            LOG_WRN("DS3231 lost power; setting it to the build time");
            ds3231_write(build_time_unix());
        }
        rtc_ok = true;
        LOG_INF("DS3231 clock in use");
        return;
    }
    LOG_ERR("DS3231 not found, falling back to the temporary clock");
#endif
    base_unix = (last_saved_unix > 0)
                    ? last_saved_unix + CONFIG_KINBOX_FALLBACK_OFFLINE_SECS
                    : FALLBACK_EPOCH;
    base_uptime_ms = k_uptime_get();
    (void)days_from_civil; (void)civil_from_days;
}

int64_t kclock_now(void)
{
#if CONFIG_KINBOX_USE_DS3231
    int64_t t;

    if (rtc_ok && ds3231_read(&t) == 0) {
        return t;
    }
#endif
    return base_unix + (k_uptime_get() - base_uptime_ms) / 1000;
}

bool kclock_is_rtc(void)
{
    return rtc_ok;
}
