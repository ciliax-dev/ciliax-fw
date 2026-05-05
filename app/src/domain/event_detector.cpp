#include "domain/event_detector.hpp"

EventDetector::EventDetector(IClock& clock, IEventPublisher& publisher, IStatusIndicator& status)
    : clock_(clock), publisher_(publisher), status_(status) {}

void EventDetector::on_trigger() {
    if (state_ == State::Idle) {
        enter_reporting();
    }
}

void EventDetector::tick() {}

void EventDetector::enter_reporting() {
    state_ = State::Reporting;
    state_entered_at_ms_ = clock_.now_ms();
    ++event_count_;
    publisher_.publish({event_count_, state_entered_at_ms_});
    status_.set_pattern(StatusPattern::FastBlink);
}
