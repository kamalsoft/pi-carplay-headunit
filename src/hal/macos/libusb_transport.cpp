#include "hal/macos/libusb_transport.h"

#include <climits>

#include <libusb.h>

namespace hu::hal::macos {

namespace {

// Interface of the first configuration: first bulk IN and first bulk OUT endpoint, as node-CarPlay picks them.
bool find_bulk_endpoints(const libusb_config_descriptor* cfg, uint8_t& iface, uint8_t& in, uint8_t& out) {
  if (!cfg || cfg->bNumInterfaces == 0 || cfg->interface[0].num_altsetting == 0) return false;
  const libusb_interface_descriptor& alt = cfg->interface[0].altsetting[0];
  bool have_in = false;
  bool have_out = false;
  for (uint8_t i = 0; i < alt.bNumEndpoints; ++i) {
    const libusb_endpoint_descriptor& ep = alt.endpoint[i];
    if ((ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK) continue;
    if (ep.bEndpointAddress & LIBUSB_ENDPOINT_IN) {
      if (!have_in) in = ep.bEndpointAddress;
      have_in = true;
    } else {
      if (!have_out) out = ep.bEndpointAddress;
      have_out = true;
    }
  }
  iface = alt.bInterfaceNumber;
  return have_in && have_out;
}

}  // namespace

LibusbTransport::LibusbTransport(CoreQueue& queue) : queue_(queue) {
  // ctx_ stays null on failure; open() then returns false.
  if (libusb_init(&ctx_) != 0) ctx_ = nullptr;
}

LibusbTransport::~LibusbTransport() {
  close();
  if (ctx_) libusb_exit(ctx_);
}

bool LibusbTransport::open(hal::UsbId id) {
  close();
  id_ = id;  // set before any emit, so Error events carry the requested id
  if (!ctx_) return false;

  libusb_device** list = nullptr;
  const ssize_t count = libusb_get_device_list(ctx_, &list);
  if (count < 0) return false;

  libusb_device_handle* handle = nullptr;
  for (ssize_t i = 0; i < count && !handle; ++i) {
    libusb_device_descriptor desc{};
    if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
    if (desc.idVendor != id.vendor_id || desc.idProduct != id.product_id) continue;
    if (libusb_open(list[i], &handle) != 0) handle = nullptr;  // try the next match
  }
  libusb_free_device_list(list, 1);
  if (!handle) return false;  // not present or not accessible: plain failure, no event

  // The device is there but unusable: report it as an Error event as well.
  libusb_config_descriptor* cfg = nullptr;
  uint8_t iface = 0;
  uint8_t in = 0;
  uint8_t out = 0;
  const bool have_cfg = libusb_get_active_config_descriptor(libusb_get_device(handle), &cfg) == 0;
  const bool have_eps = have_cfg && find_bulk_endpoints(cfg, iface, in, out);
  if (have_cfg) libusb_free_config_descriptor(cfg);
  if (!have_eps || libusb_claim_interface(handle, iface) != 0) {
    libusb_close(handle);
    emit(hal::UsbEvent::Error);
    return false;
  }

  handle_ = handle;
  interface_ = iface;
  in_ep_ = in;
  out_ep_ = out;
  emit(hal::UsbEvent::Attached);
  return true;
}

void LibusbTransport::close() {
  if (!handle_) return;
  libusb_release_interface(handle_, interface_);  // fails if the device is already gone; nothing more to do
  libusb_close(handle_);
  handle_ = nullptr;
}

bool LibusbTransport::is_open() const { return handle_ != nullptr; }

int LibusbTransport::read(uint8_t* buffer, size_t capacity, unsigned timeout_ms) {
  if (!handle_) return LIBUSB_ERROR_NO_DEVICE;
  if (capacity > INT_MAX) return LIBUSB_ERROR_INVALID_PARAM;
  int transferred = 0;
  const int rc = libusb_bulk_transfer(handle_, in_ep_, buffer, static_cast<int>(capacity), &transferred, timeout_ms);
  return finish_transfer(rc, transferred);
}

int LibusbTransport::write(const uint8_t* buffer, size_t size, unsigned timeout_ms) {
  if (!handle_) return LIBUSB_ERROR_NO_DEVICE;
  if (size > INT_MAX) return LIBUSB_ERROR_INVALID_PARAM;
  int transferred = 0;
  // libusb takes a non-const buffer but does not write to an OUT transfer.
  const int rc = libusb_bulk_transfer(handle_, out_ep_, const_cast<uint8_t*>(buffer), static_cast<int>(size), &transferred, timeout_ms);
  return finish_transfer(rc, transferred);
}

void LibusbTransport::set_event_sink(EventSink sink) { sink_ = std::move(sink); }

void LibusbTransport::emit(hal::UsbEvent event) {
  if (!sink_) return;
  queue_.post([sink = sink_, event, id = id_] { sink(event, id); });
}

int LibusbTransport::finish_transfer(int rc, int transferred) {
  if (rc == 0) return transferred;
  // A timeout can still carry bytes already received; report those, not the error.
  if (rc == LIBUSB_ERROR_TIMEOUT) return transferred > 0 ? transferred : rc;
  if (rc == LIBUSB_ERROR_NO_DEVICE) {
    emit(hal::UsbEvent::Detached);
    close();
    return rc;
  }
  emit(hal::UsbEvent::Error);
  return rc;
}

}  // namespace hu::hal::macos
