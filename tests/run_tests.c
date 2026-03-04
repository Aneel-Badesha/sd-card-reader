#include "unity.h"

// sdcard tests
void test_write_file_creates_file_with_correct_content(void);
void test_write_file_bad_path_returns_fail(void);
void test_read_file_returns_ok(void);
void test_read_file_missing_returns_fail(void);
void test_write_then_read_roundtrip(void);

// thumbstick tests
void test_get_values_before_init_returns_invalid_state(void);
void test_deinit_before_init_returns_ok(void);
void test_init_returns_ok(void);
void test_double_init_returns_ok(void);
void test_get_values_null_x_returns_invalid_arg(void);
void test_get_values_null_y_returns_invalid_arg(void);
void test_get_values_after_init_returns_ok(void);
void test_deinit_after_init_returns_ok(void);

// button tests
void test_init_button_returns_ok(void);
void test_init_button_propagates_gpio_failure(void);
void test_button_is_pressed_when_gpio_low(void);
void test_button_is_pressed_when_gpio_high(void);

int main(void)
{
    UNITY_BEGIN();

    // sdcard
    RUN_TEST(test_write_file_creates_file_with_correct_content);
    RUN_TEST(test_write_file_bad_path_returns_fail);
    RUN_TEST(test_read_file_returns_ok);
    RUN_TEST(test_read_file_missing_returns_fail);
    RUN_TEST(test_write_then_read_roundtrip);

    // thumbstick
    RUN_TEST(test_get_values_before_init_returns_invalid_state);
    RUN_TEST(test_deinit_before_init_returns_ok);
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_double_init_returns_ok);
    RUN_TEST(test_get_values_null_x_returns_invalid_arg);
    RUN_TEST(test_get_values_null_y_returns_invalid_arg);
    RUN_TEST(test_get_values_after_init_returns_ok);
    RUN_TEST(test_deinit_after_init_returns_ok);

    // button
    RUN_TEST(test_init_button_returns_ok);
    RUN_TEST(test_init_button_propagates_gpio_failure);
    RUN_TEST(test_button_is_pressed_when_gpio_low);
    RUN_TEST(test_button_is_pressed_when_gpio_high);

    return UNITY_END();
}
