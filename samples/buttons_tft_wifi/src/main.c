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
 * DNS & Socket Docs & Refs:
 * https://docs.zephyrproject.org/apidoc/latest/group__bsd__sockets.html
 * https://docs.zephyrproject.org/apidoc/latest/netdb_8h.html (getaddrinfo / freeaddrinfo)
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/net/dns_resolve.h (error codes)
 * https://docs.zephyrproject.org/apidoc/latest/group__ip__4__6.html (net_addr_ntop)
 *
 * AIO + MQTT Docs & Refs:
 * https://io.adafruit.com/api/docs/mqtt.html#adafruit-io-mqtt-api
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/mqtt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__evt.html
 * https://docs.zephyrproject.org/latest/doxygen/html/group__mqtt__socket.html
 * https://docs.zephyrproject.org/apidoc/latest/structsockaddr.html
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/sockets.html#secure-sockets-interface
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/net/socket.h
 *
 * AdafruitIO MQTT Settings:
 * host:port: io.adafruit.com:8883
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <errno.h>
#include <lvgl.h>
#include <lvgl_input_device.h>
#include <stdio.h>
#include <string.h>


/*
* STATIC GLOBALS AND CONSTANTS
*/

// Flag for communication between net_callback and main about wifi status
static int WifiUp = 0;

// AdafruitIO MQTT broker auth credentials, hostname, topic, and scheme
typedef struct {
	char user[48];
	char pass[48];
	char host[48];
	char topic[48];
	bool tls;
	bool valid;
} aio_conf_t;
static aio_conf_t AIOConf = {{'\0'}, {'\0'}, {'\0'}, {'\0'}, true, false};

struct sockaddr_storage AIOBroker;

#define AIO_URL_MAX_LEN (sizeof("mqtts://:@") + \
	sizeof(((aio_conf_t *)0)->user) + sizeof(((aio_conf_t *)0)->pass) + \
	sizeof(((aio_conf_t *)0)->host) + sizeof(((aio_conf_t *)0)->topic))

#define AIO_MSG_MAX_LEN (128)

// MQTT connection status
typedef struct {
	bool connected;
	bool needs_subscription;
	bool needs_get;
} aio_status_t;
static aio_status_t AIOStatus = {false, false, false};

// MQTT Buffers and context structs
static uint8_t MQRxBuf[256];
static uint8_t MQTxBuf[256];
static struct mqtt_client Ctx;
// For the fds file descriptor array, the examples in Zephyr project MQTT docs
// use the pollfd type and the poll() function, both of which gave me compiler
// errors until I figured out I needed CONFIG_POSIX_API=y. Note that docs may
// refer to zsock_pollfd and zsock_poll(). See zephyr/net/socket.h.
static struct pollfd fds[1];


/*
* MISC MQTT STUFF
*/

// Once MQTT connection is up, event loop calls this to start subscription.
// This is hardcoded to subscribe to the one topic specified in the url that
// gets parsed by the `aio conf` shell command.
//
static int register_subscription() {
	AIOStatus.needs_subscription = false;
	if (!AIOStatus.connected) {
		printk("ERR: can't subscribe because not connected.\n");
		return 1;
	}
	// This is using a C99 feature called a compound literals to initialize a
	// nested struct. Related docs:
	// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__subscription__list.html
	// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__topic.html
	// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__utf8.html
	//
	struct mqtt_topic topic = {
		.topic = (struct mqtt_utf8){
			.utf8 = (uint8_t *)AIOConf.topic,
			.size = strlen(AIOConf.topic)
		},
		.qos = MQTT_QOS_0_AT_MOST_ONCE
	};
	struct mqtt_subscription_list list = {
		.list = &topic,
		.list_count = 1,
		.message_id = 1
	};
	// Send the subscription request
	int err = mqtt_subscribe(&Ctx, &list);
	printk("mqtt_subscribe() = %d\n", err);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		switch(-err) {
		default:
			printk("ERR: mqtt_subscribe() = %d\n", err);
		}
		return err;
	}
	return 0;
}

// Publish to the topic's /get topic modifier in the styel used by AdafruitIO.
// This mechanism is an alternative to the normal MQTT retain feature.
static int publish_get() {
	// TODO: implement this
	return 0;
}

