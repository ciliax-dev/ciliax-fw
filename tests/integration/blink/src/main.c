#include <zephyr/ztest.h>

ZTEST_SUITE(blink_smoke, NULL, NULL, NULL, NULL, NULL);

ZTEST(blink_smoke, test_sanity)
{
	zassert_true(true);
}
