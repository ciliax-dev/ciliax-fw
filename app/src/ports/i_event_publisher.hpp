#pragma once
#include <cstdint>

// One detected event as seen by the publisher. Sequence is
// monotonically increasing per device boot; timestamp_ms is sampled
// from the IClock at the moment of detection.
struct EventReport {
    uint32_t sequence;
    uint32_t timestamp_ms;
};

// Abstract port for shipping detected events outward.
//
// Implementations: LogEventPublisher prints over RTT/UART for now;
// a future BLE GATT publisher will be a second adapter without any
// change to the domain.
struct IEventPublisher {
    virtual void publish(const EventReport& report) = 0;

    virtual ~IEventPublisher() = default;

    IEventPublisher() = default;
    IEventPublisher(const IEventPublisher&) = delete;
    IEventPublisher& operator=(const IEventPublisher&) = delete;
    IEventPublisher(IEventPublisher&&) = delete;
    IEventPublisher& operator=(IEventPublisher&&) = delete;
};
