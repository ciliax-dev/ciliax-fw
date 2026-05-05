#pragma once
#include "ports/i_status_indicator.hpp"

class MockStatusIndicator final : public IStatusIndicator {
  public:
    void set_pattern(StatusPattern p) override {
        current = p;
        ++change_count;
    }

    StatusPattern current{StatusPattern::Off};
    int change_count{0};
};
