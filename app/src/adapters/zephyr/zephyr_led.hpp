#pragma once

#include <zephyr/drivers/gpio.h>

#include "ports/i_led.hpp"

// ILed adapter backed by a Zephyr Devicetree GPIO. The wrapped
// gpio_dt_spec is normally fetched at the composition root via
// GPIO_DT_SPEC_GET(DT_ALIAS(...), gpios) and passed in by const-ref;
// the spec is small (pointer + pin + flags) and stored by value.
//
// The constructor configures the pin as a low-driving output, so on
// power-on the LED starts off regardless of how the driver leaves it.
class ZephyrLed final : public ILed {
  public:
    explicit ZephyrLed(const struct gpio_dt_spec& spec) : spec_{spec} {
        gpio_pin_configure_dt(&spec_, GPIO_OUTPUT_INACTIVE);
    }

    void on() override { gpio_pin_set_dt(&spec_, 1); }
    void off() override { gpio_pin_set_dt(&spec_, 0); }
    void toggle() override { gpio_pin_toggle_dt(&spec_); }

  private:
    struct gpio_dt_spec spec_;
};
