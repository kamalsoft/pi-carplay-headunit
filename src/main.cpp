#include <cstdio>
#include <exception>

#include "core/config.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <config.json>\n", argv[0]);
    return 2;
  }
  try {
    const auto c = hu::load_config(argv[1]);
    std::printf("ENV=%s decoder=%s audio=%s rss_target=%uMB rss_warn=%uMB temp_warn=%uC\n",
                c.env.c_str(), c.decoder.c_str(), c.audio_backend.c_str(),
                c.rss_target_mb, c.rss_warn_mb, c.temp_warn_c);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "config error: %s\n", e.what());
    return 1;
  }
  return 0;
}
