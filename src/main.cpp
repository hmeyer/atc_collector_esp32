// BLE collector for Xiaomi thermometers running pvvx ATC_MiThermometer
// firmware, for ESP32 boards with a small display (see display.h).
//
// For now it only shows the received data on the display (one sensor per
// page) and logs it to the USB serial console. Home Assistant export comes
// later.

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "atc_parser.h"
#include "display.h"

#ifndef BUTTON_PIN
#define BUTTON_PIN 9
#endif

namespace {

constexpr int kButtonPin = BUTTON_PIN;             // BOOT button: next page.
constexpr uint32_t kPageMs = 4000;                 // Auto advance interval.
constexpr uint32_t kRefreshMs = 250;
constexpr uint32_t kMaxDeviceAgeMs = 10 * 60 * 1000;  // Drop silent sensors.

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

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kButtonPin, INPUT_PULLUP);

  display::begin();
  display::showStarting();

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

  // Copy the entry under the lock; drawing takes a while.
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

  if (count) {
    display::showDevice(current, page, count, now);
  } else {
    display::showIdle(now, adv_seen);
  }
}
