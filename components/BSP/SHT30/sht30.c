#include "sht30.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "plant_i2c.h"

static const char *TAG = "sht30";

static i2c_master_dev_handle_t sht30_handle;

esp_err_t sht30_init(void)
{
    if (sht30_handle) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus_handle = NULL;
    esp_err_t ret = plant_i2c_get_bus(&bus_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2c_master_probe(bus_handle, SHT30_ADDR, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "probe 0x%02x failed: %s", SHT30_ADDR, esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT30_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &sht30_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SHT30 init success");
    return ESP_OK;
}

esp_err_t sht30_read_data(float *temperature, float *humidity)
{
    if (!temperature || !humidity) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!sht30_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t cmd[2] = {0x24, 0x00};
    esp_err_t ret = i2c_master_transmit(sht30_handle, cmd, sizeof(cmd), -1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "measure command failed: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6];
    ret = i2c_master_receive(sht30_handle, data, sizeof(data), -1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read data failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
    uint16_t raw_humi = ((uint16_t)data[3] << 8) | data[4];

    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity = 100.0f * ((float)raw_humi / 65535.0f);
    return ESP_OK;
}
