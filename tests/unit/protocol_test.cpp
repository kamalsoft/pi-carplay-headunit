// Derived from node-CarPlay, Copyright (c) 2022 Rhys Morgan, MIT License. See THIRD_PARTY_NOTICES.md.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/protocol/cpc200.h"

namespace {

using hu::hal::UsbId;
using namespace hu::protocol;

// Records every write. Reads serve bytes from rx (or fail when read_result is set).
class FakeTransport : public hu::hal::UsbTransport {
 public:
  bool open(UsbId) override { open_ = true; return true; }
  void close() override { open_ = false; }
  bool is_open() const override { return open_; }
  int read(uint8_t* buffer, size_t capacity, unsigned) override {
    if (read_error_) return -1;
    const size_t n = std::min(capacity, rx.size() - rx_pos);
    std::memcpy(buffer, rx.data() + rx_pos, n);
    rx_pos += n;
    return static_cast<int>(n);
  }
  int write(const uint8_t* buffer, size_t size, unsigned) override {
    if (write_error) return -1;
    writes.emplace_back(buffer, buffer + size);
    return static_cast<int>(size);
  }
  void set_event_sink(EventSink) override {}

  bool open_ = false;
  bool read_error_ = false;
  bool write_error = false;
  std::vector<uint8_t> rx;
  size_t rx_pos = 0;
  std::vector<std::vector<uint8_t>> writes;
};

std::vector<uint8_t> bytes(std::initializer_list<uint8_t> b) { return std::vector<uint8_t>(b); }

// Function-local fixtures: namespace-scope std::string/vector trips ASan container checks on macOS libc++.
// "/tmp/screen_dpi" with its NUL, as ASCII bytes.
std::vector<uint8_t> dpi_path_with_nul() {
  return {0x2f, 0x74, 0x6d, 0x70, 0x2f, 0x73, 0x63, 0x72, 0x65, 0x65, 0x6e, 0x5f, 0x64, 0x70, 0x69, 0x00};
}

// Upstream JSON.stringify output for mediaDelay 300, syncTime 1700000000, 800x640 (86 bytes).
std::vector<uint8_t> box_settings_json() {
  const char json[] = "{\"mediaDelay\":300,\"syncTime\":1700000000,\"androidAutoSizeW\":800,\"androidAutoSizeH\":640}";
  return std::vector<uint8_t>(json, json + sizeof(json) - 1);
}

}  // namespace

// ---- Header encode, byte-exact (little-endian, type check = ~type) ----

TEST(ProtocolHeader, EncodeHeartbeatIsByteExact) {
  const auto h = encode_header(MsgType::HeartBeat, 0);
  EXPECT_EQ(std::vector<uint8_t>(h.begin(), h.end()),
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
                   0xaa, 0x00, 0x00, 0x00, 0x55, 0xff, 0xff, 0xff}));
}

TEST(ProtocolHeader, EncodeOpenLengthAndCheckAreByteExact) {
  const auto h = encode_header(MsgType::Open, 28);
  EXPECT_EQ(std::vector<uint8_t>(h.begin(), h.end()),
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x1c, 0x00, 0x00, 0x00,
                   0x01, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff}));
}

// ---- Header decode round trip and rejection ----

TEST(ProtocolHeader, DecodeRoundTrip) {
  const auto h = encode_header(MsgType::Command, 4);
  FrameError err = FrameError::BadMagic;
  auto decoded = decode_header(h.data(), h.size(), &err);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(err, FrameError::None);
  EXPECT_EQ(decoded->length, 4u);
  EXPECT_EQ(decoded->type, static_cast<uint32_t>(MsgType::Command));
}

TEST(ProtocolHeader, RejectsBadMagic) {
  auto h = encode_header(MsgType::HeartBeat, 0);
  h[0] = 0x00;
  FrameError err = FrameError::None;
  EXPECT_FALSE(decode_header(h.data(), h.size(), &err).has_value());
  EXPECT_EQ(err, FrameError::BadMagic);
}

TEST(ProtocolHeader, RejectsBadTypeCheck) {
  auto h = encode_header(MsgType::HeartBeat, 0);
  h[12] = 0x00;  // type check no longer ~0xaa
  FrameError err = FrameError::None;
  EXPECT_FALSE(decode_header(h.data(), h.size(), &err).has_value());
  EXPECT_EQ(err, FrameError::BadTypeCheck);
}

