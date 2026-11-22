# Agent Task Plan

The work is split into two tracks.

- **Dependable track (D):** a strict chain. Each task consumes the previous task's output and cannot start until that output exists and passes its gate.
- **Non-dependable track (N):** independent tasks with no mutual dependency. They run in parallel with each other and with the D track, and only the tasks they feed wait on them.

Agent definitions live in `.claude/agents/`. The coordinator is the main session. It starts each wave, merges results, runs `ctest`, and updates the docs.

## Dependency graph

```
D1 spec-architect ──► D2 protocol-engineer ──► D3 hal-engineer ──► D4 integration-tester ──► D5 safety-verifier
        │                     ▲                                                                     ▲
        │              N1 license-auditor (gate: PASS)                                    N4 pi-researcher
        └──► N5 sim-engineer                                                          (device facts)
N2 docs-keeper   (after every wave)
N3 toolchain-builder (LGPL FFmpeg; independent)
```

## Dependable track (sequential)

| ID | Agent | Task | Waits for | Gate to pass |
|---|---|---|---|---|
| D1 | spec-architect | Write interface headers for HAL (video, audio, USB, power), plugin lifecycle, and state machine. No implementation. | none | Headers compile; interfaces match CLAUDE.md §5 Step 3 |
| D2 | protocol-engineer | Port the CPC200 handshake and framing from node-CarPlay (MIT) into `src/core/protocol/`, keeping the MIT notice. | D1, N1 = PASS | Unit tests for framing against captured byte fixtures |
| D3 | hal-engineer | Implement the macOS HAL behind the D1 interfaces: libusb, VideoToolbox, CoreAudio. | D2 | Builds on macOS; dongle opens on Stage 2 |
| D4 | integration-tester | Handshake, reconnect, and audio routing tests in `tests/integration/`. | D3 | All integration tests pass |
| D5 | safety-verifier | Safety tests in `tests/safety/` and `scripts/preflight_audit.sh` with PASS/FAIL output. | D4, N4 | Preflight runs and reports correctly |

## Non-dependable track (parallel)

| ID | Agent | Task | Runs in | Feeds |
|---|---|---|---|---|
| N1 | license-auditor | Check every source and dependency against `docs/LICENSING.md`. Read-only. | Wave 1 | D2 (gate) |
| N2 | docs-keeper | Update `docs/AUDIT.md`, `docs/CHANGELOG.md`, `docs/RAM_BUDGET.md` after each wave. | Every wave end | Traceability |
| N3 | toolchain-builder | Build LGPL FFmpeg (`--disable-gpl --disable-nonfree`, shared). Verify libusb and SDL2 link dynamically. | Wave 1 | D3 |
| N4 | pi-researcher | Pi Zero 2 W facts: cross-compile toolchain, V4L2 H.264 decode check, dongle current draw. Measurements go to the user, not guessed. | Wave 1, then bench | D5 |
| N5 | sim-engineer | macOS mock H.264 feed, touch events, audio tones, ignition events. Built against D1 headers only. | Wave 2 (with D2) | D4 |

## Waves

| Wave | Runs in parallel | Gate before next wave |
|---|---|---|
| **1** | D1 spec-architect, N1 license-auditor, N2 docs-keeper, N3 toolchain-builder, N4 pi-researcher | D1 headers compile. N1 result recorded. Docs updated. |
| **2** | D2 protocol-engineer (only if N1 = PASS), N5 sim-engineer | D2 framing tests pass. N5 runs against D1 headers. |
| **3** | D3 hal-engineer | Builds on macOS. Re-audit. |
| **4** | D4 integration-tester, N4 pi-researcher (bench run) | Integration tests pass. |
| **5** | D5 safety-verifier | Preflight report produced. Re-audit. Stage sign-off. |

Wave 1 has no dependency between its members, so all five start at once. Wave 2 is the first place a D task and an N task run together.

## Rules for every agent

1. Read `CLAUDE.md`, `docs/LICENSING.md`, and `docs/AUDIT.md` before starting.
2. Do not copy code from GPL, unlicensed, or Apache-without-notice sources. Only node-CarPlay (MIT) is an allowed protocol origin.
3. Work in the existing layout (CLAUDE.md §4). Do not add dependencies without updating `docs/LICENSING.md`.
4. Report results with the real command output. Do not state a test passed unless it ran.
5. Do not commit. The user decides when to commit.
6. At the end of a task, record what changed in `docs/CHANGELOG.md` under `[Unreleased]`.

## Stop conditions

- A gate fails: stop the dependent track, let the other tracks finish, and report the failure.
- A license check returns anything other than PASS: stop D2 and every task that depends on it.
- Hardware is needed and absent: mark the task blocked and report it. Do not simulate hardware results.
