/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_MQTT_H
#define ZQ3_MQTT_H

#include <zephyr/net/mqtt.h>  /* struct mqtt_client */
#include "zq3.h"              /* zq3_context */


// To use an mqtt_client struct, you always need some other buffers and a
// socket address struct for the broker's network address. This context struct
// groups all that stuff together so it's easy to pass the pointer around.
//
// For the fds file descriptor array, the examples in Zephyr project MQTT docs
// use the pollfd type and the poll() function, both of which gave me compiler
// errors until I figured out I needed CONFIG_POSIX_API=y. Note that docs may
// refer to zsock_pollfd and zsock_poll(). See zephyr/net/socket.h.
//
typedef struct {
	uint8_t rx_buf[256];
	uint8_t tx_buf[256];
	uint8_t user_buf[48];            // MQTT username string buffer
	uint8_t pass_buf[48];            // MQTT password string buffer
	uint8_t topic[48];               // MQTT topic string buffer
	uint8_t hostname[48];            // MQTT broker hostname string buffer
	struct mqtt_utf8 pass;           // UTF-8 password struct
	struct mqtt_utf8 user;           // UTF-8 username struct
	struct sockaddr_storage broker;  // Broker network address struct union
	struct mqtt_client client;       // client struct for mqtt_*() API funcs
	struct pollfd fds[1];            // socket file descriptor
	bool tls;                        // true: port 8883+TLS, false: port 1883
} zq3_mqtt_context;

#define ZQ3_MQTT_URL_MAX_LEN (sizeof("mqtts://:@") + \
	sizeof(((zq3_mqtt_context *)0)->user_buf) + \
	sizeof(((zq3_mqtt_context *)0)->pass_buf) + \
	sizeof(((zq3_mqtt_context *)0)->hostname) + \
	sizeof(((zq3_mqtt_context *)0)->topic))

int zq3_mqtt_init(
	zq3_mqtt_context *mctx,
	void (*callback)(struct mqtt_client *, const struct mqtt_evt *)
);

int zq3_mqtt_set_tls(zq3_mqtt_context *mctx, bool enabled);

int zq3_mqtt_set_username(zq3_mqtt_context *mctx, const char *src, int len);

int zq3_mqtt_set_password(zq3_mqtt_context *mctx, const char *src, int len);

int zq3_mqtt_set_hostname(zq3_mqtt_context *mctx, const char *src, int len);

int zq3_mqtt_set_topic(zq3_mqtt_context *mctx, const char *src, int len);

int zq3_mqtt_subscribe(zq3_mqtt_context *mctx);

int zq3_mqtt_publish(zq3_mqtt_context *mctx, bool toggle);

int zq3_mqtt_connect(zq3_mqtt_context *mctx);

int zq3_mqtt_poll(zq3_mqtt_context *mctx);

int zq3_mqtt_keepalive(zq3_mqtt_context *mctx);

int zq3_mqtt_disconnect(zq3_mqtt_context *mctx);

#endif /* ZQ3_MQTT_H */

