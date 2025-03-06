# SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
# SPDX-License-Identifier: MIT
#
# Bitfield encoder for ST7789V MADCTL (0x36) command


def madctl(my=0, mx=0, mv=0, ml=0, rgb=0, mh=0):
    p = (my << 7) | (mx << 6) | (mv << 5) | (ml << 4) | (rgb << 3) | (mh << 2)
    desc = {}
    desc['MY' ] = "bottom to top" if (p & 0x80) else "top to bottom"
    desc['MX' ] = "right to left" if (p & 0x40) else "left to right"
    desc['MV' ] = "reverse mode" if (p & 0x20) else "normal mode"
    desc['ML' ] = "bottom to top" if (p & 0x10) else "top to bottom"
    desc['RGB'] = "BGR" if (p & 0x08) else "RGB"
    desc['MH' ] = "right to left" if (p & 0x04) else "left to right"
    return f""" parameter = 0x{p:02x}
 D7 Page Address Order (MY): {desc['MY']}
 D6 Column Address Order (MX): {desc['MX']}
 D5 Page/Column Order (MV): {desc['MV']}
 D5 Line Address Order (ML): {desc['ML']}
 D5 RGB/BGR Order (RGB): {desc['RGB']}
 D5 Display Data Latch Order (MH): {desc['MH']}

"""

# Rotations refer to the Adafruit Feather TFT ESP32-S3 dev board

print("default (top left pixel by Boot button):")
print(madctl(my=0, mx=0, mv=0, rgb=1, mh=1))

print("90° CW (top left by battery jack):")
print(madctl(my=0, mx=1, mv=1, rgb=1, mh=1))

print("180° CW (top left by SDA pin):")
print(madctl(my=1, mx=1, mv=0, rgb=1, mh=1))

print("270° CW (top left by Adafruit logo):")
print(madctl(my=1, mx=0, mv=1, rgb=1, mh=1))
