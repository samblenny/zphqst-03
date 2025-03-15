/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_H
#define ZQ3_H


// AdafruitIO MQTT broker auth credentials, hostname, topic, and scheme
typedef struct {
	char user[48];
	char pass[48];
	char host[48];
	char topic[48];
	bool tls;
	bool valid;
} aio_conf_t;

#define AIO_URL_MAX_LEN (sizeof("mqtts://:@") + \
	sizeof(((aio_conf_t *)0)->user) + sizeof(((aio_conf_t *)0)->pass) + \
	sizeof(((aio_conf_t *)0)->host) + sizeof(((aio_conf_t *)0)->topic))

#define AIO_MSG_MAX_LEN (128)

// MQTT connection status
typedef struct {
	bool connected;
	bool needs_subscription;
	bool needs_get;
	bool pub_got_0;
	bool pub_got_1;
} aio_status_t;


#endif /* ZQ3_H */
