#pragma once

#include <cstdint>

#include "ports/i_led.hpp"

// Toggles an LED on a fixed tick cadence. The caller drives tick()
// from a periodic timer; every period_ticks calls the underlying
// ILed is toggled and the internal counter resets.
//
// Pure domain code: no Zephyr, no time source — the cadence is the
// caller's tick rate, not wall clock. period_ticks is assumed > 0;
// behavior at zero is undefined and not exercised by tests.
class Blinker {
public:
	Blinker(ILed& led, std::uint32_t period_ticks);

	void tick();

	Blinker(const Blinker&) = delete;
	Blinker& operator=(const Blinker&) = delete;
	Blinker(Blinker&&) = delete;
	Blinker& operator=(Blinker&&) = delete;
	~Blinker() = default;

private:
	ILed& led_;
	std::uint32_t period_ticks_;
	std::uint32_t count_{0};
};
