/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef ZQ3_WIFI_H
#define ZQ3_WIFI_H


int zq3_wifi_connect(const char *ssid, const char *psk);

int zq3_wifi_disconnect();


#endif /* ZQ3_WIFI_H */
