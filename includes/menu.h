#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "kindrals.h"

/*
 * Opens the pause menu. Blocks (the game is paused) until the player picks an
 * option with A (the action is applied to *kin first) or closes it with B.
 */
void menu_run(kindral *kin, int64_t now);

#endif /* MENU_H */