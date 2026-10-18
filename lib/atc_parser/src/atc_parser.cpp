#include "atc_parser.h"

#include <string.h>

namespace atc {
namespace {

constexpr uint8_t kAdNameShort = 0x08;
constexpr uint8_t kAdNameComplete = 0x09;
constexpr uint8_t kAdServiceData16 = 0x16;

constexpr uint16_t kUuidEnvironmentalSensing = 0x181A;  // atc1441 / pvvx
constexpr uint16_t kUuidBTHome = 0xFCD2;

// atc1441 / pvvx readings older than this do not block a lower resolution
// format from overwriting them.
constexpr uint32_t kPreferHighResMs = 5 * 60 * 1000;

uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

bool parseAtc1441(const uint8_t* d, Reading& out) {
  // MAC[6] temp(int16 BE, 0.1°C) hum(u8 %) batt(u8 %) batt_mv(u16 BE) count(u8)
  out.format = Format::Atc1441;
  out.has_temperature = true;
  out.temperature_c = int16_t(be16(d + 6)) / 10.0f;
  out.has_humidity = true;
  out.humidity_pct = d[8];
  out.has_battery_pct = true;
  out.battery_pct = d[9];
  out.has_battery_v = true;
  out.battery_v = be16(d + 10) / 1000.0f;
  out.has_counter = true;
  out.counter = d[12];
  return true;
}

bool parsePvvx(const uint8_t* d, Reading& out) {
  // MAC[6] temp(int16 LE, 0.01°C) hum(u16 LE, 0.01%) batt_mv(u16 LE)
  // batt(u8 %) count(u8) flags(u8)
  out.format = Format::Pvvx;
  out.has_temperature = true;
  out.temperature_c = int16_t(le16(d + 6)) / 100.0f;
  out.has_humidity = true;
  out.humidity_pct = le16(d + 8) / 100.0f;
  out.has_battery_v = true;
  out.battery_v = le16(d + 10) / 1000.0f;
  out.has_battery_pct = true;
  out.battery_pct = d[12];
  out.has_counter = true;
  out.counter = d[13];
  return true;
}

// Size of a BTHome v2 object value in bytes, -1 for variable length
// (length byte follows), 0 if unknown.
int bthomeObjectSize(uint8_t id) {
  if (id >= 0x0F && id <= 0x11) return 1;  // binary sensors
  if (id >= 0x15 && id <= 0x2D) return 1;  // binary sensors
  switch (id) {
    case 0x00: case 0x01: case 0x09: case 0x2E: case 0x2F: case 0x3A:
    case 0x46: case 0x57: case 0x58: case 0x59: case 0x60: case 0x64:
    case 0x65:
      return 1;
    case 0x02: case 0x03: case 0x06: case 0x07: case 0x08: case 0x0C:
    case 0x0D: case 0x0E: case 0x12: case 0x13: case 0x14: case 0x3C:
    case 0x3D: case 0x3F: case 0x40: case 0x41: case 0x43: case 0x44:
    case 0x45: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x51:
    case 0x52: case 0x5A: case 0x5D: case 0x5E: case 0x5F: case 0x61:
    case 0xF0:
      return 2;
    case 0x04: case 0x05: case 0x0A: case 0x0B: case 0x42: case 0x4B:
    case 0xF2:
      return 3;
    case 0x3E: case 0x4C: case 0x4D: case 0x4E: case 0x4F: case 0x50:
    case 0x55: case 0x5B: case 0x5C: case 0x62: case 0x63: case 0xF1:
      return 4;
    case 0x3B: case 0x53: case 0x54:
      return -1;
    default:
      return 0;
  }
}

bool parseBTHome(const uint8_t* d, size_t len, Reading& out) {
  if (len < 1) return false;
  const uint8_t info = d[0];
  if ((info >> 5) != 2) return false;  // not BTHome v2
  if (info & 0x01) return false;       // encrypted: no bind key support
  Reading r;
  r.format = Format::BTHome;
  bool any = false;
  size_t i = 1;
  while (i < len) {
    const uint8_t id = d[i++];
    int size = bthomeObjectSize(id);
    if (size < 0) {
      if (i >= len) break;
      size = d[i++];
    } else if (size == 0) {
      break;  // Unknown object: can't know its size, stop parsing.
    }
    if (i + size > len) break;
    const uint8_t* v = d + i;
    switch (id) {
      case 0x00: r.has_counter = true; r.counter = v[0]; break;
      case 0x01: r.has_battery_pct = true; r.battery_pct = v[0]; any = true; break;
      case 0x02: r.has_temperature = true; r.temperature_c = int16_t(le16(v)) / 100.0f; any = true; break;
      case 0x45: r.has_temperature = true; r.temperature_c = int16_t(le16(v)) / 10.0f; any = true; break;
      case 0x03: r.has_humidity = true; r.humidity_pct = le16(v) / 100.0f; any = true; break;
      case 0x2E: r.has_humidity = true; r.humidity_pct = v[0]; any = true; break;
      case 0x0C: r.has_battery_v = true; r.battery_v = le16(v) / 1000.0f; any = true; break;
      default: break;
    }
    i += size;
  }
  if (any) out = r;
  return any;
}

}  // namespace

const char* formatName(Format f) {
  switch (f) {
    case Format::Atc1441: return "atc1441";
    case Format::Pvvx: return "pvvx";
    case Format::BTHome: return "BTHome";
    default: return "-";
  }
}

bool parseServiceData(const uint8_t* data, size_t len, Reading& out) {
  if (len < 2) return false;
  const uint16_t uuid = le16(data);
  data += 2;
  len -= 2;
  if (uuid == kUuidEnvironmentalSensing) {
    if (len == 13) return parseAtc1441(data, out);
    if (len == 15) return parsePvvx(data, out);
    return false;
  }
  if (uuid == kUuidBTHome) return parseBTHome(data, len, out);
  return false;
}

bool parseAdvertisement(const uint8_t* payload, size_t len, Reading& out,
                        char* name, bool* has_name) {
  bool found = false;
  if (has_name) *has_name = false;
  size_t i = 0;
  while (i < len) {
    const uint8_t ad_len = payload[i];
    if (ad_len == 0) break;  // padding
    if (i + 1 + ad_len > len) break;  // malformed
    const uint8_t type = payload[i + 1];
    const uint8_t* ad = payload + i + 2;
    const size_t ad_data_len = ad_len - 1;
    if (type == kAdServiceData16) {
      Reading r;
      if (parseServiceData(ad, ad_data_len, r)) {
        out = r;
        found = true;
      }
    } else if ((type == kAdNameShort || type == kAdNameComplete) && name) {
      const size_t n = ad_data_len < kMaxNameLen ? ad_data_len : kMaxNameLen;
      memcpy(name, ad, n);
      name[n] = '\0';
      if (has_name) *has_name = true;
    }
    i += 1 + ad_len;
  }
  return found;
}

// ---------------------------------------------------------------------------

Device* DeviceTable::update(const uint8_t mac[6], const Reading& r,
                            const char* name, int8_t rssi, uint32_t now_ms,
                            bool* is_new) {
  if (is_new) *is_new = false;
  Device* dev = nullptr;
  Device* free_slot = nullptr;
  Device* oldest = nullptr;
  for (auto& d : devices_) {
    if (d.used && memcmp(d.mac, mac, 6) == 0) {
      dev = &d;
      break;
    }
    if (!d.used) {
      if (!free_slot) free_slot = &d;
    } else if (!oldest || int32_t(d.last_seen_ms - oldest->last_seen_ms) < 0) {
      oldest = &d;
    }
  }
  if (!dev) {
    dev = free_slot ? free_slot : oldest;
    *dev = Device();
    dev->used = true;
    memcpy(dev->mac, mac, 6);
  }

  // pvvx can be configured to alternate formats. Don't let low resolution
  // atc1441 data overwrite recent higher resolution data.
  if (r.format == Format::Atc1441 && dev->data.format != Format::None &&
      dev->data.format != Format::Atc1441 &&
      now_ms - dev->last_seen_ms < kPreferHighResMs) {
    dev->rssi = rssi;
    return nullptr;
  }

  Reading& m = dev->data;
  m.format = r.format;
  if (r.has_temperature) { m.has_temperature = true; m.temperature_c = r.temperature_c; }
  if (r.has_humidity) { m.has_humidity = true; m.humidity_pct = r.humidity_pct; }
  if (r.has_battery_pct) { m.has_battery_pct = true; m.battery_pct = r.battery_pct; }
  if (r.has_battery_v) { m.has_battery_v = true; m.battery_v = r.battery_v; }
  // Sensors repeat each advertisement several times; only count new ones.
  if (!r.has_counter || !m.has_counter || r.counter != m.counter) {
    dev->packets++;
    if (is_new) *is_new = true;
  }
  if (r.has_counter) { m.has_counter = true; m.counter = r.counter; }

  if (name && name[0]) {
    strncpy(dev->name, name, kMaxNameLen);
    dev->name[kMaxNameLen] = '\0';
  }
  dev->rssi = rssi;
  dev->last_seen_ms = now_ms;
  return dev;
}

void DeviceTable::setName(const uint8_t mac[6], const char* name) {
  if (!name || !name[0]) return;
  for (auto& d : devices_) {
    if (d.used && memcmp(d.mac, mac, 6) == 0) {
      strncpy(d.name, name, kMaxNameLen);
      d.name[kMaxNameLen] = '\0';
      return;
    }
  }
}

void DeviceTable::prune(uint32_t now_ms, uint32_t max_age_ms) {
  for (auto& d : devices_) {
    if (d.used && now_ms - d.last_seen_ms > max_age_ms) d = Device();
  }
}

size_t DeviceTable::size() const {
  size_t n = 0;
  for (const auto& d : devices_) n += d.used;
  return n;
}

const Device* DeviceTable::at(size_t i) const {
  for (const auto& d : devices_) {
    if (d.used && i-- == 0) return &d;
  }
  return nullptr;
}

}  // namespace atc
