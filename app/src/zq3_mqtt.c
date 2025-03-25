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
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/sockets.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/net/socket.h
 *
 * TLS Docs & Refs
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/mqtt.html
 * https://docs.zephyrproject.org/latest/connectivity/networking/api/sockets.html
 * https://docs.zephyrproject.org/latest/doxygen/html/group__tls__credentials.html
 * zephyr/include/zephyr/net/mqtt.h  (struct mqtt_sec_config)
 * zephyr/include/zephyr/net/tls_credentials.h
 * zephyr/samples/net/secure_mqtt_sensor_actuator/src/mqtt_client.c
 * zephyr/samples/net/secure_mqtt_sensor_actuator/src/tls_config/cert.h
 *
 * Adafruit IO MQTT Settings:
 * host:port: io.adafruit.com:8883
 */

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/tls_credentials.h>
#include "zq3.h"
#include "zq3_dns.h"
#include "zq3_mqtt.h"
#include "zq3_cert.h"


// Initialize MQTT
int zq3_mqtt_init(
	zq3_mqtt_context *mctx,
	void (*callback)(struct mqtt_client *, const struct mqtt_evt *)
) {
	// Clear string buffers to with '\0' before any strlen() calls
	memset(mctx->user_buf, 0, sizeof(mctx->user_buf));
	memset(mctx->pass_buf, 0, sizeof(mctx->pass_buf));
	memset(mctx->hostname, 0, sizeof(mctx->hostname));
	memset(mctx->topic, 0, sizeof(mctx->topic));
	// Initialize the MQTT API's client struct
	struct mqtt_client *c = &mctx->client;
	mqtt_client_init(c);
	mctx->pass.utf8 = mctx->pass_buf;
	mctx->user.utf8 = mctx->user_buf;
	mctx->pass.size = 0;
	mctx->user.size = 0;
	c->password = &mctx->pass;
	c->user_name = &mctx->user;
	c->broker = &mctx->broker;
	c->evt_cb = callback;
	c->client_id.utf8 = (uint8_t *)"i";  // protocol requires non-empty id
	c->client_id.size = strlen("i");
	c->password = &mctx->pass;
	c->user_name = &mctx->user;
	c->protocol_version = MQTT_VERSION_3_1_1;
	c->rx_buf = mctx->rx_buf;
	c->rx_buf_size = sizeof(mctx->rx_buf);
	c->tx_buf = mctx->tx_buf;
	c->tx_buf_size = sizeof(mctx->tx_buf);
	// Start by assuming TLS is turned on
	c->transport.type = MQTT_TRANSPORT_SECURE;
	mctx->tls = true;
    // Register the TLS CA certificates from zq3_certs.h
	int err;
	const char fmt[] = "ERR: tls_credential_add(%d, ...) = %d\n";
	err = tls_credential_add(zq3_cert_tags[0], TLS_CREDENTIAL_CA_CERTIFICATE,
		zq3_cert_self_signed, sizeof(zq3_cert_self_signed));
	if (err) {
		printk(fmt, zq3_cert_tags[0], err);
	}
	err = tls_credential_add(zq3_cert_tags[1], TLS_CREDENTIAL_CA_CERTIFICATE,
		zq3_cert_geotrust_g1, sizeof(zq3_cert_geotrust_g1));
	if (err) {
		printk(fmt, zq3_cert_tags[1], err);
	}
	// Configure TLS stuff in the MQTT client struct
	struct mqtt_sec_config *conf = &mctx->client.transport.tls.config;
	conf->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	conf->cipher_list = NULL;
	conf->sec_tag_list = zq3_cert_tags;
	conf->sec_tag_count = sizeof(zq3_cert_tags)/sizeof(zq3_cert_tags[0]);
	conf->hostname = mctx->hostname;
	return 0;
}

// Update context for TLS enabled (port 8883) or disabled (port 1883)
int zq3_mqtt_set_tls(zq3_mqtt_context *mctx, bool enabled) {
	if (enabled) {
		mctx->client.transport.type = MQTT_TRANSPORT_SECURE;
		mctx->tls = true;
	} else {
		mctx->client.transport.type = MQTT_TRANSPORT_NON_SECURE;
		mctx->tls = false;
	}
	return 0;
}

int zq3_mqtt_set_username(zq3_mqtt_context *mctx, const char *src, int len) {
	if (src == NULL) {
		return -EINVAL;
	}
	if (len >= sizeof(mctx->user_buf)) {
		return -EDOM;
	}
	memset(mctx->user_buf, 0, sizeof(mctx->user_buf));
	memcpy(mctx->user_buf, src, len);
	mctx->client.user_name = &mctx->user;
	mctx->client.user_name->utf8 = mctx->user_buf;
	mctx->client.user_name->size = strlen(mctx->user_buf);
	return 0;
}

