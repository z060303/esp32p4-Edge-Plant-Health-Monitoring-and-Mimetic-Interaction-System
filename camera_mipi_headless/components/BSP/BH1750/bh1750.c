#include "bh1750.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soft_i2c.h"

static const char *TAG = "bh1750";
static uint8_t bh1750_addr = BH1750_ADDR;

static bool bh1750_probe(uint8_t addr)
{
    soft_i2c_start();
    uint8_t nack = soft_i2c_write_byte((addr << 1) | 0);
    soft_i2c_stop();
    return nack == 0;
}

static esp_err_t bh1750_write_cmd(uint8_t cmd)
{
    soft_i2c_start();
    if (soft_i2c_write_byte((bh1750_addr << 1) | 0) != 0) {
        soft_i2c_stop();
        return ESP_FAIL;
    }
    if (soft_i2c_write_byte(cmd) != 0) {
        soft_i2c_stop();
        return ESP_FAIL;
    }
    soft_i2c_stop();
    return ESP_OK;
}

esp_err_t bh1750_init(void)
{
    soft_i2c_init();
    vTaskDelay(pdMS_TO_TICKS(50));

    bool has_0x23 = bh1750_probe(0x23);
    bool has_0x5c = bh1750_probe(0x5c);
    ESP_LOGI(TAG, "probe 0x23=%d 0x5c=%d scl=%d sda=%d",
             has_0x23, has_0x5c, SOFT_I2C_SCL_PIN, SOFT_I2C_SDA_PIN);
    if (has_0x23) {
        bh1750_addr = 0x23;
    } else if (has_0x5c) {
        bh1750_addr = 0x5c;
    } else {
        return ESP_FAIL;
    }

    esp_err_t ret = bh1750_write_cmd(BH1750_POWER_ON);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "power on failed");
        return ret;
    }

    (void)bh1750_write_cmd(BH1750_RESET);
    ret = bh1750_write_cmd(BH1750_CONT_H_MODE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "continuous high resolution mode failed");
        return ret;
    }

    ESP_LOGI(TAG, "BH1750 init success");
    return ESP_OK;
}

esp_err_t bh1750_read_light(float *lux)
{
    if (!lux) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    soft_i2c_start();
    if (soft_i2c_write_byte((bh1750_addr << 1) | 1) != 0) {
        soft_i2c_stop();
        ESP_LOGW(TAG, "read address nack at 0x%02x", bh1750_addr);
        return ESP_FAIL;
    }

    data[0] = soft_i2c_read_byte(1);
    data[1] = soft_i2c_read_byte(0);
    soft_i2c_stop();

    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    *lux = (float)raw / 1.2f;
    return ESP_OK;
}
