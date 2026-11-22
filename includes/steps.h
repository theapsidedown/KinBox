#ifndef STEPS_H
#define STEPS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Step source. With CONFIG_KINBOX_STEPS_SIM=y (default for now) steps are
 * simulated: toggle "walking" from the menu and ~2 steps/second are produced.
 * Replace steps_hw_total() with an IMU / pedometer read to use real steps.
 */
void steps_init(void);
uint32_t steps_take_new(void);        /* steps since the previous call */

void steps_sim_toggle(void);
bool steps_sim_walking(void);

#endif
