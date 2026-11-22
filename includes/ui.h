#ifndef KINBOX_UI_H
#define KINBOX_UI_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

#include "app_event.h"
#include "buttons.h"
#include "sprites.h"

/* 1.8" ST7735, portrait. Font: 8x8 pixel font -> 16 columns x 20 rows. */
#define UI_W 128
#define UI_H 160

#define COL_BG      0x0B1020
#define COL_FG      0xE8EEF8
#define COL_DIM     0x7C8AA5
#define COL_ACCENT  0xFFC857
#define COL_HP      0x4CD964
#define COL_HP_LOW  0xFF4D4D
#define COL_EP      0x4DA3FF
#define COL_MOOD    0xFF79C6
#define COL_PANEL   0x1A2340

/* ---------------- Scenes ---------------- */

typedef struct ui_scene {
    const char *name;
    bool needs_link;                       /* dropped automatically when the BLE link ends */
    void (*enter)(void *arg);              /* build widgets on ui_screen()                 */
    void (*leave)(void);
    void (*resume)(void);                  /* a scene above was popped                     */
    void (*on_btn)(enum button b);
    void (*on_event)(const app_event *e);  /* every net event, after social handled it     */
    void (*on_second)(void);
} ui_scene_t;

void ui_init(void);
void ui_push(const ui_scene_t *sc, void *arg);
void ui_pop(void);
void ui_replace(const ui_scene_t *sc, void *arg);
void ui_pop_to_home(void);
void ui_reset(const ui_scene_t *sc, void *arg);   /* sc becomes the only scene */
lv_obj_t *ui_screen(void);
const ui_scene_t *ui_top(void);

/* Main-loop hooks */
void ui_run(void);                         /* lv_timer_handler()                          */
void ui_handle_btn(enum button b);
void ui_handle_event(const app_event *e);
void ui_second(void);
void ui_process_social(void);              /* drain social notes: toasts, launches, link loss */

/* Dialogs */
void ui_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ui_toast_then(void (*cb)(void *), void *ud, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void ui_confirm(const char *text, void (*cb)(bool yes, void *ud), void *ud);

/* Screen */
void ui_screen_power(bool on);
bool ui_screen_is_on(void);
void ui_activity(void);
void ui_show_goodbye(void);

/* Level-ups and newly learned moves queued on the kindral. Shows them. */
void ui_flush_pending(void);

/* Start-up flows */
void ui_start_setup(void);                 /* first boot: trainer name -> hatch -> nickname */
void ui_start_home(void);

/* ---------------- Widget helpers ---------------- */

lv_obj_t *ui_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
lv_obj_t *ui_label(lv_obj_t *parent, int x, int y, uint32_t color, const char *text);
lv_obj_t *ui_text(lv_obj_t *parent, int x, int y, int w, uint32_t color, const char *text);
lv_obj_t *ui_image(lv_obj_t *parent, int x, int y, const lv_image_dsc_t *src);
lv_obj_t *ui_bar(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
void ui_bar_set(lv_obj_t *bar, int value, int max);
void ui_bar_color(lv_obj_t *bar, uint32_t color);

#define UI_MENU_MAX_ROWS 10
typedef struct {
    lv_obj_t *row[UI_MENU_MAX_ROWS];
    lv_obj_t *txt[UI_MENU_MAX_ROWS];
    int vis, rowh;
    const char *const *items;
    int count, cursor, first;
    uint32_t disabled;                     /* bit i = item i greyed out */
} ui_menu;

void ui_menu_create(ui_menu *m, lv_obj_t *parent, int x, int y, int w, int rowh, int visible);
void ui_menu_set(ui_menu *m, const char *const *items, int count);
void ui_menu_move(ui_menu *m, int dir);
void ui_menu_redraw(ui_menu *m);

/* ---------------- Scene table ---------------- */
extern const ui_scene_t scene_home, scene_menu, scene_stats, scene_moves, scene_items,
    scene_settings, scene_msg, scene_keyboard, scene_hatch, scene_peer_near, scene_link,
    scene_social, scene_gift, scene_trade, scene_battle, scene_learn;

/* Keyboard */
typedef struct {
    const char *title;
    const char *initial;
    int max_chars;
    bool allow_empty;
    void (*done)(const char *text);
} ui_keyboard_args;

void ui_keyboard_open(const ui_keyboard_args *args);

/* Another KinBox came close (EVT_NET_PEER_NEAR): alert the user, or quietly ignore it. */
void ui_peer_near(const app_event *e);

/* Peer prompt */
typedef struct {
    uint8_t idx;
    char trainer[12];
    uint8_t species, level;
} ui_peer_args;

#endif
