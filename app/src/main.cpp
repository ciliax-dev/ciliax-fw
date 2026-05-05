#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "adapters/zephyr/zephyr_led.hpp"
#include "domain/blinker.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {

// led0 is the Devicetree alias every Nordic DK overlays for "the green
// status LED on the board." Resolved at compile time; the resulting
// gpio_dt_spec carries the device pointer + pin + active-level flags.
const struct gpio_dt_spec kLed0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// Tick rate is set by k_msleep(kTickPeriodMs) below; with period 10 the
// blinker toggles every 500 ms (~1 Hz visible blink).
constexpr std::uint32_t kBlinkPeriodTicks = 10;
constexpr std::uint32_t kTickPeriodMs = 50;

} // namespace

int main() {
    if (!gpio_is_ready_dt(&kLed0)) {
        LOG_ERR("led0 gpio not ready");
        return -1;
    }

    ZephyrLed led{kLed0};
    Blinker blinker{led, kBlinkPeriodTicks};

    LOG_INF("blinker running (period=%u ticks, tick=%u ms)", kBlinkPeriodTicks, kTickPeriodMs);

    while (true) {
        blinker.tick();
        k_msleep(kTickPeriodMs);
    }

    return 0;
}
