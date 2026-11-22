#include <gtest/gtest.h>

#include <string>

#include "core/config.h"

namespace {

std::string sim_json(const std::string& resources = R"("rss_target_mb": 260, "rss_warn_mb": 280, "temp_warn_c": 75)") {
  return R"({"env": "macos_sim", "decoder": "videotoolbox", "audio_backend": "coreaudio",
    "usb": {"vendor_id": 1314, "product_ids": [1520, 1521]},
    "resources": {)" + resources + "}}";
}

}  // namespace

TEST(Config, LoadsShippedProfiles) {
  const auto sim = hu::load_config(HU_CONFIG_DIR "/macos_sim.json");
  EXPECT_EQ(sim.env, "macos_sim");
  const auto pi = hu::load_config(HU_CONFIG_DIR "/pi_prod.json");
  EXPECT_EQ(pi.env, "pi_prod");
  EXPECT_EQ(pi.rss_target_mb, 260u);
  EXPECT_EQ(pi.usb_product_ids.size(), 2u);
}

TEST(Config, AcceptsValidSimProfile) {
  EXPECT_NO_THROW(hu::parse_config(sim_json()));
}

TEST(Config, RejectsUnknownEnv) {
  auto text = sim_json();
  text.replace(text.find("macos_sim"), 9, "windows");
  EXPECT_THROW(hu::parse_config(text), std::runtime_error);
}

TEST(Config, RejectsPiTargetAboveBudget) {
  const std::string pi = R"({"env": "pi_prod", "decoder": "v4l2m2m", "audio_backend": "alsa",
    "usb": {"vendor_id": 1314, "product_ids": [1520]},
    "resources": {"rss_target_mb": 300, "rss_warn_mb": 320, "temp_warn_c": 75}})";
  EXPECT_THROW(hu::parse_config(pi), std::runtime_error);
}

TEST(Config, RejectsTargetAboveWarn) {
  EXPECT_THROW(hu::parse_config(sim_json(R"("rss_target_mb": 300, "rss_warn_mb": 280, "temp_warn_c": 75)")),
               std::runtime_error);
}

TEST(Config, RejectsEmptyProductIds) {
  auto text = sim_json();
  text.replace(text.find("[1520, 1521]"), 12, "[]");
  EXPECT_THROW(hu::parse_config(text), std::runtime_error);
}

TEST(Config, RejectsMissingField) {
  EXPECT_THROW(hu::parse_config(R"({"env": "macos_sim"})"), std::exception);
}

TEST(Config, RejectsMalformedJson) {
  EXPECT_THROW(hu::parse_config("{not json"), std::exception);
}
