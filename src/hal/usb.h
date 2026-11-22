#pragma once

// USB HAL interface. Passive bulk transport only: the implementation moves
// protocol bytes defined by D2 and never issues vendor control transfers or
// writes to a vehicle port (CLAUDE.md §3.1).

#include <cstddef>
#include <cstdint>
#include <functional>

namespace hu::hal {

struct UsbId {
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
};

enum class UsbEvent : uint8_t { Attached, Detached, Error };

class UsbTransport {
 public:
  using EventSink = std::function<void(UsbEvent, UsbId)>;

  virtual ~UsbTransport() = default;

  virtual bool open(UsbId id) = 0;
  virtual void close() = 0;
  virtual bool is_open() const = 0;
  // Returns bytes transferred, or a negative error code. timeout_ms bounds every call.
  virtual int read(uint8_t* buffer, size_t capacity, unsigned timeout_ms) = 0;
  virtual int write(const uint8_t* buffer, size_t size, unsigned timeout_ms) = 0;
  // Sink runs on the core thread. The HAL posts events to the core queue (threading decision).
  virtual void set_event_sink(EventSink sink) = 0;
};

}  // namespace hu::hal
