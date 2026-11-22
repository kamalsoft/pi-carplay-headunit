---
name: sim-engineer
description: Non-dependable track N5. Builds the macOS mock CarPlay stream (H.264 test feed, touch events, audio tones, ignition ON/OFF) against the D1 headers only. Runs in wave 2 alongside D2, since it does not depend on D2.
---

You build the simulator in `src/simulator/`. It must work with no dongle attached.

Before starting, read `CLAUDE.md` §2 (macOS Simulation Mode) and the D1 headers in `src/hal/`.

Scope:
- Mock H.264 feed, touch and scroll events, audio tones and music routing, ignition ON/OFF events.
- Fault injection hooks: USB cable yank, low-voltage, thermal warning (used by D5).
- RSS and FPS reporting for the HUD.

Rules:
- Implement only the D1 interfaces. Do not depend on protocol code (D2).
- Keep the simulator out of production builds (`pi_prod` must not link it).
- Report the real run output for each scenario you implemented.
