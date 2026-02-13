/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Peripheral (right half) display: slideshow of placeholder art
 * with battery + connection status overlaid in the top-right corner.
 *
 * Display: 128x32 SSD1306 OLED
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/usb.h>
#include <zmk/split/bluetooth/peripheral.h>

#include "peripheral_status.h"
#include "util.h"

/* Provided by art.c */
extern const lv_img_dsc_t *anim_imgs[];
extern const size_t anim_imgs_len;

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_state {
    uint8_t battery;
    bool charging;
    bool connected;
};

static struct peripheral_state last_state = {0};

/* ── Battery overlay on canvas ──────────────────────────────────── */
static void draw_peripheral_battery(lv_obj_t *canvas, struct peripheral_state *state) {
    lv_draw_rect_dsc_t rect_bg;
    init_rect_dsc(&rect_bg, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_fg;
    init_rect_dsc(&rect_fg, LVGL_FOREGROUND);

    /* Battery icon: 20x8 in top-right area of a 32x32 canvas */
    lv_canvas_draw_rect(canvas, 0, 0, 20, 8, &rect_fg);
    lv_canvas_draw_rect(canvas, 1, 1, 18, 6, &rect_bg);
    lv_canvas_draw_rect(canvas, 2, 2, (state->battery + 5) / 6, 4, &rect_fg);
    lv_canvas_draw_rect(canvas, 20, 2, 2, 4, &rect_fg);
    lv_canvas_draw_rect(canvas, 21, 3, 1, 2, &rect_bg);

    /* Connection dot: filled circle at (25, 10) */
    if (state->connected) {
        lv_canvas_draw_rect(canvas, 24, 10, 4, 4, &rect_fg);
    } else {
        /* Draw outline only for disconnected */
        lv_canvas_draw_rect(canvas, 24, 10, 4, 4, &rect_fg);
        lv_canvas_draw_rect(canvas, 25, 11, 2, 2, &rect_bg);
    }
}

static void update_peripheral_cb(struct peripheral_state state) {
    last_state = state;

    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_canvas_fill_bg(widget->obj, LVGL_BACKGROUND, LV_OPA_COVER);
        draw_peripheral_battery(widget->obj, &last_state);
    }
}

/* ── Battery event listener ─────────────────────────────────────── */
static struct peripheral_state peripheral_get_state(const zmk_event_t *eh) {
    return (struct peripheral_state){
        .battery = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .charging = zmk_usb_is_powered(),
#else
        .charging = false,
#endif
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL)
        .connected = zmk_split_bt_peripheral_is_connected(),
#else
        .connected = true,
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_state,
                            update_peripheral_cb, peripheral_get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_battery_state_changed);

/* ── Init ───────────────────────────────────────────────────────── */
int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    /* Full-screen animation image for the slideshow */
    lv_obj_t *art_anim = lv_animimg_create(parent);
    lv_obj_set_size(art_anim, 128, 32);
    lv_obj_align(art_anim, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_animimg_set_src(art_anim, (const void **)anim_imgs, anim_imgs_len);
    lv_animimg_set_duration(art_anim, CONFIG_CUSTOM_ANIMATION_SPEED);
    lv_animimg_set_repeat_count(art_anim, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(art_anim);

    /* Battery/status canvas overlaid in top-right corner */
    widget->obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(widget->obj, widget->cbuf, 32, 16, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(widget->obj, LV_ALIGN_TOP_RIGHT, 0, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}
