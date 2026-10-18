// SSD1306 OLED over I2C (ESP32-C3 OLED board: 0.42" 72x40, or 128x64).

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "display.h"

#ifndef OLED_SDA
#define OLED_SDA 5
#endif
#ifndef OLED_SCL
#define OLED_SCL 6
#endif
#ifndef OLED_72X40
#define OLED_72X40 1
#endif
// Position of the visible 72x40 window in the controller's 128x64 RAM.
#ifndef OLED_X_OFFSET
#define OLED_X_OFFSET 30
#endif
#ifndef OLED_Y_OFFSET
#define OLED_Y_OFFSET 12
#endif

namespace display {
namespace {

// The 0.42" panel shows a 72x40 window of the SSD1306's 128x64 RAM. u8g2's
// own 72X40_ER driver assumes a different window position on these boards
// (clipped text, garbage columns), so drive the full 128x64 and draw at an
// offset instead.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#if OLED_72X40
const uint8_t* const kFont = u8g2_font_5x8_tf;
constexpr int kLineHeight = 8;
constexpr int kX = OLED_X_OFFSET, kY = OLED_Y_OFFSET, kW = 72, kH = 40;
#else
const uint8_t* const kFont = u8g2_font_7x13_tf;
constexpr int kLineHeight = 13;
constexpr int kX = 0, kY = 0, kW = 128, kH = 64;
#endif

void drawLine(int line, const char* fmt, ...) {
  char buf[32];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  oled.drawStr(kX, kY + (line + 1) * kLineHeight - 1, buf);
}

}  // namespace

void begin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  oled.begin();
  oled.setFont(kFont);
  oled.setClipWindow(kX, kY, kX + kW, kY + kH);
}

void showStarting() {
  // A frame around the visible area: if an edge is missing or doubled,
  // adjust OLED_X_OFFSET / OLED_Y_OFFSET.
  oled.clearBuffer();
  oled.drawFrame(kX, kY, kW, kH);
  oled.drawStr(kX + 3, kY + 2 * kLineHeight - 1, "ATC");
  oled.drawStr(kX + 3, kY + 3 * kLineHeight - 1, "collector");
  oled.sendBuffer();
  delay(1500);
}

void showIdle(uint32_t now_ms, uint32_t adv_seen) {
  static const char kSpinner[] = "|/-\\";
  oled.clearBuffer();
  drawLine(0, "Scanning %c", kSpinner[(now_ms / 250) % 4]);
  drawLine(1, "no sensors");
  drawLine(2, "adv %lu", (unsigned long)adv_seen);
  drawLine(3, "up %lus", (unsigned long)(now_ms / 1000));
  oled.sendBuffer();
}

void showDevice(const atc::Device& d, size_t index, size_t count, uint32_t now_ms) {
  const auto& r = d.data;
  oled.clearBuffer();
  // Name, or the MAC suffix (pvvx default names are ATC_xxxxxx anyway).
  if (d.name[0]) {
    drawLine(0, "%u/%u %s", unsigned(index + 1), unsigned(count), d.name);
  } else {
    drawLine(0, "%u/%u %02X%02X%02X", unsigned(index + 1), unsigned(count),
             d.mac[3], d.mac[4], d.mac[5]);
  }
  char t[12] = "--", h[12] = "--";
  if (r.has_temperature) snprintf(t, sizeof(t), "%.1f\xB0" "C", r.temperature_c);
  if (r.has_humidity) snprintf(h, sizeof(h), "%.0f%%", r.humidity_pct);
  drawLine(1, "%s %s", t, h);
  drawLine(2, "Bat %u%% %.2fV", r.has_battery_pct ? r.battery_pct : 0,
           r.has_battery_v ? r.battery_v : 0.0f);
  drawLine(3, "%ddBm %lus", d.rssi, (unsigned long)((now_ms - d.last_seen_ms) / 1000));
  drawLine(4, "%s n=%lu", atc::formatName(r.format), (unsigned long)d.packets);
  oled.sendBuffer();
}

}  // namespace display
