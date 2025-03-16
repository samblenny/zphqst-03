<!-- SPDX-License-Identifier: MIT -->
<!-- SPDX-FileCopyrightText: Copyright 2025 Sam Blenny -->

# zphqst-03

**WORK IN PROGRESS**

This is meant for the Adafruit Feather TFT ESP32-S3 board.


## 2025-03-14 Update: New official Zephyr board defs

Pull request
[boards: adafruit: add initial support for esp32s3 feather \# 75844](https://github.com/zephyrproject-rtos/zephyr/pull/75844)
of the zephyr-rtos/zephyr GitHub repo was merged on March 14, 2025, adding
official support for the Adafruit Feather ESP32-S3 and Feather TFT ESP32-S3
boards. I hadn't previously noticed that PR or the related issue,
[Add Support for Adafruit ESP32-S3 and ESP32-S2 Feather Boards \# 68512](https://github.com/zephyrproject-rtos/zephyr/issues/68512).

I plan to review the PR changes and consider how they might interact with,
or conflict with, what I've been doing here while I was unaware of the pending
PR. Perhaps I can just switch over to using the official board def, but I'll
need to see how they handled things in the Devicetree files.


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


### LVGL GUI with Serial Shell Wifi/MQTT Config

This is a simple IoT remote control dashboard demo thing that demonstrates
button input, LVGL GUI widgets on the TFT, and networking with MQTT over wifi.
To keep the GUI code simple, this uses Zephyr's serial shell for entering
authentication credentials and starting the network connections.
(see [zphqst-03/samples/zphqst-03](samples/zphqst-03))

To build and run the demo:
```
(.venv) $ make clean && make shell && make flash
(.venv) $ make monitor
```

Once you're connected to the serial monitor, you can connect to a wifi network
with the `wifi scan` and `wifi connect` commands:

```
uart:~$ wifi scan
...
uart:~$ wifi connect -s "$SSID" -p "$PASSPHRASE" -k 1
uart:~$ wifi status
uart:~$ wifi disconnect
```


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
192.168.0.50
```

Reconfigure `mosquitto` MQTT broker to listen on wifi IP (DANGER!)
```
$ cat <<EOF | sudo tee /etc/mosquitto/conf.d/LAN-listener.conf
persistence false
allow_anonymous true
listener 1883 192.168.0.50
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
running an MQTT broker listening on IP address 192.168.0.50, you could start
two terminal windows (or use `tmux`), and do this:

Terminal 1 (use Ctrl-C to disconnect `mosquitto_sub` when done):
```
$ mosquitto_sub --debug -v -L mqtt://192.168.0.50:1883/test
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
$ mosquitto_pub --debug -L mqtt://192.168.0.50:1883/test -m "hello world"
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


## License

Files in this repo use a mix of **Apache-2.0** and/or **MIT** licensing. Check
SPDX header comments for details.
