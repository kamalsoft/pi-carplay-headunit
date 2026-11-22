#pragma once

// Core state machine: states, events, and the transition table. Header-only
// so the table can be checked at compile time. Timing (for example the 2 s
// standby reset) is enforced by the watchdog, not here.

#include <cstdint>
#include <optional>

namespace hu::core {

enum class State : uint8_t { Boot, Standby, Connecting, Projecting, Shutdown };

enum class Event : uint8_t {
  Started,
  DongleAttached,
  DongleDetached,
  HandshakeFailed,
  PhoneConnected,
  PhoneDisconnected,
  IgnitionOff,
  ShutdownSignal,  // SIGTERM or SIGINT
};

struct Transition {
  State from;
  Event on;
  State to;
};

// Each (from, on) pair appears at most once. Shutdown has no outgoing rows.
inline constexpr Transition kTransitions[] = {
    {State::Boot, Event::Started, State::Standby},
    {State::Standby, Event::DongleAttached, State::Connecting},
    {State::Connecting, Event::PhoneConnected, State::Projecting},
    {State::Connecting, Event::HandshakeFailed, State::Standby},
    {State::Connecting, Event::DongleDetached, State::Standby},
    {State::Projecting, Event::PhoneDisconnected, State::Connecting},
    {State::Projecting, Event::DongleDetached, State::Standby},
    {State::Boot, Event::IgnitionOff, State::Shutdown},
    {State::Boot, Event::ShutdownSignal, State::Shutdown},
    {State::Standby, Event::IgnitionOff, State::Shutdown},
    {State::Standby, Event::ShutdownSignal, State::Shutdown},
    {State::Connecting, Event::IgnitionOff, State::Shutdown},
    {State::Connecting, Event::ShutdownSignal, State::Shutdown},
    {State::Projecting, Event::IgnitionOff, State::Shutdown},
    {State::Projecting, Event::ShutdownSignal, State::Shutdown},
};

// Returns the next state, or nullopt when the event is not valid in this state.
inline constexpr std::optional<State> next_state(State from, Event on) {
  for (const auto& t : kTransitions) {
    if (t.from == from && t.on == on) {
      return t.to;
    }
  }
  return std::nullopt;
}

}  // namespace hu::core
