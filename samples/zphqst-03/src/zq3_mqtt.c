/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include "zq3.h"


// Once MQTT connection is up, event loop calls this to start subscription.
// This is hardcoded to subscribe to the one topic specified in zq3_context.
//
int zq3_register_sub(zq3_context *zctx, struct mqtt_client *mctx) {
	if (zctx->state < CONNACK) {
		printk("ERR: can't subscribe, mqtt not connected.\n");
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
			.utf8 = (uint8_t *)zctx->topic,
			.size = strlen(zctx->topic)
		},
		.qos = MQTT_QOS_0_AT_MOST_ONCE
	};
	struct mqtt_subscription_list list = {
		.list = &topic,
		.list_count = 1,
		.message_id = 1
	};
	// Send the subscription request
	int err = mqtt_subscribe(mctx, &list);
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


// Connect to MQTT broker
int zq3_mqtt_connect(zq3_context *zctx, struct mqtt_client *mctx) {
	if (!zctx->valid) {
		printk("ERR: AIO is not configured (try 'aio conf ...')\n");
		return 1;
	}
	if (!zctx->wifi_up) {
		printk("ERR: Wifi not connected (try `wifi connect ...`)\n");
		return 2;
	}
	int err = mqtt_connect(mctx);
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
	zctx->fds[0].fd = mctx->transport.tcp.sock;
	zctx->fds[0].events = ZSOCK_POLLIN;
	return 0;
}


// Disconnect from MQTT broker
int zq3_mqtt_disconnect(struct mqtt_client *mctx) {
	int err = mqtt_disconnect(mctx);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		switch(-err) {
		case ENOTCONN:
			printk("ERR: Socket was not connected\n");
			break;
		default:
			printk("ERR: %d\n", err);
		}
		return err;
	}
	return 0;
}
