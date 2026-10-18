// ST7789 color LCD over SPI (Waveshare ESP32-S3-LCD-1.47: 172x320, used in
// landscape). Drawing goes to an in-RAM canvas that is flushed in one go, so
// the periodic redraw doesn't flicker.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "display.h"

#ifndef LCD_MOSI
#define LCD_MOSI 45
#endif
#ifndef LCD_SCLK
#define LCD_SCLK 40
#endif
#ifndef LCD_CS
#define LCD_CS 42
#endif
#ifndef LCD_DC
#define LCD_DC 41
#endif
#ifndef LCD_RST
#define LCD_RST 39
#endif
#ifndef LCD_BL
#define LCD_BL 48
#endif

namespace display {
namespace {

constexpr int kWidth = 320;  // landscape
constexpr int kHeight = 172;

// RGB565
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGrey = 0x8410;
constexpr uint16_t kHeader = 0x10A2;
constexpr uint16_t kBlue = 0x5D1F;
constexpr uint16_t kGreen = 0x3666;
constexpr uint16_t kOrange = 0xFC60;
constexpr uint16_t kRed = 0xF8A2;
constexpr uint16_t kCyan = 0x5F7F;

Arduino_DataBus* bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED);
// The 172 px wide panel sits at column offset 34 of the controller's 240.
Arduino_GFX* panel = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */,
                                        172, 320, 34, 0, 34, 0);
Arduino_GFX* gfx = new Arduino_Canvas(kWidth, kHeight, panel);

// Built-in 6x8 font scaled by `size`.
void text(int x, int y, uint8_t size, uint16_t color, const char* fmt, ...) {
  char buf[48];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(buf);
}

int textWidth(const char* s, uint8_t size) { return int(strlen(s)) * 6 * size; }

void header(const char* left, const char* right) {
  gfx->fillRect(0, 0, kWidth, 28, kHeader);
  text(6, 6, 2, kWhite, "%s", left);
  if (right) text(kWidth - 6 - textWidth(right, 2), 6, 2, kGrey, "%s", right);
}

uint16_t temperatureColor(float t) {
  if (t < 16) return kBlue;
  if (t < 25) return kGreen;
  if (t < 30) return kOrange;
  return kRed;
}

uint16_t batteryColor(uint8_t pct) {
  if (pct < 15) return kRed;
  if (pct < 40) return kOrange;
  return kGreen;
}

}  // namespace

void begin() {
  // Allocates the 320x172x2 byte frame buffer.
  if (!gfx->begin()) Serial.println("LCD init failed (out of memory?)");
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
}

void showStarting() {
  gfx->fillScreen(kBlack);
  header("ATC collector", nullptr);
  text(6, 50, 2, kWhite, "starting...");
  gfx->flush();
}

void showIdle(uint32_t now_ms, uint32_t adv_seen) {
  gfx->fillScreen(kBlack);
  header("ATC collector", nullptr);
  static const char* const kDots[] = {"", ".", "..", "..."};
  text(6, 46, 3, kCyan, "Scanning%s", kDots[(now_ms / 500) % 4]);
  text(6, 86, 2, kWhite, "no sensors yet");
  text(6, 116, 2, kGrey, "BLE adverts: %lu", (unsigned long)adv_seen);
  text(6, 142, 2, kGrey, "uptime: %lus", (unsigned long)(now_ms / 1000));
  gfx->flush();
}

void showDevice(const atc::Device& d, size_t index, size_t count, uint32_t now_ms) {
  const auto& r = d.data;
  gfx->fillScreen(kBlack);

  char name[atc::kMaxNameLen + 1];
  if (d.name[0]) {
    snprintf(name, sizeof(name), "%s", d.name);
  } else {
    snprintf(name, sizeof(name), "%02X%02X%02X", d.mac[3], d.mac[4], d.mac[5]);
  }
  char page[12];
  snprintf(page, sizeof(page), "%u/%u", unsigned(index + 1), unsigned(count));
  header(name, page);

  // Temperature, large, with a drawn degree sign (the font has none).
  if (r.has_temperature) {
    char t[12];
    snprintf(t, sizeof(t), "%.1f", r.temperature_c);
    const uint16_t c = temperatureColor(r.temperature_c);
    text(6, 42, 6, c, "%s", t);
    const int x = 6 + textWidth(t, 6) + 6;
    gfx->drawCircle(x + 5, 47, 5, c);
    gfx->drawCircle(x + 5, 47, 4, c);
    text(x + 14, 42, 3, c, "C");
  } else {
    text(6, 42, 6, kGrey, "--.-");
  }

  // Humidity to the right.
  if (r.has_humidity) {
    text(236, 46, 4, kCyan, "%.0f", r.humidity_pct);
    text(236, 84, 2, kCyan, "%% RH");
  }

  // Footer: battery, signal, age, format.
  if (r.has_battery_pct || r.has_battery_v) {
    text(6, 112, 2, r.has_battery_pct ? batteryColor(r.battery_pct) : kWhite,
         "Bat %u%%  %.2fV", r.has_battery_pct ? r.battery_pct : 0,
         r.has_battery_v ? r.battery_v : 0.0f);
  }
  text(6, 138, 2, kGrey, "%ddBm  %lus ago", d.rssi,
       (unsigned long)((now_ms - d.last_seen_ms) / 1000));
  text(6, 158, 1, kGrey, "%02X:%02X:%02X:%02X:%02X:%02X  %s  n=%lu", d.mac[0],
       d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5], atc::formatName(r.format),
       (unsigned long)d.packets);
  gfx->flush();
}

}  // namespace display
