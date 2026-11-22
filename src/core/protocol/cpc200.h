// Derived from node-CarPlay, Copyright (c) 2022 Rhys Morgan, MIT License. See THIRD_PARTY_NOTICES.md.
#pragma once

// CPC200 (Carlinkit 1314:1520/1521) message framing and handshake encoding.
// Ported from node-CarPlay src/modules/messages/{common,sendable}.ts and the
// DongleDriver start() init sequence. Only frames that upstream sends are
// built here (CLAUDE.md 3.1). Video, audio decode and inbound parsing are not
// ported. Timers (1 s wifiConnect delay, 2 s heartbeat) belong to the core.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hal/usb.h"

namespace hu::protocol {

constexpr uint32_t kMagic = 0x55aa55aa;
constexpr size_t kHeaderSize = 16;
// shortcut: 8 MiB inbound payload cap is our guess (upstream has none), revisit against bench captures.
constexpr uint32_t kMaxPayload = 8u * 1024u * 1024u;

// Message types this layer sends (upstream MessageType).
enum class MsgType : uint32_t {
  Open = 0x01,
  Command = 0x08,
  BoxSettings = 0x19,
  SendFile = 0x99,
  HeartBeat = 0xaa,
};

// Command values sent during the handshake (upstream CommandMapping).
enum class Cmd : uint32_t {
  Mic = 7,
  BoxMic = 15,
  Wifi24g = 24,
  Wifi5g = 25,
  AudioTransferOff = 23,
  AudioTransferOn = 22,
  WifiEnable = 1000,
  WifiConnect = 1002,
};

// Raw type is kept as-is: the dongle sends many types this layer does not parse.
struct Header {
  uint32_t length = 0;
  uint32_t type = 0;
};

enum class FrameError { None, Transport, ShortRead, ShortBuffer, BadMagic, BadTypeCheck, BadLength };

struct Frame {
  Header header;
  std::vector<uint8_t> payload;
};

// Subset of upstream DongleConfig used by the handshake. Defaults match DEFAULT_CONFIG.
struct HandshakeConfig {
  uint32_t width = 800;
  uint32_t height = 640;
  uint32_t fps = 20;
  uint32_t format = 5;
  uint32_t packet_max = 49152;
  uint32_t ibox_version = 2;
  uint32_t phone_work_mode = 2;
  uint32_t dpi = 160;
  bool night_mode = false;
  uint32_t hand = 0;  // 0 = LHD, 1 = RHD
  std::string box_name = "nodePlay";
  uint32_t media_delay = 300;
  bool audio_transfer = false;
  bool wifi_5ghz = true;
  bool mic_box = false;
  std::optional<bool> android_work_mode;  // sent only when set
};

std::array<uint8_t, kHeaderSize> encode_header(MsgType type, uint32_t length);
std::vector<uint8_t> encode_message(MsgType type, const std::vector<uint8_t>& payload);

// Returns nullopt and sets *error on any malformed header.
std::optional<Header> decode_header(const uint8_t* data, size_t size, FrameError* error = nullptr);

// Init messages in upstream send order. sync_time_s goes into the BoxSettings JSON.
std::vector<std::vector<uint8_t>> build_handshake(const HandshakeConfig& cfg, uint64_t sync_time_s);
// Sent once, about 1 s after the handshake.
std::vector<uint8_t> build_wifi_connect();
// Sent every 2 s while the session is open.
std::vector<uint8_t> build_heartbeat();

// True only when the transport accepted every byte.
bool send_message(hal::UsbTransport& transport, const std::vector<uint8_t>& message, unsigned timeout_ms);
// Reads one header, then exactly header.length payload bytes.
std::optional<Frame> read_frame(hal::UsbTransport& transport, unsigned timeout_ms, FrameError* error = nullptr);

}  // namespace hu::protocol
