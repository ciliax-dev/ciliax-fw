#include <etl/array.h>
#include <etl/string_view.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {

class Greeter {
public:
	explicit constexpr Greeter(const etl::string_view& name) : name_{name} {}

	[[nodiscard]] constexpr const etl::string_view& name() const { return name_; }

private:
	const etl::string_view& name_;
};

constexpr etl::string_view kProjectName{"my-project"};
constexpr etl::array<int, 4> kPrimes{2, 3, 5, 7};

} // namespace

int main(void)
{
	constexpr Greeter greeter{kProjectName};

	LOG_INF("hello from %.*s",
		static_cast<int>(greeter.name().size()),
		greeter.name().data());
	LOG_INF("first prime: %d", kPrimes[0]);

	while (true) {
		k_msleep(1000);
	}

	return 0;
}
