#include <zephyr/kernel.h>
#include "steps.h"

#define SIM_STEPS_PER_SEC 2

static uint32_t reported_total;
static bool sim_walking;
static int64_t sim_last_ms;
static uint64_t sim_accum_ms_steps;     /* ms * steps/s, to avoid rounding drift */
static uint32_t sim_total;

/* TODO: return the cumulative step count from your sensor (IMU/pedometer). */
static uint32_t steps_hw_total(void)
{
    return 0;
}

void steps_init(void)
{
    reported_total = 0;
    sim_last_ms = k_uptime_get();
}

bool steps_sim_walking(void)
{
    return sim_walking;
}

void steps_sim_toggle(void)
{
    sim_walking = !sim_walking;
    sim_last_ms = k_uptime_get();
}

uint32_t steps_take_new(void)
{
#if CONFIG_KINBOX_STEPS_SIM
    int64_t now = k_uptime_get();

    if (sim_walking) {
        sim_accum_ms_steps += (uint64_t)(now - sim_last_ms) * SIM_STEPS_PER_SEC;
        uint32_t whole = (uint32_t)(sim_accum_ms_steps / 1000);

        sim_accum_ms_steps %= 1000;
        sim_total += whole;
    }
    sim_last_ms = now;
    uint32_t total = sim_total;
#else
    uint32_t total = steps_hw_total();
#endif
    uint32_t delta = total - reported_total;

    reported_total = total;
    return delta;
}
