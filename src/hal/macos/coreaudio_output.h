#pragma once

// AudioOutput over CoreAudio AudioQueue (AudioToolbox framework). One queue per
// channel. Media and Navigation are independent; ducking is set_gain() on the
// Media queue's volume parameter.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "hal/audio.h"

namespace hu::hal::macos {

class CoreAudioOutput final : public hal::AudioOutput {
 public:
  CoreAudioOutput();
  ~CoreAudioOutput() override;

  CoreAudioOutput(const CoreAudioOutput&) = delete;
  CoreAudioOutput& operator=(const CoreAudioOutput&) = delete;

  bool open(hal::AudioChannel channel, hal::PcmFormat format, const std::string& device) override;
  void close(hal::AudioChannel channel) override;
  size_t write(hal::AudioChannel channel, const uint8_t* pcm, size_t size) override;
  void set_gain(hal::AudioChannel channel, float gain) override;
  uint32_t buffered_ms(hal::AudioChannel channel) const override;
  uint64_t underruns(hal::AudioChannel channel) const override;

 public:
  struct Channel;  // defined in coreaudio_output.cpp; public only so the AudioQueue callback can reach it

 private:
  Channel* slot(hal::AudioChannel channel) const;

  std::array<std::unique_ptr<Channel>, 2> channels_;
};

}  // namespace hu::hal::macos
