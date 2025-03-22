<!-- SPDX-License-Identifier: MIT -->
<!-- SPDX-FileCopyrightText: Copyright 2025 Sam Blenny -->

# zphqst-03

**WORK IN PROGRESS**

This is a simple IoT toggle switch demo application for Zephyr on the
Adafruit Feather TFT ESP32-S3 board.

Features:
- LVGL graphics for network status and toggle switch widget
- MQTT networking over Wifi
- Provisioning over USB serial (credentials saved to NVM flash)
- Boot button starts network connection
- Boot button controls MQTT toggle switch once connected


## Dev Tool Setup

To prepare for building this project:

1. To run Zephyr's `west` commandline tool, you need to set up a Zephyr
   project, including a Python virtual environment and the zephyr git repo.
   In my examples, the zephyr project directory is `~/code/zephyr-workspace`.

2. To build for ESP32-S3, you need to install the Zephyr SDK including the
   `xtensa-espressif_esp32s3_zephyr-elf` toolchain. The basic getting started
   guide instructions install all the toolchains. You only need to pay
   attention to specific toolchains if you want to do a custom install to save
   disk space and bandwidth.

3. The first time you run `west flash` on a board that had CircuitPython
   installed, you may need to activate the board's ESP32-S3 built in bootloader
   using the button sequence: hold BOOT, press and release RESET, release BOOT.
   Once you have installed the Zephyr bootloader, `west flash` should work
   without needing to press any buttons.

4. To build with wifi support, you will need to fetch the `hal_espressif` blobs
   if you have not already done so:

   ```
   west blobs fetch hal_espressif
   ```

5. Clone this repository into your Zephyr project directory. For example:

   ```
   cd ~/code/zephyr-workspace
   git clone https://github.com/samblenny/zphqst-03.git
   ```

To learn more about setting up a Zephyr project directory, you can read the
Zephyr Project
[Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

The directions here were written and tested for a terminal shell on Debian 12
Linux. Probably they will work about the same on recent versions of Ubuntu.
For other operating systems, you may need to adapt the instructions to suit
your local setup.


## Build & Run

To get started, you need to activate your Python venv that contains `west` and
change into the `zphqst-03` directory within your Zephyr project directory.

For example:

```
$ cd ~/code/zephyr-workspace
$ source .venv/bin/activate
(.venv) $ cd zphqst-03
```

The examples below use `make` in a terminal on Debian 12 to run commands for
make targets defined in the zphqst-03 [Makefile](Makefile). Using `make`
avoids a lot of typing that would otherwise be required to provide commandline
options to `west`.

To build and flash the [IoT toggle switch](app) app:

```
make clean
make app
make flash
```


### Provision Network Credentials

When you first install the app, in order to connect to the network, you must
first provision the board with Wifi and MQTT login credentials. To do that,
you connect by USB serial to the Zephyr shell and use the `settings` command.

Depending on how you've used your Feather TFT board previously, it might
already have data written to the settings partition used by Zephyr's
Non-Volatile Storage (NVS) subsystem. In that case, you'll need to erase the
partition before writing the settings.

You can access the Zephyr shell with the command:

```
make monitor
```

The `make monitor` command runs `west espressif monitor` (keyboard shortcut to
exit the serial monitor is **Ctrl+]**). If you prefer a different serial
monitor program, like `tio` or `screen`, you could try one of these:

```
tio /dev/ttyACM0
screen -fn /dev/ttyACM0 115200
```