// Handle MQTT events
static void mq_handler(struct mqtt_client *client, const struct mqtt_evt *e) {
	switch (e->type) {
	case MQTT_EVT_CONNACK:
		printk("CONNACK\n");
		// Set flag telling the event loop to register subscription
		AIOStatus.needs_subscription = true;
		break;
	case MQTT_EVT_DISCONNECT:
		printk("DISCONNECT\n");
		AIOStatus.connected = false;
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
		// Set flag telling event loop to publish to /get topic modifier
		AIOStatus.needs_get = true;
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
* AIO SHELL COMMANDS
*/

// Check that the config was parsed well
static void print_AIOConf() {
	printk(
		"AIO Config:\n"
		"  user:  '%s'\n"
		"  pass:  '%s'\n"
		"  host:  '%s'\n"
		"  topic: '%s'\n"
		"  tls:   %s\n"
		"  valid: %s\n",
		AIOConf.user, AIOConf.pass, AIOConf.host, AIOConf.topic,
		AIOConf.tls ? "true" : "false", AIOConf.valid ? "true" : "false"
	);
}

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
	const char *cursor = url;
	if (!memchr(url, '\0', AIO_URL_MAX_LEN)) {
		printk("ERROR: URL is too long\n");
		return 2;
	}

	// Clear config struct and begin normal parsing
	memset(AIOConf.user, 0, sizeof(AIOConf.user));
	memset(AIOConf.pass, 0, sizeof(AIOConf.pass));
	memset(AIOConf.host, 0, sizeof(AIOConf.host));
	memset(AIOConf.topic, 0, sizeof(AIOConf.topic));
	AIOConf.tls = false;
	AIOConf.valid = false;

	// Parse URL scheme prefix (should be "mqtt://" or "mqtts://")
	char *tls_scheme = "mqtts://";
	char *nontls_scheme = "mqtt://";
	if (strncmp(url, tls_scheme, strlen(tls_scheme)) == 0) {
		// Scheme = MQTT with TLS
		cursor += strlen(tls_scheme);
		AIOConf.tls = true;
	} else if (strncmp(url, nontls_scheme, strlen(nontls_scheme)) == 0) {
		// Scheme = unencrypted MQTT
		cursor += strlen(nontls_scheme);
		AIOConf.tls = false;
	} else {
		printk("ERROR: expected mqtt:// or mqtts://\n");
		return 3;
	}

	// Parse username (whatever is between end of scheme and the next ':')
	char *delim = strchr((char *)cursor, ':');
	int len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing ':' after username\n");
		return 4;
	} else if (len >= sizeof(AIOConf.user)) {
		printk("ERR: username too long\n");
		return 5;
	} else {
		// Copy username string (relies on len < sizeof(...))
		memcpy(AIOConf.user, cursor, len);
		cursor = delim + 1;
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
		// Copy password string (relies on len < sizeof(...))
		memcpy(AIOConf.pass, cursor, len);
		cursor = delim + 1;
	}

	// Parse host (whatever is between '@' and '/')
	delim = strchr((char *)cursor, '/');
	len = delim - cursor;
	if (!delim) {
		printk("ERR: missing '/' after hostname\n");
		return 8;
	} else if (len < 1) {
		printk("ERR: hostname can't be blank\n");
		return 9;
	} else if (len >= sizeof(AIOConf.host)) {
		printk("ERR: hostname too long\n");
		return 10;
	} else {
		// Copy hostname string (relies on len < sizeof(...))
		memcpy(AIOConf.host, cursor, len);
		cursor = delim + 1;
	}

	// Parse topic (whatever is after '/')
	len = strlen(cursor);
	if (len <= 0) {
		printk("ERR: topic can't be blank\n");
		return 11;
	} else if (len >= sizeof(AIOConf.topic)) {
		printk("ERR: topic too long\n");
		return 12;
	} else {
		// Copy topic string (relies on len < sizeof(...))
		memcpy(AIOConf.topic, cursor, len);
	}

	// Resolve hostname to IPv4 IP (IPv6 not supported)
	// This requires CONFIG_POSIX_API=y and CONFIG_NET_SOCKETS_POSIX_NAMES=y.
	struct addrinfo *res= NULL;
	const struct addrinfo hint = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	const char * service = AIOConf.tls ? "8883" : "1883";
	int err = getaddrinfo(AIOConf.host, service, &hint, &res);
	if (err) {
		printk("check DNS_EAI_SYSTEM = %d\n", DNS_EAI_SYSTEM);
		switch(err) {
		case DNS_EAI_SYSTEM:
			printk("ERR: DNS_EAI_SYSTEM: Is wifi connected?\n");
			break;
		default:
			// Look for enum with EAI_* in include/zephyr/net/dns_resolve.h
			printk("ERR: DNS fail %d\n", err);
		}
	} else if (!res || !(res->ai_addr) || (res->ai_family != AF_INET)) {
		// This shouldn't happen, but check anyway because null pointer
		// dereference errors are no fun.
		printk("ERR: DNS result struct was damaged\n");
		printk("res: %p\n"
			" .ai_addr: %p\n"
			" .ai_family: %d\n",
			res,
			res ? res->ai_addr : NULL,
			res ? res->ai_family : -1
		);
	} else {
		// At this point, we can trust that res and res->ai_addr are not NULL
		// and that the result is an IPv4 address.

		// Copy IPv4 address from DNS lookup (src) to broker struct (dst)
		//
		// DANGER! This uses weird pointer casting because that's how the
		// socket API expects you to handle the possibility that a DNS lookup
		// can resolve to either or both of IPv4 and IPv6 addresses.
		//
		// CAUTION: This ignores the possibility of more than one IP address
		// and just uses the address from the first result.
		//
		struct sockaddr_in *src = (struct sockaddr_in *)res->ai_addr;
		struct sockaddr_in *dst = (struct sockaddr_in *)&AIOBroker;
		dst->sin_family = AF_INET;
		dst->sin_addr.s_addr = src->sin_addr.s_addr;
		// Set TLS or non-TLS port with network byte order conversion function
		dst->sin_port = htons(AIOConf.tls ? 8883 : 1883);

		// Debug print the DNS lookup result
		char ip_str[INET_ADDRSTRLEN];  // max length IPv4 address string
		net_addr_ntop(AF_INET, &dst->sin_addr, ip_str, sizeof(ip_str));
		printk("DNS IPv4 result: %s\n", ip_str);
	}
	// IMPORTANT: free getaddrinfo() result to avoid memory leak
	freeaddrinfo(res);

	// Success
	AIOConf.valid = true;
	print_AIOConf();
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
	if (!AIOConf.valid) {
		printk("ERR: AIO is not configured (try 'aio conf ...')\n");
		return 1;
	}
	int err = mqtt_connect(&Ctx);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		switch(-err) {
		case EISCONN:
			printk("ERR: Socket already connected\n");
			break;
		case EAFNOSUPPORT:
			printk("ERR: Addr family not supported\n");
			break;
		case ECONNREFUSED:
			printk("ERR: Connection refused\n");
			break;
		case ETIMEDOUT:
			printk("ERR: Connection timed out\n");
			break;
		default:
			printk("ERR: %d\n", err);
		}
		return err;
	}
	fds[0].fd = Ctx.transport.tcp.sock;
	fds[0].events = ZSOCK_POLLIN;

	AIOStatus.connected = true;
	return 0;
}

