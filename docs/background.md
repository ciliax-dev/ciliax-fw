# Background and rationale — NCS C++ project setup

The reasoning behind the project's technical choices: why NCS, why Zephyr, why hexagonal architecture, why these particular Kconfig flags. The decisions themselves live in the focused docs; this file exists so future contributors (and future sessions) can audit *why* and adjust judgment calls without re-deriving the analysis.

For the canonical decisions, see:
- [cpp_subset.md](cpp_subset.md) — language features in/out
- [architecture.md](architecture.md) — ports-and-adapters layout and rules
- [development.md](development.md) — TDD loop, test pyramid, sanitizers, verification

Current as of May 2026 (NCS v3.2.x is the current stable release; v3.3.0 is in development). Revisit this document — particularly the C++-standard ceiling and sanitizer support — every time you bump NCS.

---

## 1. The platform

**nRF Connect SDK** is Nordic's umbrella SDK. It bundles **Zephyr RTOS** as its core, plus Nordic-specific drivers, wireless stacks (BLE, Thread, Matter, Wi-Fi, LTE-M/NB-IoT), and the MCUboot bootloader. When you pick NCS, you pick Zephyr — the C++ story, build system, test framework, and HAL conventions are all Zephyr's.

Two paths Nordic offers:
- **NCS Bare Metal** (in `sdk-nrf-bm`) — for simple BLE apps on the nRF54L series that don't want an RTOS. Smaller, simpler, but you lose Zephyr's testing/HAL ecosystem.
- The **standard Zephyr-based** path. This is what we use, and what almost any non-trivial project should use.

