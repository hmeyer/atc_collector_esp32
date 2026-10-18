#include <string.h>
#include <unity.h>

#include "atc_parser.h"

using namespace atc;

void setUp() {}
void tearDown() {}

static const uint8_t kMac[6] = {0xA4, 0xC1, 0x38, 0x12, 0x34, 0x56};

// pvvx custom: 22.53 °C, 45.67 %, 2950 mV, 85 %, count 7, plus a name.
static const uint8_t kPvvxAdv[] = {
    0x02, 0x01, 0x06,                                // flags
    0x12, 0x16, 0x1A, 0x18,                          // service data 0x181A
    0x56, 0x34, 0x12, 0x38, 0xC1, 0xA4,              // MAC (LE)
    0xCD, 0x08, 0xD7, 0x11, 0x86, 0x0B, 0x55, 0x07, 0x05,
    0x0A, 0x09, 'A', 'T', 'C', '_', '1', '2', '3', '4', '5'};  // name

// atc1441: -5.3 °C, 60 %, 90 %, 3000 mV, count 3.
static const uint8_t kAtcAdv[] = {
    0x10, 0x16, 0x1A, 0x18,
    0xA4, 0xC1, 0x38, 0x12, 0x34, 0x56,  // MAC (BE)
    0xFF, 0xCB, 0x3C, 0x5A, 0x0B, 0xB8, 0x03};

// BTHome v2: packet 0x12, battery 100 %, 25.06 °C, 50.55 %, 2.932 V.
static const uint8_t kBTHomeAdv[] = {
    0x02, 0x01, 0x06,
    0x11, 0x16, 0xD2, 0xFC, 0x40,
    0x00, 0x12, 0x01, 0x64, 0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13, 0x0C, 0x74, 0x0B};

void test_pvvx() {
  Reading r;
  char name[kMaxNameLen + 1];
  bool has_name;
  TEST_ASSERT_TRUE(parseAdvertisement(kPvvxAdv, sizeof(kPvvxAdv), r, name, &has_name));
  TEST_ASSERT_EQUAL(int(Format::Pvvx), int(r.format));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 22.53, r.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 45.67, r.humidity_pct);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 2.950, r.battery_v);
  TEST_ASSERT_EQUAL_UINT8(85, r.battery_pct);
  TEST_ASSERT_EQUAL_UINT8(7, r.counter);
  TEST_ASSERT_TRUE(has_name);
  TEST_ASSERT_EQUAL_STRING("ATC_12345", name);
}

void test_atc1441_negative_temperature() {
  Reading r;
  TEST_ASSERT_TRUE(parseAdvertisement(kAtcAdv, sizeof(kAtcAdv), r));
  TEST_ASSERT_EQUAL(int(Format::Atc1441), int(r.format));
  TEST_ASSERT_FLOAT_WITHIN(0.001, -5.3, r.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 60, r.humidity_pct);
  TEST_ASSERT_EQUAL_UINT8(90, r.battery_pct);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 3.0, r.battery_v);
  TEST_ASSERT_EQUAL_UINT8(3, r.counter);
}

void test_bthome() {
  Reading r;
  TEST_ASSERT_TRUE(parseAdvertisement(kBTHomeAdv, sizeof(kBTHomeAdv), r));
  TEST_ASSERT_EQUAL(int(Format::BTHome), int(r.format));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 25.06, r.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 50.55, r.humidity_pct);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 2.932, r.battery_v);
  TEST_ASSERT_EQUAL_UINT8(100, r.battery_pct);
  TEST_ASSERT_EQUAL_UINT8(0x12, r.counter);
}

void test_bthome_encrypted_ignored() {
  uint8_t adv[sizeof(kBTHomeAdv)];
  memcpy(adv, kBTHomeAdv, sizeof(adv));
  adv[7] = 0x41;  // encryption flag
  Reading r;
  TEST_ASSERT_FALSE(parseAdvertisement(adv, sizeof(adv), r));
}

void test_malformed_and_unrelated() {
  Reading r;
  const uint8_t truncated[] = {0x12, 0x16, 0x1A, 0x18, 0x00};
  TEST_ASSERT_FALSE(parseAdvertisement(truncated, sizeof(truncated), r));
  const uint8_t other_uuid[] = {0x05, 0x16, 0x0F, 0x18, 0x01, 0x02};
  TEST_ASSERT_FALSE(parseAdvertisement(other_uuid, sizeof(other_uuid), r));
  TEST_ASSERT_FALSE(parseAdvertisement(nullptr, 0, r));
}

void test_table_merge_and_prefer_high_res() {
  DeviceTable t;
  Reading pvvx, atc;
  parseAdvertisement(kPvvxAdv, sizeof(kPvvxAdv), pvvx);
  parseAdvertisement(kAtcAdv, sizeof(kAtcAdv), atc);

  bool is_new;
  TEST_ASSERT_NOT_NULL(t.update(kMac, pvvx, "ATC_12345", -60, 1000, &is_new));
  TEST_ASSERT_TRUE(is_new);
  TEST_ASSERT_NOT_NULL(t.update(kMac, pvvx, nullptr, -61, 2000, &is_new));  // repeat
  TEST_ASSERT_FALSE(is_new);
  TEST_ASSERT_EQUAL(1, t.size());
  TEST_ASSERT_EQUAL_UINT32(1, t.at(0)->packets);
  TEST_ASSERT_EQUAL_STRING("ATC_12345", t.at(0)->name);

  // Recent pvvx data wins over atc1441.
  TEST_ASSERT_NULL(t.update(kMac, atc, nullptr, -62, 3000));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 22.53, t.at(0)->data.temperature_c);

  t.prune(4000, 10000);
  TEST_ASSERT_EQUAL(1, t.size());
  t.prune(20000, 10000);
  TEST_ASSERT_EQUAL(0, t.size());
}

void test_table_eviction() {
  DeviceTable t;
  Reading r;
  parseAdvertisement(kPvvxAdv, sizeof(kPvvxAdv), r);
  uint8_t mac[6] = {1, 2, 3, 4, 5, 0};
  for (uint8_t i = 0; i < DeviceTable::kCapacity + 1; i++) {
    mac[5] = i;
    t.update(mac, r, nullptr, 0, 1000 + i);
  }
  TEST_ASSERT_EQUAL(DeviceTable::kCapacity, t.size());
  for (size_t i = 0; i < t.size(); i++) TEST_ASSERT_NOT_EQUAL(0, t.at(i)->mac[5]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pvvx);
  RUN_TEST(test_atc1441_negative_temperature);
  RUN_TEST(test_bthome);
  RUN_TEST(test_bthome_encrypted_ignored);
  RUN_TEST(test_malformed_and_unrelated);
  RUN_TEST(test_table_merge_and_prefer_high_res);
  RUN_TEST(test_table_eviction);
  return UNITY_END();
}
