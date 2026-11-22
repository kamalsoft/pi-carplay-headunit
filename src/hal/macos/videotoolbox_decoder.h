#pragma once

// VideoDecoder: H.264 through the LGPL FFmpeg build (dynamic) with the
// VideoToolbox hwaccel. No software fallback: open() fails if VideoToolbox is
// unavailable. FFmpeg types stay in the .cpp; this header is HAL-only.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "hal/macos/core_queue.h"
#include "hal/video.h"

namespace hu::hal::macos {

class VideoToolboxDecoder final : public hal::VideoDecoder {
 public:
  explicit VideoToolboxDecoder(CoreQueue& queue);
  ~VideoToolboxDecoder() override;

  VideoToolboxDecoder(const VideoToolboxDecoder&) = delete;
  VideoToolboxDecoder& operator=(const VideoToolboxDecoder&) = delete;

  bool open(uint32_t width, uint32_t height) override;
  void close() override;
  bool submit(const uint8_t* annexb, size_t size, uint64_t pts_us) override;
  void set_frame_sink(FrameSink sink) override;
  hal::DecodeStats stats() const override;

 private:
  struct Impl;
  void receive_frames(uint64_t submit_us);
  void deliver(uint64_t submit_us);

  CoreQueue& queue_;
  FrameSink sink_;
  hal::DecodeStats stats_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hu::hal::macos
