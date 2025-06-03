#include "unity.h"
#include "rg_bme280.h"

TEST_CASE("rg_bme280_read_values returns invalid arg on NULL parameters", "[rg_bme280]")
{
    rg_bme280_t dev = {0};
    (void)dev;
    esp_err_t err = rg_bme280_read_values(NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
