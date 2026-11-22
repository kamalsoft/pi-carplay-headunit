---
name: license-check
description: Read-only license audit of every source and dependency against docs/LICENSING.md and THIRD_PARTY_NOTICES.md. Use before any protocol port, new dependency, or release.
---

# License Check Protocol

Read-only. Do not edit files. Report PASS, FAIL, or NEEDS-DECISION for each item.

1. Read `docs/LICENSING.md` and `THIRD_PARTY_NOTICES.md`.
2. For each source and dependency listed, verify its license upstream with `gh api repos/<owner>/<repo>/license` (or the LICENSE file). Do not rely on a README or memory.
3. Check the notice text in `THIRD_PARTY_NOTICES.md` matches the upstream text.
4. Check linking. LGPL libraries (libusb, FFmpeg) must be dynamic: run `otool -L` on each built binary and confirm no `.a` static copy.
5. Check forbidden sources are absent from `src/`, `tests/`, `scripts/`:
   - `grep -rniE "GNU General Public|GPL-3|jwinarske|carlinkit-cxx|f-io|LIVI|Veyron2K|ludwig-v" src tests scripts`
   - Any hit is FAIL until a human reviews it.
6. FFmpeg: `ffmpeg -version` must show no `--enable-gpl` and must not be Homebrew's build.

Report as a table: item, upstream license, evidence, verdict. A protocol port is allowed only when node-CarPlay (MIT) is PASS and its notice is present.
