/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wifi + Network Manager Docs & Refs:
 * https://docs.zephyrproject.org/latest/doxygen/html/group__net__mgmt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structnet__mgmt__event__callback.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/l2/wifi/wifi_shell.c
 */

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>     // NET_EVENT_WIFI_CONNECT_RESULT, ...
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include "zq3.h"
#include "zq3_mqtt.h"
#include "zq3_lvgl.h"
#include "zq3_url.h"
#include "zq3_wifi.h"


/*
* STATIC GLOBALS AND CONSTANTS
*/

// Context for wifi status, mqtt config, and mqtt status
static zq3_context ZCtx = {
	.ssid = {'\0'},
	.psk =  {'\0'},
	.user = {'\0'},
	.pass = {'\0'},
	.host = {'\0'},
	.topic = {'\0'},
	.tls = true,
	.mqtt_ok = false,
	.state = OFFLINE,
	.keypress = false,
	.got_0 = false,
	.got_1 = false,
	.toggle = UNKNOWN,
};

struct sockaddr_storage AIOBroker;

// MQTT context struct
static struct mqtt_client Ctx;


/*
* NETWORK EVENT HANDLERS
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
		ZCtx.state = CONNACK;
		break;
	case MQTT_EVT_DISCONNECT:
		printk("DISCONNECT\n");
		ZCtx.state = MQTT_ERR;
		ZCtx.toggle = UNKNOWN;   // because we're no longer subscribed
		break;
	case MQTT_EVT_PUBLISH:
		// This happens when the broker informs us that somebody published a
		// message to a topic we have subscribed to. The parameter, e->param,
		// will be of type mqtt_publish_param.

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
		ZCtx.state = SUBACK;
		break;
	case MQTT_EVT_PINGRESP:
		// This can be useful, but it's noisy
		//printk("PINGRESP\n");
		break;
	default:
		break;
	}
}

// Handle network manager events (wifi up / wifi down)
static void net_callback(
	struct net_mgmt_event_callback *cb,
	uint32_t mgmt_event,
	struct net_if *iface
) {
	switch(mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		printk("NET_EVENT_WIFI_CONNECT_RESULT\n");
		ZCtx.state = WIFI_UP;
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		printk("NET_EVENT_WIFI_DISCONNECT_RESULT\n");
		ZCtx.state = WIFI_ERR;
		break;
	default:
		printk("net: unknown event\n");
	}
}


/*
* SHELL COMMANDS
*/

// Connect to Wifi
static int cmd_wifi_up(const struct shell *shell, size_t argc, char *argv[]) {
	return zq3_wifi_connect(ZCtx.ssid, ZCtx.psk);
}

// Disconnect from Wifi
static int cmd_wifi_dn(const struct shell *shell, size_t argc, char *argv[]) {
	return zq3_wifi_disconnect();
}

// Connect to MQTT broker
static int cmd_up(const struct shell *shell, size_t argc, char *argv[]) {
	int err = zq3_mqtt_connect(&ZCtx, &Ctx);
	if (err) {
		ZCtx.state = MQTT_ERR;
		return err;
	}
	ZCtx.state = CONNWAIT;
	return 0;
}

// Disconnect from MQTT broker
static int cmd_dn(const struct shell *shell, size_t argc, char *argv[]) {
	int err = zq3_mqtt_disconnect(&Ctx);
	if (err) {
		ZCtx.state = MQTT_ERR;
		return err;
	}
	ZCtx.state = MQTT_ERR;  // CAUTION: WIFI_UP would trigger a reconnect
	return 0;
}

// Reload settings. (you can use this after `settings write ...`)
static int cmd_reload(const struct shell *shell, size_t argc, char *argv[]) {
	// Clear wifi and MQTT settings from context struct
	memset(ZCtx.ssid, 0, sizeof(ZCtx.ssid));
	memset(ZCtx.psk, 0, sizeof(ZCtx.psk));
	memset(ZCtx.user, 0, sizeof(ZCtx.user));
	memset(ZCtx.pass, 0, sizeof(ZCtx.pass));
	memset(ZCtx.host, 0, sizeof(ZCtx.host));
	memset(ZCtx.topic, 0, sizeof(ZCtx.topic));
	ZCtx.tls = false;
	ZCtx.mqtt_ok = false;
	// Load saved settings
	return settings_load();
}

