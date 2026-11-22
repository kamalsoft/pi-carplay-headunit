# Audit Report

- **Audit date:** 2026-11-21
- **Scope:** repository state after Step 1 (scaffold + config validation)
- **Method:** file inventory, build with `-Werror` + AddressSanitizer/UBSan, unit tests, licensing review of sources and dependencies, check against CLAUDE.md
- **Result:** build clean, 8/8 unit tests pass. 0 open critical issues. 2 high, 5 medium, 4 low, 2 info.

## Verified as correct

| Check | Evidence |
|---|---|
| Build passes with `-Werror`, ASan, UBSan | `cmake --build build` |
| Config validation rejects bad input | 8/8 tests in `tests/unit/config_test.cpp` |
| Shipped profiles load and validate | `./build/headunit config/macos_sim.json` and `config/pi_prod.json` |
| No third-party protocol code in the repo | file inventory: only project-authored files present |
| Build output ignored by git | `.gitignore` lists `build/` |
| Licensing rules recorded | `docs/LICENSING.md` |

## Findings

Severity: **High** blocks distribution or legal safety. **Medium** blocks a CLAUDE.md milestone. **Low** is hygiene. **Info** is a note.

| ID | Sev | Finding | Evidence | Owner / Step | Status |
|---|---|---|---|---|---|
| A-01 | High | No project `LICENSE` file. Our own code had no license, so it could not be shipped or accepted from others. | `LICENSE` is Apache-2.0 (verified against GitHub's license API text) | User decision | Closed 2026-11-21 |
| A-02 | High | Protocol origin is decided (node-CarPlay, MIT) but the port is not written. Any future copy from `carlinkit-cxx` would drop the MIT notice. | `docs/LICENSING.md` | protocol-engineer, Step 2 | Open (mitigated by policy) |
| A-03 | Medium | Homebrew `ffmpeg` is GPL-3.0. Not linked yet, but Step 2 would link it by default if not checked. | `brew info ffmpeg` | toolchain-builder | Open |
| A-04 | Medium | Git connection. Repo is linked to `origin` (github.com/kamalsoft/pi-carplay-headunit, public), HEAD `a1cd617` matches `origin/main`, working tree clean. | `git status` clean; `git rev-parse HEAD origin/main` equal | User + docs-keeper | Closed 2026-11-21 |
| A-05 | Medium | RAM figures are estimates only. The 260/280 MB limits are not yet measured. | `docs/RAM_BUDGET.md` | sim-engineer, Step 4 | Open |
| A-06 | Medium | Pi Zero 2 W hardware H.264 decode is unverified. The reference project validated only Pi 4 and Pi 5. | `carlinkit-cxx` README | pi-researcher | Open |
| A-07 | Medium | CLAUDE.md §3 safety items (power monitor, OverlayFS, SIGTERM/USB recovery) are not implemented. Expected at this stage, but nothing yet tracks them. | CLAUDE.md §3 | safety-verifier, Step 5 | Open (planned) |
| A-08 | Medium | Apple CarPlay branding and dongle-use legal review not done. | `docs/LICENSING.md` open items | User + counsel | Open |
| A-09 | Low | The 260 MB limit is hardcoded in `src/core/config.cpp` and also in `config/pi_prod.json`. Two sources of truth. | `config.cpp` pi_prod check | core owner | Open |
| A-10 | Low | Sanitizer and `-Werror` flags are passed on the command line, not stored in CMake presets, so they are easy to drop. | build command history | core owner | Open |
| A-11 | Low | Empty directories (`scripts`, `src/hal`, `src/plugins`, `src/simulator`, `src/ui`, `tests/integration`, `tests/safety`) will not survive `git` commits. | `find -type d -empty` | Each owner when adding files | Open |
| A-12 | Low | `main()` exit codes and the usage error path have no tests. | `tests/unit/` | integration-tester | Open |
| A-13 | Info | `HU_CONFIG_DIR` is set from the source tree, so tests break if the tree moves. Acceptable for now. | `CMakeLists.txt` | none | Accepted |
| A-14 | Info | Homebrew installed `sdl2-compat` (Zlib) for SDL2. It is a compatibility layer over SDL3. Check the runtime on Pi before relying on it. | `brew list` | hal-engineer | Open |

## Recommendations (priority order)

1. **Decide the project license (A-01)** before any further code is shared.
2. **Run `git init`** and commit Step 1, so later audits have a baseline (A-04).
3. **Build an LGPL FFmpeg** before Step 2 and never link Homebrew ffmpeg (A-03).
4. **Port the protocol from node-CarPlay only**, keeping its MIT notice, after the license auditor signs off (A-02).
5. **Single-source the RSS ceiling** from one constant (A-09).

## Re-audit

Re-run this audit at the end of each wave in `docs/AGENTS.md`. Update the status column rather than rewriting history, and record the date.
