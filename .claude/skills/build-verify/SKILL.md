---
name: build-verify
description: Configure, build with warnings-as-errors and sanitizers, run unit tests, and run the headunit binary against both profiles. Use after any code change, before reporting a task as done.
---

# Build Verify Protocol

Report the real output of each step. Do not state a step passed unless it ran.

The gate compiler is Homebrew LLVM 22. Apple clang 21 aborts under ASan inside gtest startup (a toolchain false positive, decided 2026-11-21). Do not use Apple clang for the gate.

1. Configure with LLVM 22 and strict flags (sanitizers and warnings as errors). Use `build-llvm`, which keeps `build/` (it holds the local FFmpeg build):
   ```
   CXX=/opt/homebrew/opt/llvm/bin/clang++ cmake -S . -B build-llvm -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -Wall -Wextra -Werror"
   ```
2. Build: `cmake --build build-llvm`. Any warning is a failure.
3. Test: `./build-llvm/unit_tests`. Report the count (41 expected at wave 2). ASan and UBSan must print nothing.
4. Run the binary against both shipped profiles:
   - `./build-llvm/headunit config/macos_sim.json`
   - `./build-llvm/headunit config/pi_prod.json`
   Both must print `ENV=...` and exit 0.
5. Check the build tree is clean in git: `git status --short` should list only files you meant to change.

Do not commit. Report the test count and any failing test by name.
