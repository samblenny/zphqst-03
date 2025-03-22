/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zq3_mqtt.h"


// Parse an MQTT broker url into fields of the zq3_mqtt_context struct
// Examples of expected url format (TLS and non-TLS):
//   mqtts://Blinka:password@io.adafruit.com/Blinka/feeds/test
//   mqtt://:@192.168.0.50/test
//
int zq3_url_parse(zq3_mqtt_context *mctx, const char *url) {
	const char *cursor = url;

	// Parse URL scheme prefix (should be "mqtt://" or "mqtts://")
	char *tls_scheme = "mqtts://";
	char *nontls_scheme = "mqtt://";
	if (strncmp(url, tls_scheme, strlen(tls_scheme)) == 0) {
		// Scheme = MQTT with TLS
		cursor += strlen(tls_scheme);
		zq3_mqtt_set_tls(mctx, true);
	} else if (strncmp(url, nontls_scheme, strlen(nontls_scheme)) == 0) {
		// Scheme = unencrypted MQTT
		cursor += strlen(nontls_scheme);
		zq3_mqtt_set_tls(mctx, false);
	} else {
		printk("ERROR: expected mqtt:// or mqtts://\n");
		return -EINVAL;
	}

	// Parse username (whatever is between end of scheme and the next ':')
	char *delim = strchr((char *)cursor, ':');
	int len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing ':' after username\n");
		return -EINVAL;
	} else if (len >= sizeof(mctx->user_buf)) {
		printk("ERR: username too long\n");
		return -EINVAL;
	} else {
		// Copy username string
		zq3_mqtt_set_username(mctx, cursor, len);
		cursor = delim + 1;
	}

	// Parse password (whatever is between ':' and '@')
	delim = strchr((char *)cursor, '@');
	len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing '@' after password\n");
		return -EINVAL;
	} else if (len >= sizeof(mctx->pass_buf)) {
		printk("ERR: password too long\n");
		return -EINVAL;
	} else {
		// Copy password string
		zq3_mqtt_set_password(mctx, cursor, len);
		cursor = delim + 1;
	}

	// Parse host (whatever is between '@' and '/')
	delim = strchr((char *)cursor, '/');
	len = delim - cursor;
	if (!delim) {
		printk("ERR: missing '/' after hostname\n");
		return -EINVAL;
	} else if (len < 1) {
		printk("ERR: hostname can't be blank\n");
		return -EINVAL;
	} else if (len >= sizeof(mctx->hostname)) {
		printk("ERR: hostname too long\n");
		return -EINVAL;
	} else {
		// Copy hostname string
		zq3_mqtt_set_hostname(mctx, cursor, len);
		cursor = delim + 1;
	}

	// Parse topic (whatever is after '/')
	len = strlen(cursor);
	if (len <= 0) {
		printk("ERR: topic can't be blank\n");
		return -EINVAL;
	} else if (len >= sizeof(mctx->topic)) {
		printk("ERR: topic too long\n");
		return -EINVAL;
	} else {
		// Copy topic string
		zq3_mqtt_set_topic(mctx, cursor, len);
	}

	return 0;
}

