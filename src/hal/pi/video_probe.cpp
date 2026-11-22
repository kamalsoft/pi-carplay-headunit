// Pi video probe: decodes an Annex B H.264 file through h264_v4l2m2m and prints the frame count,
// frame size and decode latency. No network, no USB. Build-only tool, not linked into headunit.
// Usage: video_probe <stream.h264> [width height]   (defaults 800 640)

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "hal/macos/core_queue.h"
#include "hal/pi/v4l2_decoder.h"

namespace {

// Splits an Annex B stream into access units as [begin, end) byte ranges (begin includes the start code).
// A new access unit starts at an AUD, SPS, PPS or SEI, or at a slice with first_mb_in_slice == 0,
// once the current unit already holds a slice.
std::vector<std::pair<size_t, size_t>> split_access_units(const std::vector<uint8_t>& b) {
  std::vector<size_t> nal_starts;  // index of the NAL header byte
  for (size_t i = 0; i + 3 < b.size(); ++i) {
    if (b[i] == 0 && b[i + 1] == 0 && b[i + 2] == 1) {
      nal_starts.push_back(i + 3);
      i += 2;
    }
  }

  std::vector<std::pair<size_t, size_t>> units;
  size_t unit_begin = 0;
  bool has_slice = false;
  for (size_t k = 0; k < nal_starts.size(); ++k) {
    const size_t start = nal_starts[k];
    const uint8_t type = b[start] & 0x1f;
    const bool slice = type == 1 || type == 5;
    const bool first_mb0 = slice && start + 1 < b.size() && (b[start + 1] & 0x80);
    const bool boundary = type == 6 || type == 7 || type == 8 || type == 9 || first_mb0;
    if (k > 0 && boundary && has_slice) {
      units.emplace_back(unit_begin, start - 3);
      unit_begin = start - 3;
      has_slice = false;
    }
    if (slice) has_slice = true;
  }
  units.emplace_back(unit_begin, b.size());
  return units;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 4) {
    std::fprintf(stderr, "usage: %s <stream.h264> [width height]\n", argv[0]);
    return 2;
  }
  const uint32_t width = argc == 4 ? static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 800;
  const uint32_t height = argc == 4 ? static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 640;

  std::ifstream in(argv[1], std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }
  const std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const auto units = split_access_units(data);
  std::printf("file bytes %zu, access units %zu\n", data.size(), units.size());

  hu::hal::macos::CoreQueue queue;
  hu::hal::pi::V4l2Decoder decoder(queue);
  uint64_t sink_frames = 0;
  uint32_t out_w = 0, out_h = 0;
  size_t out_bytes = 0;
  decoder.set_frame_sink([&](const hu::hal::VideoFrame& f) {
    ++sink_frames;
    out_w = f.width;
    out_h = f.height;
    out_bytes = f.size;
  });

  if (!decoder.open(width, height)) {
    std::printf("decoder open %ux%u: failed (see stderr)\n", width, height);
    return 1;
  }
  std::printf("decoder open %ux%u: ok (h264_v4l2m2m)\n", width, height);

  // Latency samples: stats().last_latency_us, read after each drain whenever a frame was counted.
  uint64_t lat_sum = 0, lat_max = 0, lat_n = 0, last_decoded = 0;
  auto sample = [&] {
    const auto s = decoder.stats();
    if (s.frames_decoded != last_decoded) {
      last_decoded = s.frames_decoded;
      lat_sum += s.last_latency_us;
      if (s.last_latency_us > lat_max) lat_max = s.last_latency_us;
      ++lat_n;
    }
  };

  const auto t0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < units.size(); ++i) {
    const auto [begin, end] = units[i];
    decoder.submit(data.data() + begin, end - begin, static_cast<uint64_t>(i) * 33333);
    queue.drain();
    sample();
  }
  decoder.close();
  queue.drain();
  sample();
  const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

  const auto s = decoder.stats();
  std::printf("frames decoded %llu, dropped %llu, delivered to sink %llu\n",
              static_cast<unsigned long long>(s.frames_decoded), static_cast<unsigned long long>(s.frames_dropped),
              static_cast<unsigned long long>(sink_frames));
  std::printf("frame size %ux%u NV12 %zu bytes\n", out_w, out_h, out_bytes);
  std::printf("latency us: mean %llu, max %llu, last %u (samples %llu)\n",
              static_cast<unsigned long long>(lat_n ? lat_sum / lat_n : 0), static_cast<unsigned long long>(lat_max),
              s.last_latency_us, static_cast<unsigned long long>(lat_n));
  std::printf("wall time %lld ms\n", static_cast<long long>(wall_ms));
  return 0;
}
