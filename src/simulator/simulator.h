#pragma once

// macOS mock CarPlay stream (CLAUDE.md §2). Simulator only: CMake builds it
// when HU_SIMULATOR is ON, which requires HU_ENV=macos_sim. It implements the
// D1 HAL interfaces and nothing else: no protocol code (D2), no platform libraries.
//
// Threading: sinks run on the core thread (decided). Every posting method
// (attach, yank, feed, tap, set_ignition, ...) may be called from any thread;
// it queues a task in EventQueue. Tasks run only inside Simulator::pump(), on
// the core thread, and that is the only place sinks and state change. The
// read-side methods (open, close, read, submit, poll, buffered_ms, stats) are
// core-thread only.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "hal/audio.h"
#include "hal/power.h"
#include "hal/touch.h"
#include "hal/usb.h"
#include "hal/video.h"

namespace hu::sim {

using Task = std::function<void()>;

class EventQueue {
 public:
  void post(Task task);  // any thread
  size_t drain();        // core thread: runs every task posted so far, FIFO
 private:
  std::mutex mutex_;
  std::deque<Task> tasks_;
};

uint64_t now_us();  // steady clock, microseconds

// Frames per second over the last second. Core thread only.
class FpsCounter {
 public:
  void tick(uint64_t t_us);
  double fps(uint64_t t_us) const;

 private:
  std::deque<uint64_t> stamps_;
};

// Test pattern: NV12 luma ramp that scrolls each frame, flat chroma. Annex B
// input is checked for a start code only; the content is not decoded.
class SimVideoDecoder final : public hal::VideoDecoder {
 public:
  explicit SimVideoDecoder(EventQueue& queue) : queue_(queue) {}

  bool open(uint32_t width, uint32_t height) override;
  void close() override;
  bool submit(const uint8_t* annexb, size_t size, uint64_t pts_us) override;
  void set_frame_sink(FrameSink sink) override { sink_ = std::move(sink); }
  hal::DecodeStats stats() const override { return stats_; }

  FpsCounter fps;

 private:
  EventQueue& queue_;
  FrameSink sink_;
  hal::DecodeStats stats_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  bool open_ = false;
  uint32_t epoch_ = 0;  // bumped on close; frames queued before a close are dropped
  uint64_t frame_index_ = 0;
};

// Raw byte feed on the read side. No framing: D2 owns the protocol.
class SimUsbTransport final : public hal::UsbTransport {
 public:
  explicit SimUsbTransport(EventQueue& queue) : queue_(queue) {}

  bool open(hal::UsbId id) override;
  void close() override;
  bool is_open() const override { return open_; }
  int read(uint8_t* buffer, size_t capacity, unsigned timeout_ms) override;
  int write(const uint8_t* buffer, size_t size, unsigned timeout_ms) override;
  void set_event_sink(EventSink sink) override { sink_ = std::move(sink); }

  void attach(hal::UsbId id);  // posts UsbEvent::Attached
  void yank();                 // cable pulled: posts one UsbEvent::Detached, closes the transport
  void feed(std::vector<uint8_t> bytes);  // queued for read()

  uint64_t bytes_read() const { return bytes_read_; }
  uint64_t bytes_written() const { return bytes_written_; }

 private:
  EventQueue& queue_;
  EventSink sink_;
  hal::UsbId id_{};
  bool present_ = false;
  bool open_ = false;
  std::deque<uint8_t> feed_;
  uint64_t bytes_read_ = 0;
  uint64_t bytes_written_ = 0;
};

// Touch and scroll events on pointer 0, timestamped when posted.
class SimInputSource final : public hal::InputSource {
 public:
  explicit SimInputSource(EventQueue& queue) : queue_(queue) {}

  void set_sink(Sink sink) override { sink_ = std::move(sink); }  // set before pump

  void tap(uint16_t x, uint16_t y);
  // Down at (x, y), `steps` Moves toward y + dy, Up at the end. steps 0 acts as 1.
  void scroll(uint16_t x, uint16_t y, int dy, unsigned steps);
  void key(hal::KeyCode key);  // press, then release