Build system stack: **CMake + Kconfig + Devicetree + west** (Nordic/Zephyr's meta-tool). You don't get to swap any of those out — they're how Zephyr works. You *can* still apply modern CMake practices on top of it.

---

## 2. C++ on NCS — what's real, what's not

### What works

- Zephyr supports up to **C++20** as of mid-2025. Default standard in Zephyr is still C++11; flip it with `CONFIG_STD_CPP20=y` in `prj.conf`.
- **Minimal C++ runtime** is what we use (`CONFIG_REQUIRES_FULL_LIBCPP` intentionally *not* set). This provides ABI bits — `new`/`delete`, static initialization, vtable support — without linking full libstdc++. The header-only STL (`std::array`, `std::span`, `std::optional`, `std::variant`, `std::expected`, `<type_traits>`, `<concepts>`, `<bit>`, most of `<algorithm>`) works because those are templates compiled into your code. The heap-using parts (`std::vector`, `std::string`, `std::map`, `<iostream>`, wall-clock `<chrono>`) won't link — by design, so the compiler enforces "no heap-using containers" instead of relying on code review. Use ETL for containers; use Zephyr's `k_uptime_*` for time.
- The Zephyr SDK toolchain (GCC) is the supported/tested compiler. Clang works in places but isn't the primary path.
- `std::array`, `std::optional`, `std::variant`, `std::span`, `constexpr`/`consteval`, `std::chrono`, lambdas, templates — all usable.

### What doesn't, or hurts

- **Exceptions are disabled by default** (`-fno-exceptions`). Don't fight this — design around `std::expected` (or `tl::expected` for pre-C++23) and error-code returns. Exceptions on a Cortex-M cost flash, RAM, and unwinding overhead you don't need.
- **RTTI is typically off.** Avoid `dynamic_cast` and `typeid`.
- **C++23 isn't supported** by the build system yet (people have tried forcing `-std=c++23` and run into issues). C++20 is the realistic ceiling.
- **`<bluetooth.h>` and some other Zephyr headers use C99 designated initializers** that don't compile under C++. The standard workaround: keep BLE setup in a `.c` file with a thin C-callable wrapper, called from C++. This has been a known issue for years and isn't going away.
- **No `std::thread` / `std::mutex`** out of the box — Zephyr provides its own primitives (`k_thread`, `k_mutex`, `k_sem`). Don't try to bridge them; use Zephyr's directly or wrap them in your own thin RAII types.
- File extension matters: Zephyr picks the C++ compiler based on suffix (`.cpp`, `.cxx`).

### The pragmatic stance

Use C++ for **application logic, abstractions, and domain code**. Leave drivers, Zephyr APIs, and wireless stack interaction in C (or in thin C-wrapper layers). This is also the boundary that makes mocking practical — and it's why our architecture has a hard split between domain/ports and adapters. See [architecture.md](architecture.md).

---

## 3. Project structure rationale

### Workspace application pattern

Nordic publishes [`ncs-example-application`](https://github.com/nrfconnect/ncs-example-application) as a starting template. It demonstrates the **west workspace application** layout, where your repo *is* the manifest repository. NCS gets pulled in as a dependency, not the other way around. This is the layout we use:

```
ciliax-fw/                   ← repo root, also the west manifest repo
├── west.yml                 ← pins NCS version, lists dependencies
├── CLAUDE.md                ← standing context for sessions
├── app/
│   ├── CMakeLists.txt
│   ├── prj.conf             ← Kconfig defaults
│   ├── boards/              ← per-board overlays
│   └── src/
│       ├── main.cpp
│       ├── domain/          ← pure logic, no Zephyr deps
│       ├── ports/           ← abstract interfaces
│       └── adapters/        ← concrete impls: zephyr/, mock/
├── tests/
│   ├── host/                ← host tests, plain g++ + GoogleTest
│   └── integration/         ← native_sim ztest (and bsim later)
├── docs/                    ← this file, plus the canonical decision docs
├── scripts/                 ← Docker-based reproducible build
└── .github/workflows/       ← CI
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

Pin to **release tags**, not branches. Use the manifest allowlist — it can shave gigabytes and minutes off `west update`. Without an allowlist, you'll clone every Zephyr module ever made.

### Three configuration languages — know what each does

- **Kconfig** (`prj.conf`, `Kconfig.*`): software/feature selection. "Enable C++20", "use libstdc++", "include the BLE stack". Compile-time switches.
- **Devicetree** (`*.dts`, `*.overlay`): hardware description. "I2C0 is on these pins, this sensor is at address 0x68". Generates C macros (`DT_NODELABEL(...)`).
- **CMake** (`CMakeLists.txt`): how to build. `target_sources(app PRIVATE ...)`, link options, custom commands.

Don't put hardware-pin choices in CMake. Don't put feature flags in devicetree. Each layer has a job.

### Multiple build configurations

Use `EXTRA_CONF_FILE` for variants:

```bash
west build -b nrf5340dk/nrf5340/cpuapp app -- -DEXTRA_CONF_FILE="debug.conf"
west build -b nrf5340dk/nrf5340/cpuapp app -- -DEXTRA_CONF_FILE="release.conf;feature_x.conf"
```

Use **sysbuild** (the multi-image successor to child/parent images) for projects with bootloaders, network cores, or TF-M.

---

## 4. CI/CD rationale

### Reproducibility first

The single rule: **the build runs in a Docker container with a pinned NCS version, on every developer's machine and in CI.** Once "works on my machine" stops being a thing, everything else gets easier.

Two ready-made image options:

- **`nordicplayground/nrfconnect-sdk`** (Nordic-affiliated) — built nightly against the last 5 NCS branches. This is what we use.
- **`hardwario/nrf-connect-sdk-build`** — pinned to specific NCS releases, also good.

You can also build your own from those as a base, adding any project-specific tools (it ends up around 3–4 GB; that's normal).

Pin by digest, not just tag, in CI:

```yaml
container:
  image: nordicplayground/nrfconnect-sdk@sha256:abc123...
```

### GitHub Actions skeleton

The Zephyr project publishes an action that handles workspace init:

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
        run: west build -b nrf5340dk/nrf5340/cpuapp app
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

## 5. Observability after deployment

Two free wins worth setting up at project start, not later:

- **Coredumps + symbolication.** Zephyr has a coredump subsystem; enable it on debug builds. When something crashes in the field, you get a stack trace.
- **Versioned firmware images via MCUboot.** Even if you don't ship OTA on day one, building with MCUboot from the start means OTA is a config flip, not a re-architecture. Nordic and Memfault have written extensively about pairing this with remote observability.

---

## 6. Useful references

- Zephyr C++ language support: https://docs.zephyrproject.org/latest/develop/languages/cpp/index.html
- Zephyr ztest framework: https://docs.zephyrproject.org/latest/develop/test/ztest.html
- Zephyr `native_sim`: https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html
- bsim boards (BLE simulation): https://docs.zephyrproject.org/latest/boards/native/doc/bsim_boards_design.html
- NCS example application: https://github.com/nrfconnect/ncs-example-application
- NCS Docker image: https://github.com/NordicPlayground/nrf-docker
- Memfault Interrupt blog (NCS + GitHub Actions): https://interrupt.memfault.com/blog/ncs-github-actions
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- Embedded Template Library (ETL): https://www.etlcpp.com/
