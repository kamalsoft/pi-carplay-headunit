#include "simulator/simulator.h"

#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace hu::sim {

namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr double kAudioBufferMs = 500.0;
constexpr uint64_t kFpsWindowUs = 1000000;
constexpr uint64_t kRssWarnKb = 280ull * 1024ull;  // CLAUDE.md §2

}  // namespace

void EventQueue::post(Task task) {
  std::lock_guard<std::mutex> lock(mutex_);
  tasks_.push_back(std::move(task));
}

size_t EventQueue::drain() {
  std::deque<Task> batch;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    batch.swap(tasks_);
  }
  for (auto& task : batch) {
    task();
  }
  return batch.size();
}

uint64_t now_us() {
  using namespace std::chrono;
  return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

void FpsCounter::tick(uint64_t t_us) {
  stamps_.push_back(t_us);
  while (!stamps_.empty() && stamps_.front() + kFpsWindowUs < t_us) {
    stamps_.pop_front();
  }
}

double FpsCounter::fps(uint64_t t_us) const {
  const auto in_window = std::count_if(stamps_.begin(), stamps_.end(),
                                       [t_us](uint64_t s) { return s + kFpsWindowUs >= t_us; });
  return static_cast<double>(in_window);
}

bool SimVideoDecoder::open(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0 || width % 2 != 0 || height % 2 != 0) {
    return false;  // NV12 needs even dimensions
  }
  width_ = width;
  height_ = height;
  open_ = true;
  return true;
}

void SimVideoDecoder::close() {
  open_ = false;
  ++epoch_;
}

bool SimVideoDecoder::submit(const uint8_t* annexb, size_t size, uint64_t pts_us) {
  // Trust boundary: reject input without an Annex B start code.
  const bool start3 = size >= 3 && annexb[0] == 0 && annexb[1] == 0 && annexb[2] == 1;
  const bool start4 = size >= 4 && annexb[0] == 0 && annexb[1] == 0 && annexb[2] == 0 && annexb[3] == 1;
  if (!open_ || !(start3 || start4)) {
    ++stats_.frames_dropped;
    return false;
  }
  const uint32_t w = width_;
  const uint32_t h = height_;
  const uint32_t epoch = epoch_;
  const uint64_t index = frame_index_++;
  const uint64_t submitted = now_us();
  queue_.post([this, w, h, epoch, index, pts_us, submitted] {
    if (epoch != epoch_) {
      ++stats_.frames_dropped;  // closed after this frame was submitted
      return;
    }
    const size_t luma = static_cast<size_t>(w) * h;
    std::vector<uint8_t> nv12(luma * 3 / 2, 128);
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        nv12[y * w + x] = static_cast<uint8_t>(x + y + index * 4);
      }
    }
    const hal::VideoFrame frame{w, h, hal::PixelFormat::Nv12, pts_us, nv12.data(), nv12.size()};
    if (sink_) {
      sink_(frame);
    }
    ++stats_.frames_decoded;
    const uint64_t done = now_us();
    stats_.last_latency_us = static_cast<uint32_t>(done - submitted);
    fps.tick(done);
  });
  return true;
}

bool SimUsbTransport::open(hal::UsbId id) {
  if (!present_ || id.vendor_id != id_.vendor_id || id.product_id != id_.product_id) {
    return false;
  }
  open_ = true;
  return true;
}

void SimUsbTransport::close() {
  open_ = false;
  feed_.clear();
}

int SimUsbTransport::read(uint8_t* buffer, size_t capacity, unsigned /*timeout_ms*/) {
  if (!open_) {
    return -1;
  }
  // Returns at once: 0 means nothing queued (the timeout case).
  const size_t n = std::min(capacity, feed_.size());
  std::copy_n(feed_.begin(), n, buffer);
  feed_.erase(feed_.begin(), feed_.begin() + static_cast<std::ptrdiff_t>(n));
  bytes_read_ += n;
  return static_cast<int>(n);
}

int SimUsbTransport::write(const uint8_t* /*buffer*/, size_t size, unsigned /*timeout_ms*/) {
  if (!open_) {
    return -1;
  }
  bytes_written_ += size;
  return static_cast<int>(size);
}

void SimUsbTransport::attach(hal::UsbId id) {
  queue_.post([this, id] {
    id_ = id;
    present_ = true;
    if (sink_) {
      sink_(hal::UsbEvent::Attached, id);
    }
  });
}

