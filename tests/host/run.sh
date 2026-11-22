#!/bin/sh
# Host-side logic tests (no Zephyr needed). Usage: ./run.sh
set -e
cd "$(dirname "$0")"
R=../..
gcc -std=gnu11 -Wall -Wextra -Wno-missing-field-initializers -g -Istubs -I$R/includes \
    test_logic.c $R/kindrals/kindrals.c $R/kindrals/moves.c $R/kindrals/traits.c $R/kindrals/types.c \
    $R/kindrals/abilities.c $R/kindrals/items.c $R/kindrals/inventory.c $R/battle/battle.c $R/actions/walk.c \
    -o /tmp/kinbox_logic_test
/tmp/kinbox_logic_test
