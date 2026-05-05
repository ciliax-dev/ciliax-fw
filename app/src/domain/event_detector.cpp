#include "domain/event_detector.hpp"

EventDetector::EventDetector(IClock& clock, IEventPublisher& publisher, IStatusIndicator& status)
    : clock_(clock), publisher_(publisher), status_(status) {}

void EventDetector::on_trigger() {
    if (state_ == State::Idle) {
        enter_reporting();
    }
}

void EventDetector::tick() {
    const uint32_t elapsed = clock_.now_ms() - state_entered_at_ms_;
    switch (state_) {
    case State::Reporting:
        if (elapsed >= kReportingDurationMs) {
            enter_cooldown();
        }
        break;
    case State::Cooldown:
        if (elapsed >= kCooldownDurationMs) {
            enter_idle();
        }
        break;
    case State::Idle:
        break;
    }
}

void EventDetector::enter_reporting() {
    state_ = State::Reporting;
    state_entered_at_ms_ = clock_.now_ms();
    ++event_count_;
    publisher_.publish({event_count_, state_entered_at_ms_});
    status_.set_pattern(StatusPattern::FastBlink);
}

void EventDetector::enter_cooldown() {
    state_ = State::Cooldown;
    state_entered_at_ms_ = clock_.now_ms();
    status_.set_pattern(StatusPattern::SolidOn);
}

void EventDetector::enter_idle() {
    state_ = State::Idle;
    state_entered_at_ms_ = clock_.now_ms();
    status_.set_pattern(StatusPattern::Off);
}
