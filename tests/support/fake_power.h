#pragma once

// Scripted hal::PowerMonitor. Tests set status and the core polls it.

#include "hal/power.h"

namespace hu::integration {

class FakePowerMonitor final : public hal::PowerMonitor {
 public:
  hal::PowerStatus poll() override {
    ++polls;
    return status;
  }

  hal::PowerStatus status;
  int polls = 0;
};

}  // namespace hu::integration
