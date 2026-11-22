# RAM Budget

Target: app + receiver RSS under **260 MB** on Pi Zero 2 W (512 MB total, OS separate).
Warning: **280 MB** (HUD flags it on macOS sim).

These are **estimates**, not measurements. Step 4 replaces them with profiler numbers.

| Component | Est. RSS |
|---|---|
| Core state machine, config, plugin registry | ~10 MB |
| SDL2 UI and display | ~25 MB |
| H.264 decode and frame buffers (FFmpeg) | ~35 MB |
| libusb and CPC200 protocol | ~5 MB |
| Audio (ALSA / CoreAudio) and buffers | ~10 MB |
| Plugin headroom | ~10 MB |
| **Total estimate** | **~95 MB** (≈165 MB under target) |

Pi Zero 2 W has no hardware-decode validation in the reference project (it was validated on Pi 4 and Pi 5 only). Verify on the device before trusting the decode row.

## Pi Zero 2 W facts (research)

Research only. No Pi is connected to this machine, so no measured values appear here. Status: **CONFIRMED** (cited), **UNKNOWN** (no source found or not checked), **BLOCKED** (needs hardware). Audit ref: A-06.

### 1. Cross-compile toolchain (aarch64 Raspberry Pi OS Lite)

- **CONFIRMED.** Install the toolchain with `sudo apt install crossbuild-essential-arm64`. The target triplet is `aarch64-linux-gnu-`, for example `ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-`. Build deps include `libc6-dev`. Source: https://www.raspberrypi.com/documentation/computers/linux_kernel.html (Raspberry Pi official docs, cross-compiling section).
- **UNKNOWN.** Sysroot. The Pi docs do not mention a sysroot. Community guides suggest rsync-ing the Pi's `/usr/include` and `/usr/lib` to the host. Found in search results only, not fetched: https://github.com/abhiTronix/raspberry-pi-cross-compilers/wiki/Cross-Compiler-CMake-Usage-Guide-with-rsynced-Raspberry-Pi-64-bit-OS
- **UNKNOWN (not built).** CMake toolchain file skeleton. Proposed, not compiled against a Pi sysroot. Structure follows the generic gcc-aarch64-linux-gnu pattern seen in https://chromium.googlesource.com/external/github.com/DynamoRIO/dynamorio/+/a2f8bcc95def89de92141e627e53c796b1377089/make/toolchain-aarch64.cmake (search result only, not fetched).

```cmake
# cmake/toolchains/pi-zero2w-aarch64.cmake (UNVALIDATED skeleton)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
# Placeholder: point at a sysroot copied from the Pi, if target libraries are linked.
set(CMAKE_FIND_ROOT_PATH /opt/pi-sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

Use it with `cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/pi-zero2w-aarch64.cmake`. No build has been run with it.

### 2. Hardware H.264 decode (bcm2835-codec, V4L2 M2M) on Zero 2 W

- **CONFIRMED (vendor spec).** The Pi Zero 2 W lists "H.264, MPEG-4 decode (1080p30)" and "512MB SDRAM". Source: https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/
- **CONFIRMED (chip identity).** The Zero 2 W uses RP3A0, a system-in-package. Its die is the BCM2710A1 (quad-core Cortex-A53 at 1 GHz). The brief's "BCM2710A1" is the die inside the RP3A0 package. Source: https://www.raspberrypi.com/documentation/computers/processors.html
- **CONFIRMED (driver is not SoC-gated).** The driver `drivers/staging/vc04_services/bcm2835-codec/bcm2835-v4l2-codec.c` (raspberrypi/linux `rpi-6.6.y`) is a `platform_driver` named `"bcm2835-codec"`. It has no `of_match_table` and no SoC restriction. The only BCM2835 reference is a `MODULE_DESCRIPTION`. Source: https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.6.y/drivers/staging/vc04_services/bcm2835-codec/bcm2835-v4l2-codec.c
- **CONFIRMED (no DT node).** The fetched `bcm270x.dtsi` contains no `bcm2835-codec`, `bcm2835-vchiq`, or codec node. Source: https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.6.y/arch/arm/boot/dts/broadcom/bcm270x.dtsi
- **UNKNOWN.** Whether H.264 is in the decoder's compressed-input list on the Zero 2 W. The driver builds its format list at runtime from the VideoCore firmware's `MMAL_PARAMETER_SUPPORTED_ENCODINGS` response. This depends on firmware, which was not checked.
- **UNKNOWN.** Where the platform device is registered. It is not in the driver file. It is probably created by the VCHIQ/firmware side, not checked.
- **BLOCKED.** Whether `/dev/video*` with the `bcm2835-codec` driver exists on a running Zero 2 W, and whether `h264_v4l2m2m` decodes. Needs a booted board. Commands: `v4l2-ctl --list-devices` and `ffmpeg -decoders | grep v4l2m2m`.

Reading: the vendor spec and the SoC-agnostic driver support a Zero 2 W hardware H.264 path, but the device node is not verified. Keep the decode row as an estimate until the BLOCKED check runs.

### 3. CPC200-CCPA current draw

- **CONFIRMED (published spec, not measured).** Power consumption 0.75 W. Rated power input 5V ±0.2 at 1.0 A. Source: Carlinkit product listing, as shown in search results. The page itself returned HTTP 429 and was not fetched: https://carlinkitcarplay.com/products/carlinkit-wirless-adapter-for-android-head-unit-support-carplay-and-android-auto
- **Conflict.** A third-party manual (manuals.plus) lists 0.25 W and 5V 1–2.1A. Source: https://manuals.plus/ae/1005003092956975. The 0.75 W figure is the more consistent one across sources.
- **Derived, not measured.** 0.75 W at 5 V is about 0.15 A average. This is arithmetic on the published figure, not a bench reading.
- **UNKNOWN.** Measured idle or peak current. No published measurement was found.
- **UNKNOWN.** OTG port current budget on the Zero 2 W. Not researched in this pass.
- **BLOCKED.** Measured draw. Needs a USB power meter on the Pi with the dongle attached.

No measured numbers in this section. Sources are secondary where marked above.
