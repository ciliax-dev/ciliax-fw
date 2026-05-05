#pragma once

// Abstract port for a single binary-state LED.
//
// Implementations live in adapters/: ZephyrLed wraps a gpio_dt_spec for
// production, MockLed records calls for host unit tests. Domain code only
// ever sees this interface.
struct ILed {
    virtual void on() = 0;
    virtual void off() = 0;
    virtual void toggle() = 0;

    virtual ~ILed() = default;

    ILed() = default;
    ILed(const ILed&) = delete;
    ILed& operator=(const ILed&) = delete;
    ILed(ILed&&) = delete;
    ILed& operator=(ILed&&) = delete;
};