int zq3_mqtt_set_password(zq3_mqtt_context *mctx, const char *src, int len) {
	if (src == NULL) {
		return -EINVAL;
	}
	if (len >= sizeof(mctx->pass_buf)) {
		return -EDOM;
	}
	memset(mctx->pass_buf, 0, sizeof(mctx->pass_buf));
	memcpy(mctx->pass_buf, src, len);
	mctx->client.password = &mctx->pass;
	mctx->client.password->utf8 = mctx->pass_buf;
	mctx->client.password->size = strlen(mctx->pass_buf);
	return 0;
}

int zq3_mqtt_set_topic(zq3_mqtt_context *mctx, const char *src, int len) {
	if (src == NULL) {
		return -EINVAL;
	}
	if (len >= sizeof(mctx->topic)) {
		return -EDOM;
	}
	memset(mctx->topic, 0, sizeof(mctx->topic));
	memcpy(mctx->topic, src, len);
	return 0;
}

int zq3_mqtt_set_hostname(zq3_mqtt_context *mctx, const char *src, int len) {
	if (src == NULL) {
		return -EINVAL;
	}
	if (len >= sizeof(mctx->hostname)) {
		return -EDOM;
	}
	memset(mctx->hostname, 0, sizeof(mctx->hostname));
	memcpy(mctx->hostname, src, len);
	return 0;
}

// Once MQTT connection is up, event loop calls this to start subscription.
// This is hardcoded to subscribe to the one topic specified in zq3_context.
// Related docs:
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__subscription__list.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__topic.html
// - https://docs.zephyrproject.org/latest/doxygen/html/structmqtt__utf8.html
//
int zq3_mqtt_subscribe(zq3_mqtt_context *mctx) {
	// This uses a C99 compound literal to initialize a nested struct
	struct mqtt_subscription_list list = {
		.list = &(struct mqtt_topic){
			.topic = (struct mqtt_utf8){
				.utf8 = mctx->topic,
				.size = strlen(mctx->topic)
			},
			.qos = MQTT_QOS_0_AT_MOST_ONCE
		},
		.list_count = 1,
		.message_id = 1
	};
	// Send the subscription request
	int err = mqtt_subscribe(&mctx->client, &list);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		printk("ERR: mqtt_subscribe() = %d\n", err);
	}
	return err;
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
zq3_mqtt_publish(zq3_mqtt_context *mctx, bool toggle)
{
	char * state = toggle ? "1" : "0";
	// Build a C99 compound literal representing the message to be published
	const struct mqtt_publish_param param = {
		.message = (struct mqtt_publish_message){
			.topic = (struct mqtt_topic){
				.topic = (struct mqtt_utf8){
					.utf8 = mctx->topic,
					.size = strlen(mctx->topic),
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
	int err = mqtt_publish(&mctx->client, &param);
	if (err) {
		printk("ERR: mqtt_publish() = %d\n", err);
	}
	return err;
}


// Connect to MQTT broker
int zq3_mqtt_connect(zq3_mqtt_context *mctx) {
	// Use DNS to resolve hostname to IPv4 IP (IPv6 not supported)
	int err = zq3_dns_resolve(mctx->hostname, mctx->tls,
		(struct sockaddr_storage *)mctx->client.broker);
	if (err) {
		return err;
	}

	err = mqtt_connect(&mctx->client);
	if(err) {
		// https://docs.zephyrproject.org/apidoc/latest/errno_8h.html
		const char *fmt = "ERR: mqtt_connect() = %d %s\n";
		switch(-err) {
		case ENOENT:
			printk(fmt, err, "ENOENT: TLS CA cert valid?");
			break;
		case ECONNREFUSED:
			printk(fmt, err, "ECONNREFUSED: Broker service running?");
			break;
		default:
			printk(fmt, err, "");
		}
		return err;
	}
	if (mctx->tls) {
		mctx->fds[0].fd = mctx->client.transport.tls.sock;
	} else {
		mctx->fds[0].fd = mctx->client.transport.tcp.sock;
	}
	mctx->fds[0].events = ZSOCK_POLLIN;
	return 0;
}

// Poll for incoming packets (note: poll() requires CONFIG_POSIX_API=y)
int zq3_mqtt_poll(zq3_mqtt_context *mctx) {
	if (poll(mctx->fds, 1, 0) > 0) {
		return mqtt_input(&mctx->client);
	}
	return 0;
}

// Send an MQTT PINGREQ ping several seconds before the keepalive timer is due
// to run out. This keeps the TCP connection to the MQTT broker open so it can
// send us messages on subscribed topics as they are published.
//
int zq3_mqtt_keepalive(zq3_mqtt_context *mctx) {
	int remaining_ms = mqtt_keepalive_time_left(&mctx->client);
	if (remaining_ms < 5000) {
		return mqtt_live(&mctx->client);
	}
	return 0;
}

// Disconnect from MQTT broker
int zq3_mqtt_disconnect(zq3_mqtt_context *mctx) {
	int err = mqtt_disconnect(&mctx->client);
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
