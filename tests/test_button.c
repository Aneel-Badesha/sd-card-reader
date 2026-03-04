#include "unity.h"

#include "button.h"
#include "driver/gpio.h"

void setUp(void)
{
    stub_set_gpio_level(1);
    stub_set_gpio_config_rc(ESP_OK);
}

void tearDown(void)
{
}

void test_init_button_returns_ok(void)
{
    esp_err_t rc = init_button(GPIO_NUM_0);
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}

void test_init_button_propagates_gpio_failure(void)
{
    stub_set_gpio_config_rc(ESP_FAIL);
    esp_err_t rc = init_button(GPIO_NUM_0);
    TEST_ASSERT_EQUAL(ESP_FAIL, rc);
}

void test_button_is_pressed_when_gpio_low(void)
{
    stub_set_gpio_level(0);
    TEST_ASSERT_TRUE(button_is_pressed(GPIO_NUM_0));
}

void test_button_is_pressed_when_gpio_high(void)
{
    stub_set_gpio_level(1);
    TEST_ASSERT_FALSE(button_is_pressed(GPIO_NUM_0));
}
