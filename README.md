# atc_collector_esp32

ESPHome firmware that turns small ESP32 boards with a display into
**Bluetooth proxies for Home Assistant**. They extend HA's Bluetooth range so
Xiaomi thermometers (LYWSD03MMC etc., e.g. running
[pvvx ATC_MiThermometer](https://github.com/pvvx/ATC_MiThermometer)) anywhere in
the flat show up in HA's Xiaomi BLE / BTHome integrations. HA combines all
proxies and its own adapter, so a sensor heard by several boards is still one
device.

| Board | Config | Display |
|---|---|---|
| "ESP32-C3 OLED Entwicklungsboard" | `esphome/atc-c3.yaml` | 0.42" 72x40 SSD1306 (I2C) |
| [Waveshare ESP32-S3-LCD-1.47](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47) | `esphome/atc-s3.yaml` | 1.47" 172x320 ST7789 (SPI), landscape |

Shared settings are in `esphome/common.yaml`:

- **Passive proxy**: only forwards advertisements, never connects to devices.
- **Status screen**: Wi-Fi signal, HA connection, BLE advertisements per minute,
  uptime. The BOOT button, or the *Display* switch in HA, turns it on and off.
- **Diagnostics in HA**: Wi-Fi signal, uptime, advertisements per minute.
- **Self-updates**: the board checks the manifest on GitHub Pages every 6 h and
  offers new firmware as an update entity in HA.

## Flashing and setup (browser only)

1. Open [hmeyer.github.io/atc_collector_esp32](https://hmeyer.github.io/atc_collector_esp32/)
   in desktop Chrome or Edge, plug in the board, click **Connect** and install.
   The firmware for the connected chip (C3 or S3) is picked automatically.
2. Choose **Configure Wi-Fi** after installing. Without it, the board opens a
   hotspot named after itself with a setup page.
3. HA lists the board under *Settings → Devices & services* as a discovered
   ESPHome device. Configure it; no key is needed (see Security).
4. Newly reachable sensors appear as discovered Xiaomi BLE / BTHome devices.

Every push to `main` builds both boards on GitHub Actions and publishes the
flasher, the images and `manifest.json` to GitHub Pages. Pull requests attach
the images as a workflow artifact.

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
- Firmware can't be pushed from the LAN; boards only pull from GitHub Pages.

## Hardware notes

ESP32-C3 OLED board: OLED on I2C SDA=5, SCL=6, BOOT button GPIO9. The 72x40
glass sits at (30, 12) of the controller's 128x64 RAM (`oled_x`/`oled_y` in
`atc-c3.yaml`). Wi-Fi transmit power is reduced to 8.5 dB because full power
causes dropouts on these boards.

Waveshare ESP32-S3-LCD-1.47: LCD MOSI/SCLK/CS/DC/RST = 45/40/42/41/39,
backlight 48, BOOT button GPIO0.

## Development

```
pip install esphome==2026.9.1
esphome compile esphome/atc-c3.yaml   # or atc-s3.yaml
pio test -e native                    # unit tests for lib/atc_parser
```

`lib/atc_parser` is a host-tested parser for pvvx ATC_MiThermometer
advertisements (atc1441, pvvx custom, BTHome v2), kept for showing sensor
readings on the boards' displays.
