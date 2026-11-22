#include "core/config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace hu {
namespace {

void require(bool ok, const char* msg) {
  if (!ok) throw std::runtime_error(msg);
}

}  // namespace

Config parse_config(const std::string& json_text) {
  const auto j = nlohmann::json::parse(json_text);
  Config c;
  c.env = j.at("env").get<std::string>();
  c.decoder = j.at("decoder").get<std::string>();
  c.audio_backend = j.at("audio_backend").get<std::string>();
  c.usb_vendor_id = j.at("usb").at("vendor_id").get<uint16_t>();
  c.usb_product_ids = j.at("usb").at("product_ids").get<std::vector<uint16_t>>();
  c.rss_target_mb = j.at("resources").at("rss_target_mb").get<unsigned>();
  c.rss_warn_mb = j.at("resources").at("rss_warn_mb").get<unsigned>();
  c.temp_warn_c = j.at("resources").at("temp_warn_c").get<unsigned>();

  if (c.env == "macos_sim") {
    require(c.decoder == "videotoolbox" && c.audio_backend == "coreaudio",
            "macos_sim requires videotoolbox decoder and coreaudio backend");
  } else if (c.env == "pi_prod") {
    require(c.decoder == "v4l2m2m" && c.audio_backend == "alsa",
            "pi_prod requires v4l2m2m decoder and alsa backend");
    require(c.rss_target_mb <= 260, "pi_prod rss_target_mb must be <= 260");
  } else {
    throw std::runtime_error("env must be macos_sim or pi_prod");
  }
  require(!c.usb_product_ids.empty(), "usb.product_ids must not be empty");
  require(c.rss_target_mb <= c.rss_warn_mb, "rss_target_mb must be <= rss_warn_mb");
  require(c.temp_warn_c > 0 && c.temp_warn_c <= 85, "temp_warn_c must be in 1..85");
  return c;
}

Config load_config(const std::string& path) {
  std::ifstream f(path);
  require(f.good(), "cannot open config file");
  std::stringstream ss;
  ss << f.rdbuf();
  return parse_config(ss.str());
}

}  // namespace hu
