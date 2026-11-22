// Integration (simulator, no dongle): simulator power faults reach a plugin's
// onSuspend, and a later clear reaches onResume. Backend: simulator.
// Built only when HU_SIMULATOR is ON.

#include <gtest/gtest.h>

#include <string_view>

#include "core/config.h"
#include "hal/power.h"
#include "plugins/plugin.h"
#include "simulator/simulator.h"

namespace {

using hu::plugin::HeadUnitPlugin;

class FaultProbePlugin final : public HeadUnitPlugin {
 public:
  std::string_view name() const override { return "fault_probe"; }
  bool init(const hu::Config&) override { return true; }
  void onCarPlayConnect(const hu::plugin::CarPlaySession&) override {}
  void onCarPlayDisconnect(const hu::plugin::CarPlaySession&) override {}
  void onAudioFocusChange(hu::plugin::AudioFocus) override {}
  void onSuspend() override { ++suspends; }
  void onResume() override { ++resumes; }
  void onShutdown() override {}

  int suspends = 0;
  int resumes = 0;
};

// Test-side policy. Production has no power dispatcher yet, so this mirrors
// CLAUDE.md §3.2: suspend on live under-voltage (0x1) or CPU above 75 C.
// Calls onSuspend and onResume on transitions only.
class PowerDispatch {
 public:
  explicit PowerDispatch(HeadUnitPlugin& plugin) : plugin_(plugin) {}

  void apply(const hu::hal::PowerStatus& p) {
    const bool want = (p.throttled_flags & hu::hal::kThrottleLiveUndervoltage) != 0 || p.cpu_temp_c > 75.0f;
    if (want && !suspended_) {
      plugin_.onSuspend();
    } else if (!want && suspended_) {
      plugin_.onResume();
    }
    suspended_ = want;
  }

 private:
  HeadUnitPlugin& plugin_;
  bool suspended_ = false;
};

}  // namespace

TEST(FaultPath, LowVoltageReachesPluginOnSuspend) {
  hu::sim::Simulator sim;
  FaultProbePlugin plugin;
  PowerDispatch dispatch(plugin);
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.low_voltage(true);
  sim.pump();
  dispatch.apply(sim.power.poll());

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 0);
}

TEST(FaultPath, LowVoltageClearReachesPluginOnResume) {
  hu::sim::Simulator sim;
  FaultProbePlugin plugin;
  PowerDispatch dispatch(plugin);
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.low_voltage(true);
  sim.pump();
  dispatch.apply(sim.power.poll());
  sim.low_voltage(false);
  sim.pump();
  dispatch.apply(sim.power.poll());

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 1);
}

TEST(FaultPath, ThermalWarningReachesPluginOnSuspend) {
  hu::sim::Simulator sim;
  FaultProbePlugin plugin;
  PowerDispatch dispatch(plugin);
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.thermal_warning(78.0f);
  sim.pump();
  dispatch.apply(sim.power.poll());

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 0);
}

TEST(FaultPath, ThermalClearReachesPluginOnResume) {
  hu::sim::Simulator sim;
  FaultProbePlugin plugin;
  PowerDispatch dispatch(plugin);
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.thermal_warning(78.0f);
  sim.pump();
  dispatch.apply(sim.power.poll());
  sim.thermal_warning(60.0f);
  sim.pump();
  dispatch.apply(sim.power.poll());

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 1);
}
