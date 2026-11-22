#pragma once

// Video HAL interface. H.264 access units in, decoded frames out.
// Platform decoders (VideoToolbox on macOS, V4L2 M2M on Pi) implement this in D3.
// No platform headers belong here.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace hu::hal {

enum class PixelFormat : uint8_t { Nv12, Bgra8 };

struct VideoFrame {
  uint32_t width = 0;
  uint32_t height = 0;
  PixelFormat format = PixelFormat::Nv12;
  uint64_t pts_us = 0;
  const uint8_t* data = nullptr;  // borrowed: valid only during the FrameSink call
  size_t size = 0;
};

struct DecodeStats {
  uint64_t frames_decoded = 0;
  uint64_t frames_dropped = 0;
  uint32_t last_latency_us = 0;  // submit to frame-out, feeds the HUD
};

class VideoDecoder {
 public:
  using FrameSink = std::function<void(const VideoFrame&)>;

  virtual ~VideoDecoder() = default;

  virtual bool open(uint32_t width, uint32_t height) = 0;
  virtual void close() = 0;
  // annexb: one H.264 access unit with Annex B start codes. False if rejected.
  virtual bool submit(const uint8_t* annexb, size_t size, uint64_t pts_us) = 0;
  // Sink runs on the core thread. The HAL posts frames to the core queue (threading decision).
  virtual void set_frame_sink(FrameSink sink) = 0;
  virtual DecodeStats stats() const = 0;
};

}  // namespace hu::hal
