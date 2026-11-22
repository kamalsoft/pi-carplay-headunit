// Integration (fake transport, no dongle): USB unplug and replug through the
// production core (src/core/core.h), which uses the transition table in
// src/core/state.h. Backend: fake UsbTransport and fake PowerMonitor only.

#include <gtest/gtest.h>

#include <vector>

#include "core/core.h"
#include "core/protocol/cpc200.h"
#include "core/state.h"
#include "fake_clock.h"
#include "fake_power.h"
#include "fake_usb_transport.h"

namespace {

using hu::core::Core;
using hu::core::Event;
using hu::core::State;
using hu::core::next_state;
using hu::hal::UsbId;
using hu::integration::FakeClock;
using hu::integration::FakePowerMonitor;
using hu::integration::FakeUsbTransport;

const UsbId kCcpa{0x1314, 0x1520};

struct Rig {
  FakeClock clock;
  FakeUsbTransport usb;
  FakePowerMonitor power;
  Core core{usb, power, clock, {}};
};

// Runs queued events and records the state if it changed.
void settle(Rig& r, std::vector<State>& trace) {
  r.core.pump();
  if (trace.back() != r.core.state()) trace.push_back(r.core.state());
}

}  // namespace

TEST(Reconnect, UnplugThenReplugWalksStandbyAndConnectingThroughTable) {
  Rig r;
  std::vector<State> trace{State::Boot};

  r.core.start();
  settle(r, trace);
  r.usb.attach(kCcpa);
  settle(r, trace);  // Connecting
  ASSERT_EQ(r.core.state(), State::Connecting);
  r.core.post(Event::PhoneConnected);
  settle(r, trace);  // Projecting
  ASSERT_EQ(r.core.state(), State::Projecting);

  // Unplug: the fake emits Detached, the core must go to Standby.
  r.usb.detach();
  settle(r, trace);
  EXPECT_EQ(r.core.state(), State::Standby);

  // Replug: the fake emits Attached, the core must go back through Connecting.
  r.usb.attach(kCcpa);
  settle(r, trace);
  EXPECT_EQ(r.core.state(), State::Connecting);

  const std::vector<State> expected = {State::Boot, State::Standby, State::Connecting,
                                       State::Projecting, State::Standby, State::Connecting};
  EXPECT_EQ(trace, expected);
}

TEST(Reconnect, DetachWhileProjectingIsNotAnIllegalTransition) {
  // Table row (Projecting, DongleDetached) -> Standby. A missing row would
  // make the core ignore the unplug and stay in Projecting.
  EXPECT_EQ(next_state(State::Projecting, Event::DongleDetached), State::Standby);

  Rig r;
  r.core.start();
  r.usb.attach(kCcpa);
  r.core.pump();
  r.core.post(Event::PhoneConnected);
  r.core.pump();
  ASSERT_EQ(r.core.state(), State::Projecting);
  r.usb.detach();
  r.core.pump();
  EXPECT_EQ(r.core.state(), State::Standby);
}

TEST(Reconnect, ReplugRestartsHandshakeFromFirstMessage) {
  Rig r;
  r.core.start();
  r.usb.attach(kCcpa);
  r.core.pump();
  const auto first = r.usb.writes;
  ASSERT_EQ(first.size(), 11u);

  // Unplug closes the transport; replug reopens it and the handshake runs again.
  r.usb.detach();
  r.core.pump();
  r.usb.writes.clear();
  r.usb.attach(kCcpa);
  r.core.pump();

  EXPECT_EQ(r.usb.open_count, 2);
  ASSERT_EQ(r.usb.writes.size(), 11u);
  EXPECT_EQ(r.usb.writes, first);
}

TEST(Reconnect, UnplugWhileConnectingReturnsToStandby) {
  Rig r;
  r.core.start();
  r.usb.attach(kCcpa);
  r.core.pump();
  ASSERT_EQ(r.core.state(), State::Connecting);
  r.usb.detach();
  r.core.pump();
  EXPECT_EQ(r.core.state(), State::Standby);
}
