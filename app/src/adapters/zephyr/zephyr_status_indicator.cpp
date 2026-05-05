#include "adapters/zephyr/zephyr_status_indicator.hpp"

ZephyrStatusIndicator::ZephyrStatusIndicator(const struct gpio_dt_spec& spec) : spec_(spec) {
    gpio_pin_configure_dt(&spec_, GPIO_OUTPUT_INACTIVE);
    blink_.self = this;
    k_work_init_delayable(&blink_.work, &ZephyrStatusIndicator::blink_handler);
}

void ZephyrStatusIndicator::set_pattern(StatusPattern pattern) {
    pattern_ = pattern;
    switch (pattern) {
    case StatusPattern::Off:
        k_work_cancel_delayable(&blink_.work);
        gpio_pin_set_dt(&spec_, 0);
        break;
    case StatusPattern::SolidOn:
        k_work_cancel_delayable(&blink_.work);
        gpio_pin_set_dt(&spec_, 1);
        break;
    case StatusPattern::FastBlink:
        k_work_schedule(&blink_.work, K_MSEC(kFastBlinkHalfPeriodMs));
        break;
    }
}

void ZephyrStatusIndicator::blink_handler(struct k_work* work) {
    auto* dwork = k_work_delayable_from_work(work);
    auto* ctx = CONTAINER_OF(dwork, BlinkContext, work);
    if (ctx->self->pattern_ != StatusPattern::FastBlink) {
        return;
    }
    gpio_pin_toggle_dt(&ctx->self->spec_);
    k_work_reschedule(dwork, K_MSEC(kFastBlinkHalfPeriodMs));
}
