#pragma once

// High-level visual state, decoupled from how it gets rendered.
// The Zephyr adapter maps each pattern to a GPIO sequence (off /
// fast-blink via k_work_delayable / steady on); the mock adapter
// just records the most recent value for tests.
enum class StatusPattern { Off, FastBlink, SolidOn };

// Abstract port for the on-device status indicator (an LED today,
// possibly a multi-color or display surface later).
struct IStatusIndicator {
    virtual void set_pattern(StatusPattern pattern) = 0;

    virtual ~IStatusIndicator() = default;

    IStatusIndicator() = default;
    IStatusIndicator(const IStatusIndicator&) = delete;
    IStatusIndicator& operator=(const IStatusIndicator&) = delete;
    IStatusIndicator(IStatusIndicator&&) = delete;
    IStatusIndicator& operator=(IStatusIndicator&&) = delete;
};
