// Stage 1 demo (CLAUDE.md §5 Stage 1): drives the real core against the macOS
// simulator with no dongle attached. Prints a timeline and a HUD line each
// second. Every event here is simulated, including the phone connection.

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "core/clock.h"
#include "core/config.h"
#include "core/core.h"
#include "simulator/simulator.h"

namespace {

using hu::core::Event;
using hu::core::State;

uint64_t g_t0_ms = 0;

void logf(const char* fmt, ...) {
  std::printf("[%6.2f s] ", static_cast<double>(hu::core::SystemClock().now_ms() - g_t0_ms) / 1000.0);
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stdout, fmt, args);
  va_end(args);
  std::printf("\n");
}

const char* state_name(State s) {
  switch (s) {
    case State::Boot: return "Boot";
    case State::Standby: return "Standby";
    case State::Connecting: return "Connecting";
    case State::Projecting: return "Projecting";
    case State::Shutdown: return "Shutdown";
  }
  return "?";
}

// Plugin that reports every lifecycle call, so the demo shows the dispatch.
class DemoPlugin final : public hu::plugin::HeadUnitPlugin {
 public:
  std::string_view name() const override { return "demo-status"; }
  bool init(const hu::Config&) override { logf("plugin: init"); return true; }
  void onCarPlayConnect(const hu::plugin::CarPlaySession& s) override {
    logf("plugin: CarPlay session %u connected", s.id);
  }
  void onCarPlayDisconnect(const hu::plugin::CarPlaySession& s) override {
    logf("plugin: CarPlay session %u disconnected", s.id);
  }
  void onAudioFocusChange(hu::plugin::AudioFocus f) override {
    logf("plugin: audio focus %s", f == hu::plugin::AudioFocus::Navigation ? "Navigation" : "Media");
  }
  void onSuspend() override { logf("plugin: SUSPEND (power throttled)"); }
  void onResume() override { logf("plugin: RESUME (power normal)"); }
  void onShutdown() override { logf("plugin: shutdown"); }
};

struct Step {
  uint64_t at_ms;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  const std::string config_path = argc > 1 ? argv[1] : "config/macos_sim.json";
  hu::Config config;
  try {
    config = hu::load_config(config_path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "config error: %s\n", e.what());
    return 1;
  }

  hu::sim::Simulator sim;
  hu::core::SystemClock clock;
  DemoPlugin plugin;
  hu::core::Core core(sim.usb, sim.power, clock, {&plugin});

  // Wire the simulator's HAL sinks into the core, as the real HAL will.
  sim.usb.set_event_sink([&](hu::hal::UsbEvent e, hu::hal::UsbId id) { core.post_usb(e, id); });
  sim.power.set_ignition_sink([&](hu::hal::IgnitionState s) {
    logf("ignition %s", s == hu::hal::IgnitionState::On ? "ON" : "OFF");
    if (s == hu::hal::IgnitionState::Off) core.post(Event::IgnitionOff);
  });
  sim.input.set_sink([&](const hu::hal::InputEvent& ev) {
    if (const auto* t = std::get_if<hu::hal::TouchEvent>(&ev)) {
      if (t->phase != hu::hal::TouchPhase::Move) {
        logf("touch %s at (%u, %u)", t->phase == hu::hal::TouchPhase::Down ? "down" : "up", t->x, t->y);
      }
    } else if (const auto* k = std::get_if<hu::hal::KeyEvent>(&ev)) {
      logf("key: %d", static_cast<int>(k->key));
    }
  });
  sim.video.set_frame_sink([&](const hu::hal::VideoFrame&) {});

  g_t0_ms = clock.now_ms();
  core.start();
  logf("state: %s (config %s, profile %s)", state_name(core.state()), config_path.c_str(), config.env.c_str());
  sim.video.open(800, 640);

  const std::vector<Step> steps = {
      {500, [&] { logf("simulated dongle plugged in"); sim.usb.attach({1314, 1520}); }},
      {2000, [&] { logf("simulated phone connects over the dongle"); core.post(Event::PhoneConnected); }},
      {2500, [&] { sim.input.tap(400, 240); }},
      {3000, [&] { sim.input.scroll(400, 300, -200, 10); }},
      {3500, [&] { logf("audio: Media tone 440 Hz, 1 s"); sim.audio.play_tone(hu::hal::AudioChannel::Media, 440.0f, 1000); }},
      {4000, [&] { logf("audio: Navigation tone 880 Hz, 300 ms"); sim.audio.play_tone(hu::hal::AudioChannel::Navigation, 880.0f, 300); }},
      {4500, [&] { logf("fault: undervoltage ON"); sim.low_voltage(true); }},
      {5500, [&] { logf("fault: undervoltage OFF"); sim.low_voltage(false); }},
      {6000, [&] { logf("fault: CPU 78 C"); sim.thermal_warning(78.0f); }},
      {6800, [&] { logf("fault: CPU 60 C"); sim.thermal_warning(60.0f); }},
      {7500, [&] { logf("fault: USB cable pulled"); sim.yank_usb(); }},
      {8500, [&] { sim.power.set_ignition(hu::hal::IgnitionState::Off); }},
  };
  const uint64_t end_ms = 9000;

  size_t next = 0;
  uint64_t last_hud_s = 0;
  uint64_t last_frame_ms = 0;
  State last_state = core.state();
  while (true) {
    const uint64_t elapsed = clock.now_ms() - g_t0_ms;
    if (elapsed >= end_ms) break;

    while (next < steps.size() && elapsed >= steps[next].at_ms) {
      steps[next].run();
      ++next;
    }

    // A test frame every ~33 ms while the core is projecting.
    if (core.state() == State::Projecting && elapsed - last_frame_ms >= 33) {
      const uint8_t annexb[] = {0, 0, 0, 1, 0x65, 0x88, 0x84, 0x00};
      sim.video.submit(annexb, sizeof(annexb), elapsed * 1000);
      last_frame_ms = elapsed;
    }

    sim.pump();
    core.pump();
    core.tick();

    if (core.state() != last_state) {
      logf("state: %s -> %s", state_name(last_state), state_name(core.state()));
      last_state = core.state();
    }

    if (elapsed / 1000 > last_hud_s) {
      last_hud_s = elapsed / 1000;
      const auto h = sim.hud();
      logf("HUD fps=%.1f decode=%uus media=%ums nav=%ums underruns=%llu usb=%s rss=%lluKB%s",
           h.fps, h.decode_latency_us, h.media_buffer_ms, h.nav_buffer_ms,
           static_cast<unsigned long long>(h.underruns), h.usb_open ? "open" : "closed",
           static_cast<unsigned long long>(h.rss_kb), h.rss_over_warn ? " OVER 280MB" : "");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  sim.pump();
  core.pump();
  logf("demo finished in state %s, bytes written to dongle=%llu", state_name(core.state()),
       static_cast<unsigned long long>(sim.usb.bytes_written()));
  return 0;
}