// These macros add the `aio *` shell commands for controlling the MQTT broker
// connection in the Zephyr shell over USB serial. MQTT broker config gets
// read from 'zq3/url' setting stored in NVM flash (`settings write ...`).
//
SHELL_STATIC_SUBCMD_SET_CREATE(aio_cmds,
	SHELL_CMD(wifi_up, NULL, "Wifi connect", cmd_wifi_up),
	SHELL_CMD(wifi_dn, NULL, "Wifi disconnect", cmd_wifi_dn),
	SHELL_CMD(up, NULL, "AIO MQTT broker connect", cmd_up),
	SHELL_CMD(dn, NULL, "AIO MQTT broker disconnect", cmd_dn),
	SHELL_CMD(reload, NULL, "Reload settings", cmd_reload),
	SHELL_SUBCMD_SET_END
);


/*
* SETTINGS READING CALLBACKS
* To write settings to flash, use the Zephyr shell `settings` command.
* Docs:
* - https://docs.zephyrproject.org/latest/services/storage/settings/index.html
* - https://docs.zephyrproject.org/latest/doxygen/html/group__settings.html
*/

// This callback runs each time settings_load() reads a key from the NVM flash
// backend which matches the configured prefix. The API docs confusingly refer
// to this as "set" callback. The semantics seem like what I'd expect from a
// getter. But, whatever. To read the saved values of keys, this works.
//
static int
set_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	char buf[256];
	if (len >= sizeof(buf)) {
		printk("setting value for key '%s' is too big: %d\n", key, len);
		return -EMSGSIZE;
	}
	int rc = read_cb(cb_arg, buf, sizeof(buf));
	if (rc < 0) {
		printk("ERR: settings read_cb(%s) = %d\n", key, rc);
		return rc;
	}
	buf[sizeof(buf)-1] = '\0';  // make sure string is null terminated
	int vlen = strlen(buf);
	if (strcmp("url", key) == 0) {
		// Parse MQTT URL and save components to context struct
		if (vlen >= ZQ3_URL_MAX_LEN) {
			printk("ERR: setting for '%s' is too long: %d\n", key, vlen);
			return -EOVERFLOW;
		}
		int err = zq3_url_parse(&ZCtx, buf);
		if (err) {
			printk("ERR settings: failed to parse url: %d\n", err);
			return err;
		}
		// Success
		ZCtx.mqtt_ok = true;
		// Update string lengths in MQTT context (see main() inits section)
		if(Ctx.password != NULL) {
			Ctx.password->size = strlen(Ctx.password->utf8);
		}
		if(Ctx.user_name != NULL) {
			Ctx.user_name->size = strlen(Ctx.user_name->utf8);
		}
	} else if (strcmp("ssid", key) == 0) {
		// Save Wifi SSID to context struct
		if (vlen >= sizeof(ZCtx.ssid)) {
			printk("ERR: setting for '%s' is too long: %d\n", key, vlen);
			return -EOVERFLOW;
		}
		memset(ZCtx.ssid, 0, sizeof(ZCtx.ssid));
		memcpy(ZCtx.ssid, buf, vlen);
	} else if (strcmp("psk", key) == 0) {
		// Save Wifi passphrase to context struct
		if (vlen >= sizeof(ZCtx.psk)) {
			printk("ERR: setting for '%s' is too long: %d\n", key, vlen);
			return -EOVERFLOW;
		}
		memset(ZCtx.psk, 0, sizeof(ZCtx.psk));
		memcpy(ZCtx.psk, buf, vlen);
	}
	printk("Settings SET: '%s'\n", key);
	return 0;
}

// This macro configures the callbacks to be invoked by settings_load()
SETTINGS_STATIC_HANDLER_DEFINE(zq3, "zq3", NULL, set_cb, NULL, NULL);


/*
* KEYPAD BUTTON PRESS CALLBACK
*/

// Callback to handle keypad input events
static void keypad_pressed_callback(lv_event_t *event) {
	ZCtx.keypress = true;
}


/*
* MAIN
*/

