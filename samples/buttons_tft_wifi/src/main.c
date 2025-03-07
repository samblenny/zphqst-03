/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Docs & References:
 * https://docs.lvgl.io/master/details/integration/adding-lvgl-to-your-project/connecting_lvgl.html
 * https://docs.zephyrproject.org/apidoc/latest/group__clock__apis.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/subsys/display/lvgl/src/main.c
 * https://docs.lvgl.io/9.2/overview/event.html#add-events-to-a-widget
 * https://docs.lvgl.io/9.2/porting/indev.html#keypad-or-keyboard
 * https://docs.lvgl.io/9.2/porting/timer_handler.html
 * https://docs.lvgl.io/9.2/overview/display.html  (change bg color)
 * https://docs.lvgl.io/9.2/overview/color.html  (color constants)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <lvgl.h>
#include <lvgl_input_device.h>


static void btn1_callback(lv_event_t *event) {
	printk("button clicked\n");
}

int main(void) {
	// Inits
	lv_init();
	lv_tick_set_cb(k_uptime_get_32);
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	// Change background color (see also lv_palette_main())
	lv_obj_t *scr = lv_screen_active();
	lv_color_t bg = lv_palette_darken(LV_PALETTE_GREY, 4);
	lv_obj_set_style_bg_color(scr, bg, 0);

	// Make widgets
	lv_obj_t *btn1 = lv_button_create(lv_screen_active());
	lv_obj_align(btn1, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(btn1, btn1_callback, LV_EVENT_CLICKED, NULL);
	lv_obj_add_event_cb(btn1, btn1_callback, LV_EVENT_PRESSED, NULL);
	lv_obj_t *label = lv_label_create(btn1);
	lv_label_set_text(label, "Button 1");

	// Event loop
	lv_timer_handler();
	display_blanking_off(display);
	while(1) {
		uint32_t holdoff_ms = lv_timer_handler();
		k_msleep(holdoff_ms);
	}
}
