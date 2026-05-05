# Phase 1 — Domain State Machine

**Goal:** Build a button-triggered "event detected → reporting → cooldown" FSM as pure C++, fully verified on the host before any Zephyr-specific code is written. By the end, the entire feature is TDD'd without flashing the chip; adding Zephyr adapters then makes it work on the DK first try.

**Mode:** Claude-executable, with two human-gate steps (a smoke test on the DK and a manual flash check after coverage is in place).

**Hardware:** nRF5340 DK only.

**Branch:** `feature/event-detector` — Claude never lands on `main` per [CLAUDE.md](../../CLAUDE.md). All work happens on this branch and merges via PR.

## Status

Size column uses t-shirt sizing calibrated to a focused-execution day:

| Size | Rough wall-clock | Character |
|------|------------------|-----------|
| **XS** | < 1 h | Mechanical, pattern already established |
| **S**  | 1–3 h | Small but needs some thought / new flags |
| **M**  | ½–1 day | Real design or integration; multiple pieces to wire |
| **L**  | 1–2 days | Multi-component, novel, or has known unknowns |
| **XL** | 2+ days | Significantly uncertain or research-heavy |

Estimates are best-case-with-no-surprises; debugging a property-test failure or a flaky devicetree alias can bump a step one size up.

| Step | Title | Size | State |
|------|-------|------|-------|
| 0  | Code coverage scaffolding                        | S  | not started |
| 1  | Ports (no implementations)                       | XS | not started |
| 2  | Mock adapters                                    | XS | not started |
| 3  | TDD: initial state + first transition (two-pass) | M  | not started |
| 4  | TDD: time-based transitions                      | S  | not started |
| 5  | TDD: guard conditions                            | XS | not started |
| 6  | Property-style robustness test                   | S  | not started |
| 7  | Zephyr adapters                                  | M  | not started |
| 8  | Composition root with deferred-from-ISR trigger  | M  | not started |
| 9  | Smoke test on hardware                           | XS | not started |
| 10 | native_sim ztest with `CONFIG_TEST_HOOKS`        | M  | not started |
| 11 | Docs follow-up commit                            | S  | not started |

Roll-up: ~4× M, 4× S, 4× XS → roughly **3–5 focused days** end-to-end if nothing goes sideways. The four M steps (3, 7, 8, 10) are where time will actually go.

