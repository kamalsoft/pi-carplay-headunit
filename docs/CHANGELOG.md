# Changelog

All notable changes to this project are recorded here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Interface headers only (D1): `src/hal/{video,audio,usb,touch,power}.h`, `src/plugins/plugin.h` (HeadUnitPlugin lifecycle), `src/core/state.h` (state enum, event enum, transition table). No implementations.
- CMake `hu_interfaces` INTERFACE target (links no platform libraries) and interface tests in `tests/unit/interfaces_test.cpp`.
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

### Changed
- D1 interfaces, after review decisions:
  - `HeadUnitPlugin` gains `onCarPlayDisconnect`, `onSuspend`, `onResume` (CLAUDE.md §3.2 throttling).
  - `kThrottledUndervoltageMask` is replaced by `kThrottleLiveUndervoltage` (0x1, gates suspension now) and `kThrottleStickyMask` (0x50000, logged).
  - Ignition source decided: GPIO sense line on the car's 12V ACC, not yet wired.
  - Threading decided: sinks (`UsbTransport`, `VideoDecoder`, `InputSource`) run on the core thread. The HAL posts to a core queue.

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
