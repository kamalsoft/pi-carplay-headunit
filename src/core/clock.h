#pragma once

// Injected time source for the core timers (A-21). Tests use a fake clock so
// the 1 s, 2 s and power-poll timers run without sleeps.

#include <chrono>
#include <cstdint>

namespace hu::core {

class Clock {
 public:
  virtual ~Clock() = default;

  virtual uint64_t now_ms() const = 0;       // monotonic, for timers
  virtual uint64_t unix_time_s() const = 0;  // wall time, BoxSettings syncTime
};

class SystemClock final : public Clock {
 public:
  uint64_t now_ms() const override {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }
  uint64_t unix_time_s() const override {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
  }
};

}  // namespace hu::core
