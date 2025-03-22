/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions for managing the Wifi connection
 *
 * Docs & Refs:
 * - https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/l2/wifi/wifi_shell.c
 * - https://docs.zephyrproject.org/latest/connectivity/networking/api/net_mgmt.html
 * - https://docs.zephyrproject.org/latest/doxygen/html/group__net__mgmt.html
 * - https://docs.zephyrproject.org/latest/doxygen/html/group__wifi__mgmt.html
 * - https://docs.zephyrproject.org/apidoc/latest/group__net__if.html
 * - https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/net/wifi.h
 */

#include <zephyr/net/net_mgmt.h>   /* net_mgmt() */
#include <zephyr/net/net_if.h>     /* net_if_get_wifi_sta() */
#include <zephyr/net/wifi.h>       /* WIFI_SECURITY_TYPE_WPA_PSK, ... */
#include <zephyr/net/wifi_mgmt.h>  /* NET_REQUEST_WIFI_CONNECT, ... */


// Connect to the specified Wifi AP using WPA2-PSK
int zq3_wifi_connect(const char *ssid, const char *psk) {
	if (ssid == NULL || strlen(ssid) == 0) {
		printk("ERR: Wifi connect: SSID not specified\n");
		return -EINVAL;
	}
	if (psk == NULL || strlen(psk) == 0) {
		printk("ERR: Wifi connect: PSK not specified\n");
		return -EINVAL;
	}
	if (strlen(ssid) > WIFI_SSID_MAX_LEN) {
		printk("ERR: Wifi connect: SSID is too long\n");
		return -EINVAL;
	}
	// References for wifi_connect_req_params:
	// - zephyr/include/zephyr/net/wifi.h        (enums)
	// - zephyr/include/zephyr/net/wifi_mgmt.h   (struct def)
	// - zephyr/samples/net/l2/wifi/wifi_shell.c (default values)
	struct wifi_connect_req_params params = {
		.ssid = ssid,
		.ssid_length = strlen(ssid),
		.psk = psk,
		.psk_length = strlen(psk),
		.band = WIFI_FREQ_BAND_2_4_GHZ,         // enum wifi_frequency_bands
		.channel = WIFI_CHANNEL_ANY,
		.security = WIFI_SECURITY_TYPE_PSK,     // enum wifi_security_type
		.mfp = WIFI_MFP_OPTIONAL,
		.eap_ver = 1,
		.ignore_broadcast_ssid = 0,
		.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ, // wifi_frequency_bandwidths
		.verify_peer_cert = false,
	};
	struct net_if *i = net_if_get_wifi_sta();
	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, i, &params, sizeof(params));
	if (err) {
		printk("ERR: Wifi connect: net_mgmt() = %d\n", err);
	} else {
		printk("[Wifi CONNECT requested]\n");
	}
	return err;
}

// Disconnect from Wifi
int zq3_wifi_disconnect() {
	struct net_if *i = net_if_get_wifi_sta();
	int err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, i, NULL, 0);
	switch (err) {
	case 0:
		printk("[Wifi DISCONNECT requested]\n");
		return 0;
	case -EALREADY:
		printk("Wifi disconnect: already disconnected\n");
		return 0;
	default:
		printk("ERR: Wifi disconnect: net_mgmt() = %d\n", err);
		return err;
	}
}
