#include <gtest/gtest.h>

#include <optional>
#include <type_traits>

#include "core/state.h"
#include "hal/audio.h"
#include "hal/power.h"
#include "hal/touch.h"
#include "hal/usb.h"
#include "hal/video.h"
#include "plugins/plugin.h"

using hu::core::Event;
using hu::core::next_state;
using hu::core::State;

namespace {

constexpr State kLiveStates[] = {State::Boot, State::Standby, State::Connecting, State::Projecting};

}  // namespace

TEST(Interfaces, HalInterfacesAreAbstract) {
  static_assert(std::is_abstract_v<hu::hal::VideoDecoder>);
  static_assert(std::is_abstract_v<hu::hal::AudioOutput>);
  static_assert(std::is_abstract_v<hu::hal::UsbTransport>);
  static_assert(std::is_abstract_v<hu::hal::InputSource>);
  static_assert(std::is_abstract_v<hu::hal::PowerMonitor>);
  static_assert(std::is_abstract_v<hu::plugin::HeadUnitPlugin>);
  SUCCEED();
}

TEST(StateMachine, BootStartsInStandby) {
  EXPECT_EQ(next_state(State::Boot, Event::Started), State::Standby);
}

TEST(StateMachine, ConnectPathReachesProjecting) {
  EXPECT_EQ(next_state(State::Standby, Event::DongleAttached), State::Connecting);
  EXPECT_EQ(next_state(State::Connecting, Event::PhoneConnected), State::Projecting);
}

TEST(StateMachine, WirelessPhoneDropReturnsToConnecting) {
  EXPECT_EQ(next_state(State::Projecting, Event::PhoneDisconnected), State::Connecting);
}

TEST(StateMachine, DongleYankResetsToStandby) {
  EXPECT_EQ(next_state(State::Projecting, Event::DongleDetached), State::Standby);
  EXPECT_EQ(next_state(State::Connecting, Event::DongleDetached), State::Standby);
  EXPECT_EQ(next_state(State::Connecting, Event::HandshakeFailed), State::Standby);
}

TEST(StateMachine, IgnitionOffAndSignalsShutDownFromEveryLiveState) {
  for (State s : kLiveStates) {
    EXPECT_EQ(next_state(s, Event::IgnitionOff), State::Shutdown);
    EXPECT_EQ(next_state(s, Event::ShutdownSignal), State::Shutdown);
  }
}

TEST(StateMachine, ShutdownIsTerminal) {
  for (Event e : {Event::Started, Event::DongleAttached, Event::DongleDetached, Event::HandshakeFailed,
                  Event::PhoneConnected, Event::PhoneDisconnected, Event::IgnitionOff, Event::ShutdownSignal}) {
    EXPECT_FALSE(next_state(State::Shutdown, e).has_value());
  }
}

TEST(StateMachine, UndefinedPairIsRejected) {
  EXPECT_FALSE(next_state(State::Standby, Event::PhoneConnected).has_value());
  EXPECT_FALSE(next_state(State::Boot, Event::PhoneConnected).has_value());
}

TEST(StateMachine, TableIsDeterministic) {
  for (const auto& a : hu::core::kTransitions) {
    int matches = 0;
    for (const auto& b : hu::core::kTransitions) {
      if (a.from == b.from && a.on == b.on) {
        ++matches;
      }
    }
    EXPECT_EQ(matches, 1);
  }
}

TEST(PowerInterface, ThrottleMasksMatchSafetySpec) {
  EXPECT_EQ(hu::hal::kThrottleLiveUndervoltage, 0x1u);
  EXPECT_EQ(hu::hal::kThrottleStickyMask, 0x50000u);
  EXPECT_EQ(hu::hal::PowerStatus{}.ignition, hu::hal::IgnitionState::Unknown);
}
