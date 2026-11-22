---
name: build-verify
description: Configure, build with warnings-as-errors and sanitizers, run unit tests, and run the headunit binary against both profiles. Use after any code change, before reporting a task as done.
---

# Build Verify Protocol

Report the real output of each step. Do not state a step passed unless it ran.

1. Configure with strict flags (sanitizers and warnings as errors):
   ```
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -Wall -Wextra -Werror"
   ```
2. Build: `cmake --build build`. Any warning is a failure.
3. Test: `ctest --test-dir build --output-on-failure`, and `./build/unit_tests` for the per-test count.
4. Run the binary against both shipped profiles:
   - `./build/headunit config/macos_sim.json`
   - `./build/headunit config/pi_prod.json`
   Both must print `ENV=...` and exit 0.
5. Check the build tree is clean in git: `git status --short` should list only files you meant to change.

Do not commit. Report the test count and any failing test by name.
