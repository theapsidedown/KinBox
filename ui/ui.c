#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "ui.h"
#include "game.h"
#include "social.h"
#include "net.h"
#include "storage.h"

LOG_MODULE_REGISTER(ui, LOG_LEVEL_INF);

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static const struct gpio_dt_spec backlight = GPIO_DT_SPEC_GET(DT_ALIAS(lcd_backlight), gpios);

/* ------------------------------------------------------------------ */
/* Scene stack                                                         */
/* ------------------------------------------------------------------ */

#define STACK_MAX 10

typedef struct {
    const ui_scene_t *sc;
    void *arg;
    lv_obj_t *scr;
} frame_t;

static frame_t stack[STACK_MAX];
static int depth;
static lv_style_t style_screen;
static bool screen_on = true;
static int idle_s;

lv_obj_t *ui_screen(void)
{
    return depth ? stack[depth - 1].scr : lv_screen_active();
}

const ui_scene_t *ui_top(void)
{
    return depth ? stack[depth - 1].sc : NULL;
}

void ui_init(void)
{
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return;
    }
    if (gpio_is_ready_dt(&backlight)) {
        gpio_pin_configure_dt(&backlight, GPIO_OUTPUT_ACTIVE);
    }
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(COL_BG));
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, lv_color_hex(COL_FG));
    lv_style_set_text_font(&style_screen, &lv_font_unscii_8);
    lv_style_set_pad_all(&style_screen, 0);
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_radius(&style_screen, 0);

    display_blanking_off(display_dev);
    screen_on = true;
}

