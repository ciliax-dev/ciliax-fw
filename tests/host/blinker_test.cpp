#include <gtest/gtest.h>

#include "adapters/mock/mock_led.hpp"
#include "domain/blinker.hpp"

TEST(BlinkerTest, DoesNotToggleBeforeReachingPeriod) {
    MockLed led;
    Blinker blinker{led, 5};

    for (int i = 0; i < 4; ++i) {
        blinker.tick();
    }

    EXPECT_EQ(led.toggle_calls, 0);
}

TEST(BlinkerTest, TogglesOnceWhenPeriodReached) {
    MockLed led;
    Blinker blinker{led, 5};

    for (int i = 0; i < 5; ++i) {
        blinker.tick();
    }

    EXPECT_EQ(led.toggle_calls, 1);
}

TEST(BlinkerTest, TogglesAtEachPeriodBoundary) {
    MockLed led;
    Blinker blinker{led, 3};

    for (int i = 0; i < 9; ++i) {
        blinker.tick();
    }

    EXPECT_EQ(led.toggle_calls, 3);
}

TEST(BlinkerTest, PeriodOfOneTogglesEveryTick) {
    MockLed led;
    Blinker blinker{led, 1};

    for (int i = 0; i < 10; ++i) {
        blinker.tick();
    }

    EXPECT_EQ(led.toggle_calls, 10);
}
