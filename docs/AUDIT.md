# Audit Report

- **Audit date:** 2026-11-21 (Wave 1 end)
- **Scope:** repository state at end of Wave 1 (`wave-1-foundations`): scaffold, interface headers, state table, project FFmpeg build. Notices are on `license-apache-2-notices` (PR #1), not yet on this branch.
- **Method:** file inventory, build with `-Werror` + AddressSanitizer/UBSan, unit tests, licensing review of sources and dependencies, check against CLAUDE.md
- **Result:** build clean, 18/18 unit tests pass. 0 open critical issues. 19 findings: 2 high, 8 medium, 6 low, 3 info.

## Verified as correct

Re-run 2026-11-21 on `wave-1-foundations`. Output shown is from the commands.

| Check | Evidence |
|---|---|
| Build passes with `-Werror`, ASan, UBSan | `cmake --build build` → `ninja: no work to do.`, exit 0 (incremental; nothing to rebuild). Flags in `build/CMakeCache.txt`: `-fsanitize=address,undefined -Wall -Wextra -Werror` |
| Unit tests pass | `./build/unit_tests` → `[  PASSED  ] 18 tests.` (suites: Config 8, Interfaces, StateMachine, PowerInterface; `tests/unit/config_test.cpp`, `tests/unit/interfaces_test.cpp`) |
| Config validation rejects bad input | `./build/unit_tests --gtest_filter='Config*'` → `[  PASSED  ] 8 tests.` |
| Shipped profiles load and validate | `./build/headunit config/macos_sim.json` and `config/pi_prod.json` both exit 0 and print `ENV=...` |
| No third-party protocol code in the repo | `git ls-files` filtered for `carplay`, `cxx`, `node_modules`: no matches |
| Build output ignored by git | `git check-ignore -v build` → `.gitignore:1:build/` |
| Licensing rules recorded | `docs/LICENSING.md` (FFmpeg build record added, uncommitted) |
| Project FFmpeg is LGPL, no GPL flag | `./build/third_party/ffmpeg/bin/ffmpeg -hide_banner -version \| grep -c -- --enable-gpl` → `0`. `ffmpeg -L` reports LGPL 2.1 or later. Version 9.1 |

## Findings

Severity: **High** blocks distribution or legal safety. **Medium** blocks a CLAUDE.md milestone. **Low** is hygiene. **Info** is a note.

| ID | Sev | Finding | Evidence | Owner / Step | Status |
|---|---|---|---|---|---|
| A-01 | High | No project `LICENSE` file. Our own code had no license, so it could not be shipped or accepted from others. | `LICENSE` is Apache-2.0 (verified against GitHub's license API text) | User decision | Closed 2026-11-21 |
| A-02 | High | Protocol origin is decided (node-CarPlay, MIT). Any future copy from `carlinkit-cxx` would drop the MIT notice. | `docs/LICENSING.md`; license-auditor gate PASS on node-CarPlay MIT (2026-11-21); `THIRD_PARTY_NOTICES.md` tracked on this branch; protocol port in `src/core/protocol/` carries the MIT header | protocol-engineer, Step 2 | Port written (handshake and framing only); D2 done pending wave-2 commit |
| A-03 | Medium | Homebrew `ffmpeg` is GPL-3.0. Step 2 would link it by default if not checked. | `brew info ffmpeg`: GPL-3.0-or-later. Project LGPL FFmpeg 9.1 at `build/third_party/ffmpeg`; `grep -c -- --enable-gpl` → 0. Homebrew ffmpeg 9.0.2_1 still installed, not linked | toolchain-builder | Closed (build). Homebrew ffmpeg must not be linked |
| A-04 | Medium | Git connection. Repo is linked to `origin` (github.com/kamalsoft/pi-carplay-headunit, public); `main` is at `a1cd617`. Feature branches are pushed with PRs (#1). | `git remote -v`; `git log --oneline -1 main` | User + docs-keeper | Closed 2026-11-21 |
| A-05 | Medium | RAM figures are estimates only. The 260/280 MB limits are not yet measured. | `docs/RAM_BUDGET.md` (Pi research added; figures still estimates) | sim-engineer, Step 4 | Open (estimates; no measurement) |
| A-06 | Medium | Pi Zero 2 W hardware H.264 decode is unverified. The reference project validated only Pi 4 and Pi 5. | `carlinkit-cxx` README; `docs/RAM_BUDGET.md` §2 | pi-researcher | Open (decode device node BLOCKED, needs board) |
| A-07 | Medium | CLAUDE.md §3 safety items (power monitor, OverlayFS, SIGTERM/USB recovery) are not implemented. Expected at this stage, but nothing yet tracks them. | CLAUDE.md §3 | safety-verifier, Step 5 | Open (planned) |
| A-08 | Medium | Apple CarPlay branding and dongle-use legal review not done. | `docs/LICENSING.md` open items | User + counsel | Open |
| A-09 | Low | The 260 MB limit is hardcoded in `src/core/config.cpp` and also in `config/pi_prod.json`. Two sources of truth. | `src/core/config.cpp:36` (`<= 260`); `config/pi_prod.json:10` (`"rss_target_mb": 260`) | core owner | Open (still in two places) |
| A-10 | Low | Sanitizer and `-Werror` flags are passed on the command line, not stored in CMake presets, so they are easy to drop. | build command history | core owner | Open |
| A-11 | Low | Empty directories (`scripts`, `src/hal`, `src/plugins`, `src/simulator`, `src/ui`, `tests/integration`, `tests/safety`) will not survive `git` commits. | `find -type d -empty` | Each owner when adding files | Open |
| A-12 | Low | `main()` exit codes and the usage error path have no tests. | `tests/unit/` | integration-tester | Open |
| A-13 | Info | `HU_CONFIG_DIR` is set from the source tree, so tests break if the tree moves. Acceptable for now. | `CMakeLists.txt` | none | Accepted |
| A-14 | Info | Homebrew installed `sdl2-compat` (Zlib) for SDL2. It is a compatibility layer over SDL3. Check the runtime on Pi before relying on it. | `brew list` | hal-engineer | Open |
| A-15 | Info | Process note: the four named agent types did not load in the session that created them. They need a session restart before use. | Reported at end of Wave 1; not re-checked in this pass | User (restart session) | Open |
| A-16 | Low | `.claude/skills/trust-layer-validator/` covers decentralized identity, credential schemas, and agent-to-agent payloads. This project has none of those. | `head .claude/skills/trust-layer-validator/SKILL.md` | User decision (keep or remove) | Open |
| A-17 | Medium | `wave-1-foundations` is not based on `license-apache-2-notices`, so `THIRD_PARTY_NOTICES.md` is absent on this branch. | `ls THIRD_PARTY_NOTICES.md` → no such file; `git show license-apache-2-notices:THIRD_PARTY_NOTICES.md` has content | Integrator, before any distribution | Open (until branches are combined) |
| A-18 | Medium | CPC200-CCPA current draw is UNKNOWN. Sources conflict: 0.75 W (Carlinkit listing) vs 0.25 W (manuals.plus). Needs bench measurement. Links to A-05. | `docs/RAM_BUDGET.md` §3 | User (USB power meter on Pi) | Open (BLOCKED on measurement) |
| A-19 | Low | The FFmpeg build auto-detected Homebrew libxcb and libX11. The configure line has no `--disable-xlib --disable-libxcb`, and `otool -L` shows both linked. Add those flags for a self-contained LGPL build. | `ffmpeg -version` configuration line; `otool -L build/third_party/ffmpeg/bin/ffmpeg` | toolchain-builder | Open |

## Recommendations (priority order)

1. **Decide the project license (A-01)** before any further code is shared.
2. **Run `git init`** and commit Step 1, so later audits have a baseline (A-04).
3. **Done: LGPL FFmpeg 9.1 built (A-03).** Never link Homebrew ffmpeg. Add `--disable-xlib --disable-libxcb` for a self-contained build (A-19).
4. **Port the protocol from node-CarPlay only**, keeping its MIT notice, after the license auditor signs off (A-02). Merge the notices branch first (A-17).
5. **Single-source the RSS ceiling** from one constant (A-09).

## Re-audit

Re-run this audit at the end of each wave in `docs/AGENTS.md`. Update the status column rather than rewriting history, and record the date.

- Last re-audit: 2026-11-21 (Wave 1 end).
