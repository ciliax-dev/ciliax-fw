#include "adapters/zephyr/log_event_publisher.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(events, LOG_LEVEL_INF);

void LogEventPublisher::publish(const EventReport& report) {
    LOG_INF("event seq=%u t=%u", report.sequence, report.timestamp_ms);
}
