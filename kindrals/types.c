#include "types.h"

/* chart[attacking move type][defending type], order = enum kindral_types:
 *            Ignis Aqua Terra Aero Flora Lume Nox                          */
static const uint8_t chart[KIN_TYPE_COUNT][KIN_TYPE_COUNT] = {
    /* Ignis */ {100, 100, 150, 100, 150,  70,  70},
    /* Aqua  */ {100, 100,  70, 150,  70, 150, 100},
    /* Terra */ { 70, 150, 100, 150,  70, 100, 100},
    /* Aero  */ {100,  70,  70, 100, 100, 150, 150},
    /* Flora  */ { 70, 150, 150, 100, 100, 100,  70},
    /* Lume  */ {150,  70, 100,  70, 100, 100, 150},
    /* Nox   */ {150, 100, 100,  70, 150,  70, 100},
};

int types_effectiveness_pct(enum kindral_types move_type, enum kindral_types defender)
{
    if ((unsigned)move_type >= KIN_TYPE_COUNT || (unsigned)defender >= KIN_TYPE_COUNT) {
        return 100;
    }
    return chart[move_type][defender];
}
