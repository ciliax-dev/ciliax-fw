#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "adapters/zephyr/log_event_publisher.hpp"
#include "adapters/zephyr/zephyr_clock.hpp"
#include "adapters/zephyr/zephyr_status_indicator.hpp"
#include "domain/event_detector.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {

// Devicetree aliases provided by every Nordic DK overlay: sw0 is
// button 1, led0 is the green status LED. Resolved at compile time.
const struct gpio_dt_spec kButton = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec kLed = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// Single-TU init order is declaration order. Do not reorder:
// g_status borrows kLed, and g_detector borrows the three globals
// above it. Splitting these into separate TUs would re-introduce the
// static-init-order problem the architecture deliberately avoids.
ZephyrClock g_clock;
LogEventPublisher g_publisher;
ZephyrStatusIndicator g_status{kLed};
EventDetector g_detector{g_clock, g_publisher, g_status};

// Bridge from GPIO ISR context to thread context. on_button_pressed
// runs in interrupt context where LOG_INF and the FSM's downstream
// calls would be unsafe; it submits trigger_work to the system work
// queue, which then runs process_trigger in thread context.
void process_trigger(struct k_work*) {
    g_detector.on_trigger();
}
K_WORK_DEFINE(trigger_work, process_trigger);

struct gpio_callback button_cb;

void on_button_pressed(const struct device*, struct gpio_callback*, gpio_port_pins_t) {
    k_work_submit(&trigger_work);
}

constexpr uint32_t kTickPeriodMs = 50;

} // namespace

int main() {
    if (!gpio_is_ready_dt(&kButton) || !gpio_is_ready_dt(&kLed)) {
        LOG_ERR("button or led gpio not ready");
        return -1;
    }

    gpio_pin_configure_dt(&kButton, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&kButton, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb, on_button_pressed, BIT(kButton.pin));
    gpio_add_callback(kButton.port, &button_cb);

    LOG_INF("event detector running (tick=%u ms)", kTickPeriodMs);

    while (true) {
        g_detector.tick();
        k_msleep(kTickPeriodMs);
    }

    return 0;
}
