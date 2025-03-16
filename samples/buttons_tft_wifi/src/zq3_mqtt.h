/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_MQTT_H
#define ZQ3_MQTT_H

#include <zephyr/net/mqtt.h>  /* struct mqtt_client */
#include "zq3.h"              /* zq3_context */

int zq3_register_sub(zq3_context *zctx, struct mqtt_client *mctx);

int zq3_publish_get();

int zq3_mqtt_connect(zq3_context *zctx, struct mqtt_client *mctx);

#endif /* ZQ3_MQTT_H */

