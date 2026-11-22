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

## FFmpeg build record (track N3)
- Version: FFmpeg 9.1 (`ffmpeg-9.1.tar.xz` from ffmpeg.org/releases)
- SHA-256: `1a4d394ae63d190a526cd052eb4415c156b5c5e2df37efab41a7de0b8fde2433`
- Verified: PGP signature (`.asc`) good against FFmpeg release key `FCF986EA15E6E293A5644F10B4322F04D67658D8`
- Configure (run from an out-of-tree build dir):
  `configure --prefix=/Users/kamalsoft/dev/pi/pi-carplay-headunit/build/third_party/ffmpeg --disable-gpl --disable-nonfree --enable-shared --disable-static --disable-doc`
- Result: `ffmpeg -version` shows no `--enable-gpl`. The LGPL wording is in the libraries (`strings libavutil.61.dylib` reports "LGPL version 2.1 or later"); the ffmpeg binary itself prints no license line. Libraries are dynamic (`.dylib`); no x264/x265.
- Note: configure auto-detected Homebrew libxcb/libX11 (used by the xcb grab device); harmless to the license, but disable if a fully self-contained build is wanted.

## Open items
- Project license: **Apache-2.0** (`LICENSE`), chosen 2026-11-21.
- `THIRD_PARTY_NOTICES.md` lists every notice above. Update it when a dependency is added or linked.
- Apple CarPlay: the dongle carries Apple's licensed authentication. Do not use Apple logos, the "CarPlay" name as a product name, or claim Apple certification. Personal use with a dongle you own is the intended scope; confirm with counsel before any sale or distribution.
