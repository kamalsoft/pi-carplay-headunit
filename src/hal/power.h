#pragma once

// Power HAL interface. Reads throttle flags, CPU temperature, and ignition
// state for the core watchdog. It never writes to the vehicle (CLAUDE.md §3.1).

#include <cstdint>

namespace hu::hal {

enum class IgnitionState : uint8_t { Unknown, Off, On };

struct PowerStatus {
  uint32_t throttled_flags = 0;  // raw `vcgencmd get_throttled` bitmask
  float cpu_temp_c = 0.0f;
  IgnitionState ignition = IgnitionState::Unknown;  // simulated in macos_sim
};

// Under-voltage now (live). Gates plugin suspension, so the core reacts at once.
inline constexpr uint32_t kThrottleLiveUndervoltage = 0x1;
// "Has occurred" bits (sticky, CLAUDE.md §3.2). Logged for diagnosis; they stay set until reboot.
inline constexpr uint32_t kThrottleStickyMask = 0x50000;

// Ignition comes from a GPIO sense line on the car's 12V ACC, through an
// isolating divider or opto-isolator. Wiring is not done yet; macos_sim simulates it.

class PowerMonitor {
 public:
  virtual ~PowerMonitor() = default;

  virtual PowerStatus poll() = 0;
};

}  // namespace hu::hal