void SimUsbTransport::yank() {
  queue_.post([this] {
    if (!present_) {
      return;  // nothing to yank
    }
    present_ = false;
    open_ = false;
    feed_.clear();
    if (sink_) {
      sink_(hal::UsbEvent::Detached, id_);
    }
  });
}

void SimUsbTransport::feed(std::vector<uint8_t> bytes) {
  queue_.post([this, data = std::move(bytes)] { feed_.insert(feed_.end(), data.begin(), data.end()); });
}

void SimInputSource::touch(hal::TouchPhase phase, uint16_t x, uint16_t y) {
  const hal::TouchEvent ev{phase, 0, x, y, now_us()};
  queue_.post([this, ev] {
    if (sink_) {
      sink_(ev);
    }
  });
}

void SimInputSource::tap(uint16_t x, uint16_t y) {
  touch(hal::TouchPhase::Down, x, y);
  touch(hal::TouchPhase::Up, x, y);
}

void SimInputSource::scroll(uint16_t x, uint16_t y, int dy, unsigned steps) {
  const int n = static_cast<int>(std::max(1u, steps));
  const auto clamp16 = [](int v) { return static_cast<uint16_t>(std::clamp(v, 0, 65535)); };
  touch(hal::TouchPhase::Down, x, y);
  uint16_t last_y = y;
  for (int i = 1; i <= n; ++i) {
    last_y = clamp16(static_cast<int>(y) + dy * i / n);
    touch(i == n ? hal::TouchPhase::Up : hal::TouchPhase::Move, x, last_y);
  }
}

void SimInputSource::key(hal::KeyCode key) {
  const uint64_t t = now_us();
  const hal::KeyEvent press{key, true, t};
  const hal::KeyEvent release{key, false, t};
  queue_.post([this, press] {
    if (sink_) {
      sink_(press);
    }
  });
  queue_.post([this, release] {
    if (sink_) {
      sink_(release);
    }
  });
}

bool SimAudioOutput::open(hal::AudioChannel channel, hal::PcmFormat format, const std::string& /*device*/) {
  // S16 only. The device name is ignored: the sim has one output.
  if (format.sample_rate == 0 || format.channels == 0 || format.bits_per_sample != 16) {
    return false;
  }
  Channel& c = ch_[index(channel)];
  c.open = true;
  c.format = format;
  c.bytes_per_us = static_cast<double>(format.sample_rate) * format.channels * 2 / 1e6;
  c.capacity = c.bytes_per_us * 1e6 * kAudioBufferMs / 1000.0;
  c.pending = 0.0;
  c.last_us = now_us();
  c.playing = false;
  c.underruns = 0;
  c.tone_rest.clear();
  return true;
}

void SimAudioOutput::close(hal::AudioChannel channel) {
  Channel& c = ch_[index(channel)];
  c.open = false;
  c.pending = 0.0;
  c.playing = false;
  c.tone_rest.clear();
}

void SimAudioOutput::advance(const Channel& c) const {
  if (!c.open) {
    return;
  }
  const uint64_t now = now_us();
  c.pending -= static_cast<double>(now - c.last_us) * c.bytes_per_us;
  c.last_us = now;
  if (c.pending <= 0.0) {
    c.pending = 0.0;
    if (c.playing) {
      // shortcut: any drain to empty counts as an underrun, including a tone that simply ended. A real feeder that stops at end-of-track trips it too; upgrade when the core reports feed state.
      ++c.underruns;
      c.playing = false;
    }
  }
}

size_t SimAudioOutput::accept(Channel& c, size_t size) {
  if (!c.open) {
    return 0;
  }
  advance(c);
  const size_t room = static_cast<size_t>(c.capacity - c.pending);
  size_t n = std::min(size, room);
  const size_t block = static_cast<size_t>(c.format.channels) * 2;
  n -= n % block;  // whole frames only
  if (n > 0) {
    c.pending += static_cast<double>(n);
    c.playing = true;
  }
  return n;
}

size_t SimAudioOutput::write(hal::AudioChannel channel, const uint8_t* /*pcm*/, size_t size) {
  return accept(ch_[index(channel)], size);
}

void SimAudioOutput::set_gain(hal::AudioChannel channel, float gain) {
  ch_[index(channel)].gain = std::clamp(gain, 0.0f, 1.0f);
}

uint32_t SimAudioOutput::buffered_ms(hal::AudioChannel channel) const {
  const Channel& c = ch_[index(channel)];
  advance(c);
  if (c.bytes_per_us <= 0.0) {
    return 0;
  }
  return static_cast<uint32_t>(c.pending / c.bytes_per_us / 1000.0);
}

