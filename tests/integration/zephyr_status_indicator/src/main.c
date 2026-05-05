#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

extern void set_off(void);
extern void set_fast_blink(void);
extern void set_solid_on(void);
extern int read_pin(void);

ZTEST_SUITE(zephyr_status_indicator, NULL, NULL, NULL, NULL, NULL);

ZTEST(zephyr_status_indicator, test_solid_on_drives_pin_high) {
    set_off();
    set_solid_on();
    zassert_equal(read_pin(), 1);
    set_off();
}

ZTEST(zephyr_status_indicator, test_off_drives_pin_low) {
    set_solid_on();
    set_off();
    zassert_equal(read_pin(), 0);
}

ZTEST(zephyr_status_indicator, test_fast_blink_toggles_pin) {
    set_off();
    int before = read_pin();
    set_fast_blink();
    /* 100 ms half-period; 150 ms guarantees at least one toggle has fired. */
    k_msleep(150);
    int after = read_pin();
    zassert_not_equal(before, after);
    set_off();
}
