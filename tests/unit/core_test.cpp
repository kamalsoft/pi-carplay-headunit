// Unit tests for the production core (src/core/core.h): HAL event mapping, the
// cadence of the core timers, power dispatch, and plugin callbacks. Uses the
// fake clock, so no test sleeps.

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/core.h"
#include "core/protocol/cpc200.h"
#include "fake_clock.h"
#include "fake_power.h"
#include "fake_usb_transport.h"

namespace {

using hu::core::Core;
using hu::core::Event;
using hu::core::State;
using hu::hal::UsbEvent;
using hu::hal::UsbId;
using hu::integration::FakeClock;
using hu::integration::FakePowerMonitor;
using hu::integration::FakeUsbTransport;

const UsbId kCcpa{0x1314, 0x1520};

class RecordingPlugin final : public hu::plugin::HeadUnitPlugin {
 public:
  std::string_view name() const override { return "recording"; }
  bool init(const hu::Config&) override { return true; }
  void onCarPlayConnect(const hu::plugin::CarPlaySession& s) override { connects.push_back(s.id); }
  void onCarPlayDisconnect(const hu::plugin::CarPlaySession& s) override { disconnects.push_back(s.id); }
  void onAudioFocusChange(hu::plugin::AudioFocus) override {}
  void onSuspend() override { ++suspends; }
  void onResume() override { ++resumes; }
  void onShutdown() override { ++shutdowns; }

  std::vector<uint32_t> connects;
  std::vector<uint32_t> disconnects;
  int suspends = 0;
  int resumes = 0;
  int shutdowns = 0;
};

struct Rig {
  FakeClock clock;
  FakeUsbTransport usb;
  FakePowerMonitor power;
  RecordingPlugin plugin;
  Core core{usb, power, clock, {&plugin}};

  void connect() {
    core.start();
    usb.attach(kCcpa);
    core.pump();
  }
  void project() {
    core.post(Event::PhoneConnected);
    core.pump();
  }
};

}  // namespace

TEST(CoreEventMapping, UsbAttachedMapsToDongleAttached) {
  EXPECT_EQ(hu::core::map_usb_event(UsbEvent::Attached), Event::DongleAttached);
}

TEST(CoreEventMapping, UsbDetachedMapsToDongleDetached) {
  EXPECT_EQ(hu::core::map_usb_event(UsbEvent::Detached), Event::DongleDetached);
}

TEST(CoreEventMapping, UsbErrorMapsToDongleDetached) {
  EXPECT_EQ(hu::core::map_usb_event(UsbEvent::Error), Event::DongleDetached);
}

TEST(CoreTimers, WifiConnectFiresOnceOneSecondAfterHandshake) {
  Rig r;
  r.connect();
  ASSERT_EQ(r.usb.writes.size(), 11u);
  r.clock.advance(999);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 11u);
  r.clock.advance(1);
  r.core.tick();
  ASSERT_EQ(r.usb.writes.size(), 12u);
  EXPECT_EQ(r.usb.writes[11], hu::protocol::build_wifi_connect());
  r.clock.advance(5000);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 12u);
}

TEST(CoreTimers, WifiConnectCancelledWhenLeavingConnecting) {
  Rig r;
  r.connect();
  r.project();
  r.clock.advance(1000);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 11u);
}

TEST(CoreTimers, HeartbeatOnlyWhileProjectingEveryTwoSeconds) {
  Rig r;
  r.connect();
  r.clock.advance(1000);
  r.core.tick();  // wifi connect, 12 writes
  r.clock.advance(1000);
  r.core.tick();  // Connecting: no heartbeat
  EXPECT_EQ(r.usb.writes.size(), 12u);

  r.project();  // entered at t = 2000, first heartbeat due at 4000
  r.clock.advance(1999);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 12u);
  r.clock.advance(1);
  r.core.tick();
  ASSERT_EQ(r.usb.writes.size(), 13u);
  EXPECT_EQ(r.usb.writes[12], hu::protocol::build_heartbeat());
  r.clock.advance(2000);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 14u);

  // Unplug leaves Projecting: heartbeats stop.
  r.usb.detach();
  r.core.pump();
  r.clock.advance(10000);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 14u);
}

TEST(CoreTimers, PowerIsPolledOncePerSecond) {
  Rig r;
  r.core.start();
  r.core.tick();
  EXPECT_EQ(r.power.polls, 1);
  r.clock.advance(999);
  r.core.tick();
  EXPECT_EQ(r.power.polls, 1);
  r.clock.advance(1);
  r.core.tick();
  EXPECT_EQ(r.power.polls, 2);
}

TEST(CorePower, LiveUndervoltageSuspendsOnceAndClearResumes) {
  Rig r;
  r.core.start();
  r.power.status.throttled_flags = hu::hal::kThrottleLiveUndervoltage;
  r.core.tick();
  EXPECT_EQ(r.plugin.suspends, 1);
  r.clock.advance(1000);
  r.core.tick();  // still low: no second suspend
  EXPECT_EQ(r.plugin.suspends, 1);

  r.power.status.throttled_flags = 0;
  r.clock.advance(1000);
  r.core.tick();
  EXPECT_EQ(r.plugin.resumes, 1);
  EXPECT_EQ(r.plugin.suspends, 1);
}

TEST(CorePower, ThermalSuspendIsStrictlyAboveSeventyFive) {
  Rig r;
  r.core.start();
  r.power.status.cpu_temp_c = 75.0f;
  r.core.tick();
  EXPECT_EQ(r.plugin.suspends, 0);
  r.clock.advance(1000);
  r.power.status.cpu_temp_c = 75.1f;
  r.core.tick();
  EXPECT_EQ(r.plugin.suspends, 1);
}

TEST(CorePower, StickyBitsAloneDoNotSuspend) {
  Rig r;
  r.core.start();
  r.power.status.throttled_flags = hu::hal::kThrottleStickyMask;  // has occurred, not live
  r.core.tick();
  EXPECT_EQ(r.plugin.suspends, 0);
  EXPECT_EQ(r.plugin.resumes, 0);
}

TEST(CorePlugins, CarPlaySessionCallbacksFollowProjecting) {
  Rig r;
  r.connect();
  r.project();
  EXPECT_EQ(r.plugin.connects, std::vector<uint32_t>{1});
  EXPECT_TRUE(r.plugin.disconnects.empty());
  r.usb.detach();
  r.core.pump();
  EXPECT_EQ(r.plugin.disconnects, std::vector<uint32_t>{1});
}

TEST(CorePlugins, ShutdownSignalDisconnectsAndShutsDownOnce) {
  Rig r;
  r.connect();
  r.project();
  r.core.post(Event::ShutdownSignal);
  r.core.pump();
  EXPECT_EQ(r.core.state(), State::Shutdown);
  EXPECT_EQ(r.plugin.disconnects, std::vector<uint32_t>{1});
  EXPECT_EQ(r.plugin.shutdowns, 1);
  EXPECT_FALSE(r.usb.is_open());

  r.core.post(Event::ShutdownSignal);  // no outgoing rows from Shutdown
  r.core.pump();
  EXPECT_EQ(r.plugin.shutdowns, 1);
}
