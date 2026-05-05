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

    State state() const { return state_; }
    uint32_t event_count() const { return event_count_; }
    uint32_t dropped_count() const { return dropped_count_; }

  private:
    static constexpr uint32_t kReportingDurationMs = 500;
    static constexpr uint32_t kCooldownDurationMs = 2000;

    void enter_reporting();

    IClock& clock_;
    IEventPublisher& publisher_;
    IStatusIndicator& status_;
    State state_{State::Idle};
    uint32_t state_entered_at_ms_{0};
    uint32_t event_count_{0};
    uint32_t dropped_count_{0};
};
