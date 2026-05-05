#pragma once

#include "ports/i_led.hpp"

// Test double for ILed. Records call counts and tracks the latched state so
// host unit tests can assert how the domain drove the LED. Lives in the
// header so tests pull it in without a separate translation unit.
class MockLed final : public ILed {
  public:
    void on() override {
        ++on_calls;
        state = true;
    }

    void off() override {
        ++off_calls;
        state = false;
    }

    void toggle() override {
        ++toggle_calls;
        state = !state;
    }

    int on_calls{0};
    int off_calls{0};
    int toggle_calls{0};
    bool state{false};
};
