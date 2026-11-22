#pragma once

// Core driver (AUDIT A-20 to A-23). Owns the state machine and the event queue,
// runs the CPC200 handshake and the core timers, and dispatches power state
// to plugins (CLAUDE.md 3.2). Threading: HAL threads call post() and
// post_usb(); the core thread alone calls start(), pump(), tick() and state().
// Transitions come only from next_state() in state.h.

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "core/clock.h"
#include "core/protocol/cpc200.h"
#include "core/state.h"
#include "hal/power.h"
#include "hal/usb.h"
#include "plugins/plugin.h"

namespace hu::core {

// Timer periods (AUDIT A-21).
inline constexpr uint64_t kWifiConnectDelayMs = 1000;
inline constexpr uint64_t kHeartbeatPeriodMs = 2000;
inline constexpr uint64_t kPowerPollPeriodMs = 1000;
// Thermal suspend threshold, strictly above (CLAUDE.md 3.2).
inline constexpr float kThermalSuspendC = 75.0f;
inline constexpr unsigned kWriteTimeoutMs = 100;

// Maps a HAL cable event to a state event. Error maps to DongleDetached: the
// state table has no error event, and a failed transfer means the link is gone.
Event map_usb_event(hal::UsbEvent e);

class Core {
 public:
  Core(hal::UsbTransport& usb, hal::PowerMonitor& power, const Clock& clock,
       std::vector<plugin::HeadUnitPlugin*> plugins, protocol::HandshakeConfig handshake = {});
  ~Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // Core thread. Boot -> Standby.
  void start();
  // Any thread. Queues a state event.
  void post(Event e);
  // Any thread. Maps the HAL event with map_usb_event and queues it.
  void post_usb(hal::UsbEvent e, hal::UsbId id);
  // Core thread. Runs every queued event in FIFO order.
  void pump();
  // Core thread. Runs due timers and the power poll, then pump().
  void tick();

  // Core thread only.
  State state() const { return state_; }

 private:
  struct Message {
    Event event;
    hal::UsbId id;
  };

  void enqueue(Message m);
  void apply(const Message& m);
  void leave(State from);
  void enter(State to, hal::UsbId id);
  bool send_handshake();
  void apply_power(const hal::PowerStatus& p);

  hal::UsbTransport& usb_;
  hal::PowerMonitor& power_;
  const Clock& clock_;
  std::vector<plugin::HeadUnitPlugin*> plugins_;
  protocol::HandshakeConfig handshake_;

  State state_ = State::Boot;
  std::optional<uint64_t> wifi_due_;
  std::optional<uint64_t> heartbeat_due_;
  uint64_t next_power_poll_ms_ = 0;
  bool suspended_ = false;
  uint32_t last_sticky_ = 0;  // logging only (CLAUDE.md 3.2)
  plugin::CarPlaySession session_{};
  uint32_t session_seq_ = 0;

  std::mutex mu_;
  std::deque<Message> queue_;
};

}  // namespace hu::core
