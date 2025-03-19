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
 *
 * Wifi + Network Manager Docs & Refs:
 * https://docs.zephyrproject.org/latest/doxygen/html/group__net__mgmt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structnet__mgmt__event__callback.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/l2/wifi/wifi_shell.c
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>  /* display_blanking_off() */
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>  /* NET_EVENT_WIFI_CONNECT_RESULT, etc */
#include <zephyr/shell/shell.h>
#include <lvgl.h>
#include <lvgl_input_device.h>
#include "zq3.h"
#include "zq3_mqtt.h"
#include "zq3_url.h"


/*
* STATIC GLOBALS AND CONSTANTS
*/

// Context for wifi status, mqtt config, and mqtt status
static zq3_context ZCtx = {
	.user = {'\0'},
	.pass = {'\0'},
	.host = {'\0'},
	.topic = {'\0'},
	.tls = true,
	.valid = false,
	.state = MQTT_DOWN,
	.wifi_up = false,
	.btn1_clicked = false,
	.got_0 = false,
	.got_1 = false,
	.toggle = UNKNOWN,
};

struct sockaddr_storage AIOBroker;

// MQTT context struct
static struct mqtt_client Ctx;


/*
* MISC MQTT STUFF
*/


// Handle MQTT events.
// Related docs:
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__evt.html
// - https://docs.zephyrproject.org/latest/doxygen/html/unionmqtt__evt__param.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__publish__param.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__publish__message.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__topic.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__binstr.html
//
static void mq_handler(struct mqtt_client *client, const struct mqtt_evt *e) {
	switch (e->type) {
	case MQTT_EVT_CONNACK:
		printk("CONNACK\n");
		// Update state telling the event loop to register subscription
		ZCtx.state = CONNACK;
		break;
	case MQTT_EVT_DISCONNECT:
		printk("DISCONNECT\n");
		ZCtx.state = MQTT_DOWN;
		ZCtx.toggle = UNKNOWN;   // because we're no longer subscribed
		break;
	case MQTT_EVT_PUBLISH:
		// This happens when the broker informs us that somebody published a
		// message to a topic we have subscribed to. The parameter, e->param,
		// will be of type mqtt_publish_param.
		printk("PUBLISH\n");
		// First extract topic from the param struct and check for match
		const struct mqtt_publish_message *m = &e->param.publish.message;
		const uint8_t *topic = m->topic.topic.utf8;
		uint32_t t_len = m->topic.topic.size;
		if (t_len >= sizeof(ZCtx.topic)
			|| memcmp(topic, ZCtx.topic, t_len)
		) {
			printk("ignoring unknown topic (len = %d)\n", t_len);
			return;
		}
		// Second, get the payload data and parse it.
		// CAUTION: You can't get the payload data from the payload struct.
		// Instead you have to call this function (see mqtt_evt_type docs).
		uint8_t buf[32] = {0};
		uint32_t payload_len = m->payload.len;
		int count = mqtt_read_publish_payload(&Ctx, buf, sizeof(buf));
		if (count != 1 || count != payload_len) {
			printk("ERR: unexpected payload length: %d\n", count);
			return;
		}
		// Parse payload data for expected messages: "1" or "0"
		switch((char)buf[0]) {
		case '0':
			printk("PUB GOT 0\n");
			ZCtx.got_0 = true;
			break;
		case '1':
			printk("PUB GOT 1\n");
			ZCtx.got_1 = true;
			break;
		default:
			printk("PUB GOT unknown value\n");
		}
		break;
	case MQTT_EVT_SUBACK:
		printk("SUBACK\n");
		// Update state telling event loop to publish to /get topic modifier
		ZCtx.state = SUBACK;
		break;
	case MQTT_EVT_PINGRESP:
		printk("PINGRESP\n");
		break;
	default:
		break;
	}
}


/*
* AIO SHELL COMMANDS
*/


