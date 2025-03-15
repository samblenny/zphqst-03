# SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
# SPDX-License-Identifier: Apache-2.0 OR MIT

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr)
project(buttons_tft_wifi)
target_sources(app PRIVATE
	src/main.c
	src/zq3_mqtt.c
)