static int
cmd_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	AIOStatus.connected = false;
	int err = mqtt_disconnect(&Ctx);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		switch(-err) {
		case ENOTCONN:
			printk("ERR: Socket was not connected\n");
		default:
			printk("ERR: %d\n", err);
		}
		return err;
	}
	return 0;
}

static int cmd_pub(const struct shell *shell, size_t argc, char *argv[]) {
	printk("cmd_pub\n"); // TODO: zap
	// Avoid dereferencing null pointers if URL argument is missing
	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		printk("ERROR: expected a message\n");
		return 1;
	}
	// Avoid processing a string that is unterminated or too long. The point
	// of this is to guard against weird edge cases that could cause strchr()
	// to misbehave. If we pass this check, using strchr() should be fine.
	const char *msg = argv[1];
	if (!memchr(msg, '\0', AIO_MSG_MAX_LEN)) {
		printk("ERROR: message too long (max: %d bytes)\n", AIO_MSG_MAX_LEN);
		return 2;
	}
	return 0;
}


/*
* EVENT CALLBACKS FOR LVGL AND NETWORKING
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
* SHELL SUBCOMMAND ARRAY MACROS
*/

// These macros add the `aio *` shell commands for configuring and testing the
// MQTT broker in the Zephyr shell over usb serial.  Combined with the `wifi
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
	SHELL_CMD_ARG(pub, NULL, "Publish message to the topic\n"
		"Usage: pub <message>",
		cmd_pub, 2, 0),
	SHELL_CMD(disconnect, NULL, "Disconnect from broker", cmd_disconnect),
	SHELL_SUBCMD_SET_END
);


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
	struct mqtt_utf8 pass = {(uint8_t *)AIOConf.pass, strlen(AIOConf.pass)};
	struct mqtt_utf8 user = {(uint8_t *)AIOConf.user, strlen(AIOConf.user)};
	Ctx.broker = &AIOBroker;
	Ctx.evt_cb = mq_handler;
	Ctx.client_id.utf8 = (uint8_t *)"id";  // protocol requires non-empty id
	Ctx.client_id.size = strlen("id");
	Ctx.password = NULL; //&pass;
	Ctx.user_name = NULL; //&user;
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
		// Update the LVGL user interface status line
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

		// Check on MQTT (note: poll() requires CONFIG_POSIX_API=y)
		if (AIOStatus.connected) {
			// Respond to incoming MQTT messages if needed
			if (poll(fds, 1, 0) > 0) {
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

			// Subscribe if subscribe flag was set by CONNACK handler. Doing
			// this here lets the event handler return quickly.
			if (AIOStatus.needs_subscription) {
				register_subscription();
			}

			// Publish to the topic's /get topic modifier in the manner that
			// AdafruitIO uses as an alternative to normal MQTT retain. This
			// flag gets set by the SUBACK event handler.
			if (AIOStatus.needs_get) {
				publish_get();
			}
		}

		// Call LVGL then sleep until time for the next tick
		uint32_t holdoff_ms = lv_timer_handler();
		k_msleep(holdoff_ms);
	}
}
