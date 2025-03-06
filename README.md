<!-- SPDX-License-Identifier: MIT -->
<!-- SPDX-FileCopyrightText: Copyright 2025 Sam Blenny -->

# zphqst-03

**WORK IN PROGRESS**

This is meant for the Adafruit Feather TFT ESP32-S3 board.


## Build & Run

**Note:** The first time you `west flash` on a board that had CircuitPython
installed, you may need to activate the ESP32-S3 built in bootloader with the
button sequence: hold BOOT, press and release RESET, release BOOT.

Build and run Zephyr's display sample on Debian 12 using the zphqst-03
[Makefile](Makefile):

```
$ cd ~/code/zephyr-workspace
$ source .venv/bin/activate
(.venv) $ cd zphqst-03
(.venv) $ make clean
(.venv) $ make display
(.venv) $ make flash
```

If you want to run Zephyr's shell sample, do:

```
(.venv) $ make clean
(.venv) $ make shell
(.venv) $ make flash
(.venv) $ make monitor
```

The `make monitor` command runs `west espressif monitor`. Keyboard shortcut to
exit the serial monitor is **Ctrl+]**.


## License

Files in this repo use a mix of **Apache-2.0** and/or **MIT** licensing. Check
SPDX header comments for details.
