# SPDX-License-Identifier: Apache-2.0 OR MIT
# SPDX-FileCopyrightText: Copyright 2025 Sam Blenny

# Uncomment the next line if you want extra debug info from cmake
#_CMAKE_ECHO=-DCMAKE_EXECUTE_PROCESS_COMMAND_ECHO=STDERR

# Build the "display" sample that draws a white background with red, green,
# blue and gray boxes in the corners. The gray box animates in a cycle of
# fading gray from black to white.
display:
	west build -b feather_tft_esp32s3/esp32s3/procpu  \
		../zephyr/samples/drivers/display             \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}

# Build plain Zephyr shell
shell:
	west build -b feather_tft_esp32s3/esp32s3/procpu  \
		../zephyr/samples/subsys/shell/shell_module/  \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}

# Build Zephyr shell with wifi support
wifishell:
	west build -b feather_tft_esp32s3/esp32s3/procpu  \
		../zephyr/samples/net/wifi/shell              \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}

# Build Zephyr button sample
button:
	west build -b feather_tft_esp32s3/esp32s3/procpu  \
		../zephyr/samples/basic/button                \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}

# Build Zephyr LVGL shell sample
lvgl:
	west build -b feather_tft_esp32s3/esp32s3/procpu  \
		../zephyr/samples/modules/lvgl/demos          \
		-- -DBOARD_ROOT=$$(pwd) ${_CMAKE_ECHO}        \
		-DCONFIG_LV_COLOR_16_SWAP=y                   \
		-DCONFIG_LV_Z_DEMO_MUSIC=n -DCONFIG_LV_Z_DEMO_KEYPAD_AND_ENCODER=y

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

.PHONY: display shell wifishell button menuconfig flash monitor clean
