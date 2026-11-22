# Licensing Rules

Every dependency and every borrowed line must be legal to ship. Check before adding.

## Sources

| Source | License | Use |
|---|---|---|
| rhysmorgan134/node-CarPlay | MIT (© 2022 Rhys Morgan) | **Allowed.** Port the CPC200 protocol from it. Keep its MIT notice in every file derived from it. |
| jwinarske/carlinkit-cxx | Apache-2.0 | **Reference only.** Its `src/protocol.h` is a port of node-CarPlay but drops the MIT notice, so do not copy it. |
| jwinarske/drm-cxx | MIT | Allowed if used. Not used in Step 1. |
| f-io/LIVI, f-io/FastCarPlay, Veyron2K/Carlinkit-CPC200-Autokit-Reverse-Engineering, ludwig-v/wireless-carplay-dongle-reverse-engineering, den67rus firmware repos | GPL-3.0 | **Forbidden.** Do not copy, port, or read them to write our code. |
| Unlicensed or missing-license repos (e.g. ppietak/node-carplay) | none | **Forbidden.** No license means no rights. |
| Carlinkit firmware (on the dongle) | proprietary | **Do not** dump, modify, or redistribute. Use only the USB protocol. |

## Dependencies

| Library | License | Link mode |
|---|---|---|
| nlohmann/json | MIT | static, header-only |
| GoogleTest | BSD-3-Clause | tests only |
| SDL2 (Homebrew `sdl2-compat`) | Zlib | any |
| libusb | LGPL-2.1-or-later | **dynamic only** (keeps the app relinkable) |
| FFmpeg | LGPL-2.1+ build only | **dynamic only**. Homebrew `ffmpeg` is GPL-3.0 and must not be used. Build our own with `--disable-gpl --disable-nonfree` (no x264/x265). Step 2. |

## Open items
- Project license for our own code is not chosen yet. Pick before first public release.
- `THIRD_PARTY_NOTICES.md` must list every notice above before distribution.
- Apple CarPlay: the dongle carries Apple's licensed authentication. Do not use Apple logos, the "CarPlay" name as a product name, or claim Apple certification. Personal use with a dongle you own is the intended scope; confirm with counsel before any sale or distribution.
