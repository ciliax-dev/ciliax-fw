#include <gtest/gtest.h>

#include "adapters/mock/mock_clock.hpp"
#include "adapters/mock/mock_event_publisher.hpp"
#include "adapters/mock/mock_status_indicator.hpp"
#include "domain/event_detector.hpp"

class EventDetectorTest : public ::testing::Test {
  protected:
    MockClock clock;
    MockEventPublisher publisher;
    MockStatusIndicator status;
    EventDetector detector{clock, publisher, status};
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
    detector.tick(); // -> Cooldown
    clock.advance(2000);
    detector.tick(); // -> Idle
    EXPECT_EQ(detector.state(), EventDetector::State::Idle);
    EXPECT_EQ(status.current, StatusPattern::Off);
}

TEST_F(EventDetectorTest, FullCycleTakesExpectedTotalTime) {
    clock.set(0);
    detector.on_trigger();
    clock.advance(500);
    detector.tick();
    clock.advance(2000);
    detector.tick();
    EXPECT_EQ(detector.state(), EventDetector::State::Idle);
    EXPECT_EQ(clock.now_ms(), 2500u);
}
