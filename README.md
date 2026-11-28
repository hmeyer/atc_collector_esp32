# atc_collector_esp32

ESPHome firmware that turns small ESP32 boards into **Bluetooth proxies for
Home Assistant**. They extend HA's Bluetooth range so
Xiaomi thermometers (LYWSD03MMC etc., e.g. running
[pvvx ATC_MiThermometer](https://github.com/pvvx/ATC_MiThermometer)) anywhere in
the flat show up in HA's Xiaomi BLE / BTHome integrations. HA combines all
proxies and its own adapter, so a sensor heard by several boards is still one
device.

| Board | Config | Display | Extras |
|---|---|---|---|
| "ESP32-C3 OLED Entwicklungsboard" | `esphome/atc-c3.yaml` | 0.42" 72x40 OLED (I2C) | |
| [Waveshare ESP32-S3-LCD-1.47](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47) | `esphome/atc-s3.yaml` | 1.47" 172x320 ST7789 (SPI), landscape | SCD4x CO₂, CC1101 433 MHz |
| [LILYGO TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display) (ESP32) | `esphome/atc-tdisplay.yaml` | 1.14" 135x240 ST7789 (SPI), landscape | SCD4x CO₂ |

Shared settings are in `esphome/common.yaml`:

- **Passive proxy**: only forwards advertisements, never connects to devices.
- **Diagnostics in HA**: Wi-Fi signal, uptime, advertisements per minute.
- **Self-updates**: the board checks its manifest on GitHub Pages after
  connecting, every hour and via a *Check for firmware update* button, and
  offers new firmware as an update entity in HA.

Boards with a screen show Wi-Fi signal, HA connection, BLE advertisements per
minute and uptime; the BOOT button, or the *Display* switch in HA, turns it on
and off (`esphome/display_toggle.yaml`). `esphome/co2.yaml` and
`esphome/radio433.yaml` add the SCD4x and CC1101.

## Flashing and setup (browser only)

1. Open [hmeyer.github.io/atc_collector_esp32](https://hmeyer.github.io/atc_collector_esp32/)
   in desktop Chrome or Edge, plug in the board, click **Connect** under your
   board and install.
2. Choose **Configure Wi-Fi** after installing. Without it, the board opens a
   hotspot named after itself with a setup page.
3. HA lists the board under *Settings → Devices & services* as a discovered
   ESPHome device. Configure it; no key is needed (see Security).
4. Newly reachable sensors appear as discovered Xiaomi BLE / BTHome devices.

Every push to `main` builds all boards on GitHub Actions and publishes the
flasher, the images and one `<board>.manifest.json` per board to GitHub Pages.
Each board's firmware version is a hash of its resolved ESPHome config (plus
the ESPHome version), so a board only rebuilds and is only offered an update
when its own firmware changed. The old shared `manifest.json` (C3 and
Waveshare S3) is still published for boards flashed before the per-board
manifests. Pull requests attach the images
as a workflow artifact.

One-time repo setup: *Settings → Pages → Source: GitHub Actions*. Pages needs
the repository to be public (on the free plan).

## Security

The firmware is public, so nothing secret is compiled in:

- Wi-Fi is provisioned at runtime (Improv / hotspot) and stored on the device.
- The HA connection is **not encrypted** for now. HA can't yet provision an
  encryption key at runtime, and a compiled-in key would be public. Anyone on
  the LAN can therefore connect to a board and receive what it hears. That's
  limited on purpose: the proxy is passive (it can't be used to connect to BLE
  devices such as locks) and the readings are unencrypted broadcasts anyway.
- Updates are pulled from GitHub Pages; there's no ESPHome OTA endpoint for
  pushing firmware from the LAN. Exception: while the fallback hotspot is
  active (Wi-Fi unreachable for about a minute), the captive portal's web
  server, which includes a firmware upload page, runs without a password.

## Hardware notes

ESP32-C3 OLED board: OLED on I2C SDA=5, SCL=6, BOOT button GPIO9. The OLED
controller has 132 columns (SH1106-like), so it's driven with the SH1106 model;
the 72x40 glass is at (28, 12) of the frame buffer (`oled_x`/`oled_y` in
`atc-c3.yaml`). Wi-Fi transmit power is reduced to 8.5 dB because full power
causes dropouts on these boards.

Waveshare ESP32-S3-LCD-1.47: LCD MOSI/SCLK/CS/DC/RST = 45/40/42/41/39,
backlight 48, BOOT button GPIO0. Header: SCD4x SCL/SDA = 1/2; CC1101
CSN/SCK/MOSI/MISO/GDO0 = 10/12/11/13/9, tuned to 433.42 MHz (Somfy RTS). Only
GDO0 is used, shared by receiver and transmitter (open-drain): the GDO2 line
(wired to 8) carried no signal in testing.

Somfy RTS blinds (`esphome/somfy_rts.yaml`, one `esphome/somfy_blind.yaml`
include per blind): the board is an additional RTS remote per blind, with its
own address (from the MAC) and a rolling code kept in flash. In HA each blind
is a cover (up / down / stop = "My") plus a *my position* and a *pair* button.
To pair, hold PROG on the blind's original remote until it jogs, then press
*pair* in HA; the original remote keeps working. RTS gives no position
feedback.

LILYGO TTGO T-Display: display via ESPHome's `T-DISPLAY` preset (SPI 18/19,
CS 5, DC 16, RST 23), backlight GPIO4 (dimmable), BOOT button GPIO0. The SCD4x
plugs straight into the header row G G 21 22 at the end opposite USB:
GND -> G, SCL -> GPIO21, SDA -> GPIO22. Its VDD pin is removed from the header
(it would land on the second G) and wired to 3V on the other row instead. Not
5V: the breakout's pull-ups would put 5 V on the I2C pins.

Its LiPo connector charges a battery, but with continuous Bluetooth scanning
and Wi-Fi it only lasts hours, so treat it as a backup at most.

## Development

```
uv venv && source .venv/bin/activate
uv pip install esphome==2026.9.1
esphome compile esphome/atc-c3.yaml   # or atc-s3, atc-tdisplay
pio test -e native                    # unit tests for lib/atc_parser
```

`lib/atc_parser` is a host-tested parser for pvvx ATC_MiThermometer
advertisements (atc1441, pvvx custom, BTHome v2), kept for showing sensor
readings on the boards' displays.
