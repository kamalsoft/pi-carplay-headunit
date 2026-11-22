#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

#include "simulator/simulator.h"

namespace hal = hu::hal;
using hu::sim::Simulator;

TEST(Simulator, ConnectDetachAndIgnitionOffArriveInOrder) {
  Simulator sim;
  std::vector<std::string> log;
  const hal::UsbId id{1314, 1520};

  sim.usb.set_event_sink([&](hal::UsbEvent ev, hal::UsbId) {
    if (ev == hal::UsbEvent::Attached) {
      log.push_back("attached");
      EXPECT_TRUE(sim.usb.open(id));  // the core opens on Attached
    } else if (ev == hal::UsbEvent::Detached) {
      log.push_back("detached");
    }
  });
  sim.power.set_ignition_sink([&](hal::IgnitionState s) {
    log.push_back(s == hal::IgnitionState::Off ? "ignition_off" : "ignition_on");
  });

  sim.usb.attach(id);                          // connect
  sim.yank_usb();                              // one Detached
  sim.power.set_ignition(hal::IgnitionState::Off);
  EXPECT_TRUE(log.empty()) << "sinks must not run before pump()";

  EXPECT_EQ(sim.pump(), 3u);
  const std::vector<std::string> expected{"attached", "detached", "ignition_off"};
  EXPECT_EQ(log, expected);
  EXPECT_FALSE(sim.usb.is_open());
  EXPECT_EQ(sim.power.poll().ignition, hal::IgnitionState::Off);
}

TEST(Simulator, LowVoltageAndThermalFaultsShowInPowerStatus) {
  Simulator sim;
  sim.low_voltage(true);
  sim.thermal_warning(76.0f);
  sim.pump();
  const hal::PowerStatus hot = sim.power.poll();
  EXPECT_EQ(hot.throttled_flags, hal::kThrottleLiveUndervoltage);
  EXPECT_GT(hot.cpu_temp_c, 75.0f);

  sim.low_voltage(false);
  sim.pump();
  EXPECT_EQ(sim.power.poll().throttled_flags, 0u);
}

TEST(Simulator, VideoTestPatternTouchAndAudioTone) {
  Simulator sim;

  // Video: NV12 test pattern, one frame per valid access unit.
  std::vector<size_t> frame_sizes;
  uint8_t first_luma = 0xFF;
  sim.video.set_frame_sink([&](const hal::VideoFrame& f) {
    frame_sizes.push_back(f.size);
    first_luma = f.data[0];
  });
  ASSERT_TRUE(sim.video.open(16, 8));
  const uint8_t au[] = {0, 0, 0, 1, 0x65, 0x88};
  EXPECT_TRUE(sim.video.submit(au, sizeof au, 0));
  const uint8_t bad[] = {0x65, 0x88};  // no start code
  EXPECT_FALSE(sim.video.submit(bad, sizeof bad, 0));
  sim.pump();
  ASSERT_EQ(frame_sizes.size(), 1u);
  EXPECT_EQ(frame_sizes[0], 16u * 8u * 3u / 2u);
  EXPECT_EQ(first_luma, 0);
  EXPECT_EQ(sim.video.stats().frames_decoded, 1u);
  EXPECT_EQ(sim.video.stats().frames_dropped, 1u);

  // Input: a tap is Down then Up; a scroll of 3 steps is Down, 2 Moves, Up.
  std::vector<hal::TouchPhase> phases;
  sim.input.set_sink([&](const hal::InputEvent& ev) {
    if (const auto* t = std::get_if<hal::TouchEvent>(&ev)) {
      phases.push_back(t->phase);
    }
  });
  sim.input.tap(10, 20);
  sim.input.scroll(10, 100, -30, 3);
  sim.pump();
  using P = hal::TouchPhase;
  const std::vector<P> expected{P::Down, P::Up, P::Down, P::Move, P::Move, P::Up};
  EXPECT_EQ(phases, expected);

  // Audio: a 1 s tone into a 500 ms buffer. The rest waits for later pumps.
  ASSERT_TRUE(sim.audio.open(hal::AudioChannel::Media, hal::PcmFormat{}, ""));
  EXPECT_FALSE(sim.audio.open(hal::AudioChannel::Navigation, hal::PcmFormat{48000, 2, 24}, ""));
  sim.play_tone(hal::AudioChannel::Media, 440.0f, 1000);
  sim.pump();
  EXPECT_LE(sim.audio.buffered_ms(hal::AudioChannel::Media), 500u);
  EXPECT_GT(sim.audio.buffered_ms(hal::AudioChannel::Media), 400u);
  sim.audio.set_gain(hal::AudioChannel::Navigation, 1.5f);
  EXPECT_FLOAT_EQ(sim.audio.gain(hal::AudioChannel::Navigation), 1.0f);  // clamped
}
