#pragma once

// Fake core::Clock. Time moves only through advance(), so timer tests never sleep.

#include <cstdint>

#include "core/clock.h"

namespace hu::integration {

class FakeClock final : public core::Clock {
 public:
  uint64_t now_ms() const override { return now_ms_; }
  // Fixed, so handshake bytes (BoxSettings syncTime) are identical across replugs.
  uint64_t unix_time_s() const override { return 1700000000; }

  void advance(uint64_t ms) { now_ms_ += ms; }

 private:
  uint64_t now_ms_ = 0;
};

}  // namespace hu::integration
