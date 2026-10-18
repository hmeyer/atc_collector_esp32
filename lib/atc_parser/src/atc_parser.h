// Parser for BLE advertisements sent by Xiaomi LYWSD03MMC (and similar)
// thermometers running https://github.com/pvvx/ATC_MiThermometer.
//
// Supported advertising formats (selectable in the pvvx flasher):
//   - "atc1441": service data UUID 0x181A, 13 bytes, big endian, 0.1 °C.
//   - "pvvx custom": service data UUID 0x181A, 15 bytes, little endian, 0.01 °C.
//   - "BTHome v2" (unencrypted): service data UUID 0xFCD2.
// Encrypted formats (BTHome encrypted, Mi "MiBeacon") are recognised but ignored.
//
// Plain C++ without Arduino dependencies so it can be unit tested on the host.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace atc {

enum class Format : uint8_t { None, Atc1441, Pvvx, BTHome };

const char* formatName(Format f);

// One decoded advertisement. Only fields with has_* set are valid; a BTHome
// sensor may spread its values over several advertisements.
struct Reading {
  Format format = Format::None;
  bool has_temperature = false;
  float temperature_c = 0;
  bool has_humidity = false;
  float humidity_pct = 0;
  bool has_battery_pct = false;
  uint8_t battery_pct = 0;
  bool has_battery_v = false;
  float battery_v = 0;
  bool has_counter = false;
  uint8_t counter = 0;
};

constexpr size_t kMaxNameLen = 20;

// Parses a raw advertisement / scan response payload (a sequence of AD
// structures). Returns true if sensor data was found. If `name` is non-null
// and the payload contains a (short or complete) local name, it is copied
// there NUL-terminated (truncated to kMaxNameLen) and `*has_name` is set.
bool parseAdvertisement(const uint8_t* payload, size_t len, Reading& out,
                        char* name = nullptr, bool* has_name = nullptr);

// Parses the content of one "Service Data - 16 bit UUID" AD structure
// (i.e. starting with the little endian UUID). Returns true on sensor data.
bool parseServiceData(const uint8_t* data, size_t len, Reading& out);

// ---------------------------------------------------------------------------
// Table of recently seen sensors.

struct Device {
  bool used = false;
  uint8_t mac[6] = {};  // Most significant byte first, as printed.
  char name[kMaxNameLen + 1] = {};
  Reading data;         // Merged state of all readings.
  uint32_t last_seen_ms = 0;
  uint32_t packets = 0;  // Advertisements carrying new sensor data.
  int8_t rssi = 0;
};

class DeviceTable {
 public:
  static constexpr size_t kCapacity = 16;

  // Merges a reading into the entry for `mac` (creating it, possibly evicting
  // the least recently seen entry). `name` may be null. Returns the entry or
  // null if the reading was dropped. If `is_new` is non-null it is set to
  // whether this was a new measurement rather than a repeated advertisement.
  Device* update(const uint8_t mac[6], const Reading& r, const char* name,
                 int8_t rssi, uint32_t now_ms, bool* is_new = nullptr);

  // Sets the name of an already known device (names may arrive in a scan
  // response without sensor data).
  void setName(const uint8_t mac[6], const char* name);

  // Removes devices not seen for `max_age_ms`.
  void prune(uint32_t now_ms, uint32_t max_age_ms);

  size_t size() const;
  // i-th used entry in insertion-slot order, or null.
  const Device* at(size_t i) const;

 private:
  Device devices_[kCapacity];
};

}  // namespace atc
