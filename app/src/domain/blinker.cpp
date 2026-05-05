#include "domain/blinker.hpp"

Blinker::Blinker(ILed& led, std::uint32_t period_ticks)
	: led_{led}
	, period_ticks_{period_ticks}
{
}

void Blinker::tick()
{
	++count_;
	if (count_ >= period_ticks_) {
		led_.toggle();
		count_ = 0;
	}
}