TEST(ProtocolHeader, RejectsShortBuffer) {
  const auto h = encode_header(MsgType::HeartBeat, 0);
  FrameError err = FrameError::None;
  EXPECT_FALSE(decode_header(h.data(), 15, &err).has_value());
  EXPECT_EQ(err, FrameError::ShortBuffer);
}

TEST(ProtocolHeader, RejectsOversizeLengthEvenWithValidCheck) {
  // Valid magic and type check, length above kMaxPayload.
  auto h = encode_header(MsgType::SendFile, kMaxPayload + 1);
  FrameError err = FrameError::None;
  EXPECT_FALSE(decode_header(h.data(), h.size(), &err).has_value());
  EXPECT_EQ(err, FrameError::BadLength);
}

// ---- Message fixtures, byte-exact ----

TEST(ProtocolMessages, HeartbeatBytes) {
  EXPECT_EQ(build_heartbeat(),
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
                   0xaa, 0x00, 0x00, 0x00, 0x55, 0xff, 0xff, 0xff}));
}

TEST(ProtocolMessages, WifiConnectCommandBytes) {
  // Command 1002 = 0x3ea, payload is u32 LE.
  EXPECT_EQ(build_wifi_connect(),
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x04, 0x00, 0x00, 0x00,
                   0x08, 0x00, 0x00, 0x00, 0xf7, 0xff, 0xff, 0xff,
                   0xea, 0x03, 0x00, 0x00}));
}

TEST(ProtocolMessages, DpiSendFileBytes) {
  HandshakeConfig cfg;  // dpi 160
  const auto msgs = build_handshake(cfg, 1700000000);
  // SendFile payload: u32 name len 16, "/tmp/screen_dpi\0", u32 content len 4, u32 160.
  std::vector<uint8_t> expected = bytes({0xaa, 0x55, 0xaa, 0x55, 0x1c, 0x00, 0x00, 0x00,
                                         0x99, 0x00, 0x00, 0x00, 0x66, 0xff, 0xff, 0xff,
                                         0x10, 0x00, 0x00, 0x00});
  const auto path = dpi_path_with_nul();
  expected.insert(expected.end(), path.begin(), path.end());
  for (uint8_t b : bytes({0x04, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00})) expected.push_back(b);
  ASSERT_EQ(msgs.size(), 11u);
  EXPECT_EQ(msgs[0], expected);
}

TEST(ProtocolMessages, OpenPayloadBytes) {
  HandshakeConfig cfg;
  const auto msgs = build_handshake(cfg, 0);
  // width 800, height 640, fps 20, format 5, packetMax 49152, iBox 2, phoneMode 2.
  EXPECT_EQ(msgs[1],
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x1c, 0x00, 0x00, 0x00,
                   0x01, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff,
                   0x20, 0x03, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00,
                   0x14, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
                   0x00, 0xc0, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                   0x02, 0x00, 0x00, 0x00}));
}

TEST(ProtocolMessages, BoxSettingsJsonIsByteExact) {
  HandshakeConfig cfg;
  const auto msgs = build_handshake(cfg, 1700000000);
  const auto& box = msgs[6];
  // Header: type 0x19, check ~0x19 = 0xe6, length 86 (0x56).
  EXPECT_EQ(std::vector<uint8_t>(box.begin(), box.begin() + 16),
            bytes({0xaa, 0x55, 0xaa, 0x55, 0x56, 0x00, 0x00, 0x00,
                   0x19, 0x00, 0x00, 0x00, 0xe6, 0xff, 0xff, 0xff}));
  const auto json = box_settings_json();
  EXPECT_EQ(std::vector<uint8_t>(box.begin() + 16, box.end()), json);
  EXPECT_EQ(box.size(), 16 + json.size());
}

// ---- Handshake sequence against a recording fake transport ----

