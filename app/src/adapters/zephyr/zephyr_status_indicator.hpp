#pragma once

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "ports/i_status_indicator.hpp"

// IStatusIndicator implementation that drives a single GPIO LED.
//
// Off / SolidOn map directly onto gpio_pin_set_dt; FastBlink toggles
// the pin from a k_work_delayable so the FSM's set_pattern call stays
// non-blocking. The work and its back-pointer live in a nested
// standard-layout struct so CONTAINER_OF in the static handler is
// well-defined despite this class itself having a vtable.
class ZephyrStatusIndicator final : public IStatusIndicator {
  public:
    explicit ZephyrStatusIndicator(const struct gpio_dt_spec& spec);

    void set_pattern(StatusPattern pattern) override;

  private:
    struct BlinkContext {
        struct k_work_delayable work;
        ZephyrStatusIndicator* self;
    };

    static void blink_handler(struct k_work* work);

    static constexpr uint32_t kFastBlinkHalfPeriodMs = 100;

    struct gpio_dt_spec spec_;
    StatusPattern pattern_{StatusPattern::Off};
    BlinkContext blink_{};
};
