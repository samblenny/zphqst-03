/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define WIDTH (DT_PROP(DT_CHOSEN(zephyr_display), width))
#define HEIGHT (DT_PROP(DT_CHOSEN(zephyr_display), height))
static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

int main(void) {
	printf("display: %s, %dx%d\n", display->name, WIDTH, HEIGHT);
	while(1) {
		k_msleep(16);
	}
}
