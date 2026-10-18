# atc_collector_esp32

BLE collector for Xiaomi Mijia thermometers (LYWSD03MMC and similar) running
[pvvx ATC_MiThermometer](https://github.com/pvvx/ATC_MiThermometer), for the
"ESP32-C3 OLED Entwicklungsboard" (ESP32-C3 with a 0.42" 72x40 SSD1306).
This is the ESP32 counterpart of
[atc_mi_collector](https://github.com/hmeyer/atc_mi_collector). It will
eventually report to Home Assistant. For now it's a sign of life: it shows the
received readings on the OLED and logs them to the USB serial console.

## Features

- Passively collects advertisements in all unencrypted pvvx formats: **pvvx
  custom**, **atc1441** and **BTHome v2**. Encrypted formats are ignored.
- OLED: one page per sensor (name, temperature, humidity, battery, RSSI, age,
  format, number of measurements). Pages advance every 4 s; the BOOT button
  skips to the next one. With no sensors it shows a scanning screen that
  counts all BLE advertisements seen.
- Sensors that stay silent for 10 minutes are dropped. Up to 16 sensors are tracked.

## Flashing (browser only, nothing to install)

Every push builds the firmware on GitHub Actions. On the default branch it
publishes a web flasher (ESP Web Tools) to GitHub Pages:

1. One-time setup: in the repo, go to *Settings → Pages* and set **Source** to
   **GitHub Actions**.
2. Open `https://hmeyer.github.io/atc_collector_esp32/` in desktop Chrome or Edge,
   plug in the board, and click **Connect**.

Every workflow run (any branch) also attaches the merged image
`atc_collector_esp32c3.bin` as an artifact. It can be flashed at offset `0x0`
with any esptool frontend, e.g. https://espressif.github.io/esptool-js/.

## Hardware notes

| | GPIO |
|---|---|
| OLED SDA | 5 |
| OLED SCL | 6 |
| BOOT button | 9 |

Other variants: change `OLED_SDA`, `OLED_SCL`, and `OLED_72X40=0` (128x64
panel) in `platformio.ini`.

## Development

```
pio test -e native      # parser unit tests on the host
pio run -e esp32c3_oled # firmware; also produces .pio/build/esp32c3_oled/merged.bin
```

The parser (`lib/atc_parser`) has no Arduino dependencies.
