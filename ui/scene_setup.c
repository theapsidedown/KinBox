/*
 * First boot: trainer name -> egg hatches into a random kindral -> nickname.
 * Nothing is written to flash until the nickname step is finished.
 */
#include <string.h>
#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "kclock.h"
#include "storage.h"
#include "net.h"
#include "abilities.h"
#include "moves.h"

static void nick_done(const char *text)
{
    kindrals_set_name(&g_game.kin, text);      /* empty => keeps the species name */
    g_game.visible = true;
    g_game.setup_done = true;
    g_game.last_unix = kclock_now();
    storage_save_now(SAVE_ALL);                /* first save */
    net_set_profile(g_game.trainer, g_game.kin.id.species, g_game.kin.stats.level);
    net_set_visible(true);
    ui_start_home();
}

static void hatch_next(void)
{
    static const ui_keyboard_args a = {
        .title = "Nickname?", .initial = "", .max_chars = KIN_NAME_MAX_CHARS,
        .allow_empty = true, .done = nick_done,
    };
    ui_keyboard_open(&a);
}

/* ---------------- Hatch scene ---------------- */

static lv_obj_t *egg, *info;
static bool hatched;
static int wobble;

static void hatch_enter(void *arg)
{
    ARG_UNUSED(arg);
    hatched = false;
    egg = ui_image(ui_screen(), 40, 30, &spr_egg);
    info = ui_text(ui_screen(), 8, 100, 112, COL_FG, "An egg! Something is moving inside...\n\nA: Hatch");
}

static void hatch_second(void)
{
    if (!hatched) {
        wobble = !wobble;
        lv_obj_set_x(egg, wobble ? 43 : 37);
    }
}

static void hatch_btn(enum button b)
{
    if (b != BTN_A) {
        return;
    }
    if (hatched) {
        hatch_next();
        return;
    }
    kindrals_init(&g_game.kin, kindrals_random_species(), kclock_now());
    inv_give_starter_items(&g_game.inv);
    hatched = true;

    const kin_species *sp = kindrals_species(g_game.kin.id.species);
    char buf[160];

    lv_image_set_src(egg, spr_front(g_game.kin.id.species));
    lv_obj_set_x(egg, 40);
    snprintf(buf, sizeof(buf), "It's a %s!\n%s / %s\nAbility:\n%s\n\nA: Next",
             sp->species_name, kindrals_type_name(sp->type), kindrals_trait_name(sp->trait),
             abilities_get(g_game.kin.id.ability)->name);
    lv_label_set_text(info, buf);
}

const ui_scene_t scene_hatch = {
    .name = "hatch", .enter = hatch_enter, .on_btn = hatch_btn, .on_second = hatch_second,
};

/* ---------------- Entry points ---------------- */

static void trainer_done(const char *text)
{
    snprintf(g_game.trainer, sizeof(g_game.trainer), "%s", text);
    ui_reset(&scene_hatch, NULL);
}

void ui_start_setup(void)
{
    static const ui_keyboard_args a = {
        .title = "Trainer name?", .initial = "", .max_chars = KIN_NAME_MAX_CHARS,
        .allow_empty = false, .done = trainer_done,
    };
    ui_keyboard_open(&a);
}

void ui_start_home(void)
{
    ui_reset(&scene_home, NULL);
}