 private:
  void touch(hal::TouchPhase phase, uint16_t x, uint16_t y);

  EventQueue& queue_;
  Sink sink_;
};

// Two PCM channels, S16 only. Buffers drain in real time, so buffered_ms and
// underruns depend on the clock.
class SimAudioOutput final : public hal::AudioOutput {
 public:
  bool open(hal::AudioChannel channel, hal::PcmFormat format, const std::string& device) override;
  void close(hal::AudioChannel channel) override;
  size_t write(hal::AudioChannel channel, const uint8_t* pcm, size_t size) override;
  void set_gain(hal::AudioChannel channel, float gain) override;
  uint32_t buffered_ms(hal::AudioChannel channel) const override;
  uint64_t underruns(hal::AudioChannel channel) const override;

  // Generates a sine tone (quarter full scale) and writes as much as the buffer takes.
  // The rest waits in tone_rest and goes out on flush_tones(), which pump() calls.
  void play_tone(hal::AudioChannel channel, float hz, uint32_t ms);
  void flush_tones();
  float gain(hal::AudioChannel channel) const { return ch_[index(channel)].gain; }

 private:
  struct Channel {
    bool open = false;
    hal::PcmFormat format;
    float gain = 1.0f;
    double bytes_per_us = 0.0;
    double capacity = 0.0;  // bytes; 500 ms of audio
    mutable double pending = 0.0;
    mutable uint64_t last_us = 0;
    mutable bool playing = false;
    mutable uint64_t underruns = 0;
    std::vector<uint8_t> tone_rest;
  };

  static size_t index(hal::AudioChannel c) { return static_cast<size_t>(c); }
  void advance(const Channel& c) const;
  size_t accept(Channel& c, size_t size);

  mutable Channel ch_[2];
};

// Ignition starts On. Changes post; the sink runs only on a real change.
class SimPowerMonitor final : public hal::PowerMonitor {
 public:
  using IgnitionSink = std::function<void(hal::IgnitionState)>;

  explicit SimPowerMonitor(EventQueue& queue);

  hal::PowerStatus poll() override { return status_; }

  void set_ignition_sink(IgnitionSink sink) { sink_ = std::move(sink); }
  void set_ignition(hal::IgnitionState state);  // posts
  void set_throttled_flags(uint32_t flags);     // posts; low voltage is hal::kThrottleLiveUndervoltage
  void set_cpu_temp_c(float temp_c);            // posts

 private:
  EventQueue& queue_;
  hal::PowerStatus status_;
  IgnitionSink sink_;
};

// Snapshot for the Debug & Resource HUD (CLAUDE.md §2).
struct HudReport {
  double fps = 0.0;
  uint32_t decode_latency_us = 0;
  uint32_t media_buffer_ms = 0;
  uint32_t nav_buffer_ms = 0;
  uint64_t underruns = 0;
  bool usb_open = false;
  uint64_t rss_kb = 0;
  bool rss_over_warn = false;  // above 280 MB (CLAUDE.md §2)
};

// Current RSS on Linux (/proc/self/statm). Elsewhere it is peak RSS (getrusage).
uint64_t rss_kb();

class Simulator {
 public:
  Simulator();

  // Core thread. Runs every task posted so far, in order, then flushes tone
  // audio. Returns the number of tasks run.
  size_t pump();

  // Fault hooks (CLAUDE.md §3, used by D5). Any thread; they apply at pump().
  void yank_usb() { usb.yank(); }
  void low_voltage(bool on);
  void thermal_warning(float cpu_temp_c) { power.set_cpu_temp_c(cpu_temp_c); }

  void play_tone(hal::AudioChannel channel, float hz, uint32_t ms);  // posts
  HudReport hud() const;  // core thread

 private:
  EventQueue queue_;  // declared first so the components can reference it and die before it

 public:
  SimVideoDecoder video;
  SimUsbTransport usb;
  SimInputSource input;
  SimAudioOutput audio;
  SimPowerMonitor power;
};

}  // namespace hu::sim
