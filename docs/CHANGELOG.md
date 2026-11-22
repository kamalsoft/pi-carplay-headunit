# Changelog

All notable changes to this project are recorded here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `docs/AUDIT.md`: audit report with 14 tracked findings and status.
- `docs/AGENTS.md`: agent task plan split into a dependable (sequential) track and a non-dependable (parallel) track, with wave gates.
- `.claude/agents/`: agent definitions for each role in the plan.
- `docs/README.md`: index of project documentation and the rules for keeping it current.

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
