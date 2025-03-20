/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
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
#include <zephyr/net/mqtt.h>
#include "zq3.h"
#include "zq3_dns.h"


// Once MQTT connection is up, event loop calls this to start subscription.
// This is hardcoded to subscribe to the one topic specified in zq3_context.
// Related docs:
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__subscription__list.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__topic.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__utf8.html
//
int zq3_mqtt_subscribe(zq3_context *zctx, struct mqtt_client *mctx) {
	if (zctx->state < CONNACK) {
		printk("ERR: can't subscribe, mqtt not connected.\n");
		return 1;
	}
	// This is using a C99 feature called a compound literals to initialize a
	// nested struct.
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
int zq3_mqtt_publish_get(zq3_context *zctx, struct mqtt_client *mctx) {
	// TODO: implement this
	printk("TODO: IMPLEMENT PUBLISH GET\n");
	return 0;
}

// Publish new toggle switch state to the topic.
// Related docs:
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__publish__param.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__publish__message.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__topic.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__utf8.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__binstr.html
//
int
zq3_mqtt_publish(zq3_context *zctx, struct mqtt_client *mctx, bool toggle)
{
	char * state = toggle ? "1" : "0";
	printk("Publishing toggle state = %s\n", state);
	// Build a C99 compound literal representing the message to be published
	const struct mqtt_publish_param param = {
		.message = (struct mqtt_publish_message){
			.topic = (struct mqtt_topic){
				.topic = (struct mqtt_utf8){
					.utf8 = (uint8_t *)zctx->topic,
					.size = strlen(zctx->topic),
				},
				.qos = MQTT_QOS_0_AT_MOST_ONCE,
			},
			.payload = (struct mqtt_binstr){
				.data = (uint8_t *)state,
				.len = strlen(state),
			},
		},
		.message_id = 0,
		.dup_flag = 0,
		.retain_flag = 0,
	};
	// Publish it
	int err = mqtt_publish(mctx, &param);
	if (err) {
		switch(err) {
		default:
			printk("ERR: mqtt_publish() = %d\n", err);
		}
	}
	return err;
}


// Connect to MQTT broker
int zq3_mqtt_connect(zq3_context *zctx, struct mqtt_client *mctx) {
	if (!zctx->mqtt_ok) {
		printk("ERR: MQTT broker not configured\n");
		return 1;
	}
	if (!zctx->wifi_up) {
		printk("ERR: Wifi not connected\n");
		return 2;
	}
	if (mctx->broker == NULL) {
		printk("ERR: MQTT context's broker struct is null\n");
		return 3;
	}

	// Use DNS to resolve hostname to IPv4 IP (IPv6 not supported)
	int err = zq3_dns_resolve(zctx, (struct sockaddr_storage *)mctx->broker);
	if (err) {
		return err;
	}

	err = mqtt_connect(mctx);
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
		if (zctx->state >= CONNWAIT) {
			zctx->state = MQTT_ERR;
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