uint64_t SimAudioOutput::underruns(hal::AudioChannel channel) const {
  const Channel& c = ch_[index(channel)];
  advance(c);
  return c.underruns;
}

void SimAudioOutput::play_tone(hal::AudioChannel channel, float hz, uint32_t ms) {
  Channel& c = ch_[index(channel)];
  if (!c.open) {
    return;
  }
  const size_t frames = static_cast<size_t>(c.format.sample_rate) * ms / 1000;
  const size_t block = static_cast<size_t>(c.format.channels) * 2;
  c.tone_rest.reserve(c.tone_rest.size() + frames * block);
  for (size_t f = 0; f < frames; ++f) {
    const double t = static_cast<double>(f) / c.format.sample_rate;
    const auto s = static_cast<int16_t>(std::sin(kTwoPi * hz * t) * 0.25 * 32767.0);
    const auto lo = static_cast<uint8_t>(static_cast<uint16_t>(s) & 0xFF);
    const auto hi = static_cast<uint8_t>(static_cast<uint16_t>(s) >> 8);
    for (uint8_t ch = 0; ch < c.format.channels; ++ch) {
      c.tone_rest.push_back(lo);
      c.tone_rest.push_back(hi);
    }
  }
  flush_tones();
}

void SimAudioOutput::flush_tones() {
  for (auto& c : ch_) {
    if (c.tone_rest.empty()) {
      continue;
    }
    const size_t n = accept(c, c.tone_rest.size());
    c.tone_rest.erase(c.tone_rest.begin(), c.tone_rest.begin() + static_cast<std::ptrdiff_t>(n));
  }
}

SimPowerMonitor::SimPowerMonitor(EventQueue& queue) : queue_(queue) {
  status_.ignition = hal::IgnitionState::On;
}

void SimPowerMonitor::set_ignition(hal::IgnitionState state) {
  queue_.post([this, state] {
    if (status_.ignition == state) {
      return;  // report changes only
    }
    status_.ignition = state;
    if (sink_) {
      sink_(state);
    }
  });
}

void SimPowerMonitor::set_throttled_flags(uint32_t flags) {
  queue_.post([this, flags] { status_.throttled_flags = flags; });
}

void SimPowerMonitor::set_cpu_temp_c(float temp_c) {
  queue_.post([this, temp_c] { status_.cpu_temp_c = temp_c; });
}

uint64_t rss_kb() {
#if defined(__linux__)
  // /proc/self/statm reports pages: size resident shared text lib data dt
  std::FILE* f = std::fopen("/proc/self/statm", "r");
  if (f == nullptr) {
    return 0;
  }
  unsigned long long size = 0;
  unsigned long long resident = 0;
  const int fields = std::fscanf(f, "%llu %llu", &size, &resident);
  std::fclose(f);
  if (fields != 2) {
    return 0;
  }
  return resident * static_cast<uint64_t>(sysconf(_SC_PAGESIZE)) / 1024;
#else
  // shortcut: getrusage gives peak RSS, not current. ru_maxrss is bytes on macOS. Upgrade to mach task_info only if the HUD needs live values.
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return static_cast<uint64_t>(usage.ru_maxrss) / 1024;
#endif
}

Simulator::Simulator()
    : video(queue_), usb(queue_), input(queue_), audio(), power(queue_) {}

size_t Simulator::pump() {
  const size_t ran = queue_.drain();
  audio.flush_tones();
  return ran;
}

void Simulator::low_voltage(bool on) {
  power.set_throttled_flags(on ? hal::kThrottleLiveUndervoltage : 0);
}

void Simulator::play_tone(hal::AudioChannel channel, float hz, uint32_t ms) {
  queue_.post([this, channel, hz, ms] { audio.play_tone(channel, hz, ms); });
}

HudReport Simulator::hud() const {
  HudReport r;
  r.fps = video.fps.fps(now_us());
  r.decode_latency_us = video.stats().last_latency_us;
  r.media_buffer_ms = audio.buffered_ms(hal::AudioChannel::Media);
  r.nav_buffer_ms = audio.buffered_ms(hal::AudioChannel::Navigation);
  r.underruns = audio.underruns(hal::AudioChannel::Media) + audio.underruns(hal::AudioChannel::Navigation);
  r.usb_open = usb.is_open();
  r.rss_kb = rss_kb();
  r.rss_over_warn = r.rss_kb > kRssWarnKb;
  return r;
}

}  // namespace hu::sim
