/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_H
#define ZQ3_H

#include <zephyr/net/socket.h>  /* pollfd */


// Sequence of connection states from no-connectivity up to mqtt-ready-to-go.
typedef enum {
	OFFLINE,   // waiting for button press to initiate wifi connection
	WIFI_ERR,  // waiting for wifi error recovery (something went wrong)
	WIFIWAIT,  // waiting for wifi to connect
	WIFI_UP,   // wifi up, mqtt not connected to broker yet
	MQTT_ERR,  // waiting for error recovery (something went wrong)
	CONNWAIT,  // waiting for MQTT CONNACK event
	CONNACK,   // subscribe to MQTT topic
	SUBWAIT,   // waiting for MQTT SUBACK
	SUBACK,    // publish to /get topic modifier
	READY,     // task: respond to button pushes or publish events
} zq3_state;

// Possible states for local cached value of an MQTT toggle switch topic
typedef enum {
	UNKNOWN,  // toggle state unknown (because no MQTT connection yet, etc)
	OFF,      // toggle is off
	ON,       // toggle is on
} zq3_toggle;

// Adafruit IO MQTT broker auth credentials, hostname, topic, and scheme
typedef struct {
	char ssid[32];       // wifi ssid
	char psk[64];        // wifi WPA2 passphrase
	char user[48];       // MQTT username
	char pass[48];       // MQTT password
	char host[48];       // MQTT broker's hostname
	char topic[48];      // MQTT topic
	bool tls;            // MQTT should use TLS
	bool mqtt_ok;        // MQTT configuration is valid (url parse worked)
	zq3_state state;     // MQTT connection state (independent of wifi)
	bool keypress;       // flag to notify main loop of lvgl keypad press
	bool got_0;          // flag for receiving MQTT PUBLISH message "0"
	bool got_1;          // flag for receiving MQTT PUBLISH message "1"
	zq3_toggle toggle;   // current state of toggle switch
	// For the fds file descriptor array, the examples in Zephyr project MQTT
	// docs use the pollfd type and the poll() function, both of which gave me
	// compiler errors until I figured out I needed CONFIG_POSIX_API=y. Note
	// that docs may refer to zsock_pollfd and zsock_poll(). See
	// zephyr/net/socket.h.
	struct pollfd fds[1];
} zq3_context;

#define ZQ3_URL_MAX_LEN (sizeof("mqtts://:@") + \
	sizeof(((zq3_context *)0)->user) + sizeof(((zq3_context *)0)->pass) + \
	sizeof(((zq3_context *)0)->host) + sizeof(((zq3_context *)0)->topic))

#define ZQ3_MSG_MAX_LEN (128)


#endif /* ZQ3_H */