int main(void) {
	// Inits
	struct net_mgmt_event_callback net_status;
	zq3_lvgl_context LCtx;
	zq3_lvgl_init(&LCtx, keypad_pressed_callback);
	SHELL_CMD_REGISTER(aio, &aio_cmds, "Adafruit IO MQTT commands", NULL);
	settings_subsys_init();
	printk("Loading Settings\n");
	settings_load();

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

	// Register to get updates about wifi connection status
	net_mgmt_init_event_callback(&net_status, net_callback,
		NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&net_status);

	// Event loop
	zq3_state prev_state = ZCtx.state;
	zq3_toggle prev_toggle = ZCtx.toggle;
	zq3_lvgl_timer_handler();
	const char *offline_message = "Press\nBOOT button\nto connect";
	zq3_lvgl_show_message(&LCtx, offline_message);
	while(1) {
		int err;
		// Update GUI when Wifi/MQTT connection state changes
		if (prev_state != ZCtx.state) {
			prev_state = ZCtx.state;
			switch(ZCtx.state) {
			case OFFLINE:
				// This happens when wifi disconnects for some reason
				printk("[OFFLINE]\n");
				zq3_lvgl_wifi_status(&LCtx, false);
				zq3_lvgl_show_message(&LCtx, offline_message);
				break;
			case WIFI_ERR:
				printk("[WIFI_ERR]\n");
				zq3_lvgl_wifi_status(&LCtx, false);
				zq3_lvgl_show_message(&LCtx, "Wifi Error\n(check settings)");
				break;
			case WIFIWAIT:
				printk("[WIFIWAIT]\n");
				zq3_lvgl_show_message(&LCtx, "Connecting...");
				break;
			case WIFI_UP:
				// Light up the wifi icon in the statusbar
				printk("[WIFI_UP]\n");
				zq3_lvgl_wifi_status(&LCtx, true);
				// Attempt to connect to the MQTT broker (once)
				int err = zq3_mqtt_connect(&ZCtx, &Ctx);
				if (err) {
					ZCtx.state = MQTT_ERR;
				} else {
					ZCtx.state = CONNWAIT;
				}
				break;
			case MQTT_ERR:
				// Problem with MQTT settings, broker unreachable, etc.
				printk("[MQTT_ERR]\n");
				zq3_lvgl_show_message(&LCtx, "MQTT Error\n(check settings)");
				break;
			case CONNWAIT:
				// MQTT is connecting... just wait silently
				printk("[CONWAIT]\n");
				break;
			case CONNACK:
				// MQTT connected, so subscribe to topic
				printk("[CONNACK]\n");
				err = zq3_mqtt_subscribe(&ZCtx, &Ctx);
				if (err) {
					ZCtx.state = MQTT_ERR;
				} else {
					ZCtx.state = SUBWAIT;
				}
				break;
			case SUBWAIT:
				printk("[SUBWAIT]\n");
				break;
			case SUBACK:
				printk("[SUBACK]\n");
				ZCtx.state = READY;
				break;
			case READY:
				printk("[READY]\n");

				// Reset toggle switch state to UNKNOWN/not-checked. It would
				// be possible to ask the broker for the topic's old value, but
				// I'm skipping that to keep the code simpler.
				ZCtx.toggle = UNKNOWN;
				zq3_lvgl_set_toggle(&LCtx, false);

				// Show the toggle switch in place of the status message
				zq3_lvgl_show_toggle(&LCtx);
				break;
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
		}

		// If keypad press callback set the button press flag, handle it
		if (ZCtx.keypress) {
			ZCtx.keypress = false;
			switch(ZCtx.state) {
			case WIFI_ERR:
				// Retry from error state (maybe after changing settings)...
				// fall through to the OFFLINE case
			case OFFLINE:
				// Attempt to start a wifi connection
				printk("starting wifi connection\n");
				err = zq3_wifi_connect(ZCtx.ssid, ZCtx.psk);
				if (err) {
					ZCtx.state = WIFI_ERR;
					printk("ERR: wifi connect: %d\n", err);
				} else {
					ZCtx.state = WIFIWAIT;
				}
				break;
			case MQTT_ERR:
				// Trigger an MQTT connection retry attempt (maybe after
				// changing settings, fixing the MQTT broker, or whatever)
				ZCtx.state = WIFI_UP;
				break;
			case READY:
				// MQTT is up and ready: key press means toggle the switch
				// and publish its new value.
				//
				// This will apply the following transformations to .toggle:
				//   UKNOWN becomes ON
				//   OFF    becomes ON
				//   ON     becomes OFF
				bool new_state = ZCtx.toggle != ON;
				ZCtx.toggle = new_state ? ON : OFF;
				printk("Publishing toggle state: %d\n", new_state ? 1 : 0);
				err = zq3_mqtt_publish(&ZCtx, &Ctx, new_state);
				if (err) {
					ZCtx.state = MQTT_ERR;
				}
				break;
			default:
				printk("Keypad pressed (NOP)\n");
			}
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
				zq3_lvgl_set_toggle(&LCtx, false);
				break;
			case ON:
				zq3_lvgl_set_toggle(&LCtx, true);
				break;
			}
		}

		// Call LVGL then sleep until time for the next tick
		uint32_t holdoff_ms = zq3_lvgl_timer_handler();
		k_msleep(holdoff_ms);
	}
}
