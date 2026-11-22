# Changelog

All notable changes to this project are recorded here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Interface headers only (D1): `src/hal/{video,audio,usb,touch,power}.h`, `src/plugins/plugin.h` (HeadUnitPlugin lifecycle), `src/core/state.h` (state enum, event enum, transition table). No implementations.
- CMake `hu_interfaces` INTERFACE target (links no platform libraries) and interface tests in `tests/unit/interfaces_test.cpp`.
- CPC200 protocol framing and handshake (D2), ported from node-CarPlay (MIT, © 2022 Rhys Morgan) in `src/core/protocol/cpc200.{h,cpp}`. Source: `src/modules/messages/common.ts` (blob `780e9d9cd157d7658523b90230bfd16b2290026a`), `src/modules/messages/sendable.ts` (blob `4cd8c45470e55d5651bc6eda24551278055b9f27`), `src/modules/DongleDriver.ts` start() init order (blob `005759420eca0b8705e2bb327ecddb81a00cefb0`), default branch `master`. Covers 16-byte LE header with type check `~type`, 8 MiB inbound payload cap (our guess, upstream has none; marked shortcut in code), inbound `read_frame`, and the 11-message init sequence plus optional androidWorkMode. Not ported: inbound message parsing (readable.ts), video, audio, touch, file icons. Timers (1 s wifiConnect, 2 s heartbeat) stay in the core. Builds against `hal::UsbTransport` only; no libusb.
- `tests/unit/protocol_test.cpp`: 20 byte-exact tests (header encode and decode, rejected bad magic, bad type check, short buffer and oversize length, handshake write order against a recording fake transport, inbound framing errors).
- CMake target `hu_protocol` (static) and `unit_tests` now also links it (appended lines only).
- Known issue: `unit_tests` aborts under Apple clang 21 with `-fsanitize=address` (container-overflow inside gtest startup). A control build without any protocol code aborts the same way, and Homebrew LLVM 22 passes all 41 tests with the same sanitizer flags. Toolchain decision pending.
- macOS mock CarPlay simulator (N5) in `src/simulator/simulator.{h,cpp}`, standard library only, implementing only the D1 HAL interfaces: test-pattern NV12 video decoder, raw USB byte feed with attach/yank, touch and scroll and key input, S16 Media and Navigation audio with a 500 ms real-time buffer model and sine tones, power monitor with ignition (reported on change), throttle flags and CPU temperature. Sinks run only in `Simulator::pump()` on the core thread; other threads post. Fault hooks: `yank_usb()`, `low_voltage()` (throttled_flags 0x1), `thermal_warning()` (cpu_temp_c). HUD snapshot (`hud()`): FPS over one second, decode latency, audio buffer ms, underruns, USB state, RSS, and a flag above 280 MB.
- `tests/unit/simulator_test.cpp`: connect, one Detached, and ignition OFF arrive in order; low-voltage and thermal faults; test-pattern frame, touch and scroll sequence, and tone buffer bounds.
- CMake `HU_ENV` (macos_sim or pi_prod; Apple defaults to macos_sim) and `HU_SIMULATOR` option (default ON only for macos_sim). Enabling it under pi_prod is a configure error. `hu_simulator` is linked only into `unit_tests`, never into `headunit`.
- `docs/AUDIT.md`: audit report with 14 tracked findings and status.
- `docs/AGENTS.md`: agent task plan split into a dependable (sequential) track and a non-dependable (parallel) track, with wave gates.
- `.claude/agents/`: agent definitions for each role in the plan.
- `docs/README.md`: index of project documentation and the rules for keeping it current.
- `LICENSE`: Apache License 2.0 (official text).
- `THIRD_PARTY_NOTICES.md`: notices for nlohmann/json (MIT, shipped) and node-CarPlay (MIT, planned), with full MIT texts. Covers libusb and FFmpeg (LGPL-2.1+, dynamic linking) with distribution obligations, and sdl2-compat (Zlib).
- `LICENSES/LGPL-2.1.txt` and `LICENSES/sdl2-compat-Zlib.txt`: full license texts for the LGPL and Zlib components, from upstream.
- `.claude/skills/license-check`: read-only license audit gate (verifies upstream licenses, dynamic linking, forbidden sources).
- `.claude/skills/build-verify`: strict configure, build, test, and profile-run sequence for every code change.
- `build/third_party/ffmpeg`: FFmpeg 9.1 built LGPL-only (`--disable-gpl --disable-nonfree --enable-shared`), recorded in `docs/LICENSING.md`. Not committed (under `build/`).
- `docs/RAM_BUDGET.md`: "Pi Zero 2 W facts (research)" section with cited sources. No measured values.

