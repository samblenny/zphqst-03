/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_MQTT_H
#define ZQ3_MQTT_H

#include <zephyr/net/mqtt.h>  /* struct mqtt_client */
#include "zq3.h"              /* aio_status_t, aio_conf_t */

int zq3_register_sub(
	aio_status_t *status,
	aio_conf_t *conf,
	struct mqtt_client *ctx
);

int zq3_publish_get();

#endif /* ZQ3_MQTT_H */

