#pragma once
#include "ports/i_event_publisher.hpp"

// Counter + last-event mock matching the MockLed convention.
// std::vector is forbidden by the project's C++ subset; if a future
// test needs the full series, switch to etl::vector with a fixed N.
class MockEventPublisher final : public IEventPublisher {
  public:
    void publish(const EventReport& r) override {
        last = r;
        ++publish_calls;
    }

    EventReport last{};
    int publish_calls{0};
};
