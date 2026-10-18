# atc_collector_esp32

BLE collector for Xiaomi Mijia thermometers (LYWSD03MMC and similar) running
[pvvx ATC_MiThermometer](https://github.com/pvvx/ATC_MiThermometer), for these boards:

| Board | PlatformIO env | Display |
|---|---|---|
| "ESP32-C3 OLED Entwicklungsboard" | `esp32c3_oled` | 0.42" 72x40 SSD1306 (I2C) |
| [Waveshare ESP32-S3-LCD-1.47](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47) | `esp32s3_lcd147` | 1.47" 172x320 ST7789 (SPI), used in landscape |

This is the ESP32 counterpart of
[atc_mi_collector](https://github.com/hmeyer/atc_mi_collector). It will
eventually report to Home Assistant. For now it's a sign of life: it shows the
received readings on the display and logs them to the USB serial console.

## Features

- Passively collects advertisements in all unencrypted pvvx formats: **pvvx
  custom**, **atc1441** and **BTHome v2**. Encrypted formats are ignored.
- Display: one page per sensor (name, temperature, humidity, battery, RSSI, age,
  format, number of measurements). Pages advance every 4 s; the BOOT button
  skips to the next one. The color LCD color-codes temperature and battery. With no sensors it shows a scanning screen that
  counts all BLE advertisements seen.
- Sensors that stay silent for 10 minutes are dropped. Up to 16 sensors are tracked.

## Flashing (browser only, nothing to install)

Every push builds the firmware on GitHub Actions. On the default branch it
publishes a web flasher (ESP Web Tools) to GitHub Pages:

1. One-time setup: in the repo, go to *Settings → Pages* and set **Source** to
   **GitHub Actions**.
2. Open [hmeyer.github.io/atc_collector_esp32](https://hmeyer.github.io/atc_collector_esp32/) in desktop Chrome or Edge,
   plug in the board, and click **Connect**.

The flasher picks the right firmware from the connected chip (C3 or S3).
Every workflow run (any branch) also attaches the merged images
`atc_collector_<env>.bin` as an artifact. It can be flashed at offset `0x0`
with any esptool frontend, e.g. https://espressif.github.io/esptool-js/.

## Hardware notes

ESP32-C3 OLED board:

| | GPIO |
|---|---|
| OLED SDA | 5 |
| OLED SCL | 6 |
| BOOT button | 9 |

Other variants: change `OLED_SDA`, `OLED_SCL`, and `OLED_72X40=0` (128x64
panel) in `platformio.ini`. If the frame on the startup screen is clipped or
shifted, adjust `OLED_X_OFFSET` / `OLED_Y_OFFSET` (position of the visible
72x40 window in the controller's 128x64 RAM).

Waveshare ESP32-S3-LCD-1.47:

| | GPIO |
|---|---|
| LCD MOSI / SCLK / CS / DC / RST | 45 / 40 / 42 / 41 / 39 |
| LCD backlight | 48 |
| BOOT button | 0 |

A new board needs a `display_<name>.cpp` implementing `src/display.h` and a
PlatformIO env that selects it via `build_src_filter`.

## Development

```
pio test -e native      # parser unit tests on the host
pio run                 # firmware for all boards; also .pio/build/<env>/merged.bin
```

The parser (`lib/atc_parser`) has no Arduino dependencies.
