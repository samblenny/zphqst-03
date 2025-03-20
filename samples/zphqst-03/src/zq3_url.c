/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zq3.h"


// Debug print the config resulting from parsing the MQTT URL
void zq3_url_print_conf(zq3_context *zctx) {
	printk(
		"MQTT Broker Config:\n"
		"  ssid:   '%s'\n"
		"  psk:    '%s'\n"
		"  user:   '%s'\n"
		"  pass:   '%s'\n"
		"  host:   '%s'\n"
		"  topic:  '%s'\n"
		"  tls:     %s\n"
		"  mqtt_ok: %s\n",
		zctx->ssid, zctx->psk,
		zctx->user, zctx->pass, zctx->host, zctx->topic,
		zctx->tls ? "true" : "false", zctx->mqtt_ok ? "true" : "false"
	);
}

// Parse an MQTT broker url into fields of the zq3_context struct
// Examples of expected url format (TLS and non-TLS):
//   mqtts://Blinka:password@io.adafruit.com/Blinka/feeds/test
//   mqtt://:@192.168.0.50/test
//
int zq3_url_parse(zq3_context *zctx, const char *url) {
	// Clear MQTT config fields and begin parsing
	memset(zctx->user, 0, sizeof(zctx->user));
	memset(zctx->pass, 0, sizeof(zctx->pass));
	memset(zctx->host, 0, sizeof(zctx->host));
	memset(zctx->topic, 0, sizeof(zctx->topic));
	zctx->tls = false;
	zctx->mqtt_ok = false;

	const char *cursor = url;

	// Parse URL scheme prefix (should be "mqtt://" or "mqtts://")
	char *tls_scheme = "mqtts://";
	char *nontls_scheme = "mqtt://";
	if (strncmp(url, tls_scheme, strlen(tls_scheme)) == 0) {
		// Scheme = MQTT with TLS
		cursor += strlen(tls_scheme);
		zctx->tls = true;
	} else if (strncmp(url, nontls_scheme, strlen(nontls_scheme)) == 0) {
		// Scheme = unencrypted MQTT
		cursor += strlen(nontls_scheme);
		zctx->tls = false;
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
	} else if (len >= sizeof(zctx->user)) {
		printk("ERR: username too long\n");
		return 5;
	} else {
		// Copy username string (relies on len < sizeof(...))
		memcpy(zctx->user, cursor, len);
		cursor = delim + 1;
	}

	// Parse password (whatever is between ':' and '@')
	delim = strchr((char *)cursor, '@');
	len = delim - cursor;
	if (!delim || len < 0) {
		printk("ERR: missing '@' after password\n");
		return 6;
	} else if (len >= sizeof(zctx->pass)) {
		printk("ERR: password too long\n");
		return 7;
	} else {
		// Copy password string (relies on len < sizeof(...))
		memcpy(zctx->pass, cursor, len);
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
	} else if (len >= sizeof(zctx->host)) {
		printk("ERR: hostname too long\n");
		return 10;
	} else {
		// Copy hostname string (relies on len < sizeof(...))
		memcpy(zctx->host, cursor, len);
		cursor = delim + 1;
	}

	// Parse topic (whatever is after '/')
	len = strlen(cursor);
	if (len <= 0) {
		printk("ERR: topic can't be blank\n");
		return 11;
	} else if (len >= sizeof(zctx->topic)) {
		printk("ERR: topic too long\n");
		return 12;
	} else {
		// Copy topic string (relies on len < sizeof(...))
		memcpy(zctx->topic, cursor, len);
	}

	return 0;
}

