/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <errno.h>
#include "zq3.h"
#include "zq3_mqtt.h"


// Once MQTT connection is up, event loop calls this to start subscription.
// This is hardcoded to subscribe to the one topic specified in the url that
// gets parsed by the `aio conf` shell command.
//
int zq3_register_sub(aio_status_t *status, aio_conf_t *conf, struct mqtt_client *ctx) {
	status->needs_subscription = false;
	if (!status->connected) {
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
			.utf8 = (uint8_t *)conf->topic,
			.size = strlen(conf->topic)
		},
		.qos = MQTT_QOS_0_AT_MOST_ONCE
	};
	struct mqtt_subscription_list list = {
		.list = &topic,
		.list_count = 1,
		.message_id = 1
	};
	// Send the subscription request
	int err = mqtt_subscribe(ctx, &list);
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
//
int zq3_publish_get() {
	// TODO: implement this
	return 0;
}
