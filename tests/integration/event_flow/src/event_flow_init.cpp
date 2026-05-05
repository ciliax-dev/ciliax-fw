#include <cstdint>

#include "adapters/mock/mock_clock.hpp"
#include "adapters/mock/mock_event_publisher.hpp"
#include "adapters/mock/mock_status_indicator.hpp"
#include "domain/event_detector.hpp"

namespace {
MockClock g_clock;
MockEventPublisher g_publisher;
MockStatusIndicator g_status;
EventDetector g_detector{g_clock, g_publisher, g_status};
} // namespace

extern "C" void trigger_event_for_test(void) {
    g_detector.on_trigger();
}

extern "C" uint32_t get_event_count_for_test(void) {
    return g_detector.event_count();
}
