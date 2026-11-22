// Integration (fake transport, no dongle): CPC200 handshake driven through
// hu::protocol against tests/integration/fake_usb_transport.h.
// Backend: fake UsbTransport only. No hardware was opened.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "core/protocol/cpc200.h"
#include "fake_usb_transport.h"

namespace {

using hu::hal::UsbId;
using namespace hu::protocol;
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

// Sends the init sequence and returns how many messages the transport accepted.
size_t run_handshake(FakeUsbTransport& t, const HandshakeConfig& cfg, uint64_t sync_s) {
  size_t sent = 0;
  for (const auto& m : build_handshake(cfg, sync_s)) {
    if (!send_message(t, m, 100)) break;
    ++sent;
  }
  return sent;
}

}  // namespace

TEST(Handshake, ElevenInitMessagesWrittenInUpstreamOrder) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  ASSERT_TRUE(t.open(kCcpa));
  HandshakeConfig cfg;
  ASSERT_EQ(run_handshake(t, cfg, 1700000000), 11u);

  ASSERT_EQ(t.writes.size(), 11u);
  std::vector<uint32_t> types;
  for (const auto& w : t.writes) types.push_back(type_of(w));
  EXPECT_EQ(types, kInitTypes);
}

TEST(Handshake, InitMessagesMatchBuildHandshakeByteForByte) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  ASSERT_TRUE(t.open(kCcpa));
  HandshakeConfig cfg;
  run_handshake(t, cfg, 1700000000);

  const auto expected = build_handshake(cfg, 1700000000);
  ASSERT_EQ(t.writes.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(t.writes[i], expected[i]) << "init message " << i;
  }
}

TEST(Handshake, FirstInitMessageIsDpiFrameByteExact) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  ASSERT_TRUE(t.open(kCcpa));
  run_handshake(t, HandshakeConfig{}, 1700000000);
  ASSERT_FALSE(t.writes.empty());
  EXPECT_EQ(t.writes[0], kDpiFrame);
}

TEST(Handshake, WifiConnectIsSentAfterInitSequence) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  ASSERT_TRUE(t.open(kCcpa));
  run_handshake(t, HandshakeConfig{}, 1700000000);
  // The 1 s delay before this send is a core timer; this checks order and bytes only.
  ASSERT_TRUE(send_message(t, build_wifi_connect(), 100));

  ASSERT_EQ(t.writes.size(), 12u);
  EXPECT_EQ(t.writes[11], kWifiConnectFrame);
}

TEST(Handshake, HeartbeatFrameIsByteExactAndWrittenOnEachSend) {
  FakeUsbTransport t;
  t.attach(kCcpa);
  ASSERT_TRUE(t.open(kCcpa));
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(send_message(t, build_heartbeat(), 100));
  }
  ASSERT_EQ(t.writes.size(), 3u);
  for (const auto& w : t.writes) {
    EXPECT_EQ(w, kHeartbeatFrame);
  }
}

TEST(Handshake, NoInitMessageWrittenWhenTransportClosed) {
  FakeUsbTransport t;
  t.attach(kCcpa);  // attached but never opened
  EXPECT_EQ(run_handshake(t, HandshakeConfig{}, 0), 0u);
  EXPECT_TRUE(t.writes.empty());
}
