#pragma once

// Audio HAL interface. Two independent PCM channels: Media (music) and
// Navigation (turn voice). Navigation ducks Media through set_gain().
// Platform backends (CoreAudio on macOS, ALSA on Pi) implement this in D3.

#include <cstddef>
#include <cstdint>
#include <string>

namespace hu::hal {

enum class AudioChannel : uint8_t { Media = 0, Navigation = 1 };

struct PcmFormat {
  uint32_t sample_rate = 48000;
  uint8_t channels = 2;
  uint8_t bits_per_sample = 16;
};

class AudioOutput {
 public:
  virtual ~AudioOutput() = default;

  // device "" selects the platform default output. Reopening a channel reconfigures it.
  virtual bool open(AudioChannel channel, PcmFormat format, const std::string& device) = 0;
  virtual void close(AudioChannel channel) = 0;
  // Returns bytes accepted. Less than size means the buffer is full; the caller retries and never blocks the UI.
  virtual size_t write(AudioChannel channel, const uint8_t* pcm, size_t size) = 0;
  // gain in [0.0, 1.0].
  virtual void set_gain(AudioChannel channel, float gain) = 0;
  virtual uint32_t buffered_ms(AudioChannel channel) const = 0;  // HUD buffer health
  virtual uint64_t underruns(AudioChannel channel) const = 0;
};

}  // namespace hu::hal