See [Plan deltas](#plan-deltas) for divergences during execution.

---

## Prerequisites

[Phase 0](phase0.md) is at status `done` across all 14 steps, and the standing verification block in [`docs/development.md`](../development.md#verification-before-declaring-done) is green on a fresh checkout. If either is not true, finish Phase 0 first — Phase 1 assumes that scaffolding works (`west build`, host tests with sanitizers, native_sim ztest, CI green, `ILed` port + `ZephyrLed` / `MockLed` adapters in place).

---

## The state machine — design before code

```
                   ┌──────────────┐
       on_trigger  │              │  tick() && elapsed > T_report
       ┌──────────►│  Reporting   ├──────────────┐
       │           │              │              │
       │           └──────────────┘              ▼
┌──────┴──────┐                          ┌──────────────┐
│             │                          │              │
│    Idle     │◄─────────────────────────┤   Cooldown   │
│             │  tick() && elapsed > T_cool             │
└─────────────┘                          └──────────────┘
       ▲                                         │
       └─────────────────────────────────────────┘
                  on_trigger (ignored)
                  on_trigger during Reporting (ignored)
```

**States:** `Idle`, `Reporting`, `Cooldown`

**External inputs:**
- `on_trigger()` — called from the main thread (not ISR — see Step 8) when a trigger arrives
- `tick()` — called periodically (~50 ms) so the FSM advances time-based transitions

**Constants (start with these, tune later):**
- `kReportingDurationMs = 500`
- `kCooldownDurationMs = 2000`

**Side effects per transition:**

| From → To              | Side effects                                                  |
|------------------------|---------------------------------------------------------------|
| Idle → Reporting       | Increment event counter; publish event; status → fast blink   |
| Reporting → Cooldown   | Status → solid on                                             |
| Cooldown → Idle        | Status → off                                                  |
| Trigger while Reporting| Increment "dropped" counter; no transition                    |
| Trigger while Cooldown | Increment "dropped" counter; no transition                    |

The `tick()`-based design is deliberately simple. We're not optimising for power yet — that's a later phase. The point right now is testability.

---

## Step 0 — Code coverage scaffolding

| | |
|---|---|
| **Mode** | 🤖 Claude |
| **Why first** | Definition of done requires ≥95% line coverage on `event_detector.cpp`. Wiring `gcovr` once, before any tests are written, means every subsequent step's coverage delta is visible immediately and the threshold gate runs in CI from day one. |
| **Outputs** | Updated `tests/host/CMakeLists.txt`; new `coverage` job in `.github/workflows/ci.yml`; section in `docs/development.md` |

**Prompt for Claude:**

> Add code-coverage support to the host test build, scoped initially at the `domain/` layer:
>
> 1. In [`tests/host/CMakeLists.txt`](../../tests/host/CMakeLists.txt), add an option `ENABLE_COVERAGE` (default OFF). When ON, add `--coverage -O0 -g` to compile and link flags for the `runtests` target. Don't combine with `ENABLE_SANITIZERS` in the same build dir — coverage uses a separate `build-cov/` tree.
> 2. Add a `coverage` job to [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) that:
>    - Installs `gcc`, `g++`, `gcovr` (≥7.0).
>    - Configures with `-DENABLE_COVERAGE=ON` into `tests/host/build-cov/`.
>    - Builds and runs `ctest`.
>    - Runs `gcovr --root app/src --filter 'app/src/domain/.*' --print-summary --fail-under-line 95 --xml-pretty -o coverage.xml tests/host/build-cov/`.
>    - Uploads `coverage.xml` as an artefact.
>    - Marked `continue-on-error: true` *only until Step 11 lands* — at that point flip it to required. Today there's no domain code yet, so the threshold would fail; the job runs but doesn't gate.
> 3. Add a "Coverage" subsection to [`docs/development.md`](../development.md) under "Sanitizers" describing the local commands:
>    ```bash
>    cmake -S tests/host -B tests/host/build-cov -DENABLE_COVERAGE=ON
>    cmake --build tests/host/build-cov -j
>    ctest --test-dir tests/host/build-cov --output-on-failure
>    gcovr --root app/src --filter 'app/src/domain/.*' --print-summary tests/host/build-cov/
>    ```
> 4. Extend the `build-*/` `.gitignore` line if needed so `tests/host/build-cov/` is ignored (Phase 0's broadened pattern likely covers it already — verify).
>
> Commit on `feature/event-detector`. Do not modify the existing `format`, `host-tests`, `firmware-build`, `twister`, or `clang-tidy` jobs.

**Verify:**
```bash
cmake -S tests/host -B tests/host/build-cov -DENABLE_COVERAGE=ON
cmake --build tests/host/build-cov -j
ctest --test-dir tests/host/build-cov --output-on-failure
gcovr --root app/src --filter 'app/src/domain/.*' --print-summary tests/host/build-cov/
```
Expected: configure + build succeed; existing Phase 0 tests pass; `gcovr` reports coverage on `domain/blinker.cpp` (the only domain TU today). Threshold gate is informational until Step 11.

---

## Step 1 — Define the ports (no implementation yet)

| | |
|---|---|
| **Mode** | 🤖 Claude |
| **Outputs** | `app/src/ports/i_clock.hpp`, `app/src/ports/i_event_publisher.hpp`, `app/src/ports/i_status_indicator.hpp` |

Pure interface definitions. Compile with vanilla `g++ -std=c++20`. Match the established style in [`app/src/ports/i_led.hpp`](../../app/src/ports/i_led.hpp): destructor first, deleted copy *and* move, no Zephyr includes.

**`ports/i_clock.hpp`**
```cpp
#pragma once
#include <cstdint>

struct IClock {
    virtual uint32_t now_ms() const = 0;

    virtual ~IClock() = default;

    IClock() = default;
    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    IClock(IClock&&) = delete;
    IClock& operator=(IClock&&) = delete;
};
```

**`ports/i_event_publisher.hpp`**
```cpp
#pragma once
#include <cstdint>

struct EventReport {
    uint32_t sequence;
    uint32_t timestamp_ms;
};

struct IEventPublisher {
    virtual void publish(const EventReport& report) = 0;

    virtual ~IEventPublisher() = default;

    IEventPublisher() = default;
    IEventPublisher(const IEventPublisher&) = delete;
    IEventPublisher& operator=(const IEventPublisher&) = delete;
    IEventPublisher(IEventPublisher&&) = delete;
    IEventPublisher& operator=(IEventPublisher&&) = delete;
};
```

**`ports/i_status_indicator.hpp`**
```cpp
#pragma once

enum class StatusPattern { Off, FastBlink, SolidOn };

struct IStatusIndicator {
    virtual void set_pattern(StatusPattern pattern) = 0;

    virtual ~IStatusIndicator() = default;

    IStatusIndicator() = default;
    IStatusIndicator(const IStatusIndicator&) = delete;
    IStatusIndicator& operator=(const IStatusIndicator&) = delete;
    IStatusIndicator(IStatusIndicator&&) = delete;
    IStatusIndicator& operator=(IStatusIndicator&&) = delete;
};
```

**Success criteria:** Each header compiles with `g++ -std=c++20 -Wall -Wextra -Wpedantic -c -x c++-header app/src/ports/<file>.hpp -o /tmp/<file>.gch`. The architecture-invariant grep returns nothing.

---

## Step 2 — Write the mock adapters

| | |
|---|---|
| **Mode** | 🤖 Claude |
| **Outputs** | `app/src/adapters/mock/mock_clock.hpp`, `app/src/adapters/mock/mock_event_publisher.hpp`, `app/src/adapters/mock/mock_status_indicator.hpp` |

Mocks follow the [existing `MockLed`](../../app/src/adapters/mock/mock_led.hpp) convention: public counters and last-state fields, no STL containers, no Zephyr. Per [`docs/cpp_subset.md`](../cpp_subset.md), `std::vector` / `std::string` / `std::map` are forbidden; the mock keeps the most recent event plus a count, which is enough for every test in this phase.

**`adapters/mock/mock_clock.hpp`**
```cpp
#pragma once
#include "ports/i_clock.hpp"

class MockClock final : public IClock {
public:
    uint32_t now_ms() const override { return now_; }
    void advance(uint32_t ms) { now_ += ms; }
    void set(uint32_t ms) { now_ = ms; }

private:
    uint32_t now_{0};
};
```

**`adapters/mock/mock_event_publisher.hpp`**
```cpp
#pragma once
#include "ports/i_event_publisher.hpp"

class MockEventPublisher final : public IEventPublisher {
public:
    void publish(const EventReport& r) override {
        last = r;
        ++publish_calls;
    }

    EventReport last{};
    int         publish_calls{0};
};
```

**`adapters/mock/mock_status_indicator.hpp`**
```cpp
#pragma once
#include "ports/i_status_indicator.hpp"

class MockStatusIndicator final : public IStatusIndicator {
public:
    void set_pattern(StatusPattern p) override {
        current = p;
        ++change_count;
    }

    StatusPattern current{StatusPattern::Off};
    int           change_count{0};
};
```

**Success criteria:** All three compile with vanilla `g++ -std=c++20 -I app/src`. No further dependencies.

---

## Step 3 — TDD `EventDetector`: initial state and first transition (strict two-pass)

This is the heart of Phase 1. **Pass 1 writes the test and watches it fail; Pass 2 implements.** Don't collapse them — the failing-test step is how we know the test actually tests something. Phase 0 carved out this discipline; Phase 1 keeps it.

### Pass 1 — failing test

Create `tests/host/event_detector_test.cpp`:

```cpp
#include <gtest/gtest.h>
#include "domain/event_detector.hpp"
#include "adapters/mock/mock_clock.hpp"
#include "adapters/mock/mock_event_publisher.hpp"
#include "adapters/mock/mock_status_indicator.hpp"

class EventDetectorTest : public ::testing::Test {
protected:
    MockClock              clock;
    MockEventPublisher     publisher;
    MockStatusIndicator    status;
    EventDetector          detector{clock, publisher, status};
};

TEST_F(EventDetectorTest, StartsInIdleState) {
    EXPECT_EQ(detector.state(), EventDetector::State::Idle);
    EXPECT_EQ(detector.event_count(), 0u);
}

TEST_F(EventDetectorTest, TriggerInIdleTransitionsToReporting) {
    detector.on_trigger();
    EXPECT_EQ(detector.state(), EventDetector::State::Reporting);
}

TEST_F(EventDetectorTest, TriggerInIdleIncrementsCounter) {
    detector.on_trigger();
    EXPECT_EQ(detector.event_count(), 1u);
}

TEST_F(EventDetectorTest, TriggerInIdlePublishesEvent) {
    clock.set(12345);
    detector.on_trigger();
    EXPECT_EQ(publisher.publish_calls, 1);
    EXPECT_EQ(publisher.last.sequence, 1u);
    EXPECT_EQ(publisher.last.timestamp_ms, 12345u);
}

TEST_F(EventDetectorTest, TriggerInIdleStartsFastBlink) {
    detector.on_trigger();
    EXPECT_EQ(status.current, StatusPattern::FastBlink);
}
```

Add `event_detector_test.cpp` to the `runtests` target in `tests/host/CMakeLists.txt`. Do **not** add `event_detector.cpp` yet — it doesn't exist.

Build:
```bash
cmake --build tests/host/build
```

**Confirm it fails to compile** with a missing-header diagnostic for `domain/event_detector.hpp`. Capture the exact failure (a compile error, not a typo) and report before continuing.

### Pass 2 — minimum implementation

The `app/CMakeLists.txt` already globs `domain/*.cpp`, so simply landing the new file picks it up for the firmware build. For the host test build, add `${APP_SRC}/domain/event_detector.cpp` to `runtests` sources in `tests/host/CMakeLists.txt`.

**`app/src/domain/event_detector.hpp`**
```cpp
#pragma once
#include <cstdint>
#include "ports/i_clock.hpp"
#include "ports/i_event_publisher.hpp"
#include "ports/i_status_indicator.hpp"

class EventDetector {
public:
    enum class State { Idle, Reporting, Cooldown };

    EventDetector(IClock& clock, IEventPublisher& publisher, IStatusIndicator& status);

    void on_trigger();
    void tick();

    State    state() const { return state_; }
    uint32_t event_count() const { return event_count_; }
    uint32_t dropped_count() const { return dropped_count_; }

private:
    static constexpr uint32_t kReportingDurationMs = 500;
    static constexpr uint32_t kCooldownDurationMs  = 2000;

    void enter_reporting();
    void enter_cooldown();
    void enter_idle();

    IClock&              clock_;
    IEventPublisher&     publisher_;
    IStatusIndicator&    status_;
    State                state_{State::Idle};
    uint32_t             state_entered_at_ms_{0};
    uint32_t             event_count_{0};
    uint32_t             dropped_count_{0};
};
```

Implement `event_detector.cpp` with the minimum to pass — guards stay for Step 5; today only the Idle → Reporting path is needed.

**Success criteria:** All five tests pass. Re-run under `tests/host/build-asan` (sanitizers) and `tests/host/build-cov` (coverage) — both green; coverage on the new TU is non-zero.

---

## Step 4 — TDD time-based transitions

Add to `event_detector_test.cpp`:

```cpp
TEST_F(EventDetectorTest, TickBeforeReportingTimeoutStaysInReporting) {
    detector.on_trigger();
    clock.advance(499);
    detector.tick();
    EXPECT_EQ(detector.state(), EventDetector::State::Reporting);
}

TEST_F(EventDetectorTest, TickAfterReportingTimeoutTransitionsToCooldown) {
    detector.on_trigger();
    clock.advance(500);
    detector.tick();
    EXPECT_EQ(detector.state(), EventDetector::State::Cooldown);
    EXPECT_EQ(status.current, StatusPattern::SolidOn);
}

TEST_F(EventDetectorTest, TickAfterCooldownTimeoutTransitionsToIdle) {
    detector.on_trigger();
    clock.advance(500);
    detector.tick();           // -> Cooldown
    clock.advance(2000);
    detector.tick();           // -> Idle
    EXPECT_EQ(detector.state(), EventDetector::State::Idle);
    EXPECT_EQ(status.current, StatusPattern::Off);
}

TEST_F(EventDetectorTest, FullCycleTakesExpectedTotalTime) {
    clock.set(0);
    detector.on_trigger();
    clock.advance(500);  detector.tick();
    clock.advance(2000); detector.tick();
    EXPECT_EQ(detector.state(), EventDetector::State::Idle);
    EXPECT_EQ(clock.now_ms(), 2500u);
}
```

Run them, confirm the new ones fail, then implement `tick()`. Keep the body small — check elapsed time, transition, that's it.

**Success criteria:** All nine tests pass. State machine handles its own time without a system-clock dependency.

---

## Step 5 — TDD the guard conditions

```cpp
TEST_F(EventDetectorTest, TriggerDuringReportingIsDropped) {
    detector.on_trigger();
    detector.on_trigger();
    EXPECT_EQ(detector.event_count(), 1u);
    EXPECT_EQ(detector.dropped_count(), 1u);
    EXPECT_EQ(publisher.publish_calls, 1);
}

TEST_F(EventDetectorTest, TriggerDuringCooldownIsDropped) {
    detector.on_trigger();
    clock.advance(500);
    detector.tick();
    EXPECT_EQ(detector.state(), EventDetector::State::Cooldown);

    detector.on_trigger();
    EXPECT_EQ(detector.event_count(), 1u);
    EXPECT_EQ(detector.dropped_count(), 1u);
}

TEST_F(EventDetectorTest, TriggerAfterFullCycleIsAccepted) {
    detector.on_trigger();
    clock.advance(500);  detector.tick();
    clock.advance(2000); detector.tick();
    detector.on_trigger();
    EXPECT_EQ(detector.event_count(), 2u);
    EXPECT_EQ(detector.dropped_count(), 0u);
}
```

Same two-pass discipline (run, confirm failure, implement guards).

**Success criteria:** All 12 tests pass. The state machine rejects spurious triggers and the rejection is observable via `dropped_count()`.

---

## Step 6 — Property-style robustness test

A single test that hammers the FSM with semi-random input and verifies invariants over thousands of transitions:

```cpp
TEST_F(EventDetectorTest, InvariantsHoldUnderRapidInput) {
    constexpr int kIterations = 10000;
    for (int i = 0; i < kIterations; ++i) {
        if (i % 3 == 0) detector.on_trigger();
        clock.advance(100);
        detector.tick();

        EXPECT_LE(detector.event_count() + detector.dropped_count(),
                  static_cast<uint32_t>((i / 3) + 1));
        EXPECT_EQ(static_cast<uint32_t>(publisher.publish_calls),
                  detector.event_count());
    }
}
```

**Success criteria:** Test passes. Run it under sanitizers (`ENABLE_SANITIZERS=ON`) and under coverage (`ENABLE_COVERAGE=ON`) — both clean. Domain coverage should now be ≥95%; if not, Step 11 will fail the CI gate. This test alone catches a *lot* of real bugs.

---

## Step 7 — Write the Zephyr adapters

Now — and only now — touch Zephyr. Adapters live in `app/src/adapters/zephyr/`; the existing `app/CMakeLists.txt` glob picks them up automatically.

**`adapters/zephyr/zephyr_clock.hpp`**
```cpp
#pragma once
#include <zephyr/kernel.h>
#include "ports/i_clock.hpp"

class ZephyrClock final : public IClock {
public:
    uint32_t now_ms() const override {
        return static_cast<uint32_t>(k_uptime_get());
    }
};
```

**`adapters/zephyr/log_event_publisher.hpp`** + **`.cpp`** — `LOG_MODULE_REGISTER(events, …)` plus `LOG_INF("event seq=%u t=%u", r.sequence, r.timestamp_ms);`. `LOG_INF` is a thread-context call, which is fine because Step 8 ensures `publish()` is never reached from an ISR.

**`adapters/zephyr/zephyr_status_indicator.hpp`** wraps the same `gpio_dt_spec` pattern as `ZephyrLed`. Map `StatusPattern` to a `k_work_delayable` that toggles the GPIO at the right rate (FastBlink ≈ 100 ms half-period; SolidOn ≈ pin held high; Off ≈ pin held low). Sketch:

```cpp
class ZephyrStatusIndicator final : public IStatusIndicator {
public:
    explicit ZephyrStatusIndicator(const gpio_dt_spec& spec);
    void set_pattern(StatusPattern p) override;
private:
    static void blink_handler(struct k_work* work);
    gpio_dt_spec             spec_;
    StatusPattern            pattern_{StatusPattern::Off};
    struct k_work_delayable  work_;
};
```

`set_pattern()` is called from the main thread (Step 8 again — see the deferred trigger), so no ISR-safety constraint applies inside the implementation.

**Success criteria:** All adapters compile inside `west build`. Each one is under 50 lines.

---

## Step 8 — Composition root with deferred-from-ISR trigger

`main.cpp` is the only TU allowed to know about both the domain and Zephyr. Two design points it has to get right:

1. **The button callback runs in interrupt context.** `g_detector.on_trigger()` transitively calls `LogEventPublisher::publish` (which calls `LOG_INF`) and `ZephyrStatusIndicator::set_pattern`. Per [CLAUDE.md](../../CLAUDE.md), logging from ISRs is forbidden. The ISR therefore submits a `k_work` and the actual `on_trigger()` runs on the system work queue.
2. **Globals with cross-dependent constructors are a smell.** All four are in the same anonymous namespace, so initialisation order is the declaration order — well-defined and safe — but worth a one-line comment so a future reader doesn't move them.

```cpp
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "domain/event_detector.hpp"
#include "adapters/zephyr/zephyr_clock.hpp"
#include "adapters/zephyr/log_event_publisher.hpp"
#include "adapters/zephyr/zephyr_status_indicator.hpp"

namespace {

const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec led    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// Single-TU init order is declaration order. Don't reorder.
ZephyrClock              g_clock;
LogEventPublisher        g_publisher;
ZephyrStatusIndicator    g_status{led};
EventDetector            g_detector{g_clock, g_publisher, g_status};

void process_trigger(struct k_work*) {
    g_detector.on_trigger();
}
K_WORK_DEFINE(trigger_work, process_trigger);

struct gpio_callback button_cb;
void on_button_pressed(const struct device*, struct gpio_callback*, gpio_port_pins_t) {
    k_work_submit(&trigger_work);
}

} // namespace

int main(void) {
    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb, on_button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb);

    while (true) {
        g_detector.tick();
        k_msleep(50);
    }
}
```

Notes:
- The 50 ms tick is fine for now; an event-driven / `k_work_delayable`-based loop is a later phase when power matters.
- `k_work_submit` is ISR-safe; everything done after it (logging, GPIO toggling, FSM mutation) runs in the system work queue context.

**Success criteria:** `west build && west flash` succeeds. Pressing button 1 on the DK triggers a logged event over RTT (`west espressif monitor` or `JLinkRTTClient`). LED behaviour matches the state machine.

---

## Step 9 — Smoke test on hardware

| | |
|---|---|
| **Mode** | 🤝 Collab |

Manual verification, ~10 minutes:

- [ ] Press button → LED fast-blinks ~500 ms → solid ~2 s → off
- [ ] Log shows `event seq=1 t=<some ms>`
- [ ] Press button during the blink — second press ignored, no new log line
- [ ] Press button during the solid period — same, ignored
- [ ] Wait ~3 seconds, press button — new log line with `seq=2`
- [ ] Press button rapidly 10 times — only the first in each cycle logs; `dropped_count` reflects the rest (expose via shell command, debugger, or test hooks from Step 10)

If anything fails, **the bug is almost certainly in the adapter, not the domain** — the unit tests have already proved the domain works. This is the architecture earning its keep.

---

## Step 10 — One ztest integration test on `native_sim`

Prove the end-to-end flow runs on the simulator. This catches integration bugs (composition wiring, devicetree, Kconfig) that unit tests miss.

The test seam is gated by a Kconfig, lives in its own translation unit, and exposes C-linkage helpers callable from C ztest code.

**`app/Kconfig`** (new file at app root):
```kconfig
# Application-level Kconfig. Picked up automatically when present.

config TEST_HOOKS
    bool "Expose test-only hooks for integration tests"
    default n
    help
      When enabled, app/src/test_hooks.cpp builds an extern "C" surface
      (trigger_event_for_test, get_event_count_for_test) that ztest C
      code can call into the C++ composition root. Must stay default n
      in production builds.
```

**`app/src/test_hooks.cpp`** (added to the build only when `CONFIG_TEST_HOOKS=y`):
```cpp
#ifdef CONFIG_TEST_HOOKS
#include <cstdint>

// Defined in main.cpp's anonymous namespace; the test_hooks TU reaches in
// through external linkage exposed only in this build configuration.
extern class EventDetector& test_detector_ref();

extern "C" void trigger_event_for_test(void) {
    test_detector_ref().on_trigger();
}

extern "C" uint32_t get_event_count_for_test(void) {
    return test_detector_ref().event_count();
}
#endif
```

…with a matching `#ifdef CONFIG_TEST_HOOKS` block in `main.cpp` exposing the accessor:
```cpp
#ifdef CONFIG_TEST_HOOKS
EventDetector& test_detector_ref() { return g_detector; }
#endif
```

**`app/CMakeLists.txt`** — append:
```cmake
if(CONFIG_TEST_HOOKS)
    target_sources(app PRIVATE src/test_hooks.cpp)
endif()
```
(Don't broaden the existing `domain/` / `adapters/zephyr/` globs — `test_hooks.cpp` lives at `app/src/` deliberately.)

**`tests/integration/event_flow/CMakeLists.txt`** — model on [`tests/integration/blink/CMakeLists.txt`](../../tests/integration/blink/CMakeLists.txt). Pull in the libstdc++/ETL header glue via the helper at [`cmake/zephyr_cxx_includes.cmake`](../../cmake/zephyr_cxx_includes.cmake) (Phase 0 Step 13 delta — needed because the firmware build uses MINIMAL_LIBCPP).

**`tests/integration/event_flow/prj.conf`**:
```
CONFIG_ZTEST=y
CONFIG_LOG=y
CONFIG_CPP=y
CONFIG_STD_CPP20=y
CONFIG_TEST_HOOKS=y
```
(No `CONFIG_REQUIRES_FULL_LIBCPP` — match the firmware profile.)

**`tests/integration/event_flow/src/main.c`** — note the `test_` prefix on the function name (Phase 0 Step 13 delta — twister's symbol scanner rejects anything else):
```c
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

extern void     trigger_event_for_test(void);
extern uint32_t get_event_count_for_test(void);

ZTEST_SUITE(event_flow, NULL, NULL, NULL, NULL, NULL);

ZTEST(event_flow, test_full_cycle_increments_counter) {
    zassert_equal(get_event_count_for_test(), 0);
    trigger_event_for_test();
    k_msleep(2600);  // through reporting + cooldown
    zassert_equal(get_event_count_for_test(), 1);
}
```

**`tests/integration/event_flow/testcase.yaml`**:
```yaml
tests:
  app.event_flow:
    platform_allow:
      - native_sim
    extra_configs:
      - CONFIG_TEST_HOOKS=y
    harness: ztest
    tags: integration
```

Run:
```bash
west twister -T tests/integration/event_flow -p native_sim --inline-logs
```

**Success criteria:** Test passes locally and in CI. The existing `twister` job in `.github/workflows/ci.yml` picks it up because it already runs `-T tests/integration`.

---

## Step 11 — Docs follow-up commit

| | |
|---|---|
| **Mode** | 🤖 Claude |
| **Why a separate commit** | Per [CLAUDE.md](../../CLAUDE.md): "Docs land in a dedicated follow-up commit" so a reader bisecting or reverting one doesn't accidentally take the other. |

After Steps 0–10 are merged-or-mergeable on `feature/event-detector`:

1. Update this file's Status table — mark each step `done`, record any divergences in [Plan deltas](#plan-deltas).
2. Update [`docs/architecture.md`](../architecture.md) if any port pattern, ISR-deferral pattern, or composition-root convention crystallised here that future phases should follow (the deferred-from-ISR pattern is a likely candidate).
3. Update [`CLAUDE.md`](../../CLAUDE.md) if anything from Phase 1 belongs in standing context (e.g. "trigger ports must be ISR-safe at the boundary, deferred to work queue inside the adapter").
4. Flip the `coverage` CI job from `continue-on-error: true` to required, now that domain code exists and the threshold is meaningful.
5. Commit message: `docs: record phase 1 completion and plan deltas`. No code in this commit.

Then open the PR.

---

## Definition of done

Phase 1 is complete when **all** of these are true:

- [ ] All 11 rows in the Status table read `done`.
- [ ] [`app/src/domain/event_detector.{hpp,cpp}`](../../app/src/domain/) exists; the architecture-invariant grep returns nothing across `domain/`, `ports/`, `adapters/mock/`.
- [ ] All three new ports defined; mock and Zephyr adapters for each.
- [ ] At least 12 host unit tests, all passing, total runtime under 1 second.
- [ ] Same tests pass under `-DENABLE_SANITIZERS=ON`.
- [ ] Coverage on `app/src/domain/event_detector.cpp` ≥ 95% (Step 0's `coverage` job, now gating).
- [ ] One ztest integration test on `native_sim`, passing in CI.
- [ ] `west build` produces a flashable image; smoke test on the DK matches Step 9.
- [ ] The button trigger is deferred from ISR to the work queue (no `LOG_*` or other non-ISR-safe code reachable from the GPIO callback).
- [ ] CI runs format, host tests with sanitizers, firmware build, native_sim ztest, clang-tidy, and coverage — all green.
- [ ] You can describe the state machine on a whiteboard from memory.

When all those check, the architecture has been proven on something real, and Phase 2 (BLE peripheral) starts from a position where `IEventPublisher` gets a second adapter that publishes over GATT and the rest of the system doesn't change at all. That "doesn't change at all" is the test of whether you got Phase 1 right.

---

## Common traps

- **Letting Zephyr leak into `domain/`.** A `k_msleep` or `k_uptime_get` in the state machine breaks host testability. The grep in CLAUDE.md is non-negotiable.
- **Collapsing the two-pass TDD step.** If you write tests and implementation in the same edit, the tests can pass for the wrong reason. Step 3 is deliberately structured Pass 1 / Pass 2.
- **Implementation-driven tests.** If a test reads like a description of how the code is structured rather than what it does, rewrite it.
- **Skipping the property-style test.** It feels redundant; it isn't. It's the first time the FSM sees thousands of transitions and surfaces issues no example-based test will. It's also what pushes coverage above the gate.
- **Doing the Zephyr adapter before the unit tests.** You'll get something that "works" but the architecture stops being testable from that point on, and you'll feel it for years.
- **Putting non-trivial logic in `main.cpp`.** Composition root is for wiring, full stop. If `main.cpp` has an `if` related to business logic, that logic belongs in the domain.
- **Calling `LOG_*` (or anything non-ISR-safe) from the GPIO callback.** Always defer via `k_work_submit`. The deferred handler runs in thread context where logging, allocation, and kernel calls are all fine.
- **Bundling docs with the implementation commit.** Step 11 is separate by design — see CLAUDE.md.

The discipline in Phase 1 sets the standard for every phase after it. Cut a corner here and the edge-AI integration in a later phase will hurt much more than it should.

---

## Plan deltas

Things that diverged from the plan during execution. Recorded so a re-run, or a future reader, doesn't have to rediscover them.

*(none yet — populate as steps execute)*
