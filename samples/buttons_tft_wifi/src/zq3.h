/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_H
#define ZQ3_H


// Sequence of connection states from no connectivity up to mqtt ready to go.
typedef enum {
    WIFI_DOWN, // waiting for shell to initiate wifi connect
    WIFI_UP,   // waiting for shell to initiate MQTT connect
    MQTT_ERR,  // wait (something went wrong with MQTT connect)
    CONNWAIT,  // waiting for MQTT CONNACK event
    CONNACK,   // subscribe to MQTT topic
    SUBWAIT,   // waiting for MQTT SUBACK
    SUBACK,    // publish to /get topic modifier
    READY,     // task: respond to button pushes or publish events
} zq3_state;

// AdafruitIO MQTT broker auth credentials, hostname, topic, and scheme
typedef struct {
	char user[48];       // MQTT username
	char pass[48];       // MQTT password
	char host[48];       // MQTT broker's hostname
	char topic[48];      // MQTT topic
	bool tls;            // MQTT should use TLS
	bool valid;          // MQTT configuration is valid
    zq3_state state;
	bool pub_got_0;
	bool pub_got_1;
} zq3_context;

#define ZQ3_URL_MAX_LEN (sizeof("mqtts://:@") + \
	sizeof(((zq3_context *)0)->user) + sizeof(((zq3_context *)0)->pass) + \
	sizeof(((zq3_context *)0)->host) + sizeof(((zq3_context *)0)->topic))

#define ZQ3_MSG_MAX_LEN (128)


#endif /* ZQ3_H */