// Handle the aio conf shell command by parsing and saving the URL.
// Main point of this is to copy username, password, and host, and topic
// strings into buffers that won't get deallocated before they are needed.
//   usage: broker mqtt[s]://[<user>]:[<key>]@<host>/<topic>
//
static int cmd_conf(const struct shell *shell, size_t argc, char *argv[]) {
	// Avoid dereferencing null pointers if URL argument is missing
	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		printk("ERROR: expected a URL\n");
		return 1;
	}
	// Avoid processing a string that is unterminated or too long. The point
	// of this is to guard against weird edge cases that could cause strchr()
	// to misbehave. If we pass this check, using strchr() should be fine.
	const char *url = argv[1];
	if (!memchr(url, '\0', ZQ3_URL_MAX_LEN)) {
		printk("ERROR: URL is too long\n");
		return 2;
	}

	// Parse the URL into fields of the ZCtx struct
	int err = zq3_url_parse(&ZCtx, url);
	if (err) {
		printk("URL PARSE ERR: %d\n", err);
		return err;
	}

	// Success
	ZCtx.valid = true;
	zq3_url_print_conf(&ZCtx);

	// Update string lengths in MQTT context (see main() inits section)
	if(Ctx.password != NULL) {
		Ctx.password->size = strlen(Ctx.password->utf8);
	}
	if(Ctx.user_name != NULL) {
		Ctx.user_name->size = strlen(Ctx.user_name->utf8);
	}

	return 0;
}

static int cmd_connect(const struct shell *shell, size_t argc, char *argv[]) {
	int err = zq3_mqtt_connect(&ZCtx, &Ctx);
	if (err) {
		if (ZCtx.state >= CONNWAIT) {
			printk("[MQTT_ERR]\n");
			ZCtx.state = MQTT_ERR;
		}
		return err;
	}
	printk("[CONWAIT]\n");
	ZCtx.state = CONNWAIT;
	return 0;
}

static int
cmd_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	int err = zq3_mqtt_disconnect(&Ctx);
	if (err) {
		if (ZCtx.state >= CONNWAIT) {
			printk("[MQTT_ERR]\n");
			ZCtx.state = MQTT_ERR;
		}
		return err;
	}
	printk("[MQTT_DOWN]\n");
	ZCtx.state = MQTT_DOWN;
	return 0;
}


/*
* EVENT CALLBACKS FOR LVGL AND NETWORKING
*/

// Callback to handle keypad input events
static void pressed_callback(lv_event_t *event) {
	printk("KEYPAD CALLBACK\n");
	ZCtx.btn1_clicked = true;
}

// Handle network manager events (wifi up / wifi down)
static void net_callback(struct net_mgmt_event_callback *cb,
	uint32_t mgmt_event, struct net_if *iface)
{
	if(mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		printk("[WIFI_UP]\n");
		ZCtx.wifi_up = true;
	} else if(mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		printk("[WIFI_DOWN]\n");
		ZCtx.wifi_up = false;
	} else {
		printk("net: unknown event\n");
	}
}


/*
* SHELL SUBCOMMAND ARRAY MACROS
*/

// These macros add the `aio *` shell commands for configuring and testing the
// MQTT broker in the Zephyr shell over USB serial.  Combined with the `wifi
// connect` shell command, this makes it unnecessary to hardcode network
// authentication secrets.
//
SHELL_STATIC_SUBCMD_SET_CREATE(aio_cmds,
	SHELL_CMD_ARG(conf, NULL, "Configure MQTT broker and topic with URL\n"
		"Usage: conf mqtt[s]://[<user>]:[<key>]@<host>/<topic>\n"
		"mqtt:// uses port 1883 and mqtts:// uses 8883.\n"
		"Examples:\n"
		"conf mqtt://:@192.168.0.50/test\n"
		"conf mqtts://Blinka:aio_1234@io.adafruit.com/Blinka/f/test",
		cmd_conf, 2, 0),
	SHELL_CMD(connect, NULL, "Connect to broker", cmd_connect),
	SHELL_CMD(disconnect, NULL, "Disconnect from broker", cmd_disconnect),
	SHELL_SUBCMD_SET_END
);


/*
* LVGL UTIL FUNCTIONS
*/

