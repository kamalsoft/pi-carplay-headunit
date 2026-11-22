#include "core/core.h"

#include <cstdio>
#include <utility>

namespace hu::core {

Event map_usb_event(hal::UsbEvent e) {
  return e == hal::UsbEvent::Attached ? Event::DongleAttached : Event::DongleDetached;
}

Core::Core(hal::UsbTransport& usb, hal::PowerMonitor& power, const Clock& clock,
           std::vector<plugin::HeadUnitPlugin*> plugins, protocol::HandshakeConfig handshake)
    : usb_(usb),
      power_(power),
      clock_(clock),
      plugins_(std::move(plugins)),
      handshake_(std::move(handshake)) {
  usb_.set_event_sink([this](hal::UsbEvent e, hal::UsbId id) { post_usb(e, id); });
}

Core::~Core() {
  // The transport may outlive the core; stop it calling into a dead object.
  usb_.set_event_sink([](hal::UsbEvent, hal::UsbId) {});
}

void Core::start() {
  post(Event::Started);
  pump();
}

void Core::post(Event e) { enqueue(Message{e, hal::UsbId{}}); }

void Core::post_usb(hal::UsbEvent e, hal::UsbId id) { enqueue(Message{map_usb_event(e), id}); }

void Core::enqueue(Message m) {
  std::lock_guard<std::mutex> lock(mu_);
  queue_.push_back(m);
}

void Core::pump() {
  for (;;) {
    Message m{};
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (queue_.empty()) return;
      m = queue_.front();
      queue_.pop_front();
    }
    apply(m);
  }
}

void Core::tick() {
  const uint64_t now = clock_.now_ms();

  if (state_ == State::Connecting && wifi_due_ && now >= *wifi_due_) {
    wifi_due_.reset();
    if (!protocol::send_message(usb_, protocol::build_wifi_connect(), kWriteTimeoutMs)) {
      post(Event::HandshakeFailed);
    }
  }

  if (state_ == State::Projecting && heartbeat_due_ && now >= *heartbeat_due_) {
    *heartbeat_due_ = now + kHeartbeatPeriodMs;
    // A failed heartbeat write is not handled here. The USB Error or Detached event drives recovery.
    protocol::send_message(usb_, protocol::build_heartbeat(), kWriteTimeoutMs);
  }

  if (now >= next_power_poll_ms_) {
    next_power_poll_ms_ = now + kPowerPollPeriodMs;
    apply_power(power_.poll());
  }

  pump();
}

void Core::apply(const Message& m) {
  const auto next = next_state(state_, m.event);
  if (!next) return;  // not valid in this state; the table is the only source of transitions
  const State from = state_;
  leave(from);
  state_ = *next;
  enter(state_, m.id);
}

void Core::leave(State from) {
  switch (from) {
    case State::Connecting:
      wifi_due_.reset();
      break;
    case State::Projecting:
      heartbeat_due_.reset();
      for (auto* p : plugins_) p->onCarPlayDisconnect(session_);
      break;
    default:
      break;
  }
}

void Core::enter(State to, hal::UsbId id) {
  switch (to) {
    case State::Standby:
      usb_.close();
      break;
    case State::Connecting:
      // Open only when closed: Projecting -> Connecting keeps the transport.
      if (!usb_.is_open() && !usb_.open(id)) {
        post(Event::HandshakeFailed);
        return;
      }
      if (!send_handshake()) {
        post(Event::HandshakeFailed);
        return;
      }
      wifi_due_ = clock_.now_ms() + kWifiConnectDelayMs;
      break;
    case State::Projecting:
      heartbeat_due_ = clock_.now_ms() + kHeartbeatPeriodMs;
      // shortcut: wireless is always false here, upgrade when the inbound parser reports the link type.
      session_ = plugin::CarPlaySession{++session_seq_, false};
      for (auto* p : plugins_) p->onCarPlayConnect(session_);
      break;
    case State::Shutdown:
      usb_.close();
      for (auto* p : plugins_) p->onShutdown();
      break;
    default:
      break;
  }
}

// Builds the init sequence on every call, so the BoxSettings sync time is current.
bool Core::send_handshake() {
  for (const auto& msg : protocol::build_handshake(handshake_, clock_.unix_time_s())) {
    if (!protocol::send_message(usb_, msg, kWriteTimeoutMs)) return false;
  }
  return true;
}

// Suspend on live under-voltage or thermal limit, and resume only when both clear
// (CLAUDE.md 3.2). The sticky bits are logged on change and never gate suspension.
void Core::apply_power(const hal::PowerStatus& p) {
  const uint32_t sticky = p.throttled_flags & hal::kThrottleStickyMask;
  if (sticky != last_sticky_) {
    std::fprintf(stderr, "power: sticky throttle bits 0x%x\n", static_cast<unsigned>(sticky));
    last_sticky_ = sticky;
  }

  const bool want = (p.throttled_flags & hal::kThrottleLiveUndervoltage) != 0 || p.cpu_temp_c > kThermalSuspendC;
  if (want == suspended_) return;
  suspended_ = want;
  for (auto* plug : plugins_) {
    if (want) {
      plug->onSuspend();
    } else {
      plug->onResume();
    }
  }
}

}  // namespace hu::core
