#pragma once

// Recording fake of hal::UsbTransport for integration and unit tests. No libusb.
// The test plays the HAL role: attach() and detach() fire events on the sink the
// way the HAL would post them. Reads are served from rx. refuse_open makes open() fail.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "hal/usb.h"

namespace hu::integration {

class FakeUsbTransport final : public hal::UsbTransport {
 public:
  bool open(hal::UsbId id) override {
    if (refuse_open || !present_ || id.vendor_id != id_.vendor_id || id.product_id != id_.product_id) {
      return false;
    }
    open_ = true;
    ++open_count;
    return true;
  }

  void close() override { open_ = false; }
  bool is_open() const override { return open_; }

  int read(uint8_t* buffer, size_t capacity, unsigned /*timeout_ms*/) override {
    if (!open_) return -1;
    const size_t n = std::min(capacity, rx.size() - rx_pos);
    std::memcpy(buffer, rx.data() + rx_pos, n);
    rx_pos += n;
    return static_cast<int>(n);
  }

  int write(const uint8_t* buffer, size_t size, unsigned /*timeout_ms*/) override {
    if (!open_) return -1;
    writes.emplace_back(buffer, buffer + size);
    return static_cast<int>(size);
  }

  void set_event_sink(EventSink sink) override { sink_ = std::move(sink); }

  // Dongle cable events. Attached makes open() succeed; Detached closes the transport.
  void attach(hal::UsbId id) {
    id_ = id;
    present_ = true;
    if (sink_) sink_(hal::UsbEvent::Attached, id_);
  }

  void detach() {
    present_ = false;
    open_ = false;
    if (sink_) sink_(hal::UsbEvent::Detached, id_);
  }

  bool refuse_open = false;
  std::vector<std::vector<uint8_t>> writes;
  std::vector<uint8_t> rx;
  size_t rx_pos = 0;
  int open_count = 0;

 private:
  EventSink sink_;
  hal::UsbId id_{};
  bool present_ = false;
  bool open_ = false;
};

}  // namespace hu::integration
