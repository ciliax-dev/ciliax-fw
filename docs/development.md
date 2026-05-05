# Development workflow

How to write, test, and verify code on this project. [CLAUDE.md](../CLAUDE.md) has the policy in condensed form; this document is the longer version with reasoning.

## TDD loop

The build-flash-debug cycle is what kills TDD on most embedded projects. The architecture in [architecture.md](architecture.md) exists specifically to give you a tight TDD loop on application code without touching the chip.

**Inner loop (seconds):** `domain/` code → host unit tests → `g++` + GoogleTest. Edit-compile-run in under a second per cycle.

**Middle loop (tens of seconds):** ztest on `native_sim` for things that touch Zephyr abstractions. Twister runs them in parallel.

**Outer loop (minutes):** flash to a real DK, smoke-test on hardware. Once per feature, not once per save.

### Workflow for a new feature

Example: "device should publish a sensor reading every 5 seconds when above threshold."

1. Write the use-case test against the domain layer with mocks for `ISensor`, `IPublisher`, `IClock`. Red.
2. Implement just enough domain logic to pass. Green. Refactor.
3. Write an integration test on `native_sim` that wires real-ish adapters together (still no flash). Red.
4. Implement the Zephyr adapter. Green.
5. (Optionally) flash to a DK, smoke-test once.

Steps 1–4 happen entirely on the host, in seconds-per-iteration. Step 5 is rare.

### Two-pass discipline

For any new behavior:

1. **Write the failing test first.** Build it. Confirm it fails for the *right reason* (compile error or assertion mismatch — not a typo). Report the failure.
2. **Implement the minimum** to make it pass. Refactor if needed. Confirm green.

Don't write tests and implementation in the same edit. The failing-test step is not optional — it's how we know the test actually tests something.

## Test pyramid

```
            ┌────────────────────┐
            │  HIL on real DK    │  ← few, slow, expensive
            ├────────────────────┤
            │  bsim integration  │  ← BLE + multi-node sims
            ├────────────────────┤
            │  Zephyr ztest on   │  ← integration on simulator
            │  native_sim        │
            ├────────────────────┤
            │  Host unit tests   │  ← many, fast, no Zephyr
            │  (plain g++ or     │
            │   BOARD=unit_      │
            │   testing)         │
            └────────────────────┘
```

### Layer 1 — host unit tests (no Zephyr)

`domain/` code has no Zephyr dependencies. Build it with vanilla GCC/Clang on the host, hook up **GoogleTest**, run in milliseconds. This is the TDD inner loop. Lives under `tests/host/`. *No microcontroller required.*

### Layer 2 — Zephyr ztest with `BOARD=unit_testing`

For modules that touch a tiny bit of Zephyr (e.g., a thin wrapper). `unit_testing` is a special board that loads only minimal Zephyr scaffolding and lets you stub the rest.

```cmake
# tests/integration/my_module/CMakeLists.txt
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_module_test)
target_sources(testbinary PRIVATE src/my_module.c src/test_my_module.c)
```

```yaml
# testcase.yaml
tests:
  my_module.basic:
    type: unit
    tags: my_module
```

### Layer 3 — `native_sim` for "almost real" integration

`native_sim` builds the Zephyr app as a **native Linux binary**. The kernel runs, threads are real Zephyr threads, but it's all on the host. So:

- Tests run in seconds, not on hardware
- You can attach gdb, valgrind, sanitizers
- Coverage data via gcov/lcov is straightforward
- It's what `twister` (the Zephyr test runner) uses by default

(`native_posix` is deprecated — use `native_sim`.)

### Layer 4 — bsim for BLE/radio integration

**BabbleSim** (bsim) simulates the Bluetooth radio environment. You can run two or more nRF devices as native processes that talk to each other through a simulated RF medium, with the actual Nordic controller code running. This is how Nordic and the Zephyr project test their own BLE stack. If you're shipping anything with BLE, set this up early.

### Layer 5 — HIL (hardware-in-the-loop)

A self-hosted CI runner with a DK plugged in over USB. Twister has `--device-testing` mode that flashes the binary, runs the test, parses serial output, reports pass/fail. Common setup: Raspberry Pi as the runner; nRF DK on USB; GitHub Actions self-hosted runner. Slower and more fragile than the layers above — don't use it for things the lower layers can catch.

## Mocking

- **C interfaces:** **FFF (Fake Function Framework)** is integrated into Zephyr (`FAKE_VOID_FUNC`, `FAKE_VALUE_FUNC`). Use it for stubbing C functions you can't substitute via interface.
- **C++ interfaces:** write fakes by hand that implement your `IFoo` interface. More readable than GoogleMock's expectation DSL and easier to debug. Reach for GoogleMock only when you need behavior verification, not just state.
- The old ztest mocking framework (`ztest_expect_value` etc.) is **deprecated** — don't use it.

## Sanitizers

