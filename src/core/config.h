#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hu {

struct Config {
  std::string env;            // "macos_sim" or "pi_prod"
  std::string decoder;        // "videotoolbox" or "v4l2m2m"
  std::string audio_backend;  // "coreaudio" or "alsa"
  uint16_t usb_vendor_id = 0;
  std::vector<uint16_t> usb_product_ids;
  unsigned rss_target_mb = 0;
  unsigned rss_warn_mb = 0;
  unsigned temp_warn_c = 0;
};

// Throws std::exception (nlohmann::json or std::runtime_error) on invalid input.
Config parse_config(const std::string& json_text);
Config load_config(const std::string& path);

}  // namespace hu
