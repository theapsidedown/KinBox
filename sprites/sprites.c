#include "sprites.h"

const lv_image_dsc_t *spr_front(uint8_t s)
{
    return (s < 10 && spr_front_table[s]) ? spr_front_table[s] : &spr_missing;
}

const lv_image_dsc_t *spr_back(uint8_t s)
{
    return (s < 10 && spr_back_table[s]) ? spr_back_table[s] : &spr_missing;
}

const lv_image_dsc_t *spr_item(uint8_t i)
{
    return (i < 13 && spr_item_table[i]) ? spr_item_table[i] : &spr_missing;
}

const lv_image_dsc_t *spr_badge(uint8_t t)
{
    return (t < 7 && spr_badge_table[t]) ? spr_badge_table[t] : &spr_missing;
}
