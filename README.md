<!-- SPDX-License-Identifier: MIT -->
<!-- SPDX-FileCopyrightText: Copyright 2025 Sam Blenny -->

# zphqst-03

**WORK IN PROGRESS**

This is meant for the Adafruit Feather TFT ESP32-S3 board.


## Dev Tool Setup

1. To run Zephyr's `west` commandline tool, you need to set up a Zephyr
   project, including a Python virtual environment and the zephyr git repo.
   In examples here, my zephyr project directory is `~/code/zephyr-workspace`.

2. To build for ESP32-S3, you need to install the Zephyr SDK including the
   `xtensa-espressif_esp32s3_zephyr-elf` toolchain.

3. The first time you run `west flash` on a board that had CircuitPython
   installed, you may need to activate the board's ESP32-S3 built in bootloader
   using the button sequence: hold BOOT, press and release RESET, release BOOT.

4. To build with wifi support, you will need to fetch the `hal_espressif` blobs
   if you have not already done so:

   ```
   west blobs fetch hal_espressif
   ```


## Build & Run

These examples assume that you have activated your Python venv that contains
`west` and that you've changed into the zphqst-03 directory within your Zephyr
project directory. For example:

```
$ cd ~/code/zephyr-workspace
$ source .venv/bin/activate
(.venv) $ cd zphqst-03
```

The samples below use `make` in a terminal on Debian 12 to run commands for
make targets defined in the zphqst-03 [Makefile](Makefile). Using `make`
avoids a lot of typing that would otherwise be required to provide commandline
options to `west`.

The `make monitor` command runs `west espressif monitor`. Keyboard shortcut to
exit the serial monitor is **Ctrl+]**.


### Zephyr Button Sample

This runs zephyr/samples/basic/button:

```
(.venv) $ make clean && make button && make flash
(.venv) $ make monitor
```

When you press the Boot button, this will turn on the red `#13` LED and print
a message to the serial console.


### Zephyr LVGL Demo

This runs zephyr/samples/subsys/display/lvgl which has a white background with
a "Hello World!" and a counter. Pressing `sw0` (the Boot button), resets the
counter to 0.

```
(.venv) $ make clean && make lvgl && make flash
```

This demo is at the wrong screen size, and the button doesn't work. But, it
does show a nice button widget and good font rendering.


### Zephyr Shell with wifi connection support

This is a customized version of the Zephyr Shell with wifi and other options
defined in [zphqst-03/samples/buttons\_tft\_wifi](samples/buttons_tft_wifi).

```
(.venv) $ make clean && make shell && make flash
(.venv) $ make monitor
```

Once you're connected to the serial monitor, you can connect to a wifi network
with the `wifi scan` and `wifi connect` commands:

```
uart:~$ wifi connect -s "$SSID" -p "$PASSPHRASE" -k 1
Connection requested
uart:~$
uart:~$
uart:~$ wifi status
Status: successful
==================
State: COMPLETED
Interface Mode: STATION
Link Mode: WIFI 4 (802.11n/HT)
SSID: $SSID
BSSID: --:--:--:--:--:--
Band: 2.4GHz
Channel: 11
Security:  WPA2-PSK
MFP: Disable
RSSI: -48
Beacon Interval: 3
DTIM: 0
TWT: Not supported
Current PHY TX rate (Mbps) : 0
Connected
[00:04:02.708,000] <inf> net_dhcpv4: Received: 192.168.0.161
uart:~$
uart:~$
uart:~$ wifi disconnect
Disconnection request done (0)
Disconnect requested
uart:~$
uart:~$
uart:~$ wifi status
Status: successful
==================
State: DISCONNECTED
```


## License

Files in this repo use a mix of **Apache-2.0** and/or **MIT** licensing. Check
SPDX header comments for details.
