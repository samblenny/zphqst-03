/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * LVGL Docs & Refs:
 * https://docs.lvgl.io/master/details/integration/adding-lvgl-to-your-project/connecting_lvgl.html
 * https://docs.zephyrproject.org/apidoc/latest/group__clock__apis.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/subsys/display/lvgl/src/main.c
 * https://docs.lvgl.io/9.2/overview/event.html#add-events-to-a-widget
 * https://docs.lvgl.io/9.2/porting/indev.html#keypad-or-keyboard
 * https://docs.lvgl.io/9.2/porting/timer_handler.html
 * https://docs.lvgl.io/9.2/overview/display.html  (change bg color)
 * https://docs.lvgl.io/9.2/overview/color.html  (color constants)
 *
 * Wifi + Network Manager Docs & Refs:
 * https://docs.zephyrproject.org/latest/doxygen/html/group__net__mgmt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structnet__mgmt__event__callback.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/l2/wifi/wifi_shell.c
 *
 * AIO + MQTT Docs & Refs:
 * https://io.adafruit.com/api/docs/mqtt.html#adafruit-io-mqtt-api
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/mqtt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__evt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/group__mqtt__socket.html
 * https://docs.zephyrproject.org/apidoc/latest/structsockaddr.html
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/sockets.html#secure-sockets-interface
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

// AdafruitIO MQTT broker hostname, port, and auth credentials
typedef struct {
	char user[64];
	char pass[64];
	char host[64];
	bool tls;
	bool valid;
} aio_config_t;
static aio_config_t AIOConf = {{'\0'}, {'\0'}, {'\0'}, true, false};
#define AIO_URL_MAX_LEN (sizeof("mqtts://:@") + 64 + 64 + 64)

// MQTT Buffers and context structs
/*
static uint8_t MQRxBuf[256];
static uint8_t MQTxBuf[256];
static struct mqtt_client Ctx;
static struct sockaddr_storage Broker;
*/

void mq_init() {
/*
	mqtt_client_init(&Ctx);
	Ctx.broker = &MQBroker;
	Ctx.evt_cb = mq_handler;
	Ctx.client_id.utf_8 = NULL;
	Ctx.client_id.size = 0;
	Ctx.password = AIOConf.pass;
	Ctx.user_name = AIOConf.user;
	Ctx.protocol_version = MQTT_VERSION_3_1_1;
	Ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;  // TODO: use TLS
	Ctx.rx_buf = MQRxBuf;
	Ctx.rx_buf_size = sizeof(MQRxBuf);
	Ctx.tx_buf = MQTxBuf;
	Ctx.tx_buf_size = sizeof(MQTxBuf);
*/
	// TODO: add TLS support
	// Ctx.transport.type = MQTT_TRANSPORT_SECURE;
	// struct mqtt_sec_config *conf = &Ctx.transport.tls.config;
	// conf->cipher_list = NULL; 
	// conf->hostname = MQTT_BROKER_HOSTNAME; 
}

void mq_handler(struct mqtt_client *client, const struct mqtt_evt *e) {
	switch (e->type) {
	case MQTT_EVT_CONNACK:
		printk("CONNACK\n");
		break;
	case MQTT_EVT_DISCONNECT:
		printk("DISCONNECT\n");
		break;
	case MQTT_EVT_PUBLISH:
		printk("PUBLISH\n");
		break;
	case MQTT_EVT_PUBACK:
		printk("PUBACK\n");
		break;
	case MQTT_EVT_PUBREC:
		printk("PUBREC\n");
		break;
	case MQTT_EVT_PUBREL:
		printk("PUBREL\n");
		break;
	case MQTT_EVT_PUBCOMP:
		printk("PUBCOMP\n");
		break;
	case MQTT_EVT_SUBACK:
		printk("SUBACK\n");
		break;
	case MQTT_EVT_UNSUBACK:
		printk("UNSUBACK\n");
		break;
	case MQTT_EVT_PINGRESP:
		printk("PINGRESP\n");
		break;
	default:
		break;
	}
}


/*
* SHELL COMMANDS
*/