static void hide_lv_widget(lv_obj_t *obj) {
	lv_obj_add_state(obj, LV_STATE_DISABLED);
	lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void show_lv_widget(lv_obj_t *obj) {
	lv_obj_clear_state(obj, LV_STATE_DISABLED);
	lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
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
	SHELL_CMD_REGISTER(aio, &aio_cmds, "AdafruitIO MQTT commands", NULL);

	// MQTT Init
	mqtt_client_init(&Ctx);
	uint8_t MQRxBuf[256];
	uint8_t MQTxBuf[256];
	struct mqtt_utf8 pass = {(uint8_t *)ZCtx.pass, strlen(ZCtx.pass)};
	struct mqtt_utf8 user = {(uint8_t *)ZCtx.user, strlen(ZCtx.user)};
	Ctx.broker = &AIOBroker;
	Ctx.evt_cb = mq_handler;
	Ctx.client_id.utf8 = (uint8_t *)"id";  // protocol requires non-empty id
	Ctx.client_id.size = strlen("id");
	Ctx.password = &pass;
	Ctx.user_name = &user;
	Ctx.protocol_version = MQTT_VERSION_3_1_1;
	Ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;  // TODO: use TLS
	Ctx.rx_buf = MQRxBuf;
	Ctx.rx_buf_size = sizeof(MQRxBuf);
	Ctx.tx_buf = MQTxBuf;
	Ctx.tx_buf_size = sizeof(MQTxBuf);
	// TODO: add TLS support
	// Ctx.transport.type = MQTT_TRANSPORT_SECURE;
	// struct mqtt_sec_config *conf = &Ctx.transport.tls.config;
	// conf->cipher_list = NULL;
	// conf->hostname = MQTT_BROKER_HOSTNAME;

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
	lv_obj_align(wifiLabel, LV_ALIGN_TOP_RIGHT, -10, 5);
	lv_obj_set_style_text_color(wifiLabel, wifiColorDown, 0);

	// Make "Waiting for MQTT Connect" label
	lv_obj_t *waiting = lv_label_create(lv_screen_active());
	lv_label_set_text(waiting, "Waiting for\nMQTT Connect\n(check shell)");
	lv_obj_set_style_text_align(waiting, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(waiting, wifiColorUp, 0);
	lv_obj_center(waiting);

	// Make toggle switch widget (gets added to keypad group later)
	lv_obj_t *toggle = lv_switch_create(lv_screen_active());
	lv_obj_set_size(toggle, 120, 60);
	lv_obj_center(toggle);

	// Set up input group so keypad press events go to the active screen. This
	// arrangement (vs. putting pressed event on the switch) lets the main
	// event loop consider both keypad presses and received MQTT messages to
	// decide what the toggle switch's CHECKED state should be. For this to
	// work, the devicetree config must provide a keypad with physical buttons
	// mapped to navigation keys such as LV_KEY_LEFT, LV_KEY_RIGHT, and
	// LV_KEY_ENTER. Minimum is LV_KEY_ENTER.
	lv_obj_t *screen = lv_screen_active();
	lv_group_t *grp = lv_group_create();
	lv_group_add_obj(grp, screen);
	lv_indev_set_group(lvgl_input_get_indev(keypad), grp);
	lv_obj_add_event_cb(screen, pressed_callback, LV_EVENT_PRESSED, NULL);

	// Start with toggle hidden and disabled (until mqtt connection is up)
	// CAUTION! Widget must be added to keypad input group *before* setting it
	// as disabled. Otherwise, re-enabling the widget later won't work.
	hide_lv_widget(toggle);

	// Register to get updates about wifi connection status
	net_mgmt_init_event_callback(&net_status, net_callback,
		NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&net_status);

	// Event loop
	bool prev_wifi_up = false;
	zq3_state prev_state = ZCtx.state;
	zq3_toggle prev_toggle = ZCtx.toggle;
	lv_timer_handler();
	display_blanking_off(display);
	while(1) {
		// Update MQTT connection status if wifi connection went down
		if ((ZCtx.state >= CONNWAIT) && !ZCtx.wifi_up) {
			mqtt_abort(&Ctx);
			printk("[MQTT_DOWN]\n");
			ZCtx.state = MQTT_DOWN;
		}

		// Set status bar wifi icon color when wifi connection changes
		if (prev_wifi_up != ZCtx.wifi_up) {
			if (ZCtx.wifi_up) {
				lv_obj_set_style_text_color(wifiLabel, wifiColorUp, 0);
			} else {
				lv_obj_set_style_text_color(wifiLabel, wifiColorDown, 0);
				hide_lv_widget(toggle);
				show_lv_widget(waiting);
			}
			prev_wifi_up = ZCtx.wifi_up;
		}

		// Set toggle switch visibility when MQTT connection changes
		if (ZCtx.wifi_up && (prev_state != ZCtx.state)) {
			prev_state = ZCtx.state;
			if (ZCtx.state == READY) {
				hide_lv_widget(waiting);
				show_lv_widget(toggle);
			} else {
				hide_lv_widget(toggle);
				show_lv_widget(waiting);
			}
		}

		// Maintain MQTT connection (note: poll() requires CONFIG_POSIX_API=y)
		if (ZCtx.state >= CONNWAIT) {
			// Respond to incoming MQTT messages if needed
			if (poll(ZCtx.fds, 1, 0) > 0) {
				mqtt_input(&Ctx);
			}

			// Send an MQTT PINGREQ ping several seconds before the keepalive
			// timer is due to run out. This keeps the TCP connection to the
			// MQTT broker open so it can send us messages on subscribed topics
			// as they are published.
			int remaining_ms = mqtt_keepalive_time_left(&Ctx);
			if (remaining_ms < 5000) {
				mqtt_live(&Ctx);
			}

			// Part of the state machine for bringing up full MQTT connection
			int err = 0;
			switch(ZCtx.state) {
			case CONNACK:
				// Subscribe to topic as soon as MQTT connection is up
				err = zq3_mqtt_subscribe(&ZCtx, &Ctx);
				if (err) {
					printk("[MQTT_ERR]\n");
					ZCtx.state = MQTT_ERR;
				} else {
					printk("[SUBWAIT]\n");
					ZCtx.state = SUBWAIT;
				}
				break;
			case SUBACK:
				// Publish to the topic's /get topic modifier as soon as the
				// subscription has been ACK'd. This is meant to work with
				// AdafruitIO which does not support normal MQTT retain.
				err = zq3_mqtt_publish_get(&ZCtx, &Ctx);
				if (err) {
					printk("[MQTT_ERR]\n");
					ZCtx.state = MQTT_ERR;
				} else {
					printk("[READY]\n");
					ZCtx.state = READY;
				}
				break;
			default:
				/* NOP */
			}
		}

		// Respond to keypad press callback (do we need to PUBLISH a message?)
		// CAUTION! This needs to happen before the check to update the toggle
		// switch widget.
		if (ZCtx.btn1_clicked && ZCtx.state == READY) {
			ZCtx.btn1_clicked = false;
			// This evaluates to true when .toggle is UNKNOWN or OFF
			bool state = ZCtx.toggle != ON;
			ZCtx.toggle = state ? ON : OFF;
			printk("KEYPRESS: publishing toggle = %d\n", state ? 1 : 0);
			zq3_mqtt_publish(&ZCtx, &Ctx, state);
		} else if (ZCtx.btn1_clicked && ZCtx.state != READY) {
			ZCtx.btn1_clicked = false;
			printk("Keypad pressed but MQTT connection is not READY\n");
		}

		// Check if MQTT message requested a change to the toggle state
		if (ZCtx.got_0) {
			ZCtx.got_0 = false;
			ZCtx.toggle = OFF;
		}
		if (ZCtx.got_1) {
			ZCtx.got_1 = false;
			ZCtx.toggle = ON;
		}

		// Update toggle button widget if toggle stated has changed
		if (prev_toggle != ZCtx.toggle) {
			prev_toggle = ZCtx.toggle;
			switch(ZCtx.toggle) {
			case UNKNOWN:
				/* NOP */
				break;
			case OFF:
				lv_obj_clear_state(toggle, LV_STATE_CHECKED);
				break;
			case ON:
				lv_obj_add_state(toggle, LV_STATE_CHECKED);
				break;
			}
		}

		// Call LVGL then sleep until time for the next tick
		uint32_t holdoff_ms = lv_timer_handler();
		k_msleep(holdoff_ms);
	}
}
