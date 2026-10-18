// BLE collector for Xiaomi thermometers running pvvx ATC_MiThermometer
// firmware, for ESP32-C3 boards with a small SSD1306 OLED.
//
// For now it only shows the received data on the OLED (one sensor per page)
// and logs it to the USB serial console. Home Assistant export comes later.

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "atc_parser.h"

#ifndef OLED_SDA
#define OLED_SDA 5
#endif
#ifndef OLED_SCL
#define OLED_SCL 6
#endif
#ifndef OLED_72X40
#define OLED_72X40 1
#endif

namespace {

constexpr int kButtonPin = 9;                      // BOOT button: next page.
constexpr uint32_t kPageMs = 4000;                 // Auto advance interval.
constexpr uint32_t kRefreshMs = 250;
constexpr uint32_t kMaxDeviceAgeMs = 10 * 60 * 1000;  // Drop silent sensors.

#if OLED_72X40
// The 0.42" panel shows a 72x40 window of the SSD1306 RAM.
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
const uint8_t* const kFont = u8g2_font_5x8_tf;
constexpr int kLineHeight = 8;
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
const uint8_t* const kFont = u8g2_font_7x13_tf;
constexpr int kLineHeight = 13;
#endif

atc::DeviceTable devices;
SemaphoreHandle_t devices_mutex;
volatile uint32_t adv_seen = 0;  // All BLE advertisements, as sign of life.

void logReading(const atc::Device& d) {
  const auto& r = d.data;
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X %-12s %-7s", d.mac[0], d.mac[1],
                d.mac[2], d.mac[3], d.mac[4], d.mac[5], d.name,
                atc::formatName(r.format));
  if (r.has_temperature) Serial.printf(" T=%.2fC", r.temperature_c);
  if (r.has_humidity) Serial.printf(" H=%.2f%%", r.humidity_pct);
  if (r.has_battery_pct) Serial.printf(" B=%u%%", r.battery_pct);
  if (r.has_battery_v) Serial.printf(" %.3fV", r.battery_v);
  if (r.has_counter) Serial.printf(" #%u", r.counter);
  Serial.printf(" %ddBm\n", d.rssi);
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* adv) override {
    adv_seen = adv_seen + 1;
    const std::vector<uint8_t>& payload = adv->getPayload();
    atc::Reading reading;
    char name[atc::kMaxNameLen + 1] = {};
    bool has_name = false;
    const bool has_data = atc::parseAdvertisement(
        payload.data(), payload.size(), reading, name, &has_name);

    // NimBLE stores addresses little endian; the table wants print order.
    const uint8_t* le = adv->getAddress().getVal();
    const uint8_t mac[6] = {le[5], le[4], le[3], le[2], le[1], le[0]};

    xSemaphoreTake(devices_mutex, portMAX_DELAY);
    if (has_data) {
      bool is_new = false;
      const atc::Device* d =
          devices.update(mac, reading, has_name ? name : nullptr,
                         int8_t(adv->getRSSI()), millis(), &is_new);
      if (d && is_new) logReading(*d);
    } else if (has_name) {
      devices.setName(mac, name);
    }
    xSemaphoreGive(devices_mutex);
  }

  void onScanEnd(const NimBLEScanResults&, int reason) override {
    // Should not happen with an endless scan; restart just in case.
    Serial.printf("Scan ended (reason %d), restarting\n", reason);
    NimBLEDevice::getScan()->start(0, false, true);
  }
} scan_callbacks;

void startScan() {
  NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  // Report every advertisement, not only the first one per device.
  scan->setScanCallbacks(&scan_callbacks, /*wantDuplicates=*/true);
  // Active scanning to receive the device name from the scan response. This
  // costs the sensors a little battery; set to false if names don't matter.
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(100);
  scan->setMaxResults(0);  // Don't keep results in memory, callbacks only.
  scan->start(0, false, true);
}

void drawLine(int line, const char* fmt, ...) {
  char buf[32];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  display.drawStr(0, (line + 1) * kLineHeight - 1, buf);
}

void drawDevice(const atc::Device& d, size_t index, size_t count, uint32_t now) {
  const auto& r = d.data;
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
  drawLine(3, "%ddBm %lus", d.rssi, (unsigned long)((now - d.last_seen_ms) / 1000));
  drawLine(4, "%s n=%lu", atc::formatName(r.format), (unsigned long)d.packets);
}

void drawIdle(uint32_t now) {
  static const char kSpinner[] = "|/-\\";
  drawLine(0, "Scanning %c", kSpinner[(now / kRefreshMs) % 4]);
  drawLine(1, "no sensors");
  drawLine(2, "adv %lu", (unsigned long)adv_seen);
  drawLine(3, "up %lus", (unsigned long)(now / 1000));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kButtonPin, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin();
  display.setFont(kFont);
  display.clearBuffer();
  drawLine(0, "ATC collector");
  drawLine(1, "starting...");
  display.sendBuffer();

  devices_mutex = xSemaphoreCreateMutex();
  startScan();
  Serial.println("ATC collector: scanning for pvvx ATC_MiThermometer sensors");
}

void loop() {
  static uint32_t last_page_ms = 0;
  static uint32_t last_draw_ms = 0;
  static size_t page = 0;
  static bool button_was_down = false;
  const uint32_t now = millis();

  const bool button_down = digitalRead(kButtonPin) == LOW;
  const bool pressed = button_down && !button_was_down;
  button_was_down = button_down;
  if (pressed || now - last_page_ms >= kPageMs) {
    page++;
    last_page_ms = now;
  } else if (now - last_draw_ms < kRefreshMs) {
    delay(20);  // Keep polling the button between redraws.
    return;
  }
  last_draw_ms = now;

  // Copy the entry under the lock; drawing takes a while over I2C.
  atc::Device current;
  size_t count;
  xSemaphoreTake(devices_mutex, portMAX_DELAY);
  devices.prune(now, kMaxDeviceAgeMs);
  count = devices.size();
  if (count) {
    page %= count;
    current = *devices.at(page);
  }
  xSemaphoreGive(devices_mutex);

  display.clearBuffer();
  if (count) {
    drawDevice(current, page, count, now);
  } else {
    drawIdle(now);
  }
  display.sendBuffer();
}
