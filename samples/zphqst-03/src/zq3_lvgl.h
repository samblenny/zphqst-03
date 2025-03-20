/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZQ3_LVGL_H
#define ZQ3_LVGL_H

#include <lvgl.h>


typedef struct {
	lv_color_t gray_dark;
	lv_color_t gray;
	lv_color_t green;
	lv_obj_t *wifi;        // status bar wifi icon
	lv_obj_t *status;      // large status label in center of screen
	lv_obj_t *toggle;      // toggle switch widget
	lv_group_t *grp;       // keypad input group
} zq3_lvgl_context;

void zq3_lvgl_init(zq3_lvgl_context *ctx, lv_event_cb_t keypad_callback);

void zq3_lvgl_show_message(zq3_lvgl_context *ctx, const char *msg);

void zq3_lvgl_show_toggle(zq3_lvgl_context *ctx);

void zq3_lvgl_set_toggle(zq3_lvgl_context *ctx, bool checked);

void zq3_lvgl_wifi_status(zq3_lvgl_context *ctx, bool up);

uint32_t zq3_lvgl_timer_handler();


#endif /* ZQ3_LVGL_H */
