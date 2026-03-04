#include "unity.h"

#include "thumbstick.h"

void setUp(void)
{
}

void tearDown(void)
{
    // Reset module state between tests
    thumbstick_deinit();
}

void test_get_values_before_init_returns_invalid_state(void)
{
    uint32_t x, y;
    esp_err_t rc = thumbstick_get_values(&x, &y);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, rc);
}

void test_deinit_before_init_returns_ok(void)
{
    esp_err_t rc = thumbstick_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}

void test_init_returns_ok(void)
{
    esp_err_t rc = thumbstick_init();
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}

void test_double_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
}

void test_get_values_null_x_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
    uint32_t y;
    esp_err_t rc = thumbstick_get_values(NULL, &y);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, rc);
}

void test_get_values_null_y_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
    uint32_t x;
    esp_err_t rc = thumbstick_get_values(&x, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, rc);
}

void test_get_values_after_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
    uint32_t x, y;
    esp_err_t rc = thumbstick_get_values(&x, &y);
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}

void test_deinit_after_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, thumbstick_init());
    esp_err_t rc = thumbstick_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}