void ui_push(const ui_scene_t *sc, void *arg)
{
    if (depth >= STACK_MAX) {
        LOG_ERR("scene stack full (%s)", sc->name);
        return;
    }
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_add_style(scr, &style_screen, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    stack[depth++] = (frame_t){ sc, arg, scr };
    lv_screen_load(scr);
    if (sc->enter) {
        sc->enter(arg);
    }
}

static void drop_top(void)
{
    frame_t f = stack[--depth];

    if (f.sc->leave) {
        f.sc->leave();
    }
    if (depth) {
        lv_screen_load(stack[depth - 1].scr);
    }
    lv_obj_delete(f.scr);
}

void ui_pop(void)
{
    if (depth <= 1) {
        return;
    }
    drop_top();
    if (stack[depth - 1].sc->resume) {
        stack[depth - 1].sc->resume();
    }
}

void ui_replace(const ui_scene_t *sc, void *arg)
{
    if (depth) {
        frame_t f = stack[--depth];

        if (f.sc->leave) {
            f.sc->leave();
        }
        /* delete after the new screen is loaded to avoid deleting the active one */
        ui_push(sc, arg);
        lv_obj_delete(f.scr);
    } else {
        ui_push(sc, arg);
    }
}

void ui_reset(const ui_scene_t *sc, void *arg)
{
    /* Make 'sc' the only scene (used after setup finishes). */
    ui_push(sc, arg);
    for (int i = 0; i < depth - 1; i++) {
        if (stack[i].sc->leave) {
            stack[i].sc->leave();
        }
        lv_obj_delete(stack[i].scr);
    }
    stack[0] = stack[depth - 1];
    depth = 1;
}

void ui_pop_to_home(void)
{
    while (depth > 1) {
        drop_top();
    }
    if (depth && stack[0].sc->resume) {
        stack[0].sc->resume();
    }
}

/* Remove every scene that only makes sense while linked. */
static void drop_link_scenes(void)
{
    bool top_changed = false;

    for (int i = depth - 1; i > 0; i--) {
        if (!stack[i].sc->needs_link) {
            continue;
        }
        frame_t f = stack[i];

        if (f.sc->leave) {
            f.sc->leave();
        }
        if (i == depth - 1) {
            top_changed = true;
            lv_screen_load(stack[i - 1].scr);
        }
        lv_obj_delete(f.scr);
        for (int j = i; j < depth - 1; j++) {
            stack[j] = stack[j + 1];
        }
        depth--;
    }
    if (top_changed && stack[depth - 1].sc->resume) {
        stack[depth - 1].sc->resume();
    }
}

/* ------------------------------------------------------------------ */
/* Main-loop hooks                                                     */
/* ------------------------------------------------------------------ */

void ui_run(void)
{
    lv_timer_handler();
}

void ui_activity(void)
{
    idle_s = 0;
}

void ui_handle_btn(enum button b)
{
    if (depth && stack[depth - 1].sc->on_btn) {
        stack[depth - 1].sc->on_btn(b);
    }
}

void ui_handle_event(const app_event *e)
{
    if (depth && stack[depth - 1].sc->on_event) {
        stack[depth - 1].sc->on_event(e);
    }
}

void ui_second(void)
{
    if (depth && stack[depth - 1].sc->on_second) {
        stack[depth - 1].sc->on_second();
    }
    if (screen_on && !g_game.in_event && ++idle_s >= CONFIG_KINBOX_SCREEN_TIMEOUT_S) {
        ui_screen_power(false);
    }
}

/* ------------------------------------------------------------------ */
/* Screen power                                                        */
/* ------------------------------------------------------------------ */

void ui_screen_power(bool on)
{
    screen_on = on;
    idle_s = 0;
    if (on) {
        display_blanking_off(display_dev);
        if (gpio_is_ready_dt(&backlight)) {
            gpio_pin_set_dt(&backlight, 1);
        }
    } else {
        if (gpio_is_ready_dt(&backlight)) {
            gpio_pin_set_dt(&backlight, 0);
        }
        display_blanking_on(display_dev);
    }
}

bool ui_screen_is_on(void)
{
    return screen_on;
}

void ui_show_goodbye(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_add_style(scr, &style_screen, 0);
    lv_screen_load(scr);
    ui_label(scr, 44, 76, COL_ACCENT, "Bye!");
    ui_screen_power(true);
    lv_timer_handler();
}

/* ------------------------------------------------------------------ */
/* Social notes -> UI                                                  */
/* ------------------------------------------------------------------ */

extern void scene_link_set_mode(bool incoming);

void ui_process_social(void)
{
    soc_note n;

    while (social_next_note(&n)) {
        switch (n.kind) {
        case NOTE_TOAST:
            ui_toast("%s", n.text);
            break;
        case NOTE_INCOMING:
            if (ui_top() != &scene_link) {
                ui_screen_power(true);
                ui_push(&scene_link, NULL);
            }
            break;
        case NOTE_LINKED:
            if (ui_top() == &scene_link) {
                ui_replace(&scene_social, NULL);
            } else {
                ui_push(&scene_social, NULL);
            }
            break;
        case NOTE_LAUNCH_BATTLE:
            ui_push(&scene_battle, NULL);
            break;
        case NOTE_LAUNCH_TRADE:
            ui_push(&scene_trade, NULL);
            break;
        case NOTE_TRADE_CLOSED:
            if (ui_top() == &scene_trade) {
                ui_pop();
            }
            break;
        case NOTE_LINK_LOST:
            drop_link_scenes();
            ui_screen_power(true);
            if (n.text[0]) {
                ui_toast("%s", n.text);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Toasts and confirms                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char text[96];
    void (*cb)(void *);
    void *ud;
    void (*ccb)(bool, void *);
    bool confirm;
} msg_t;

#define MSG_RING 8
static msg_t msg_ring[MSG_RING];
static int msg_next;

static msg_t *msg_alloc(void)
{
    msg_t *m = &msg_ring[msg_next];

    msg_next = (msg_next + 1) % MSG_RING;
    memset(m, 0, sizeof(*m));
    return m;
}

void ui_toast_then(void (*cb)(void *), void *ud, const char *fmt, ...)
{
    msg_t *m = msg_alloc();
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(m->text, sizeof(m->text), fmt, ap);
    va_end(ap);
    m->cb = cb;
    m->ud = ud;
    ui_screen_power(true);
    ui_push(&scene_msg, m);
}

void ui_toast(const char *fmt, ...)
{
    msg_t *m = msg_alloc();
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(m->text, sizeof(m->text), fmt, ap);
    va_end(ap);
    ui_screen_power(true);
    ui_push(&scene_msg, m);
}

void ui_confirm(const char *text, void (*cb)(bool, void *), void *ud)
{
    msg_t *m = msg_alloc();

    snprintf(m->text, sizeof(m->text), "%s", text);
    m->ccb = cb;
    m->ud = ud;
    m->confirm = true;
    ui_push(&scene_msg, m);
}

static msg_t *cur_msg;

static void msg_enter(void *arg)
{
    cur_msg = arg;
    lv_obj_t *s = ui_screen();

    ui_rect(s, 6, 40, 116, 80, COL_PANEL);
    ui_text(s, 12, 48, 104, COL_FG, cur_msg->text);
    ui_label(s, 12, 106, COL_DIM, cur_msg->confirm ? "A: Yes   B: No" : "A: OK");
}

static void msg_btn(enum button b)
{
    msg_t m = *cur_msg;

    if (m.confirm) {
        if (b != BTN_A && b != BTN_B) {
            return;
        }
        ui_pop();
        if (m.ccb) {
            m.ccb(b == BTN_A, m.ud);
        }
        return;
    }
    if (b == BTN_A || b == BTN_B) {
        ui_pop();
        if (m.cb) {
            m.cb(m.ud);
        }
    }
}

const ui_scene_t scene_msg = {
    .name = "msg", .enter = msg_enter, .on_btn = msg_btn,
};

/* ------------------------------------------------------------------ */
/* Widget helpers                                                      */
/* ------------------------------------------------------------------ */

static void strip(lv_obj_t *o)
{
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);

    strip(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, int x, int y, uint32_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_font(l, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *ui_text(lv_obj_t *parent, int x, int y, int w, uint32_t color, const char *text)
{
    lv_obj_t *l = ui_label(parent, x, y, color, text);

    lv_obj_set_width(l, w);             /* fixed width -> default long mode wraps */
    return l;
}

lv_obj_t *ui_image(lv_obj_t *parent, int x, int y, const lv_image_dsc_t *src)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    return img;
}

lv_obj_t *ui_bar(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *b = lv_bar_create(parent);

    strip(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, 0, LV_PART_INDICATOR);
    return b;
}

void ui_bar_set(lv_obj_t *bar, int value, int max)
{
    int pct = max > 0 ? (value * 100) / max : 0;

    lv_bar_set_value(bar, pct < 0 ? 0 : (pct > 100 ? 100 : pct), LV_ANIM_OFF);
}

void ui_bar_color(lv_obj_t *bar, uint32_t color)
{
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
}

/* ---- Menu list ---- */

void ui_menu_create(ui_menu *m, lv_obj_t *parent, int x, int y, int w, int rowh, int visible)
{
    memset(m, 0, sizeof(*m));
    m->vis = visible > UI_MENU_MAX_ROWS ? UI_MENU_MAX_ROWS : visible;
    m->rowh = rowh;
    for (int i = 0; i < m->vis; i++) {
        m->row[i] = ui_rect(parent, x, y + i * rowh, w, rowh, COL_BG);
        m->txt[i] = ui_label(m->row[i], 3, (rowh - 8) / 2, COL_FG, "");
    }
}

void ui_menu_redraw(ui_menu *m)
{
    for (int i = 0; i < m->vis; i++) {
        int idx = m->first + i;
        bool shown = idx < m->count;
        bool sel = shown && idx == m->cursor;
        bool dis = shown && (m->disabled & (1u << idx));

        lv_obj_set_style_bg_color(m->row[i], lv_color_hex(sel ? COL_ACCENT : COL_BG), 0);
        lv_obj_set_style_text_color(m->txt[i],
            lv_color_hex(sel ? COL_BG : (dis ? COL_DIM : COL_FG)), 0);
        lv_label_set_text(m->txt[i], shown ? m->items[idx] : "");
    }
}

void ui_menu_set(ui_menu *m, const char *const *items, int count)
{
    m->items = items;
    m->count = count;
    if (m->cursor >= count) {
        m->cursor = count ? count - 1 : 0;
    }
    if (m->first > m->cursor) {
        m->first = m->cursor;
    }
    ui_menu_redraw(m);
}

void ui_menu_move(ui_menu *m, int dir)
{
    if (m->count == 0) {
        return;
    }
    m->cursor = (m->cursor + dir + m->count) % m->count;
    if (m->cursor < m->first) {
        m->first = m->cursor;
    } else if (m->cursor >= m->first + m->vis) {
        m->first = m->cursor - m->vis + 1;
    }
    ui_menu_redraw(m);
}
