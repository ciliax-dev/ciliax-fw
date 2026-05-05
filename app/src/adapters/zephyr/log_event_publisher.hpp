#pragma once

#include "ports/i_event_publisher.hpp"

// IEventPublisher implementation that emits each detected event over
// the Zephyr logging subsystem (RTT / UART backend chosen by Kconfig).
// Calls into LOG_INF, which is not ISR-safe; main.cpp guarantees this
// publisher is only ever invoked from the system work queue, never
// directly from a GPIO callback.
class LogEventPublisher final : public IEventPublisher {
  public:
    void publish(const EventReport& report) override;
};
