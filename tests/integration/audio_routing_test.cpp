// Integration (simulator, no dongle): Media and Navigation tones are routed to
// their own channels, and the underrun counters are kept per channel.
// Backend: simulator (src/simulator). Built only when HU_SIMULATOR is ON.

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "hal/audio.h"
#include "simulator/simulator.h"

namespace {

using hu::hal::AudioChannel;
using hu::hal::PcmFormat;

constexpr uint32_t kToneMs = 50;

// The sim buffer drains in real time. 200 ms is four tone lengths.
void wait_for_drain() { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }

}  // namespace

TEST(AudioRouting, MediaToneLandsOnlyInMediaChannel) {
  hu::sim::Simulator sim;
  ASSERT_TRUE(sim.audio.open(AudioChannel::Media, PcmFormat{}, ""));
  ASSERT_TRUE(sim.audio.open(AudioChannel::Navigation, PcmFormat{}, ""));

  sim.play_tone(AudioChannel::Media, 440.0f, kToneMs);
  sim.pump();

  EXPECT_GT(sim.audio.buffered_ms(AudioChannel::Media), 0u);
  EXPECT_EQ(sim.audio.buffered_ms(AudioChannel::Navigation), 0u);
}

TEST(AudioRouting, NavigationToneLandsOnlyInNavigationChannel) {
  hu::sim::Simulator sim;
  ASSERT_TRUE(sim.audio.open(AudioChannel::Media, PcmFormat{}, ""));
  ASSERT_TRUE(sim.audio.open(AudioChannel::Navigation, PcmFormat{}, ""));

  sim.play_tone(AudioChannel::Navigation, 880.0f, kToneMs);
  sim.pump();

  EXPECT_GT(sim.audio.buffered_ms(AudioChannel::Navigation), 0u);
  EXPECT_EQ(sim.audio.buffered_ms(AudioChannel::Media), 0u);
}

TEST(AudioRouting, UnderrunCountersArePerChannel) {
  hu::sim::Simulator sim;
  ASSERT_TRUE(sim.audio.open(AudioChannel::Media, PcmFormat{}, ""));
  ASSERT_TRUE(sim.audio.open(AudioChannel::Navigation, PcmFormat{}, ""));

  // Media tone drains to empty: one underrun on Media only.
  sim.play_tone(AudioChannel::Media, 440.0f, kToneMs);
  sim.pump();
  wait_for_drain();
  EXPECT_EQ(sim.audio.underruns(AudioChannel::Media), 1u);
  EXPECT_EQ(sim.audio.underruns(AudioChannel::Navigation), 0u);

  // Navigation tone drains: one underrun on Navigation, Media stays at 1.
  sim.play_tone(AudioChannel::Navigation, 880.0f, kToneMs);
  sim.pump();
  wait_for_drain();
  EXPECT_EQ(sim.audio.underruns(AudioChannel::Navigation), 1u);
  EXPECT_EQ(sim.audio.underruns(AudioChannel::Media), 1u);
}
