#pragma once

// VideoDecoder for the Pi: H.264 through FFmpeg's h264_v4l2m2m (bcm2835-codec, /dev/video10),
// from the LGPL FFmpeg build (dynamic). No software fallback: open() fails if the hardware decoder
// is missing. FFmpeg types stay in the .cpp; this header is HAL-only.
// Threading: submit() and close() run on the core thread. Frames are posted to the CoreQueue and
// the sink runs when the core drains it, so the sink is always on the core thread.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "hal/macos/core_queue.h"  // shortcut: CoreQueue is platform-neutral but lives under hal/macos; move to src/hal/ when macOS code is next touched
#include "hal/video.h"

namespace hu::hal::pi {

class V4l2Decoder final : public hal::VideoDecoder {
 public:
  explicit V4l2Decoder(macos::CoreQueue& queue);
  ~V4l2Decoder() override;

  V4l2Decoder(const V4l2Decoder&) = delete;
  V4l2Decoder& operator=(const V4l2Decoder&) = delete;

  bool open(uint32_t width, uint32_t height) override;
  // Flushes frames still in the decoder into the queue, then frees it. Drain the queue after close().
  void close() override;
  bool submit(const uint8_t* annexb, size_t size, uint64_t pts_us) override;
  void set_frame_sink(FrameSink sink) override;
  hal::DecodeStats stats() const override;

 private:
  struct Impl;
  void receive_frames(uint64_t submit_us);
  void deliver(uint64_t submit_us);

  macos::CoreQueue& queue_;
  FrameSink sink_;
  hal::DecodeStats stats_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hu::hal::pi
