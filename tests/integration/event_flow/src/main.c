#include <stdint.h>
#include <zephyr/ztest.h>

extern void trigger_event_for_test(void);
extern uint32_t get_event_count_for_test(void);

ZTEST_SUITE(event_flow, NULL, NULL, NULL, NULL, NULL);

ZTEST(event_flow, test_trigger_increments_counter) {
    zassert_equal(get_event_count_for_test(), 0);
    trigger_event_for_test();
    zassert_equal(get_event_count_for_test(), 1);
}