TEST(ProtocolHandshake, WritesInUpstreamOrder) {
  FakeTransport t;
  ASSERT_TRUE(t.open(UsbId{0x1314, 0x1520}));
  HandshakeConfig cfg;  // defaults: 5 GHz, mic os, audio transfer off
  for (const auto& m : build_handshake(cfg, 1700000000)) {
    ASSERT_TRUE(send_message(t, m, 100));
  }
  ASSERT_EQ(t.writes.size(), 11u);

  // Type of each write, in order: 4 SendFile numbers/strings, Open, 5 SendFile,
  // BoxSettings, then 4 Commands.
  std::vector<uint32_t> types;
  for (const auto& w : t.writes) types.push_back(static_cast<uint32_t>(w[8] | (w[9] << 8)));
  EXPECT_EQ(types, (std::vector<uint32_t>{0x99, 0x01, 0x99, 0x99, 0x99, 0x99, 0x19, 0x08, 0x08, 0x08, 0x08}));

  // Command values in order: wifiEnable 1000, wifi5g 25, mic 7, audioTransferOff 23.
  const uint32_t expected_cmds[] = {1000, 25, 7, 23};
  for (int i = 0; i < 4; ++i) {
    const auto& w = t.writes[7 + i];
    ASSERT_EQ(w.size(), 20u);
    EXPECT_EQ(static_cast<uint32_t>(w[16] | (w[17] << 8)), expected_cmds[i]);
  }
}

TEST(ProtocolHandshake, AndroidWorkModeAppendsOneMessage) {
  HandshakeConfig cfg;
  cfg.android_work_mode = true;
  const auto msgs = build_handshake(cfg, 0);
  ASSERT_EQ(msgs.size(), 12u);
  EXPECT_EQ(msgs.back()[8], 0x99);  // SendFile
}

TEST(ProtocolHandshake, WifiConnectAfterInitAndSendFailureReported) {
  FakeTransport t;
  t.open(UsbId{0x1314, 0x1521});
  t.write_error = true;
  EXPECT_FALSE(send_message(t, build_heartbeat(), 100));
  EXPECT_TRUE(t.writes.empty());

  t.write_error = false;
  EXPECT_TRUE(send_message(t, build_wifi_connect(), 100));
  ASSERT_EQ(t.writes.size(), 1u);
  EXPECT_EQ(t.writes[0], build_wifi_connect());
}

TEST(ProtocolHandshake, NoWriteWhenClosed) {
  FakeTransport t;  // never opened
  EXPECT_FALSE(send_message(t, build_heartbeat(), 100));
  EXPECT_TRUE(t.writes.empty());
}

// ---- Inbound framing ----

TEST(ProtocolRead, ReadsHeaderThenPayload) {
  FakeTransport t;
  t.open(UsbId{0x1314, 0x1520});
  t.rx = encode_message(MsgType::Command, {0xea, 0x03, 0x00, 0x00});
  FrameError err = FrameError::Transport;
  auto f = read_frame(t, 100, &err);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(err, FrameError::None);
  EXPECT_EQ(f->header.type, static_cast<uint32_t>(MsgType::Command));
  EXPECT_EQ(f->payload, bytes({0xea, 0x03, 0x00, 0x00}));
}

TEST(ProtocolRead, RejectsBadMagicInStream) {
  FakeTransport t;
  t.open(UsbId{0x1314, 0x1520});
  t.rx = build_heartbeat();
  t.rx[3] = 0x00;
  FrameError err = FrameError::None;
  EXPECT_FALSE(read_frame(t, 100, &err).has_value());
  EXPECT_EQ(err, FrameError::BadMagic);
}

TEST(ProtocolRead, TruncatedPayloadIsShortRead) {
  FakeTransport t;
  t.open(UsbId{0x1314, 0x1520});
  t.rx = encode_message(MsgType::Command, {0x01, 0x02, 0x03, 0x04});
  t.rx.resize(t.rx.size() - 2);
  FrameError err = FrameError::None;
  EXPECT_FALSE(read_frame(t, 100, &err).has_value());
  EXPECT_EQ(err, FrameError::ShortRead);
}

TEST(ProtocolRead, TransportErrorReported) {
  FakeTransport t;
  t.open(UsbId{0x1314, 0x1520});
  t.read_error_ = true;
  FrameError err = FrameError::None;
  EXPECT_FALSE(read_frame(t, 100, &err).has_value());
  EXPECT_EQ(err, FrameError::Transport);
}