- macOS HAL (D3) in `src/hal/macos/`, behind the D1 interfaces, no D1 header changed:
  - `libusb_transport.{h,cpp}` (`LibusbTransport`, `hal::UsbTransport`): opens by vendor 1314 and product 1520 or 1521 (passed in by the caller), claims interface 0, uses the first bulk IN and OUT endpoints. Bulk read and write with timeouts. Emits Attached on open, Detached when a transfer reports no device, Error on other transfer failures or an unusable device. Timeouts return the error code without an event. No control transfers.
  - `videotoolbox_decoder.{h,cpp}` (`VideoToolboxDecoder`, `hal::VideoDecoder`): H.264 through the LGPL FFmpeg build, dynamic, with the VideoToolbox hwaccel. No software fallback. Output is NV12, copied out of the VideoToolbox surface. Latency is submit to output for the packet that produced the frame.
  - `coreaudio_output.{h,cpp}` (`CoreAudioOutput`, `hal::AudioOutput`): one AudioQueue per channel (Media, Navigation), S16 only, 20 ms buffers, 500 ms ring. Whole frames only. Gain is the queue volume and survives reopen. Underruns counted in the audio callback.
  - `core_queue.h` (`CoreQueue`): HAL threads post; the core calls `drain()`. All D1 sinks run in `drain()` on the core thread, as decided.
  - `hal_probe.cpp` (`hu_hal_probe`): build-only hardware probe. Opens the dongle, reads once with a timeout, and exercises the decoder and audio open and close. It writes nothing to the dongle. Not linked into `headunit`.
  - CMake: an `if(APPLE)` block with `hu_hal_macos` and `hu_hal_probe`. libusb is found by name (`usb-1.0`) and linked as a dylib. FFmpeg is only searched under `build/third_party/ffmpeg` with `NO_DEFAULT_PATH`, so Homebrew ffmpeg cannot be picked up. CoreAudio is linked as frameworks.
  - Not done: hotplug callbacks (Attached and Detached come from open and transfer results only), and no unit tests added. The gate's unit test count stays 41.
  - Known limits: the live USB path is untested (dongle not visible to the Mac this session). The decoder was tested on a generated 20-frame H.264 stream. Audio was opened and fed silence only; no audible output was checked.
- Integration tests (D4) in `tests/integration/`, target `integration_tests` (CMake, appended; `unit_tests` lines unchanged). 17 tests, all PASS on the LLVM 22 gate:
  - Fake transport only (`fake_usb_transport.h`, no hardware opened): `handshake_test.cpp` (6 tests: 11 init messages in order and byte-for-byte against `build_handshake`, DPI and heartbeat and wifi-connect frames byte-exact, no write when closed) and `reconnect_test.cpp` (4 tests: unplug and replug walk Standby and Connecting through `next_state`, handshake restarts from the first message, unplug while Connecting returns to Standby).
  - Simulator only (`HU_SIMULATOR`): `audio_routing_test.cpp` (3 tests: Media tone and Navigation tone land only in their channel; underrun counters are per channel) and `fault_path_test.cpp` (4 tests: low-voltage and thermal faults reach `onSuspend`, and clears reach `onResume`, via a test-side `PowerDispatch`).
  - Gaps, recorded as A-20 to A-24: no core handshake driver, no cadence timers, no power-to-plugin dispatcher, no HAL-to-state event mapping. Two of the tests supply that logic themselves and say so in their comments.
### Changed
- D1 interfaces, after review decisions:
  - `HeadUnitPlugin` gains `onCarPlayDisconnect`, `onSuspend`, `onResume` (CLAUDE.md §3.2 throttling).
  - `kThrottledUndervoltageMask` is replaced by `kThrottleLiveUndervoltage` (0x1, gates suspension now) and `kThrottleStickyMask` (0x50000, logged).
  - Ignition source decided: GPIO sense line on the car's 12V ACC, not yet wired.
  - Threading decided: sinks (`UsbTransport`, `VideoDecoder`, `InputSource`) run on the core thread. The HAL posts to a core queue.
- Sanitizer gate compiler: Homebrew LLVM 22 (`build-llvm/`). Apple clang 21 aborts under ASan inside gtest startup (toolchain false positive). Gate is 41/41 passing with LLVM 22 under ASan and UBSan. `build-verify` skill updated.

### Documentation
- `docs/CHANGELOG.md` created (this file).

## [0.1.0] - 2026-11-21

Step 1: architecture scaffold and configuration.

### Added
- CMake project (C++17) with a `headunit_core` library, `headunit` executable, and `unit_tests` target.
- Config loader and validator in `src/core/config.{h,cpp}`, rejecting unknown env, Pi RSS target above 260 MB, target above warn, empty USB product list, missing fields, and malformed JSON.
- Profiles `config/macos_sim.json` and `config/pi_prod.json`.
- 8 GoogleTest unit tests in `tests/unit/config_test.cpp`.
- `docs/LICENSING.md`: source and dependency license rules.
- `docs/RAM_BUDGET.md`: estimated RSS breakdown against the 260 MB target.
- `.gitignore` ignoring `build/`.
- Directory skeleton from CLAUDE.md §4.

### Licensing decisions
- Protocol origin: rhysmorgan134/node-CarPlay (MIT). Derived files must keep the MIT notice.
- Excluded: `jwinarske/carlinkit-cxx` protocol code (drops the MIT notice), all GPLv3 CPC200 repos, and unlicensed repos.
- FFmpeg: must be an LGPL build, dynamically linked. Homebrew `ffmpeg` (GPL-3.0) is not to be used.
- libusb: LGPL-2.1, dynamically linked.

### Dependencies installed (macOS, Homebrew)
- `sdl2-compat` (Zlib), `libusb` (LGPL-2.1), `nlohmann-json` (MIT), `googletest` (BSD-3-Clause), `ninja`.

[Unreleased]: ../docs/CHANGELOG.md
