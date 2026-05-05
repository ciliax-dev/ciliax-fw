#pragma once
#include "ports/i_clock.hpp"

class MockClock final : public IClock {
  public:
    uint32_t now_ms() const override { return now_; }

    void advance(uint32_t ms) { now_ += ms; }
    void set(uint32_t ms) { now_ = ms; }

  private:
    uint32_t now_{0};
};
