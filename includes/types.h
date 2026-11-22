#ifndef TYPES_H
#define TYPES_H

#include "kindrals.h"

/*
 * Type effectiveness. Returns a percentage to multiply damage by:
 *   150 = super effective, 100 = neutral, 70 = not very effective.
 * KIN_TYPE_NONE (typeless moves) is always neutral.
 *
 * Chart design: the 7 types sit on a ring
 *     Ignis > Flora > Terra > Aqua > Aero > Lume > Nox > (back to Ignis)
 * Each type is strong against the next two on the ring and resisted by them
 * in return, so every type has exactly 2 strengths, 2 weaknesses and 2 resists
 * (see types.c for the full matrix).
 */
int types_effectiveness_pct(enum kindral_types move_type, enum kindral_types defender);

#endif
