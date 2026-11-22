// Hardware probe for the macOS HAL. Opens the CPC200 dongle (1314:1520, then 1521), reads once with a
// timeout, and exercises the decoder and audio open/close. It never writes to the dongle, so it sends no protocol frames.
// Build-only tool: not linked into headunit.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "hal/macos/coreaudio_output.h"
#include "hal/macos/libusb_transport.h"
#include "hal/macos/videotoolbox_decoder.h"

namespace {

const char* event_name(hu::hal::UsbEvent e) {
  switch (e) {
    case hu::hal::UsbEvent::Attached: return "Attached";
    case hu::hal::UsbEvent::Detached: return "Detached";
    case hu::hal::UsbEvent::Error: return "Error";
  }
  return "?";
}

}  // namespace

int main() {
  using namespace hu;
  hal::macos::CoreQueue queue;

  hal::macos::LibusbTransport usb(queue);
  usb.set_event_sink([](hal::UsbEvent e, hal::UsbId id) {
    std::printf("usb event %s %04x:%04x\n", event_name(e), id.vendor_id, id.product_id);
  });
  const hal::UsbId candidates[] = {{1314, 1520}, {1314, 1521}};
  bool opened = false;
  for (const auto& id : candidates) {
    if (usb.open(id)) {
      std::printf("usb open %04x:%04x ok\n", id.vendor_id, id.product_id);
      opened = true;
      break;
    }
  }
  std::printf("usb tasks drained %zu\n", queue.drain());
  if (opened) {
    std::vector<uint8_t> buf(16384);
    const int n = usb.read(buf.data(), buf.size(), 1000);
    std::printf("usb read rc=%d (negative = libusb error)\n", n);
    usb.close();
    std::printf("usb closed, tasks drained %zu\n", queue.drain());
  } else {
    std::printf("usb open failed for all candidates (dongle absent or not accessible)\n");
  }

  hal::macos::VideoToolboxDecoder decoder(queue);
  std::printf("decoder open 800x640: %s\n", decoder.open(800, 640) ? "ok" : "failed");
  decoder.close();

  hal::macos::CoreAudioOutput audio;
  hal::PcmFormat fmt;
  const bool audio_ok = audio.open(hal::AudioChannel::Media, fmt, "");
  std::printf("audio open Media 48k/2ch/16: %s\n", audio_ok ? "ok" : "failed");
  if (audio_ok) {
    std::vector<uint8_t> silence(48000 * 2 * 2 / 10, 0);  // 100 ms
    std::printf("audio write accepted %zu of %zu bytes\n", audio.write(hal::AudioChannel::Media, silence.data(), silence.size()), silence.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::printf("audio buffered_ms=%u underruns=%llu\n", audio.buffered_ms(hal::AudioChannel::Media),
                static_cast<unsigned long long>(audio.underruns(hal::AudioChannel::Media)));
    audio.close(hal::AudioChannel::Media);
  }
  return 0;
}
