#include "unity.h"

#include "sdcard.h"

#include <stdio.h>
#include <string.h>

#define TMP_FILE "/tmp/test_sdcard.txt"

void setUp(void)
{
    remove(TMP_FILE);
}

void tearDown(void)
{
    remove(TMP_FILE);
}

void test_write_file_creates_file_with_correct_content(void)
{
    esp_err_t rc = sdcard_write_file(TMP_FILE, "hello world\n");
    TEST_ASSERT_EQUAL(ESP_OK, rc);

    FILE *f = fopen(TMP_FILE, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("hello world\n", buf);
}

void test_write_file_bad_path_returns_fail(void)
{
    esp_err_t rc = sdcard_write_file("/nonexistent_dir/file.txt", "data");
    TEST_ASSERT_EQUAL(ESP_FAIL, rc);
}

void test_read_file_returns_ok(void)
{
    FILE *f = fopen(TMP_FILE, "w");
    TEST_ASSERT_NOT_NULL(f);
    fprintf(f, "test content\n");
    fclose(f);

    esp_err_t rc = sdcard_read_file(TMP_FILE);
    TEST_ASSERT_EQUAL(ESP_OK, rc);
}

void test_read_file_missing_returns_fail(void)
{
    esp_err_t rc = sdcard_read_file(TMP_FILE);
    TEST_ASSERT_EQUAL(ESP_FAIL, rc);
}

void test_write_then_read_roundtrip(void)
{
    const char *content = "roundtrip test\n";
    TEST_ASSERT_EQUAL(ESP_OK, sdcard_write_file(TMP_FILE, content));
    TEST_ASSERT_EQUAL(ESP_OK, sdcard_read_file(TMP_FILE));
}
