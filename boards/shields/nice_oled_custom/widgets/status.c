/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Central (left half) display: battery, BLE profile, layer name
 * Laid out horizontally for 128x32 OLED:
 *
 *   ┌──────────────────────────────────────────────┐
 *   │ [BAT] [BT1]  Layer: DEFAULT                  │
 *   │                                              │
 *   └──────────────────────────────────────────────┘
 *
 * Top row: battery icon (20px) + profile indicator + layer name
 * All drawn on a single 128x32 canvas.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#include "status.h"
#include "util.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ── Drawing helpers ────────────────────────────────────────────── */

static void draw_top(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_fg;
    init_rect_dsc(&rect_fg, LVGL_FOREGROUND);
    lv_draw_rect_dsc_t rect_bg;
    init_rect_dsc(&rect_bg, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);

    /* Battery icon at (0, 2) — 20x8 */
    draw_battery(canvas, state);

    /* BLE profile indicator at (24, 0) */
    char profile_str[4];
    snprintf(profile_str, sizeof(profile_str), "%d", state->active_profile_index + 1);

    if (state->active_profile_connected) {
        /* Filled rectangle for connected profile */
        lv_canvas_draw_rect(canvas, 24, 0, 10, 10, &rect_fg);
        lv_draw_label_dsc_t label_inv;
        init_label_dsc(&label_inv, LVGL_BACKGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
        lv_canvas_draw_text(canvas, 24, -1, 10, &label_inv, profile_str);
    } else if (state->active_profile_bonded) {
        /* Outline rectangle for bonded but disconnected */
        lv_canvas_draw_rect(canvas, 24, 0, 10, 10, &rect_fg);
        lv_canvas_draw_rect(canvas, 25, 1, 8, 8, &rect_bg);
        lv_canvas_draw_text(canvas, 24, -1, 10, &label_dsc, profile_str);
    } else {
        /* Dotted outline for unbonded */
        lv_canvas_draw_text(canvas, 24, -1, 10, &label_dsc, profile_str);
    }

    /* Connection type indicator */
    struct zmk_endpoint_instance endpoint = state->selected_endpoint;
    const char *conn_label;
    if (endpoint.transport == ZMK_TRANSPORT_USB) {
        conn_label = "USB";
    } else {
        conn_label = "BT";
    }
    lv_canvas_draw_text(canvas, 36, -1, 20, &label_dsc, conn_label);
}

static void draw_layer(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);

    const char *layer_name = state->layer_label;
    if (layer_name == NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "L%d", state->layer_index);
        lv_canvas_draw_text(canvas, 0, 16, 128, &label_dsc, buf);
    } else {
        lv_canvas_draw_text(canvas, 0, 16, 128, &label_dsc, layer_name);
    }
}

/* ── Redraw ─────────────────────────────────────────────────────── */
static void draw_status(struct zmk_widget_status *widget, struct status_state state) {
    lv_canvas_fill_bg(widget->obj, LVGL_BACKGROUND, LV_OPA_COVER);
    draw_top(widget->obj, &state);
    draw_layer(widget->obj, &state);
}

/* ── State getters ──────────────────────────────────────────────── */
static struct status_state status_get_state(const zmk_event_t *_eh) {
    return (struct status_state){
        .battery = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .charging = zmk_usb_is_powered(),
#else
        .charging = false,
#endif
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
        .layer_index = zmk_keymap_highest_layer_active(),
        .layer_label = zmk_keymap_layer_name(zmk_keymap_highest_layer_active()),
    };
}

static void status_update_cb(struct status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_status(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_status, struct status_state, status_update_cb, status_get_state)

ZMK_SUBSCRIPTION(widget_status, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_status, zmk_layer_state_changed);

/* ── Init ───────────────────────────────────────────────────────── */
int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(widget->obj, widget->cbuf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 0, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}
