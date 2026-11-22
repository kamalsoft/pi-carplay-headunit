#pragma once

// UsbTransport over libusb-1.0 (LGPL-2.1, linked dynamically). Passive bulk I/O
// only: claims interface 0 and moves bytes on its bulk endpoints. No vendor or
// class control transfers, no writes beyond what the caller passes to write().

#include <cstddef>
#include <cstdint>

#include "hal/macos/core_queue.h"
#include "hal/usb.h"

struct libusb_context;
struct libusb_device_handle;

namespace hu::hal::macos {

class LibusbTransport final : public hal::UsbTransport {
 public:
  explicit LibusbTransport(CoreQueue& queue);
  ~LibusbTransport() override;

  LibusbTransport(const LibusbTransport&) = delete;
  LibusbTransport& operator=(const LibusbTransport&) = delete;

  bool open(hal::UsbId id) override;
  void close() override;
  bool is_open() const override;
  int read(uint8_t* buffer, size_t capacity, unsigned timeout_ms) override;
  int write(const uint8_t* buffer, size_t size, unsigned timeout_ms) override;
  void set_event_sink(EventSink sink) override;

 private:
  void emit(hal::UsbEvent event);
  int finish_transfer(int rc, int transferred);

  CoreQueue& queue_;
  EventSink sink_;
  libusb_context* ctx_ = nullptr;
  libusb_device_handle* handle_ = nullptr;
  hal::UsbId id_{};
  uint8_t interface_ = 0;
  uint8_t in_ep_ = 0;
  uint8_t out_ep_ = 0;
};

}  // namespace hu::hal::macos