Once you are in the Zephyr shell, you should see the `uart:~$` prompt. If not,
press the Enter key on your keyboard a time or two. At the prompt, to
provision your network credentials, you can write values to NVM flash using the
`settings` shell command provided by Zephyr's
[Settings](https://docs.zephyrproject.org/latest/services/storage/settings/index.html)
subsystem.

The Zephyr Settings subsystem is a key-value store. For this application, I've
configured it to use the
[Non-Volatile Storage (NVS)](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
subsystem for persistent storage. These three settings keys store the network
provisioning details for Wifi and MQTT:

| Key | Description |
| --- | ----------- |
| zq3/ssid | Wifi ssid (use quotes if it has spaces) |
| zq3/psk | Wifi WPA2-PSK passphrase (use quotes if it has spaces) |
| zq3/url | MQTT broker url: `mqtt[s]://<user>:<pass>@<hostname>/<topic>` |

Here is an example provisioning for a private test network with a local MQTT
broker listening on port 1883 of 192.168.0.100, with no encryption and
anonymous connections enabled (username and password can be blank):

```
uart:~$ settings write string zq3/ssid MySSID
uart:~$ settings write string zq3/psk "my wifi passphrase"
uart:~$ settings write string zq3/url mqtt://:@192.168.0.100/test
```

If you try writing the settings and get an error, check the section below
about erasing the NVM flash partition.


### Connecting to Wifi and MQTT

Once you have written the settings, you need to load them from NVM flash. You
can do this by either resetting the board (settings are loaded at boot) or by
running the `aio reload` Zephyr shell command.

Once the settings are loaded, you should see a "Press BOOT button to connect"
message on the Feather TFT's screen.

Press the Feather TFT's Boot button. You should see a "Connecting..." message.
Once wifi is up, the wifi icon in the statusbar (top right) should turn from
gray to green. When MQTT connects, you should see a large toggle switch widget
in the center of the screen.

If there are Wifi or MQTT connection errors, you should see an error message
on the Feather TFT's screen. To troubleshoot the problem, it's best to connect
to the serial shell so you can see more detailed error messages.

Troubleshooting Checklist:

1. Is your Wifi router working? Can you connect to it with another device?

2. Does your Wifi router use WPA2-PSK? If you need to use a different type of
   authentication or encryption, you will need to change the code as it is
   hardcoded for WPA2-PSK.

3. Does your Wifi use a captive portal? In that case, it won't work.

4. Are the wifi SSID and PSK passphrase settings correct? You can check this
   in the serial shell with:

   ```
   uart:~$ settings read zq3/ssid
   00000000: 4d 79 53 53 49 44 00                             |MySSID.          |
   uart:~$ settings read zq3/psk
   00000000: 6d 79 20 77 69 66 69 20  70 61 73 73 70 68 72 61 |my wifi  passphra|
   00000010: 73 65 00                                         |se.              |
   ```

5. Is your MQTT broker working? You can check this with the `mosquitto_pub`
   and `mosquitto_sub` MQTT command line tools for Linux. (check the following
   sections for more details on using mosquitto as a local MQTT broker).

6. Is the MQTT settings URL correct? The URL format is meant to match the
   format of `mosquitto_pub -L <URL> ...` and `mosquitto_sub -L <URL> ...`
   (except this app always requires the `:` and `@`).

7. Is your account being throttled by the MQTT broker because of exceeding
   rate limits? For example, if you use Adafruit IO, you can read about
   throttling in the
   [Adafruit IO MQTT API](https://io.adafruit.com/api/docs/mqtt.html#adafruit-io-mqtt-api)
   web docs.


## Erasing the NVM Flash Partition

The settings API uses the NVM backend with the `storage` partition that is
defined as one of Espressif's default partitions in
`zephyr/dts/common/espressif/partitions_0x0_amp_4M.dtsi`. My app's
Devicetree configuration gives this partition the label of "settings".

Depending on how you used your Feather TFT ESP32-S3 board previously, there
may be existing data in the storage partition. In that case, you might need
to erase it.

One option to erase the NVM partition would be to erase all the flash with the
[Adafruit ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)
ESP32 web flasher tool, then re-program the bootloader and firmware using
`west flash`.

You could also use Zephyr's `flash_map list` shell command to find the
partition labeled "settings" and determine its start offset and size. In the
example below, those are 0x3b0000 and 0x30000. Then, you could use the
`flash erase` shell command to erase the flash blocks for that partition.

For example:

```
uart:~$ flash_map list
ID | Device     | Device Name               | Label          | Offset   | Size
----------------------------------------------------------------------------------
 0   0x3c0a91e8   flash-controller@60002000   mcuboot          0x0        0x20000
 1   0x3c0a91e8   flash-controller@60002000   image-0          0x20000    0x150000
 2   0x3c0a91e8   flash-controller@60002000   image-1          0x170000   0x150000
 3   0x3c0a91e8   flash-controller@60002000   image-0-appcpu   0x2c0000   0x70000
 4   0x3c0a91e8   flash-controller@60002000   image-1-appcpu   0x330000   0x70000
 5   0x3c0a91e8   flash-controller@60002000   image-0-lpcore   0x3a0000   0x8000
 6   0x3c0a91e8   flash-controller@60002000   image-1-lpcore   0x3a8000   0x8000
 7   0x3c0a91e8   flash-controller@60002000   settings         0x3b0000   0x30000
 8   0x3c0a91e8   flash-controller@60002000   image-scratch    0x3e0000   0x1f000
 9   0x3c0a91e8   flash-controller@60002000   coredump         0x3ff000   0x1000
uart:~$
uart:~$ flash erase flash-controller@60002000 0x3b0000 0x30000
Erase success.
uart:~$ settings list
uart:~$
```

If you want to explore the various flash related shell commands, you can try
reading their help messages:

```
uart:~$ flash_map -h
uart:~$ flash -h
uart:~$ settings -h
```

To see the Kconfig options that enable those shell commands, check out
[app/prj.conf](app/prj.conf).



## Local MQTT Broker for Testing

**CAUTION:** This technique is risky if you don't take proper precautions.
**Don't do this on public wifi** or on a server that has a network interface
with a public IP address. On a private home network, behind a NAT router, it's
probably fine, but it would be safer to turn it off when you're done testing.

For testing MQTT stuff, it can be helpful to run your own MQTT broker locally.
There are a lot of steps involved in setting up the Zephyr code for an MQTT
client. Attempting to do all of that at once, including TLS certificates and
authentication, may be difficult to debug. It may help to start with basic
unencrypted (non-TLS) MQTT connections on a private network, then
incrementally add encryption and authentication.

This is how I set up a Debian box with the `mosquitto` MQTT broker for
unencrypted and unauthenticated access on my wifi test network:

Install packages:
```
$ sudo apt install mosquitto mosquitto-clients
```

Check the IP address assigned to my wifi interface:
```
$ hostname -I | grep -o '192[^ ]*'
192.168.0.100
```

Reconfigure `mosquitto` MQTT broker to listen on wifi IP (DANGER!)
```
$ cat <<EOF | sudo tee /etc/mosquitto/conf.d/LAN-listener.conf
persistence false
allow_anonymous true
listener 1883 192.168.0.100
listener 1883 127.0.0.1
EOF
$ sudo systemctl restart mosquitto
```

Remove the risky unencrypted configuration after initial testing is done:
```
$ sudo rm /etc/mosquitto/conf.d/LAN-listener.conf
$ sudo systemctl restart mosquitto
```


## Using `mosquitto_pub` and `mosquitto_sub` for testing

On Debian, if you install the `mosquitto-clents` package, you can publish and
subscribe to MQTT topics from the command line. For example, assuming you were
running an MQTT broker listening on IP address 192.168.0.100, you could start
two terminal windows (or use `tmux`), and do this:

Terminal 1 (use Ctrl-C to disconnect `mosquitto_sub` when done):
```
$ mosquitto_sub --debug -v -L mqtt://192.168.0.100:1883/test
Client (null) sending CONNECT
Client (null) received CONNACK (0)
Client (null) sending SUBSCRIBE (Mid: 1, Topic: test, QoS: 0, Options: 0x00)
Client (null) received SUBACK
Subscribed (mid: 1): 0
Client (null) received PUBLISH (d0, q0, r0, m0, 'test', ... (11 bytes))
test hello world
^CClient (null) sending DISCONNECT
```

Terminal 2:
```
$ mosquitto_pub --debug -L mqtt://192.168.0.100:1883/test -m "hello world"
Client (null) sending CONNECT
Client (null) received CONNACK (0)
Client (null) sending PUBLISH (d0, q0, r0, m1, 'test', ... (11 bytes))
Client (null) sending DISCONNECT
```

If you wanted to test an authenticated and TLS encrypted connection to the
Adafruit IO MQTT broker, you could do something like this (replacing `$USER`
and `$KEY` with your AIO username and API key):

Terminal 1:
```
$ mosquitto_sub -L mqtts://$USER:$KEY@io.adafruit.com:8883/$USER/f/test
```

Terminal 2:
```
$ mosquitto_pub -L mqtts://$USER:$KEY@io.adafruit.com:8883/$USER/f/test -m 1
```


## 2025-03-14 Update: New official Zephyr board defs

Pull request
[boards: adafruit: add initial support for esp32s3 feather \# 75844](https://github.com/zephyrproject-rtos/zephyr/pull/75844)
of the zephyr-rtos/zephyr GitHub repo was merged on March 14, 2025, adding
official support for the Adafruit Feather ESP32-S3 and Feather TFT ESP32-S3
boards. I hadn't previously noticed that PR or the related issue,
[Add Support for Adafruit ESP32-S3 and ESP32-S2 Feather Boards \# 68512](https://github.com/zephyrproject-rtos/zephyr/issues/68512).

After reviewing the new board defs, I decided to continue using my board def in
this repo, mainly because it configures the ST7789V display driver a bit
differently (hardware rotation, gamma curve, etc).


## License

Files in this repo use a mix of **Apache-2.0** and/or **MIT** licensing. Check
SPDX header comments for details.
