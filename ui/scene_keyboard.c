/*
 * On-screen keyboard driven by the D-pad.
 *   D-pad = move, A = press key, B = backspace.
 * Grid: 8 columns x 4 rows of 16x16 keys + a control row (Shft / 123 / DEL / OK).
 */
#include <string.h>
#include "ui.h"
#include "kindrals.h"

#define KEY 16
#define GRID_Y 72
#define COLS 8
#define ROWS 4

static const char *const layouts[3][ROWS] = {
    { "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ.-'!? " },
    { "abcdefgh", "ijklmnop", "qrstuvwx", "yz.-'!? " },
    { "01234567", "89.,-_'!", "?+*/:;&@", "#()=%$~ " },
};

static ui_keyboard_args args;
static char text[KIN_NAME_LEN];
static int len, mode, cr, cc;
static lv_obj_t *hl, *keys[ROWS][COLS], *ctrl_lbl[4], *txt_lbl, *cnt_lbl;

void ui_keyboard_open(const ui_keyboard_args *a)
{
    args = *a;
    ui_push(&scene_keyboard, NULL);
}

static void draw_text(void)
{
    char buf[KIN_NAME_LEN + 2];

    snprintf(buf, sizeof(buf), "%s_", text);
    lv_label_set_text(txt_lbl, buf);
    lv_label_set_text_fmt(cnt_lbl, "%d/%d", len, args.max_chars);
}

static void draw_keys(void)
{
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char ch = layouts[mode][r][c];

            lv_label_set_text(keys[r][c], ch == ' ' ? "_" : (char[]){ ch, 0 });
        }
    }
    lv_label_set_text(ctrl_lbl[1], mode == 2 ? "abc" : "123");
    lv_label_set_text(ctrl_lbl[0], mode == 0 ? "ABC" : "abc");
}

static void place_cursor(void)
{
    if (cr < ROWS) {
        lv_obj_set_pos(hl, cc * KEY, GRID_Y + cr * KEY);
        lv_obj_set_size(hl, KEY, KEY);
    } else {
        lv_obj_set_pos(hl, (cc / 2) * 32, GRID_Y + ROWS * KEY);
        lv_obj_set_size(hl, 32, KEY);
    }
}

static void kb_enter(void *arg)
{
    ARG_UNUSED(arg);
    lv_obj_t *s = ui_screen();

    snprintf(text, sizeof(text), "%s", args.initial ? args.initial : "");
    len = (int)strlen(text);
    mode = (len == 0) ? 0 : 1;
    cr = cc = 0;

    ui_label(s, 4, 4, COL_ACCENT, args.title);
    ui_rect(s, 2, 24, 124, 20, COL_PANEL);
    txt_lbl = ui_label(s, 6, 30, COL_FG, "");
    cnt_lbl = ui_label(s, 4, 48, COL_DIM, "");
    ui_label(s, 4, 60, COL_DIM, "A:key B:del");

    hl = ui_rect(s, 0, GRID_Y, KEY, KEY, COL_ACCENT);        /* created first = drawn below labels */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            keys[r][c] = ui_label(s, c * KEY + 4, GRID_Y + r * KEY + 4, COL_FG, "");
        }
    }
    static const char *const names[4] = { "ABC", "123", "DEL", " OK" };

    for (int i = 0; i < 4; i++) {
        ctrl_lbl[i] = ui_label(s, i * 32 + 4, GRID_Y + ROWS * KEY + 4, COL_ACCENT, names[i]);
    }
    draw_keys();
    draw_text();
    place_cursor();
}

static void restyle_cursor_text(void)
{
    /* Dark text on the highlighted key, light elsewhere. */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            lv_obj_set_style_text_color(keys[r][c],
                lv_color_hex((r == cr && c == cc) ? COL_BG : COL_FG), 0);
        }
    }
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_text_color(ctrl_lbl[i],
            lv_color_hex((cr == ROWS && cc / 2 == i) ? COL_BG : COL_ACCENT), 0);
    }
}

static void press(void)
{
    if (cr < ROWS) {
        if (len < args.max_chars) {
            text[len++] = layouts[mode][cr][cc];
            text[len] = '\0';
            if (mode == 0 && len == 1) {        /* auto-capitalise the first letter only */
                mode = 1;
                draw_keys();
            }
        }
        draw_text();
        return;
    }
    switch (cc / 2) {
    case 0: mode = (mode == 0) ? 1 : 0; draw_keys(); break;
    case 1: mode = (mode == 2) ? 1 : 2; draw_keys(); break;
    case 2: if (len) { text[--len] = '\0'; draw_text(); } break;
    case 3:
        if (len == 0 && !args.allow_empty) {
            ui_toast("Please type a name first.");
            return;
        }
        if (args.done) {
            args.done(text);
        }
        break;
    }
}

static void kb_btn(enum button b)
{
    int rows = ROWS + 1;

    switch (b) {
    case BTN_UP:    cr = (cr + rows - 1) % rows; break;
    case BTN_DOWN:  cr = (cr + 1) % rows; break;
    case BTN_LEFT:  cc = (cc + COLS - 1) % COLS; break;
    case BTN_RIGHT: cc = (cc + 1) % COLS; break;
    case BTN_A:
        press();
        /* done() may have replaced/deleted this scene (e.g. OK on the trainer name).
         * Its widgets are gone then, so never touch them again. */
        if (ui_top() != &scene_keyboard) {
            return;
        }
        break;
    case BTN_B:     if (len) { text[--len] = '\0'; draw_text(); } break;
    }
    place_cursor();
    restyle_cursor_text();
}

static void kb_resume(void)
{
    restyle_cursor_text();
}

const ui_scene_t scene_keyboard = {
    .name = "keyboard", .enter = kb_enter, .resume = kb_resume, .on_btn = kb_btn,
};
