# SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr)
project(zphqst_03)
target_sources(app PRIVATE
	src/main.c
	src/zq3_mqtt.c
	src/zq3_dns.c
	src/zq3_url.c
)
