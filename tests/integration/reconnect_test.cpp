// Integration (fake transport, no dongle): USB unplug and replug through the
// core transition table in src/core/state.h. Backend: fake UsbTransport only.

#include <gtest/gtest.h>

#include <vector>

#include "core/protocol/cpc200.h"
#include "core/state.h"
#include "fake_usb_transport.h"

namespace {

using hu::core::Event;
using hu::core::State;
using hu::core::next_state;
using hu::hal::UsbEvent;
using hu::hal::UsbId;
using hu::integration::FakeUsbTransport;
using namespace hu::protocol;

const UsbId kCcpa{0x1314, 0x1520};

// Event sink for the test: the core would map HAL events to state events. Only
// the two cable events used here are mapped.
struct CoreLoop {
  State state = State::Boot;
  std::vector<State> trace{State::Boot};
  bool apply(Event e) {
    const auto next = next_state(state, e);
    if (!next) return false;
    state = *next;
    trace.push_back(state);
    return true;
  }
};

Event to_event(UsbEvent e) {
  return e == UsbEvent::Attached ? Event::DongleAttached : Event::DongleDetached;
}

// Runs the handshake on the transport, as the Connecting state does.
void connect_and_handshake(FakeUsbTransport& t, std::vector<std::vector<uint8_t>>& out) {
  ASSERT_TRUE(t.open(kCcpa));
  for (const auto& m : build_handshake(HandshakeConfig{}, 1700000000)) {
    ASSERT_TRUE(send_message(t, m, 100));
  }
  out = t.writes;
}

}  // namespace

TEST(Reconnect, UnplugThenReplugWalksStandbyAndConnectingThroughTable) {
  FakeUsbTransport t;
  CoreLoop core;
  std::vector<UsbEvent> events;
  t.set_event_sink([&](UsbEvent e, UsbId) { events.push_back(e); });

  ASSERT_TRUE(core.apply(Event::Started));
  t.attach(kCcpa);
  ASSERT_TRUE(core.apply(to_event(events.back())));  // Connecting
  ASSERT_EQ(core.state, State::Connecting);
  ASSERT_TRUE(core.apply(Event::PhoneConnected));  // Projecting

  // Unplug: the fake emits Detached, the core must go to Standby.
  t.detach();
  ASSERT_TRUE(core.apply(to_event(events.back())));
  EXPECT_EQ(core.state, State::Standby);

  // Replug: the fake emits Attached, the core must go back through Connecting.
  t.attach(kCcpa);
  ASSERT_TRUE(core.apply(to_event(events.back())));
  EXPECT_EQ(core.state, State::Connecting);

  const std::vector<State> expected = {State::Boot, State::Standby, State::Connecting,
                                       State::Projecting, State::Standby, State::Connecting};
  EXPECT_EQ(core.trace, expected);
}

TEST(Reconnect, DetachWhileProjectingIsNotAnIllegalTransition) {
  // Table row (Projecting, DongleDetached) -> Standby. A missing row would
  // make the core ignore the unplug and stay in Projecting.
  EXPECT_EQ(next_state(State::Projecting, Event::DongleDetached), State::Standby);
}

TEST(Reconnect, ReplugRestartsHandshakeFromFirstMessage) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  std::vector<std::vector<uint8_t>> first;
  connect_and_handshake(t, first);
  ASSERT_EQ(first.size(), 11u);

  // Unplug closes the transport; replug reopens it and the handshake runs again.
  t.detach();
  t.writes.clear();
  t.attach(kCcpa);
  std::vector<std::vector<uint8_t>> second;
  connect_and_handshake(t, second);

  EXPECT_EQ(t.open_count, 2);
  ASSERT_EQ(second.size(), 11u);
  EXPECT_EQ(second, first);
}

TEST(Reconnect, UnplugWhileConnectingReturnsToStandby) {
  CoreLoop core;
  ASSERT_TRUE(core.apply(Event::Started));
  ASSERT_TRUE(core.apply(Event::DongleAttached));
  ASSERT_EQ(core.state, State::Connecting);
  ASSERT_TRUE(core.apply(Event::DongleDetached));
  EXPECT_EQ(core.state, State::Standby);
}
