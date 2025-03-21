/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL Docs & Refs:
 * https://docs.zephyrproject.org/apidoc/latest/group__clock__apis.html
 * https://docs.lvgl.io/9.2/overview/event.html#add-events-to-a-widget
 * https://docs.lvgl.io/9.2/porting/indev.html#keypad-or-keyboard
 * https://docs.lvgl.io/9.2/porting/timer_handler.html
 * https://docs.lvgl.io/9.2/widgets/switch.html (toggle switch widget)
 * https://docs.lvgl.io/9.2/overview/display.html  (change bg color)
 * https://docs.lvgl.io/9.2/overview/color.html  (color constants)
 */

#include <zephyr/kernel.h>           // k_uptime_get_32()
#include <zephyr/drivers/display.h>  // display_blanking_off()
#include <lvgl.h>
#include <lvgl_input_device.h>
#include "zq3_lvgl.h"


// Hide a widget
static void hide(lv_obj_t *obj) {
	lv_obj_add_state(obj, LV_STATE_DISABLED);
	lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Show a widget
static void show(lv_obj_t *obj) {
	lv_obj_clear_state(obj, LV_STATE_DISABLED);
	lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Initialize the GUI:
// - context struct (ctx) gets initialized with objects and values that need
//   to stay available for future reference while the GUI is in use
// - callback function pointer gets registered for keypad pressed events
//
void zq3_lvgl_init(zq3_lvgl_context *ctx, lv_event_cb_t keypad_callback) {
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	const struct device *keypad =
		DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_lvgl_keypad_input));

	lv_init();
	lv_tick_set_cb(k_uptime_get_32);

	// Save theme colors in context
	ctx->gray_dark = lv_palette_darken(LV_PALETTE_GREY, 4);
	ctx->gray = lv_palette_darken(LV_PALETTE_GREY, 3);
	ctx->green = lv_palette_main(LV_PALETTE_GREEN);

	// Change background color (see also lv_palette_main())
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, ctx->gray_dark, 0);

	// Make wifi icon label
	ctx->wifi = lv_label_create(lv_screen_active());
	lv_label_set_text(ctx->wifi, LV_SYMBOL_WIFI);
	lv_obj_align(ctx->wifi, LV_ALIGN_TOP_RIGHT, -10, 5);
	lv_obj_set_style_text_color(ctx->wifi, ctx->gray, 0);

	// Make large text status label in center of screen
	// This is initially visible
	ctx->status = lv_label_create(lv_screen_active());
	lv_label_set_text(ctx->status, "Loading...");
	lv_obj_set_style_text_align(ctx->status, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(ctx->status, ctx->green, 0);
	lv_obj_center(ctx->status);

	// Make toggle switch widget (gets added to keypad group later)
	// This is initially hidden
	ctx->toggle = lv_switch_create(lv_screen_active());
	lv_obj_set_size(ctx->toggle, 120, 60);
	lv_obj_center(ctx->toggle);
	hide(ctx->toggle);

	// Set up input group so keypad press events go to the active screen. This
	// arrangement (vs. putting pressed event on the switch) lets the main
	// event loop consider both keypad presses and received MQTT messages to
	// decide what the toggle switch's CHECKED state should be. For this to
	// work, the devicetree config must provide a keypad with physical buttons
	// mapped to navigation keys such as LV_KEY_LEFT, LV_KEY_RIGHT, and
	// LV_KEY_ENTER. Minimum is LV_KEY_ENTER.
	lv_obj_t *screen = lv_screen_active();
	ctx->grp = lv_group_create();
	lv_group_add_obj(ctx->grp, screen);
	lv_indev_set_group(lvgl_input_get_indev(keypad), ctx->grp);
	lv_obj_add_event_cb(screen, keypad_callback, LV_EVENT_PRESSED, NULL);

	display_blanking_off(display);
}

// Show a status message in the center of the screen. This is meant for errors
// and network connection status indications. So, showing a message will hide
// the big toggle switch.
void zq3_lvgl_show_message(zq3_lvgl_context *ctx, const char *msg) {
	hide(ctx->toggle);
	show(ctx->status);
	lv_label_set_text(ctx->status, msg);
	lv_obj_set_style_text_align(ctx->status, LV_TEXT_ALIGN_CENTER, 0);
}

// Show the big toggle switch. This means MQTT is connected and subscribed
void zq3_lvgl_show_toggle(zq3_lvgl_context *ctx) {
	hide(ctx->status);
	show(ctx->toggle);
}

// Change the toggle switch state: checked==true means on, false means off
void zq3_lvgl_set_toggle(zq3_lvgl_context *ctx, bool checked) {
	if (checked) {
		lv_obj_add_state(ctx->toggle, LV_STATE_CHECKED);
	} else {
		lv_obj_clear_state(ctx->toggle, LV_STATE_CHECKED);
	}
}

// Update wifi statusbar icon color: up==true means green, false means gray
void zq3_lvgl_wifi_status(zq3_lvgl_context *ctx, bool up) {
	lv_color_t color = up ? ctx->green : ctx->gray;
	lv_obj_set_style_text_color(ctx->wifi, color, 0);
}

// The main event loop must call this frequently so LVGL can update the screen
uint32_t zq3_lvgl_timer_handler() {
	return lv_timer_handler();
}
