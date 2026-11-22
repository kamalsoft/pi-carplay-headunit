// Derived from node-CarPlay, Copyright (c) 2022 Rhys Morgan, MIT License. See THIRD_PARTY_NOTICES.md.
#include "core/protocol/cpc200.h"

#include <cstdio>
#include <cstring>

namespace hu::protocol {
namespace {

// Every integer on the wire is little-endian.
void put_u32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

uint32_t get_u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

std::vector<uint8_t> u32_bytes(uint32_t v) {
  std::vector<uint8_t> out;
  put_u32(out, v);
  return out;
}

// SendFile payload: u32 name length (incl. NUL), name + NUL, u32 content length, content.
std::vector<uint8_t> build_send_file(const std::string& path, const std::vector<uint8_t>& content) {
  std::vector<uint8_t> payload;
  put_u32(payload, static_cast<uint32_t>(path.size() + 1));
  payload.insert(payload.end(), path.begin(), path.end());
  payload.push_back(0);
  put_u32(payload, static_cast<uint32_t>(content.size()));
  payload.insert(payload.end(), content.begin(), content.end());
  return encode_message(MsgType::SendFile, payload);
}

std::vector<uint8_t> build_number_file(uint32_t value, const std::string& path) {
  return build_send_file(path, u32_bytes(value));
}

std::vector<uint8_t> build_string_file(const std::string& value, const std::string& path) {
  return build_send_file(path, std::vector<uint8_t>(value.begin(), value.end()));
}

std::vector<uint8_t> build_command(Cmd cmd) {
  return encode_message(MsgType::Command, u32_bytes(static_cast<uint32_t>(cmd)));
}

// Seven u32 fields in upstream SendOpen order.
std::vector<uint8_t> build_open(const HandshakeConfig& cfg) {
  std::vector<uint8_t> payload;
  for (uint32_t v : {cfg.width, cfg.height, cfg.fps, cfg.format, cfg.packet_max, cfg.ibox_version,
                     cfg.phone_work_mode}) {
    put_u32(payload, v);
  }
  return encode_message(MsgType::Open, payload);
}

// JSON keys in the same order as upstream JSON.stringify.
std::vector<uint8_t> build_box_settings(const HandshakeConfig& cfg, uint64_t sync_time_s) {
  char json[160];
  std::snprintf(json, sizeof(json),
                "{\"mediaDelay\":%u,\"syncTime\":%llu,\"androidAutoSizeW\":%u,\"androidAutoSizeH\":%u}",
                cfg.media_delay, static_cast<unsigned long long>(sync_time_s), cfg.width, cfg.height);
  const size_t n = std::strlen(json);
  return encode_message(MsgType::BoxSettings, std::vector<uint8_t>(json, json + n));
}

}  // namespace

std::array<uint8_t, kHeaderSize> encode_header(MsgType type, uint32_t length) {
  const uint32_t t = static_cast<uint32_t>(type);
  std::array<uint8_t, kHeaderSize> out{};
  const uint32_t fields[4] = {kMagic, length, t, ~t};  // type check is the bitwise NOT of type
  for (size_t f = 0; f < 4; ++f) {
    for (size_t i = 0; i < 4; ++i) out[f * 4 + i] = static_cast<uint8_t>(fields[f] >> (8 * i));
  }
  return out;
}

std::vector<uint8_t> encode_message(MsgType type, const std::vector<uint8_t>& payload) {
  const auto header = encode_header(type, static_cast<uint32_t>(payload.size()));
  std::vector<uint8_t> out(header.begin(), header.end());
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::optional<Header> decode_header(const uint8_t* data, size_t size, FrameError* error) {
  auto fail = [error](FrameError e) {
    if (error) *error = e;
    return std::optional<Header>{};
  };
  if (size != kHeaderSize) return fail(FrameError::ShortBuffer);
  if (get_u32(data) != kMagic) return fail(FrameError::BadMagic);
  Header h;
  h.length = get_u32(data + 4);
  h.type = get_u32(data + 8);
  if (get_u32(data + 12) != ~h.type) return fail(FrameError::BadTypeCheck);
  if (h.length > kMaxPayload) return fail(FrameError::BadLength);
  if (error) *error = FrameError::None;
  return h;
}

std::vector<std::vector<uint8_t>> build_handshake(const HandshakeConfig& cfg, uint64_t sync_time_s) {
  std::vector<std::vector<uint8_t>> msgs;
  msgs.push_back(build_number_file(cfg.dpi, "/tmp/screen_dpi"));
  msgs.push_back(build_open(cfg));
  msgs.push_back(build_number_file(cfg.night_mode ? 1 : 0, "/tmp/night_mode"));
  msgs.push_back(build_number_file(cfg.hand, "/tmp/hand_drive_mode"));
  msgs.push_back(build_number_file(1, "/tmp/charge_mode"));
  msgs.push_back(build_string_file(cfg.box_name, "/etc/box_name"));
  msgs.push_back(build_box_settings(cfg, sync_time_s));
  msgs.push_back(build_command(Cmd::WifiEnable));
  msgs.push_back(build_command(cfg.wifi_5ghz ? Cmd::Wifi5g : Cmd::Wifi24g));
  msgs.push_back(build_command(cfg.mic_box ? Cmd::BoxMic : Cmd::Mic));
  msgs.push_back(build_command(cfg.audio_transfer ? Cmd::AudioTransferOn : Cmd::AudioTransferOff));
  if (cfg.android_work_mode) {
    msgs.push_back(build_number_file(*cfg.android_work_mode ? 1 : 0, "/etc/android_work_mode"));
  }
  return msgs;
}

std::vector<uint8_t> build_wifi_connect() { return build_command(Cmd::WifiConnect); }

std::vector<uint8_t> build_heartbeat() { return encode_message(MsgType::HeartBeat, {}); }

bool send_message(hal::UsbTransport& transport, const std::vector<uint8_t>& message, unsigned timeout_ms) {
  if (!transport.is_open()) return false;
  const int n = transport.write(message.data(), message.size(), timeout_ms);
  return n >= 0 && static_cast<size_t>(n) == message.size();
}

std::optional<Frame> read_frame(hal::UsbTransport& transport, unsigned timeout_ms, FrameError* error) {
  auto fail = [error](FrameError e) {
    if (error) *error = e;
    return std::optional<Frame>{};
  };
  std::array<uint8_t, kHeaderSize> raw{};
  const int hn = transport.read(raw.data(), raw.size(), timeout_ms);
  if (hn < 0) return fail(FrameError::Transport);
  if (static_cast<size_t>(hn) != kHeaderSize) return fail(FrameError::ShortRead);

  FrameError decode_error = FrameError::None;
  auto header = decode_header(raw.data(), raw.size(), &decode_error);
  if (!header) return fail(decode_error);

  Frame frame;
  frame.header = *header;
  if (header->length > 0) {
    frame.payload.resize(header->length);
    const int pn = transport.read(frame.payload.data(), frame.payload.size(), timeout_ms);
    if (pn < 0) return fail(FrameError::Transport);
    if (static_cast<size_t>(pn) != header->length) return fail(FrameError::ShortRead);
  }
  if (error) *error = FrameError::None;
  return frame;
}

}  // namespace hu::protocol
