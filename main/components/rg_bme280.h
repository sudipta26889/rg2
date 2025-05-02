#include "bme280.h"
#include "constants.h"

#define I2C_MASTER_SCL_IO GPIO_NUM_5       // GPIO number for I2C SCL
#define I2C_MASTER_SDA_IO GPIO_NUM_4       // GPIO number for I2C SDA
#define I2C_MASTER_NUM I2C_NUM_0           // I2C port number
#define I2C_MASTER_FREQ_HZ 100000          // I2C clock frequency
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;

static const std::string BTEMPTAG = std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::BME280";
static const char *RG_BME280_TAG = BTEMPTAG.c_str();

// Struct to hold sensor data
typedef struct {
    float temperature;
    float humidity;
    float pressure;
} rg_bme280_data_t;

// Function to initialize the I2C bus and BME280 sensor
static esp_err_t bme280_init(void)
{
    ESP_LOGI(RG_BME280_TAG, "Initializing I2C bus...");

    // Configure I2C parameters
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE
    };

    conf.master.clk_speed = I2C_MASTER_FREQ_HZ; 

    // Create the I2C bus
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(RG_BME280_TAG, "Failed to create I2C bus.");
        return ESP_FAIL;
    }

    ESP_LOGI(RG_BME280_TAG, "Initializing BME280 sensor...");

    // Create the BME280 sensor handle
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT); // Use 0x77 if needed
    if (bme280 == NULL) {
        ESP_LOGE(RG_BME280_TAG, "Failed to initialize BME280.");
        return ESP_FAIL;
    }

    // Initialize the BME280 sensor with default settings
    if (bme280_default_init(bme280) != ESP_OK) {
        ESP_LOGE(RG_BME280_TAG, "Failed to initialize BME280 with default settings.");
        return ESP_FAIL;
    }

    ESP_LOGI(RG_BME280_TAG, "BME280 initialization successful.");
    return ESP_OK;
}

// Function to deinitialize the BME280 sensor and I2C bus
static void bme280_deinit(void)
{
    if (bme280) {
        bme280_delete(&bme280);
        bme280 = NULL;
    }
    if (i2c_bus) {
        i2c_bus_delete(&i2c_bus);
        i2c_bus = NULL;
    }
    ESP_LOGI(RG_BME280_TAG, "BME280 and I2C bus deinitialized.");
}

// Function to read sensor data
static rg_bme280_data_t bme280_read_data(void)
{
    rg_bme280_data_t data = {0};

    if (bme280_read_temperature(bme280, &data.temperature) != ESP_OK) {
        ESP_LOGE(RG_BME280_TAG, "Failed to read temperature.");
    }
    if (bme280_read_humidity(bme280, &data.humidity) != ESP_OK) {
        ESP_LOGE(RG_BME280_TAG, "Failed to read humidity.");
    }
    if (bme280_read_pressure(bme280, &data.pressure) != ESP_OK) {
        ESP_LOGE(RG_BME280_TAG, "Failed to read pressure.");
    }

    return data;
}