// Handle the aio broker shell command by parsing and saving the URL.
// Main point of this is to copy username, password, and host strings into
// buffers that won't get deallocated before they are needed.
//   usage: broker mqtt[s]://[<user>]:[<key>]@<host>
static int cmd_broker(const struct shell *shell, size_t argc, char *argv[]) {
	AIOConf.valid = false;

	// Avoid dereferencing null pointers if URL argument is missing
	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		printk("ERROR: expected a URL\n");
		return 1;
	}
	// Avoid processing a string that is unterminated or too long. The point
	// of this is to guard against weird edge cases that could cause strchr()
	// to misbehave. If we pass this check, using strchr() should be fine.
	const char *url = argv[1];
	const char *cursor = url;
	if (!memchr(url, '\0', AIO_URL_MAX_LEN)) {
		printk("ERROR: URL is too long\n");
		return 2;
	}

	// Parse URL scheme prefix (should be "mqtt://" or "mqtts://")
	char *tls_scheme = "mqtts://";
	char *nontls_scheme = "mqtt://";
	if (strncmp(url, tls_scheme, strlen(tls_scheme)) == 0) {
		// Scheme = MQTT with TLS
		cursor += strlen(tls_scheme);
		AIOConf.tls = true;
		printk(" tls\n"); // TODO: remove
	} else if (strncmp(url, nontls_scheme, strlen(nontls_scheme)) == 0) {
		// Scheme = unencrypted MQTT
		cursor += strlen(nontls_scheme);
		AIOConf.tls = false;
		printk(" nontls\n"); // TODO: remove
	} else {
		printk("ERROR: expected mqtt:// or mqtts://\n");
		return 3;  
	}

	// Parse username (whatever is between end of scheme and the next ':')
	char *delim = strchr((char *)cursor, ':');
	printk("cursor: '%s'\n", cursor ? cursor : "NULL"); // TODO: zap
	printk("delim: '%s'\n", delim ? delim : "NULL"); // TODO: zap
	printk("len: %d\n", delim - cursor); // TODO: zap
	int len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing ':' after username\n");
		return 4;
	} else if (len >= sizeof(AIOConf.user)) {
		printk("ERR: username too long\n");
		return 5;
	} else {
		// Copy username string with zero fill (relies on len < sizeof(...))
		memset(AIOConf.user, 0, sizeof(AIOConf.user));
		memcpy(AIOConf.user, cursor, len);
		cursor = delim + 1;
		printk(" user: %s\n", AIOConf.user); // TODO: remove
	}

	// Parse password (whatever is between ':' and '@')
	delim = strchr((char *)cursor, '@');
	len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing '@' after password\n");
		return 6;
	} else if (len >= sizeof(AIOConf.pass)) {
		printk("ERR: password too long\n");
		return 7;
	} else {
		// Copy password string with zero fill (relies on len < sizeof(...))
		memset(AIOConf.pass, 0, sizeof(AIOConf.pass));
		memcpy(AIOConf.pass, cursor, len);
		cursor = delim + 1;
		printk(" pass: %s\n", AIOConf.pass); // TODO: remove
	}

	// Parse host (whatever is after '@')
	len = strlen(cursor);
	if (len == 0) {
		printk("ERR: missing hostname after '@'\n");
		return 8;
	} else if (len >= sizeof(AIOConf.host)) {
		printk("ERR: hostname too long\n");
		return 9;
	} else {
		// Copy hostname string with zero fill (relies on len < sizeof(...))
		memset(AIOConf.host, 0, sizeof(AIOConf.host));
		memcpy(AIOConf.host, cursor, len);
		printk(" host: %s\n", AIOConf.host); // TODO: remove
	}
	AIOConf.valid = true;
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

	// These macros add the `aio broker` and `aio test` shell commands for
	// configuring and testing the MQTT broker in the Zephyr shell over usb
	// serial. Combined with the `wifi connect` shell command, this makes it
	// unnecessary to hardcode network authentication secrets.
	//
	SHELL_STATIC_SUBCMD_SET_CREATE(aio_cmds,
		SHELL_CMD_ARG(broker, NULL, "configure MQTT broker with URL\n"
			"usage: broker http[s]://[<user>]:[<key>]@<host>:<port>\n"
			"    Okay to omit <user> and <key>, but keep the ':' and '@'.\n"
			"    \"mqtt://...\" uses port 1883 unencrypted.\n"
			"    \"mqtts://...\" uses port 8883 with TLS.\n"
			"    Examples:\n"
			"        broker mqtt://:@192.168.0.50\n"
			"        broker mqtts://Blinka:aio_1234@io.adafruit.com\n",
			cmd_broker, 2, 0),
		SHELL_CMD(test, NULL, "Publish a test message", cmd_test),
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
