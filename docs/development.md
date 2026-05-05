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

## Static analysis

In CI on a separate, non-blocking job:

- **`clang-tidy`** with a curated `.clang-tidy` (start from `cppcoreguidelines-*`, `bugprone-*`, `performance-*`, `readability-*`, then disable noisy ones one by one).
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