You **cannot** run ASan on a Cortex-M directly (no shadow-memory infra, no clang runtime). But on `native_sim` you can, because it's a real Linux binary. This is one of the biggest wins of building host-runnable tests.

In a test build's `prj.conf` (or via west `-DCONFIG_*=y`):

```
CONFIG_ASAN=y          # AddressSanitizer
CONFIG_UBSAN=y         # UndefinedBehaviorSanitizer
```

Run unit and integration test suites with these on in CI. ASan catches use-after-free, buffer overflows, leaks; UBSan catches signed overflow, misaligned access, shifts past type width, null deref. Both have minimal false-positive rates. UBSan is cheap enough to leave on in dev builds; ASan is heavier (~2× slowdown, 3× memory).

You'll need `libasan` installed on the host (`apt install libasan8` on Debian/Ubuntu).

## Coverage

`tests/host/CMakeLists.txt` exposes an `ENABLE_COVERAGE` option that adds `--coverage -O0 -g` to the `runtests` target. It uses gcc's gcov instrumentation, so build with `g++` (clang's `-fprofile-instr-generate` would need different reporter wiring). Coverage and sanitizers can't share a build dir — the CMake configure rejects the combination.

Local loop:

```bash
cmake -S tests/host -B tests/host/build-cov -DENABLE_COVERAGE=ON
cmake --build tests/host/build-cov -j
ctest --test-dir tests/host/build-cov --output-on-failure
gcovr --root . --filter 'app/src/domain/.*' --print-summary tests/host/build-cov/
```

CI runs the same sequence in the `coverage` job and fails if line coverage on `app/src/domain/` drops below 95 %. The job is `continue-on-error: true` until the EventDetector property test (Phase 1, Step 11) lands enough domain code to clear the threshold; the requirement is then promoted to blocking.

`gcovr` ≥ 7.0 is needed for the `--fail-under-line` flag; install via `pip install gcovr` or `apt install gcovr` on recent Debian/Ubuntu.

## Formatting

The canonical style is in [`.clang-format`](../.clang-format) at the repo root: LLVM-derived, 4-space indent, 100-column limit, pointer/reference left-aligned, spaces (no tabs). CI's `format` job checks the tree on every push.

```bash
# Check the whole tree (no changes):
clang-format --dry-run --Werror $(find app/src -name '*.cpp' -o -name '*.hpp')

# Reformat in place:
clang-format -i $(find app/src -name '*.cpp' -o -name '*.hpp')
```

`tests/host/` is included by the same find when you also lint test sources.

## Static analysis

In CI on a separate, non-blocking job:

- **`clang-tidy`** with the curated [`.clang-tidy`](../.clang-tidy) at the repo root (`bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*` minus a small set of disables, each with a one-line rationale in the file). Scope is limited via `HeaderFilterRegex` to `app/src/(domain|ports|adapters/mock)/` — adapters/zephyr/ and main.cpp wrap Zephyr APIs and are exempt. Run on a single file with: `clang-tidy app/src/domain/foo.cpp -- -std=c++20 -I app/src`. Once `tests/host/` lands, point at its `compile_commands.json` instead: `clang-tidy -p tests/host/build app/src/domain/foo.cpp`.
- **`cppcheck`** as a second opinion.
- **clang static analyzer** (`scan-build`) — finds different issues than clang-tidy.
- **Compiler warnings as errors:** `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Werror`. Catches more than most static analyzers and costs nothing.

## Verification before declaring "done"

For any task touching code, run *all* of these and confirm green:

```bash
# 1. Architecture invariant
grep -r --include='*.cpp' --include='*.hpp' --include='*.h' \
    '#include <zephyr/' app/src/domain app/src/ports app/src/adapters/mock
# (should return nothing)

# 2. Host unit tests, with sanitizers
cmake -S tests/host -B tests/host/build-asan -DENABLE_SANITIZERS=ON
cmake --build tests/host/build-asan -j
ctest --test-dir tests/host/build-asan --output-on-failure

# 3. Firmware build (matches CI). Either:
west build -b nrf5340dk/nrf5340/cpuapp app --pristine=auto   # local toolchain
./scripts/build.sh                                           # digest-pinned Docker, reproducible

# 4. Integration tests on native_sim
west twister -T tests/integration -p native_sim --inline-logs

# 5. Format check
clang-format --dry-run --Werror $(find app/src -name '*.cpp' -o -name '*.hpp')
```

If any fail, root-cause it. Don't suppress warnings, don't disable failing tests, don't lower compiler strictness to make CI green.

## CI

[`.github/workflows/ci.yml`](../.github/workflows/ci.yml) runs five jobs on every push and PR (`format`, `host-tests`, `firmware-build`, `twister`, `clang-tidy`). The first four are required; `clang-tidy` is `continue-on-error: true` while the codebase is small. `firmware-build` uploads `zephyr.hex` / `merged.hex` as run artifacts.

A concurrency group cancels older runs on the same ref, so successive pushes during a rebase don't pile up wait time on the slower Zephyr jobs.
