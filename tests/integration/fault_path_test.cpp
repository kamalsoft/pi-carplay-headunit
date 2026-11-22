// Integration (simulator, no dongle): simulator power faults reach a plugin's
// onSuspend through the production core's power dispatch (src/core/core.h), and
// a later clear reaches onResume. Backend: simulator. Built only when HU_SIMULATOR is ON.

#include <gtest/gtest.h>

#include <string_view>

#include "core/core.h"
#include "core/config.h"
#include "fake_clock.h"
#include "hal/power.h"
#include "plugins/plugin.h"
#include "simulator/simulator.h"

namespace {

using hu::plugin::HeadUnitPlugin;
using hu::integration::FakeClock;

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

// Power is polled on the core's 1 s timer, so each reading moves the clock first.
void poll_power(hu::core::Core& core, FakeClock& clock) {
  clock.advance(hu::core::kPowerPollPeriodMs);
  core.tick();
}

}  // namespace

TEST(FaultPath, LowVoltageReachesPluginOnSuspend) {
  hu::sim::Simulator sim;
  FakeClock clock;
  FaultProbePlugin plugin;
  hu::core::Core core(sim.usb, sim.power, clock, {&plugin});
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.low_voltage(true);
  sim.pump();
  core.tick();

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 0);
}

TEST(FaultPath, LowVoltageClearReachesPluginOnResume) {
  hu::sim::Simulator sim;
  FakeClock clock;
  FaultProbePlugin plugin;
  hu::core::Core core(sim.usb, sim.power, clock, {&plugin});
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.low_voltage(true);
  sim.pump();
  core.tick();
  sim.low_voltage(false);
  sim.pump();
  poll_power(core, clock);

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 1);
}

TEST(FaultPath, ThermalWarningReachesPluginOnSuspend) {
  hu::sim::Simulator sim;
  FakeClock clock;
  FaultProbePlugin plugin;
  hu::core::Core core(sim.usb, sim.power, clock, {&plugin});
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.thermal_warning(78.0f);
  sim.pump();
  core.tick();

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 0);
}

TEST(FaultPath, ThermalClearReachesPluginOnResume) {
  hu::sim::Simulator sim;
  FakeClock clock;
  FaultProbePlugin plugin;
  hu::core::Core core(sim.usb, sim.power, clock, {&plugin});
  ASSERT_TRUE(plugin.init(hu::Config{}));

  sim.thermal_warning(78.0f);
  sim.pump();
  core.tick();
  sim.thermal_warning(60.0f);
  sim.pump();
  poll_power(core, clock);

  EXPECT_EQ(plugin.suspends, 1);
  EXPECT_EQ(plugin.resumes, 1);
}
