# SPDX-License-Identifier: Apache-2.0 OR MIT
# SPDX-FileCopyrightText: Copyright 2025 Sam Blenny

# Uncomment the next line if you want extra debug info from cmake
#_CMAKE_ECHO=-DCMAKE_EXECUTE_PROCESS_COMMAND_ECHO=STDERR

# Build custom shell with wifi turned on and extra stuff disabled
app:
	west build -b feather_tft_esp32s3/esp32s3/procpu app \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}           \
		-DCONFIG_LV_COLOR_16_SWAP=y

# Interactively modify config from previous build
menuconfig:
	west build -t menuconfig

# Flash previously built firmware
flash:
	west flash

# Connect to the ESP32-S3's USB serial port
monitor:
	west espressif monitor

clean:
	rm -rf build

.PHONY: app button lvgl menuconfig flash monitor clean
