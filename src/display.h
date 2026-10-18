// Board specific screen output. Exactly one display_*.cpp is compiled per
// PlatformIO environment (see build_src_filter in platformio.ini).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "atc_parser.h"

namespace display {

void begin();
void showStarting();
// No sensors known yet; `adv_seen` counts all BLE advertisements received.
void showIdle(uint32_t now_ms, uint32_t adv_seen);
// Page `index` of `count`, one sensor per page.
void showDevice(const atc::Device& d, size_t index, size_t count, uint32_t now_ms);

}  // namespace display
