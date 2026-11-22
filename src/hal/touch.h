#pragma once

// Input HAL interface. Touch or mouse, plus rotary/keys. Platform backends
// (SDL2 on both targets) implement this in D3.

#include <cstdint>
#include <functional>
#include <variant>

namespace hu::hal {

enum class TouchPhase : uint8_t { Down, Move, Up };

struct TouchEvent {
  TouchPhase phase = TouchPhase::Down;
  uint8_t pointer_id = 0;
  uint16_t x = 0;  // projection-space coordinates; mapping is D2/D3's job
  uint16_t y = 0;
  uint64_t t_us = 0;
};

enum class KeyCode : uint8_t { Up, Down, Left, Right, Enter, Back, Home, Next, Prev, PlayPause };

struct KeyEvent {
  KeyCode key = KeyCode::Enter;
  bool pressed = false;
  uint64_t t_us = 0;
};

using InputEvent = std::variant<TouchEvent, KeyEvent>;

class InputSource {
 public:
  using Sink = std::function<void(const InputEvent&)>;

  virtual ~InputSource() = default;

  // Sink runs on the core thread. The HAL posts input to the core queue (threading decision).
  virtual void set_sink(Sink sink) = 0;
};

}  // namespace hu::hal
