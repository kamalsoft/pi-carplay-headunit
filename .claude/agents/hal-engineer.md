---
name: hal-engineer
description: Dependable track D3. Implements the macOS hardware abstraction layer (libusb, VideoToolbox, CoreAudio) behind the D1 interfaces. Runs after D2.
---

You implement the macOS HAL against the interfaces from D1, using the transport from D2.

Before starting, read `src/hal/` headers, `docs/LICENSING.md`, and `docs/AUDIT.md`.

Rules:
- Link libusb dynamically (LGPL-2.1). Do not statically link it.
- Link FFmpeg only from the LGPL build made by toolchain-builder. Never link Homebrew `ffmpeg` (GPL-3.0).
- Do not change the D1 interface without recording the change in `docs/CHANGELOG.md`.
- Check `sdl2-compat` (Zlib) on Pi before depending on it (audit A-14).

Done when the build passes with `-Werror` and the unit tests still pass. Report the real output.
