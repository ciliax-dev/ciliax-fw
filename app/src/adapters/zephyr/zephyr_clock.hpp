#pragma once

#include <zephyr/kernel.h>

#include "ports/i_clock.hpp"

// IClock implementation backed by Zephyr's monotonic uptime counter.
// k_uptime_get returns int64_t milliseconds since boot; 32 bits is
// ~49 days of run time, plenty for the foreseeable use of this clock
// (event timestamps and FSM state-elapsed math). A power-managed
// long-lived deployment that crosses the wrap should switch to the
// 64-bit value here, but the IClock contract is uint32_t today.
class ZephyrClock final : public IClock {
  public:
    uint32_t now_ms() const override { return static_cast<uint32_t>(k_uptime_get()); }
};
