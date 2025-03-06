# SPDX-License-Identifier: Apache-2.0
# Derived from zephyr/boards/espressif/esp32s3_devkitm

if(NOT "${OPENOCD}" MATCHES "^${ESPRESSIF_TOOLCHAIN_PATH}/.*")
  set(OPENOCD OPENOCD-NOTFOUND)
endif()
find_program(OPENOCD openocd PATHS ${ESPRESSIF_TOOLCHAIN_PATH}/openocd-esp32/bin NO_DEFAULT_PATH)

include(${ZEPHYR_BASE}/boards/common/esp32.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
