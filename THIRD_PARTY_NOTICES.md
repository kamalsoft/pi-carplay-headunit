# Third-Party Notices

This project is licensed under the Apache License 2.0 (see `LICENSE`).
The components below are third-party. Their licenses apply to their own code.
Full license texts for MIT components are reproduced below. LGPL and Zlib texts are in `LICENSES/`.

## Shipped in the binary

### nlohmann/json (header-only, MIT)
Used by `src/core/config.cpp` to parse configuration. Linked statically (header-only).
Upstream: https://github.com/nlohmann/json

```
MIT License 

Copyright (c) 2013-2026 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Planned or linked

### node-CarPlay (MIT, © 2022 Rhys Morgan)
Source of the CPC200 USB handshake and framing. Step 2 ports it to C++ in `src/core/protocol/`.
Every derived file must carry this header:
`// Derived from node-CarPlay, Copyright (c) 2022 Rhys Morgan, MIT License. See THIRD_PARTY_NOTICES.md.`
Upstream: https://github.com/rhysmorgan134/node-CarPlay

```
MIT License

Copyright (c) 2022 Rhys Morgan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### libusb (LGPL-2.1-or-later)
USB access on macOS and Pi. Linked dynamically only. Full text: `LICENSES/LGPL-2.1.txt`.
Upstream: https://github.com/libusb/libusb

### FFmpeg (LGPL-2.1-or-later build)
H.264 decode. Built from source with `--disable-gpl --disable-nonfree --enable-shared`, linked dynamically. Homebrew `ffmpeg` (GPL-3.0) is never used. Full text: `LICENSES/LGPL-2.1.txt`. Upstream: https://ffmpeg.org/legal.html

### sdl2-compat (Zlib)
Display layer (Homebrew build). Full text: `LICENSES/sdl2-compat-Zlib.txt`. Upstream: https://github.com/libsdl-org/sdl2-compat

## LGPL obligations (libusb, FFmpeg)
When distributing a binary that contains these libraries:
1. Ship `LICENSES/LGPL-2.1.txt` with the binary.
2. Keep them as separate shared libraries (dynamic linking). Do not link them statically.
3. Offer the source of the exact versions used (FFmpeg 9.1 tarball, the libusb release), and let users replace them. Dynamic linking satisfies the relinking requirement.
4. Keep the FFmpeg configure line in `docs/LICENSING.md` with each build.

## Build and test only (not distributed)

### GoogleTest (BSD-3-Clause)
Unit test framework. Not included in the shipped binary.
