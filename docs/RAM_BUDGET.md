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
