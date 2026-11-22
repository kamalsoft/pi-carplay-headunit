# Project: Modular Raspberry Pi Zero 2 W CarPlay Head-Unit & macOS Simulator

## 1. Objective & Hardware Context
Build a lightweight, modular, local-first CarPlay head-unit application targeting a **Raspberry Pi Zero 2 W** (quad-core Cortex-A53, 512 MB RAM, Raspberry Pi OS Lite 64-bit) paired with a **Carlinkit CPC200-CCPA** USB dongle, while supporting full **macOS desktop simulation** before any in-vehicle deployment.

- **Core User Needs (Phase 1):**
  1. **Apple Maps / Navigation Projection:** Fullscreen, low-latency CarPlay projection with responsive touch/mouse/rotary-key input forwarding so iPhone Apple Maps renders cleanly.
  2. **Music & Audio Sharing:** Clean PCM/ALSA/PulseAudio routing on Raspberry Pi (and CoreAudio on macOS) supporting CarPlay music playback, turn-by-turn navigation voice ducking/mixing, and clean audio device switching without clipping or buffer underruns.
  3. **Extensible Plugin Architecture:** Modular plugin/service registry so future modules (e.g., OBD-II telemetry, reverse camera trigger, ambient/weather widgets, voice triggers) can be added cleanly without rewriting core projection or audio pipelines.

## 2. Dual-Environment Workflow (macOS Simulation vs. Pi Hardware)
Design the codebase with a strict **Hardware Abstraction Layer (HAL)** and environment profiles (`ENV=macos_sim` vs. `ENV=pi_prod`):

1. **macOS Simulation Mode (`ENV=macos_sim`):**
   - Run the entire head-unit UI and CarPlay receiver natively on macOS (Apple Silicon) for rapid development and 100% end-to-end testing before touching the vehicle.
   - Support **Wireless iPhone connection via the Carlinkit CPC200-CCPA dongle plugged into the Mac** (using `node-usb` / `libusb` or WebUSB/native bridge) AND a **Mock/Loopback CarPlay Stream Simulator** (simulated H.264 test feed, simulated touch/scroll events, simulated audio tones/music routing, and simulated ignition ON/OFF power events) so every state transition can be tested even without the dongle attached.
   - Display a live **Debug & Resource HUD overlay** in simulation mode showing FPS, H.264 decode latency, audio buffer health, USB packet state, and simulated Pi Zero 2 W memory ceiling warnings (flagging if resident memory exceeds 280 MB).

2. **Raspberry Pi Production Mode (`ENV=pi_prod`):**
   - **Wired Car / Bench Connectivity & Display:** Support HDMI/DSI display output in fullscreen kiosk mode (no heavy desktop bloat; use lightweight SDL2/Wayland/DRM or minimal Chromium/Electron only if strictly within the 512 MB RAM budget—prefer lightweight native/C++/Rust/Node+SDL2 or minimal WebGL canvas).
   - **Phone Connectivity:** Support both **wired USB passthrough** and **wireless iPhone pairing** through the Carlinkit CPC200-CCPA dongle.
   - **Memory Discipline:** Target **< 260 MB total RSS** for the app + receiver stack on the Pi Zero 2 W so Linux OOM-killer is never triggered.

## 3. Non-Negotiable Safety & Non-Destructive Requirements (100% Pre-Car Verification)
Before real-time vehicle usage, enforce strict hardware, electrical, and automotive safety boundaries so **zero damage can occur to the car, the iPhone, or factory car settings**:

1. **Zero Vehicle Bus Write / Factory Setting Isolation:**
   - Strictly isolate the Raspberry Pi as a **passive USB/display/audio peripheral** (or standalone head-unit). Do NOT send arbitrary CAN-bus, OBD-II write frames, or undocumented USB HID/vendor commands to any factory vehicle port.
   - Ensure USB gadget/host descriptors only advertise standard, compliant HID/Audio/CDC classes if connected to an existing car USB port, with strict read-only/passive defaults.
2. **Power & Thermal Protection (Pi + Car + Phone):**
   - Include a power-state monitor (`vcgencmd get_throttled` and CPU temp polling on Pi) that gracefully throttles frame rate or suspends non-essential plugins if under-voltage (`0x50000`) or thermal warning (>75°C) occurs.
   - Disable high-current USB downstream fast-charging negotiation from the Pi Zero 2 W's limited power rail so the Pi does not brown out or back-feed unstable voltage into the car's USB port or iPhone.
3. **Read-Only Filesystem & Safe Ignition Cutoffs:**
   - Provide automated setup scripts for **OverlayFS (read-only root partition)** + tmpfs logs on Raspberry Pi OS so sudden 12V ignition shutoffs never corrupt the microSD card or leave broken lockfiles.
   - Implement clean `SIGTERM` / `SIGINT` and USB disconnect recovery handlers so unplugging the iPhone or dongle mid-navigation cleanly resets to the standby screen within 2 seconds without hanging.

