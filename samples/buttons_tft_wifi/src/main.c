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
 * https://docs.zephyrproject.org/latest/doxygen/html/group__net__mgmt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structnet__mgmt__event__callback.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/l2/wifi/wifi_shell.c
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/mqtt.html
 * https://io.adafruit.com/api/docs/mqtt.html#adafruit-io-mqtt-api
 *
 * AdafruitIO MQTT Settings:
 * host:port: io.adafruit.com:8883
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <lvgl.h>
#include <lvgl_input_device.h>
#include <stdio.h>
#include <string.h>


/*
* STATIC GLOBALS
*/

// Flag for communication between net_callback and main about wifi status
static int WifiUp = 0;

// AdafruitIO MQTT auth credential buffers
static char AIOUser[64] = {'\0'};
static char AIOKey[64] = {'\0'};

/*
* SHELL COMMANDS
*/

// usage: auth <username> <key>
static int cmd_auth(const struct shell *shell, size_t argc, char *argv[]) {
	if (argc < 3 || argv == NULL) {
		return 1;
	}
	const char *user = argv[1];
	const char *key = argv[2];
	// Copy username and key to static buffer that won't go out of scope
	strncpy(AIOUser, user, sizeof(AIOUser));
	AIOUser[sizeof(AIOUser)-1] = 0;
	strncpy(AIOKey, key, sizeof(AIOKey));
	AIOKey[sizeof(AIOKey)-1] = 0;
	printk("aio auth: username='%s' key='%s'\n", AIOUser, AIOKey);
	return 0;
}

static int cmd_test(const struct shell *shell, size_t argc, char *argv[]) {
	printk("todo: aio test\n");
	return 0;
}


/*
* EVENT CALLBACKS
*/

// Handle button events
static void btn1_callback(lv_event_t *event) {
	// TODO: trigger MQTT to AdafruitIO
	printk("button clicked\n");
}

// Hhandle network manager events (wifi up / wifi down)
static void net_callback(struct net_mgmt_event_callback *cb,
	uint32_t mgmt_event, struct net_if *iface)
{
	if(mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		WifiUp = 1;
		printk("net: WIFI_CONNECT\n");
	} else if(mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		WifiUp = 0;
		printk("net: WIFI_DISCONNECT\n");
	} else {
		printk("net: unknown event\n");
	}
}


/*
* MAIN
*/
int main(void) {
	// Inits
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	const struct device *keypad =
		DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_lvgl_keypad_input));
	struct net_mgmt_event_callback net_status;
	lv_init();
	lv_tick_set_cb(k_uptime_get_32);

	// These macros add the `aio auth` shell command for setting AdafruitIO
	// MQTT authentication credentials in the Zephyr Shell with USB serial.
	// When combined with the `wifi connect` shell command, this makes it
	// unnecessary to hardcode any network authentication secrets. Using the
	// serial shell is also good for troubleshooting because it lets you see
	// Zephyr's log messages about any memory allocation or network errors
	// that might be happening. Otherwise, you might not know.
	//
	SHELL_STATIC_SUBCMD_SET_CREATE(aio_cmds,
		SHELL_CMD_ARG(auth, NULL, "set Adafruit IO username and key\n"
			"usage: auth <username> <key>\n",
			cmd_auth, 3, 0),
		SHELL_CMD(test, NULL, "Send a test message", cmd_test),
		SHELL_SUBCMD_SET_END
	);
	SHELL_CMD_REGISTER(aio, &aio_cmds, "AdafruitIO MQTT commands", NULL);

	// Colors
	lv_color_t bgColor = lv_palette_darken(LV_PALETTE_GREY, 4);
	lv_color_t wifiColorDown = lv_palette_darken(LV_PALETTE_GREY, 3);
	lv_color_t wifiColorUp = lv_palette_main(LV_PALETTE_GREEN);

	// Change background color (see also lv_palette_main())
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, bgColor, 0);

	// Make wifi status label
	lv_obj_t *wifiLabel = lv_label_create(lv_screen_active());
	lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
	lv_label_set_text(wifiLabel, LV_SYMBOL_WARNING);
	lv_obj_align(wifiLabel, LV_ALIGN_TOP_RIGHT, -10, 5);
	lv_obj_set_style_text_color(wifiLabel, wifiColorDown, 0);

	// Make Button 1 (gets added to kepad group later)
	lv_obj_t *btn1 = lv_button_create(lv_screen_active());
	lv_obj_align(btn1, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(btn1, btn1_callback, LV_EVENT_PRESSED, NULL);
	lv_obj_t *label = lv_label_create(btn1);
	lv_label_set_text(label, "Button 1");

	// Configure a group so keypad can control focus and button pressing.
	// This expects that the devicetree config has provided a keypad with
	// pysical buttons mapped to navigation keys such as LV_KEY_LEFT,
	// LV_KEY_RIGHT, and LV_KEY_ENTER. Minimum is LV_KEY_ENTER.
	lv_group_t *grp = lv_group_create();
	lv_group_add_obj(grp, btn1);
	lv_indev_set_group(lvgl_input_get_indev(keypad), grp);

	// Register to get updates about wifi connection status
	net_mgmt_init_event_callback(&net_status, net_callback,
		NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&net_status);

	// Event loop
	int prevWifiUp = 0;
	lv_timer_handler();
	display_blanking_off(display);
	while(1) {
		// Update status line
		if (prevWifiUp != WifiUp) {
			if (WifiUp) {
				lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
				lv_obj_set_style_text_color(wifiLabel, wifiColorUp, 0);
			} else {
				lv_label_set_text(wifiLabel, LV_SYMBOL_WARNING);
				lv_obj_set_style_text_color(wifiLabel, wifiColorDown, 0);
			}
			prevWifiUp = WifiUp;
		}

		// Call LVGL then sleep until time for the next tick
		uint32_t holdoff_ms = lv_timer_handler();
		k_msleep(holdoff_ms);
	}
}
