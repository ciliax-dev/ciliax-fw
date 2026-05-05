#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "adapters/zephyr/zephyr_status_indicator.hpp"

namespace {
const struct gpio_dt_spec kLedSpec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
ZephyrStatusIndicator g_status{kLedSpec};
} // namespace

extern "C" void set_off(void) {
    g_status.set_pattern(StatusPattern::Off);
}

extern "C" void set_fast_blink(void) {
    g_status.set_pattern(StatusPattern::FastBlink);
}

extern "C" void set_solid_on(void) {
    g_status.set_pattern(StatusPattern::SolidOn);
}

extern "C" int read_pin(void) {
    return gpio_emul_output_get(kLedSpec.port, kLedSpec.pin);
}
