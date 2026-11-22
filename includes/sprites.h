#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include <lvgl.h>

/*
 * Sprite sizes:
 *   48x48  kindrals (home, stats, battle front/back, hatch, peer prompt)
 *   32x32  item icons (inventory, trade, gift)
 *   16x16  type badges (moves, stats)
 * Art lives in assets/ and is converted by tools/gen_assets.py.
 * Missing art falls back to a "?" tile.
 */
extern const lv_image_dsc_t spr_missing;
extern const lv_image_dsc_t spr_egg;
extern const lv_image_dsc_t *const spr_front_table[10];
extern const lv_image_dsc_t *const spr_back_table[10];
extern const lv_image_dsc_t *const spr_item_table[13];
extern const lv_image_dsc_t *const spr_badge_table[7];

const lv_image_dsc_t *spr_front(uint8_t species);
const lv_image_dsc_t *spr_back(uint8_t species);
const lv_image_dsc_t *spr_item(uint8_t item);
const lv_image_dsc_t *spr_badge(uint8_t type);

#endif