## 4. Required Repository Structure & Deliverables
Scaffold the repository cleanly with clear separation of concerns:

```text
pi-carplay-headunit/
├── CLAUDE.md                  # Architecture rules, RAM budgets, and safety invariants
├── config/
│   ├── macos_sim.json         # macOS simulator & CoreAudio/libusb settings
│   └── pi_prod.json           # Pi Zero 2 W ALSA, display, and resource cap settings
├── src/
│   ├── core/                  # CarPlay protocol/dongle lifecycle, state machine, watchdog
│   ├── hal/                   # Platform adapters: video, audio, touch/input, USB, power
│   ├── ui/                    # Minimalist CarPlay viewport + clean standby launcher
│   ├── plugins/               # Plugin interface + sample stub plugin (e.g., clock/status)
│   └── simulator/             # macOS mock stream, latency/RAM profiler, fault injector
├── scripts/
│   ├── setup_macos.sh         # Install macOS dependencies (libusb, SDL2/ffmpeg, etc.)
│   ├── setup_pi.sh            # Configure Pi OS Lite, udev rules, ALSA, systemd service
│   ├── enable_overlayfs.sh    # Harden SD card against sudden ignition power loss
│   └── preflight_audit.sh     # Automated 100% pre-vehicle verification test suite
└── tests/
    ├── unit/                  # State machine, plugin lifecycle, config validation
    ├── integration/           # Dongle handshake, audio routing, reconnect recovery
    └── safety/                # OOM boundary test, power-drop simulation, USB fault tests

## 5. Execution Plan (Step-by-Step)
Work iteratively and verify each milestone before moving to the next:

### Step 1 — Architecture & Tech Stack Selection: Propose the lightest reliable stack that runs identically on macOS and Pi Zero 2 W ARM64 (e.g., TypeScript/Node.js + native SDL2/FFmpeg or Rust/C++ receiver wrapping the Carlinkit CPC200-CCPA protocol such as node-carplay / LIVI protocol layer) and explain its exact RAM footprint on 512 MB hardware.

### Step 2 — Core Receiver, Maps Projection & Audio Sharing: Implement the Carlinkit USB handshake, H.264 video pipeline (hardware-accelerated VideoToolbox on macOS, V4L2/DRM on Pi), touch/keyboard input mapping, and dual-channel audio routing (Media Music stream + Navigation Voice stream).

### Step 3 — Plugin SDK: Define a clean HeadUnitPlugin lifecycle (init, onCarPlayConnect, onAudioFocusChange, onShutdown) with strict memory/CPU isolation so future features plug in safely.

### Step 4 — macOS Simulator & Fault-Injection Harness: Build the macOS simulation runner with a "Pre-Car Certification Checklist" mode that tests: (a) wireless iPhone connect/disconnect loops, (b) Apple Maps turn-by-turn + Apple Music/Spotify simultaneous audio, (c) simulated USB cable yank, (d) simulated low-voltage/thermal warning, and (e) 30-minute continuous soak test tracking RSS memory and dropped frames.

### Step 5 — Pi Deployment & Preflight Audit Script: Generate setup_pi.sh, systemd kiosk service, udev rules for the Carlinkit dongle, and preflight_audit.sh that outputs a PASS/FAIL safety report before connecting to the vehicle.

Start with Step 1: confirm the exact directory layout, dependencies, and memory budget breakdown, then begin scaffolding the core and macOS simulator.

---

### Quick Bench-Testing Checklist Before Taking It to the Car
Once Claude Code generates the project, run your validation in this exact order so nothing can affect your car:
1. **Stage 1 (macOS Pure Simulation):** Run in `ENV=macos_sim` without hardware to verify UI responsiveness, plugin hooks, and fault-recovery logic.
2. **Stage 2 (macOS + Carlinkit Dongle + iPhone):** Plug the Carlinkit CPC200-CCPA into your MacBook, connect your iPhone wirelessly, and test **Apple Maps** routing + **Music/Audio sharing** for 20–30 minutes while watching the HUD's RAM/latency meter.
3. **Stage 3 (Pi Zero 2 W on Wall Power + Monitor):** Deploy to the Pi Zero 2 W on your desk using a clean 5V/3A USB-C power supply and HDMI monitor. Run `preflight_audit.sh` and perform 5–10 abrupt power-unplug cycles after enabling `OverlayFS` to prove the SD card boots cleanly every time.
4. **Stage 4 (In-Car Power-Only Bench Check):** Power the Pi in the car from a dedicated **12V-to-5V 3A USB buck adapter** (completely isolated from the car's factory infotainment data port at first) to confirm clean startup/shutdown with your ignition before connecting any car audio or display lines.