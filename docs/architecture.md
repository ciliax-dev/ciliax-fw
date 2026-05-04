# Architecture

Hexagonal / ports-and-adapters. Application code talks to **interfaces**, not Nordic APIs. Interfaces have at least two implementations: a Zephyr/hardware one for the device, and a mock for tests. The reasoning is in [background.md §2](background.md).

## Layers

```
app/src/
├── domain/             ← pure C++ logic, NO Zephyr includes
├── ports/              ← abstract interfaces, NO Zephyr includes
├── adapters/
│   ├── zephyr/         ← Zephyr-using implementations
│   └── mock/           ← test doubles, NO Zephyr includes
└── main.cpp            ← composition root: wires domain + adapters + Zephyr
```

Top to bottom:

1. **Domain / application logic** — knows nothing about Zephyr or the chip. Pure C++. Should compile on your laptop with `g++ file.cpp`. If a piece of logic is hard to test on the host, the architecture is wrong, not the test.
2. **Ports** — abstract interfaces (`ITemperatureSensor`, `IBleAdvertiser`, `IClock`, `INonVolatileStore`). Just an interface. No `#include <zephyr/...>` allowed.
3. **Adapters** — concrete implementations. The *only* place Zephyr-specific code lives. One folder per platform: `adapters/zephyr/`, `adapters/mock/`, eventually `adapters/native/` if we add a desktop simulator.
4. **Zephyr drivers / nrfx / vendor SDK** — the bottom of the stack. You don't write these; you wrap them.

`main.cpp` is the **composition root**: the only place that news up concrete adapters and hands them to the domain.

## The architectural invariant

`domain/`, `ports/`, and `adapters/mock/` must never `#include <zephyr/...>` or any Nordic header. Run before claiming any task done:

```bash
grep -r '#include <zephyr/' app/src/domain app/src/ports app/src/adapters/mock
```

If it returns anything, the architecture is broken — fix before continuing. This boundary is what makes everything else (host tests, sanitizers, fast TDD loop) work.

## Two ways to do the abstraction

### (a) Runtime polymorphism — abstract interfaces with virtual functions

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

### (b) Compile-time polymorphism — CRTP or templates

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

For tests, instantiate `Sensor<MockDriver>`; in production, `Sensor<ZephyrI2cDriver>`. No vtable, full inlining.

### Rule of thumb

Start with virtual interfaces at module boundaries — they read more clearly and the vtable cost is usually negligible. Reach for templates when profiling or binary-size analysis says you must.

## Ports are non-negotiable

Every external dependency (clock, GPIO, BLE, network, sensor, classifier) is reached through a port. Domain code only sees interfaces. If you find yourself wanting to "reach into Zephyr internals from domain code just for this one thing," the port is missing — add it.
