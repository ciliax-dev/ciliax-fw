#pragma once
#include <cstdint>

// Abstract port for a monotonic millisecond clock.
//
// Implementations: ZephyrClock wraps k_uptime_get() in production;
// MockClock advances by hand in host unit tests so the FSM can be
// driven through time without sleeping.
struct IClock {
    virtual uint32_t now_ms() const = 0;

    virtual ~IClock() = default;

    IClock() = default;
    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    IClock(IClock&&) = delete;
    IClock& operator=(IClock&&) = delete;
};
