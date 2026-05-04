# Modern C++ on nRF Connect SDK — A Project Setup Guide

A research-backed walkthrough of how to architect, build, test, and ship a new embedded project on Nordic's nRF Connect SDK (NCS) using C++. Current as of May 2026 (NCS v3.2.x is the current stable release; v3.3.0 is in development).

---

## 1. The platform you're standing on

**nRF Connect SDK** is Nordic's umbrella SDK. It bundles **Zephyr RTOS** as its core, plus Nordic-specific drivers, wireless stacks (BLE, Thread, Matter, Wi-Fi, LTE-M/NB-IoT), and the MCUboot bootloader. When you pick NCS, you pick Zephyr — the C++ story, build system, test framework, and HAL conventions are all Zephyr's.

Two important options Nordic has added recently:
- **NCS Bare Metal** (in `sdk-nrf-bm`) for simple BLE apps on the nRF54L series that don't want an RTOS. Smaller, simpler, but you lose Zephyr's testing/HAL ecosystem.
- The **standard Zephyr-based** path. This is what you almost certainly want for a non-trivial project, and it's what the rest of this document assumes.

Build system stack: **CMake + Kconfig + Devicetree + west** (Nordic/Zephyr's meta-tool). You don't get to swap any of those out — they're how Zephyr works. You *can* still apply modern CMake practices on top of it.

---

## 2. C++ support — what's real, what's not

### What works
- Zephyr supports up to **C++20** as of mid-2025. Default standard in Zephyr is still C++11; flip it with `CONFIG_STD_CPP20=y` in `prj.conf`.
- **libstdc++** (full STL) is available via `CONFIG_GLIBCXX_LIBCPP=y`. There's also a minimal C++ runtime for tighter builds.
- The Zephyr SDK toolchain (GCC) is the supported/tested compiler. Clang works in places but isn't the primary path.
- `std::array`, `std::optional`, `std::variant`, `std::span`, `constexpr`/`consteval`, `std::chrono`, lambdas, templates — all usable.

### What doesn't, or hurts
- **Exceptions are disabled by default** (`-fno-exceptions`). Don't fight this — design around `std::expected` (or `tl::expected` for pre-C++23) and error-code returns. Exceptions on a Cortex-M cost you flash, RAM, and unwinding overhead you don't need.
- **RTTI is typically off** too. Avoid `dynamic_cast` and `typeid`.
- **C++23 isn't supported** by the build system yet (people have tried forcing `-std=c++23` and run into issues). C++20 is the realistic ceiling.
- **`<bluetooth.h>` and some other Zephyr headers use C99 designated initializers** that don't compile under C++. The standard workaround: keep BLE setup in a `.c` file with a thin C-callable wrapper, and call it from your C++ code. This has been a known issue for years and isn't going away.
- **No `std::thread`/`std::mutex`** out of the box — Zephyr provides its own primitives (`k_thread`, `k_mutex`, `k_sem`). Don't try to bridge them; use Zephyr's directly or wrap them in your own thin RAII types.
- File extension matters: Zephyr picks the C++ compiler based on suffix (`.cpp`, `.cxx`).

### The pragmatic stance
Use C++ for your **application logic, abstractions, and domain code**. Leave drivers, Zephyr APIs, and wireless stack interaction in C (or in thin C-wrapper layers). This is also the boundary that makes mocking practical.

---

## 3. A defensible "modern C++ in embedded" subset

Everyone arguing about C++ in embedded is really arguing about *which* C++. Pick a subset and write it down. A reasonable starting subset:

**Use freely**
- `constexpr` / `consteval` for compile-time tables, prescalers, CRC tables, sine tables, MAC parsing — anything you'd otherwise express as macros or magic numbers
- `static_assert` for invariants the compiler can check
- `std::array<T, N>` instead of C arrays
- `std::span<T>` for non-owning views over buffers
- `std::optional<T>` instead of sentinel values like `-1` or `nullptr`
- `std::expected<T, E>` (or `tl::expected`) instead of out-params with error codes
- Strong types / user-defined literals (`auto baud = 115200_baud;`) — prevents the entire family of "I passed milliseconds where it wanted microseconds" bugs
- `enum class`, structured bindings, `auto`, range-`for`, lambdas (non-capturing compile to function pointers)
- RAII for hardware resources (lock guards, scoped IRQ disablers, peripheral handles)
- Templates for compile-time polymorphism (CRTP, policy classes)
- `[[nodiscard]]`, `[[maybe_unused]]`, `[[likely]]`/`[[unlikely]]`

**Use carefully**
- Virtual functions — fine at module boundaries, avoid in ISRs and tight loops (vtable lookup, no inlining)
- STL containers requiring allocation (`std::vector`, `std::string`, `std::map`) — only if you've explicitly accepted heap usage and a custom allocator. Prefer `etl::*` (Embedded Template Library) or your own fixed-capacity containers
- `std::function` — type erasure costs flash; use templates or function pointers in hot paths

**Avoid**
- Exceptions (already disabled — keep it that way)
- RTTI / `dynamic_cast`
- Global objects with non-trivial constructors that depend on each other (static-initialization-order fiasco)
- Heap allocation in steady state (start-up only, if at all)

The **zero-overhead principle** is your reasoning tool: every feature should compile to code you couldn't reasonably hand-write better. If a feature breaks that, it's a code smell.

---

## 4. Project structure and build system

### Use the workspace application pattern
Nordic publishes [`ncs-example-application`](https://github.com/nrfconnect/ncs-example-application) — clone this as your starting template. It demonstrates the **west workspace application** layout, where your repo *is* the manifest repository. NCS gets pulled in as a dependency, not the other way around.

A clean layout:

```
my-project/                  ← your repo, also the west manifest repo
├── west.yml                 ← pins NCS version, lists dependencies
├── app/
│   ├── CMakeLists.txt
│   ├── prj.conf             ← Kconfig defaults
│   ├── debug.conf           ← extra config for debug builds
│   ├── release.conf
│   ├── boards/              ← per-board overlays
│   │   └── nrf52840dk_nrf52840.overlay
│   └── src/
│       ├── main.cpp
│       ├── domain/          ← pure logic, no Zephyr deps
│       ├── ports/           ← abstract interfaces (HAL boundary)
│       └── adapters/        ← concrete impls: zephyr/, mock/, ...
├── boards/                  ← out-of-tree custom boards
├── modules/                 ← your own reusable Zephyr modules
├── tests/
│   ├── unit/                ← host tests, BOARD=unit_testing or native_sim
│   └── integration/         ← bsim or HIL
├── doc/                     ← Doxygen + Sphinx
├── .github/workflows/
└── docker/                  ← pinned build image
```

### `west.yml` — pin everything

```yaml
manifest:
  version: 1.2
  remotes:
    - name: ncs
      url-base: https://github.com/nrfconnect
  projects:
    - name: nrf
      remote: ncs
      repo-path: sdk-nrf
      revision: v3.2.1          # PIN to a tag, never `main`
      import:
        name-allowlist:         # only pull what you actually use
          - zephyr
          - mcuboot
          - mbedtls
          - cmsis
          - hal_nordic
          - nrfxlib
  self:
    path: app
```

Pin to **release tags**, not branches. Use the manifest allowlist — it can shave gigabytes and minutes off `west update`. If you don't allowlist, you'll clone every Zephyr module ever made.

### Three configuration languages — know what each does
- **Kconfig** (`prj.conf`, `Kconfig.*`): software/feature selection. "Enable C++20", "use libstdc++", "include the BLE stack". Compile-time switches.
- **Devicetree** (`*.dts`, `*.overlay`): hardware description. "I2C0 is on these pins, this sensor is at address 0x68". Generates C macros (`DT_NODELABEL(...)`).
- **CMake** (`CMakeLists.txt`): how to build. `target_sources(app PRIVATE ...)`, link options, custom commands.

Don't put hardware-pin choices in CMake. Don't put feature flags in devicetree. Each layer has a job.

### Multiple build configurations
Use `EXTRA_CONF_FILE` for variants:
```bash
west build -b nrf52840dk/nrf52840 app -- -DEXTRA_CONF_FILE="debug.conf"
west build -b nrf52840dk/nrf52840 app -- -DEXTRA_CONF_FILE="release.conf;feature_x.conf"
```
And use **sysbuild** (the multi-image successor to child/parent images) for projects with bootloaders, network cores, or TF-M.

---

## 5. Architecture — hardware abstraction that survives a chip swap

### The principle
Application code talks to **interfaces**, not to Nordic APIs. Interfaces have at least two implementations: a **Zephyr/hardware** one and a **mock/fake** one for tests. This is essentially **ports and adapters** (hexagonal architecture) applied to firmware.

Layers, top to bottom:
1. **Domain / application logic** — knows nothing about Zephyr or the chip. Pure C++.
2. **Ports** — abstract interfaces (`ITemperatureSensor`, `IBleAdvertiser`, `IClock`, `INonVolatileStore`).
3. **Adapters** — concrete implementations. One folder per platform: `adapters/zephyr/`, `adapters/mock/`, eventually `adapters/native/`.
4. **Zephyr drivers / nrfx / vendor SDK** — the bottom of the stack. You don't write these; you wrap them.

### Two ways to do the abstraction in C++

**(a) Runtime polymorphism — abstract interfaces with virtual functions.**
Best when you actually need to swap implementations at runtime, or when you compose many small adapters. Pay the vtable cost knowingly.

```cpp
// ports/i_led.hpp
struct ILed {
    virtual ~ILed() = default;
    virtual void on() = 0;
    virtual void off() = 0;
    virtual void toggle() = 0;
    ILed() = default;
    ILed(const ILed&) = delete;
    ILed& operator=(const ILed&) = delete;
};

// adapters/zephyr/zephyr_led.hpp
class ZephyrLed final : public ILed {
public:
    explicit ZephyrLed(const struct gpio_dt_spec& spec) : spec_{spec} {
        gpio_pin_configure_dt(&spec_, GPIO_OUTPUT_INACTIVE);
    }
    void on()     override { gpio_pin_set_dt(&spec_, 1); }
    void off()    override { gpio_pin_set_dt(&spec_, 0); }
    void toggle() override { gpio_pin_toggle_dt(&spec_); }
private:
    gpio_dt_spec spec_;
};

// adapters/mock/mock_led.hpp
class MockLed final : public ILed {
public:
    void on()     override { ++on_calls; state = true; }
    void off()    override { ++off_calls; state = false; }
    void toggle() override { ++toggle_calls; state = !state; }
    int  on_calls{}, off_calls{}, toggle_calls{};
    bool state{false};
};
```

**(b) Compile-time polymorphism — CRTP or templates.**
Best when the adapter never changes after compile time and you want zero overhead. The interface is implicit (a "concept" of what methods must exist).

```cpp
template <typename Driver>
class Sensor {
    Driver& drv_;
public:
    explicit Sensor(Driver& d) : drv_(d) {}
    auto read() { return drv_.sample(); }   // resolved at compile time
};
```

For tests, you instantiate `Sensor<MockDriver>`; in production, `Sensor<ZephyrI2cDriver>`. No vtable, full inlining.

**Rule of thumb:** start with virtual interfaces at module boundaries (they read more clearly and cost is usually negligible), reach for templates when profiling or binary-size analysis says you must.

### What lives where
- Anything in `domain/` should compile on your laptop with `g++ file.cpp`. If it includes a Zephyr header, it's not domain code — move it.
- A port is *just an interface*. No `#include <zephyr/...>` allowed.
- An adapter is the *only* place Zephyr-specific code lives.
- `main.cpp` is the **composition root**: it news up the concrete adapters and hands them to the domain.

This boundary is what makes everything else in this document work.

---

## 6. Testing — the strategy

The standard test pyramid, adapted for embedded:

```
            ┌────────────────────┐
            │  HIL on real DK    │  ← few, slow, expensive
            ├────────────────────┤
            │  bsim integration  │  ← BLE + multi-node sims
            ├────────────────────┤
            │  Zephyr ztest on   │  ← integration on simulator
            │  native_sim        │
            ├────────────────────┤
            │  Host unit tests   │  ← many, fast, no Zephyr at all
            │  (BOARD=unit_      │
            │   testing)         │
            └────────────────────┘
```

### Layer 1 — unit tests for your domain code (host, no Zephyr)
Your `domain/` code has no Zephyr dependencies. Build it with vanilla GCC/Clang on your laptop, hook up **GoogleTest** or **Catch2**, run in milliseconds. This is your TDD inner loop. *No microcontroller required.*

### Layer 2 — Zephyr ztest with `BOARD=unit_testing`
For testing modules that do touch a tiny bit of Zephyr (e.g., a thin wrapper). `unit_testing` is a special board that loads only minimal Zephyr scaffolding and lets you stub the rest.

```cmake
# tests/unit/my_module/CMakeLists.txt
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
`native_sim` builds your Zephyr app as a **native Linux binary**. The kernel runs, threads are real Zephyr threads, but it's all on your host. This means:
- Tests run in seconds, not on hardware
- You can attach gdb, valgrind, sanitizers
- Coverage data via gcov/lcov is straightforward
- It's what `twister` (the Zephyr test runner) uses by default

Note: `native_posix` is deprecated; use `native_sim`.

### Layer 4 — bsim for BLE/radio integration
**BabbleSim** (bsim) simulates the Bluetooth radio environment. You can run two or more nRF52/nRF5340/nRF54L "devices" as native processes that talk to each other through a simulated RF medium, with the actual Nordic controller code running. This is how Nordic and the Zephyr project test their own BLE stack. If you're shipping anything with BLE, set this up early.

### Layer 5 — HIL (hardware-in-the-loop)
A self-hosted CI runner with a DK plugged in over USB. Twister has a `--device-testing` mode that flashes the binary, runs the test, parses serial output, reports pass/fail. Common setup: Raspberry Pi as the runner; nRF DK on USB; GitHub Actions self-hosted runner. Slower and more fragile than the layers above — don't use it for things the lower layers can catch.

### Mocking
- For C interfaces, **FFF (Fake Function Framework)** is integrated into Zephyr (`FAKE_VOID_FUNC`, `FAKE_VALUE_FUNC`). Use it for stubbing C functions you can't substitute via interface.
- For C++ interfaces, just **write fakes by hand** that implement your `IFoo` interface — this is more readable than GoogleMock's expectation DSL and easier to debug. Use GoogleMock when you really need behavior verification, not just state.
- The old ztest mocking framework (`ztest_expect_value` etc.) is **deprecated** — don't reach for it.

---

## 7. TDD workflow specifically

The thing that breaks TDD for most embedded developers is the build-flash-debug cycle. The whole point of the architecture above is to give you a tight TDD loop on *application* code without touching the chip.

**Inner loop (seconds):** `domain/` code → host unit tests → `g++` + GoogleTest. Edit-compile-run in under a second per cycle.

**Middle loop (tens of seconds):** ztest on `native_sim` for things that touch Zephyr abstractions. Twister runs them in parallel.

**Outer loop (minutes):** flash to a real DK, smoke-test on hardware. Do this once per feature, not once per save.

Workflow for a new feature (e.g., "device should publish a sensor reading every 5 seconds when above threshold"):

1. Write the use-case test against your domain layer with mocks for `ISensor`, `IPublisher`, `IClock`. Red.
2. Implement just enough domain logic to pass. Green. Refactor.
3. Write an integration test on `native_sim` that wires real-ish adapters together (still no flash). Red.
4. Implement the Zephyr adapter. Green.
5. (Optionally) flash to DK, smoke-test once.

Steps 1–4 happen entirely on your laptop, in seconds-per-iteration. Step 5 is rare.

---

## 8. Sanitizers and static analysis

### Runtime sanitizers (on `native_sim`)
You **cannot** run ASan on a Cortex-M directly (no shadow-memory infra, no clang runtime). But on `native_sim` you can, because it's a real Linux binary. This is one of the biggest wins of building host-runnable tests.

In your test build's `prj.conf` (or via west `-DCONFIG_*=y`):
```
CONFIG_ASAN=y          # AddressSanitizer
CONFIG_UBSAN=y         # UndefinedBehaviorSanitizer
```

Run your unit and integration test suites with these on in CI. ASan catches use-after-free, buffer overflows, leaks; UBSan catches signed overflow, misaligned access, shifts past type width, null deref. Both have minimal false-positive rates. UBSan is cheap enough you could leave it on in dev builds; ASan is heavier (roughly 2× slowdown, 3× memory).

You'll need `libasan` installed on the host (`apt install libasan8` on Debian/Ubuntu).

### Static analysis
Run all of these in CI on a separate, non-blocking job:
- **`clang-tidy`** with a curated `.clang-tidy` (start from `cppcoreguidelines-*`, `bugprone-*`, `performance-*`, `readability-*`, then disable noisy ones one by one).
- **`cppcheck`** as a second opinion.
- **clang static analyzer** (`scan-build`) — finds different issues than clang-tidy.
- **Compiler warnings as errors**: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Werror`. This catches more than most static analyzers and costs nothing.

### Coding standard
For safety-critical or just "I want to be conservative," **MISRA C++:2023** (the merged MISRA/AUTOSAR document) is the de-facto industry coding standard for embedded C++. Tools like Perforce QAC, PC-lint Plus, and Coverity check against it. For most non-safety-critical projects, the **C++ Core Guidelines** plus a project-specific addendum (no exceptions, no RTTI, no heap after init) is sufficient and free.

---

## 9. CI/CD

### Reproducibility first
The single rule: **your build runs in a Docker container with a pinned NCS version, on every developer's machine and in CI**. Once "works on my machine" stops being a thing, everything else gets easier.

Two ready-made image options:
- **`nordicplayground/nrfconnect-sdk`** (Nordic-affiliated) — built nightly against the last 5 NCS branches.
- **`hardwario/nrf-connect-sdk-build`** — pinned to specific NCS releases, also good.

Or build your own from those as a base, adding any project-specific tools (it ends up around 3–4 GB; that's normal).

Pin by digest, not just tag, in CI:
```yaml
container:
  image: nordicplayground/nrfconnect-sdk@sha256:abc123...
```

### GitHub Actions skeleton
The Zephyr project publishes an action that handles workspace init for you:

```yaml
name: build-and-test
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with: { path: app }
      - uses: zephyrproject-rtos/action-zephyr-setup@v1
        with:
          app-path: app
          toolchains: arm-zephyr-eabi
      - name: Build firmware
        working-directory: app
        run: west build -b nrf52840dk/nrf52840 app
      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware
          path: app/build/zephyr/zephyr.hex

  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with: { path: app }
      - uses: zephyrproject-rtos/action-zephyr-setup@v1
        with:
          app-path: app
          toolchains: arm-zephyr-eabi
      - name: Run twister
        working-directory: app
        run: |
          west twister -T tests/ -p native_sim -p unit_testing \
              --coverage --inline-logs

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: clang-tidy ... # against your domain/ tree (no Zephyr deps)
```

A reasonable matrix of jobs:
1. **Build** — every supported board × debug/release variant, fail on warnings
2. **Unit tests** — host tests via twister with ASan + UBSan + coverage
3. **Static analysis** — clang-tidy, cppcheck, scan-build (non-blocking initially)
4. **Format check** — `clang-format --dry-run --Werror`
5. **Binary size tracking** — `west build` then compare `.text` against the previous build, fail on regressions over a threshold
6. **Doc build** — Doxygen + Sphinx, fail on warnings
7. **HIL** (optional, on a self-hosted runner, on `main` only) — flash + serial-output check

### Caching
The west update step is slow. Cache `~/.west`, `modules/`, `zephyr/` between runs (key includes the hash of `west.yml`). This cuts cold builds from ~10 minutes to ~2 minutes.

---

## 10. Observability after deployment

Two free wins worth setting up at project start, not later:
- **Coredumps + symbolication.** Zephyr has a coredump subsystem; enable it on debug builds. When something crashes in the field, you get a stack trace.
- **Versioned firmware images via MCUboot.** Even if you don't ship OTA on day one, building with MCUboot from the start means OTA is a config flip, not a re-architecture. Nordic and Memfault have written extensively about pairing this with remote observability.

---

## 11. A concrete first-week checklist

Day 1 — scaffold:
- [ ] Fork `nrfconnect/ncs-example-application`, point `west.yml` at NCS v3.2.x (pinned), allowlist only modules you'll use
- [ ] Get a hello-world build green for your target board
- [ ] Get the same build running in a Docker container locally
- [ ] Set up `clang-format`, `.editorconfig`, `.gitignore`

Day 2 — C++ baseline:
- [ ] Enable C++20 + libstdc++ (`CONFIG_CPP=y`, `CONFIG_STD_CPP20=y`, `CONFIG_REQUIRES_FULL_LIBCPP=y`)
- [ ] Rename `main.c` → `main.cpp`, prove a class compiles and runs on the DK
- [ ] Write the BLE init C-wrapper pattern early (avoids surprise refactors)
- [ ] Decide on your subset (write `docs/cpp_subset.md` — exceptions off, RTTI off, no heap after init, etc.)

Day 3 — architecture:
- [ ] Create `domain/`, `ports/`, `adapters/zephyr/`, `adapters/mock/`
- [ ] Implement one end-to-end vertical slice: an `ILed` port, Zephyr adapter, mock adapter, a tiny domain class that uses it, a host unit test
- [ ] Prove the host unit test runs on your laptop with plain `g++`/CMake — no Zephyr, no west

Day 4 — testing infra:
- [ ] Set up GoogleTest (or Catch2) for host unit tests
- [ ] Set up a `tests/` tree with one ztest on `native_sim` that twister can find
- [ ] `west twister -T tests/ -p native_sim` should run green
- [ ] Add `CONFIG_ASAN=y CONFIG_UBSAN=y` to a sanitized test variant

Day 5 — CI:
- [ ] GitHub Actions workflow builds the firmware on push
- [ ] Same workflow runs twister with sanitizers
- [ ] Same workflow runs clang-tidy against `domain/` (start with a small ruleset, expand)
- [ ] Cache `.west` / `modules` / `zephyr`

After that you've got a real foundation: every commit is built, tested, sanitized, lint-checked, and pinned to a known SDK version, and you can write 90% of your application code without a chip plugged in.

---

## 12. Useful references

- Zephyr C++ language support: https://docs.zephyrproject.org/latest/develop/languages/cpp/index.html
- Zephyr ztest framework: https://docs.zephyrproject.org/latest/develop/test/ztest.html
- Zephyr `native_sim`: https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html
- bsim boards (BLE simulation): https://docs.zephyrproject.org/latest/boards/native/doc/bsim_boards_design.html
- NCS example application: https://github.com/nrfconnect/ncs-example-application
- NCS Docker image: https://github.com/NordicPlayground/nrf-docker
- Memfault Interrupt blog (NCS + GitHub Actions): https://interrupt.memfault.com/blog/ncs-github-actions
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- Embedded Template Library (ETL): https://www.etlcpp.com/

---

*This document reflects the state of NCS, Zephyr, and surrounding tooling as of May 2026. Pin versions; revisit the C++ standard ceiling and sanitizer support every time you bump NCS.*
