// Integration (fake transport, no dongle): CPC200 handshake driven by the
// production core (src/core/core.h) against tests/support/fake_usb_transport.h.
// Backend: fake UsbTransport and fake PowerMonitor only. No hardware was opened.

#include <gtest/gtest.h>

#include <cstdint>
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
using hu::hal::UsbId;
using namespace hu::protocol;
using hu::integration::FakeClock;
using hu::integration::FakePowerMonitor;
using hu::integration::FakeUsbTransport;

const UsbId kCcpa{0x1314, 0x1520};

// Byte-exact frames copied from tests/unit/protocol_test.cpp (fixtures there).
const std::vector<uint8_t> kHeartbeatFrame = {0xaa, 0x55, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
                                              0xaa, 0x00, 0x00, 0x00, 0x55, 0xff, 0xff, 0xff};
const std::vector<uint8_t> kWifiConnectFrame = {0xaa, 0x55, 0xaa, 0x55, 0x04, 0x00, 0x00, 0x00,
                                                0x08, 0x00, 0x00, 0x00, 0xf7, 0xff, 0xff, 0xff,
                                                0xea, 0x03, 0x00, 0x00};
// SendFile /tmp/screen_dpi = 160.
const std::vector<uint8_t> kDpiFrame = {0xaa, 0x55, 0xaa, 0x55, 0x1c, 0x00, 0x00, 0x00,
                                        0x99, 0x00, 0x00, 0x00, 0x66, 0xff, 0xff, 0xff,
                                        0x10, 0x00, 0x00, 0x00, 0x2f, 0x74, 0x6d, 0x70,
                                        0x2f, 0x73, 0x63, 0x72, 0x65, 0x65, 0x6e, 0x5f,
                                        0x64, 0x70, 0x69, 0x00, 0x04, 0x00, 0x00, 0x00,
                                        0xa0, 0x00, 0x00, 0x00};

// Upstream send order: 4 SendFile, Open, 5 SendFile, BoxSettings, 4 Command.
const std::vector<uint32_t> kInitTypes = {0x99, 0x01, 0x99, 0x99, 0x99, 0x99, 0x19, 0x08, 0x08, 0x08, 0x08};

uint32_t type_of(const std::vector<uint8_t>& frame) {
  return static_cast<uint32_t>(frame[8]) | (static_cast<uint32_t>(frame[9]) << 8);
}

// Production core on the fake transport, with the fake clock and power source.
struct Rig {
  FakeClock clock;
  FakeUsbTransport usb;
  FakePowerMonitor power;
  Core core{usb, power, clock, {}};

  // Start, then attach the dongle. The handshake runs on entering Connecting.
  void connect() {
    core.start();
    usb.attach(kCcpa);
    core.pump();
  }
};

}  // namespace

TEST(Handshake, ElevenInitMessagesWrittenInUpstreamOrder) {
  Rig r;
  r.connect();
  ASSERT_EQ(r.core.state(), State::Connecting);
  ASSERT_TRUE(r.usb.is_open());
  ASSERT_EQ(r.usb.writes.size(), 11u);

  std::vector<uint32_t> types;
  for (const auto& w : r.usb.writes) types.push_back(type_of(w));
  EXPECT_EQ(types, kInitTypes);
}

TEST(Handshake, InitMessagesMatchBuildHandshakeByteForByte) {
  Rig r;
  r.connect();

  const auto expected = build_handshake(HandshakeConfig{}, 1700000000);
  ASSERT_EQ(r.usb.writes.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(r.usb.writes[i], expected[i]) << "init message " << i;
  }
}

TEST(Handshake, FirstInitMessageIsDpiFrameByteExact) {
  Rig r;
  r.connect();
  ASSERT_FALSE(r.usb.writes.empty());
  EXPECT_EQ(r.usb.writes[0], kDpiFrame);
}

TEST(Handshake, WifiConnectIsSentOneSecondAfterInitSequence) {
  Rig r;
  r.connect();
  ASSERT_EQ(r.usb.writes.size(), 11u);

  r.clock.advance(999);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 11u);  // not yet: the delay is 1 s

  r.clock.advance(1);
  r.core.tick();
  ASSERT_EQ(r.usb.writes.size(), 12u);
  EXPECT_EQ(r.usb.writes[11], kWifiConnectFrame);

  r.clock.advance(1000);
  r.core.tick();
  EXPECT_EQ(r.usb.writes.size(), 12u);  // sent once only
}

TEST(Handshake, HeartbeatFrameIsByteExactAndSentEveryTwoSecondsWhileProjecting) {
  Rig r;
  r.connect();
  r.clock.advance(1000);
  r.core.tick();  // wifi connect
  ASSERT_EQ(r.usb.writes.size(), 12u);

  r.core.post(Event::PhoneConnected);
  r.core.pump();
  ASSERT_EQ(r.core.state(), State::Projecting);

  for (int i = 0; i < 3; ++i) {
    r.clock.advance(2000);
    r.core.tick();
  }
  ASSERT_EQ(r.usb.writes.size(), 15u);
  for (size_t i = 12; i < r.usb.writes.size(); ++i) {
    EXPECT_EQ(r.usb.writes[i], kHeartbeatFrame);
  }
}

TEST(Handshake, NoInitMessageWrittenWhenTransportWontOpen) {
  Rig r;
  r.usb.refuse_open = true;  // attached, but open() fails
  r.connect();
  EXPECT_EQ(r.core.state(), State::Standby);
  EXPECT_TRUE(r.usb.writes.empty());
}
