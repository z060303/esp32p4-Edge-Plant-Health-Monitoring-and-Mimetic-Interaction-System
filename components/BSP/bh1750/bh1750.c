#include "bh1750.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soft_i2c.h" // 包含你刚才写的软件I2C头文件

static const char *TAG = "BH1750_SOFT";

// 注意：软件I2C不需要 i2c_master_bus_handle_t 等硬件句柄

/**
 * @brief 发送命令 (适配软件I2C)
 */
static esp_err_t bh1750_write_cmd(uint8_t cmd)
{
    soft_i2c_start();
    // 写入设备地址 (写操作：ADDR << 1 | 0)
    if (soft_i2c_write_byte((BH1750_ADDR << 1) | 0) != 0) {
        soft_i2c_stop();
        return ESP_FAIL; // 未收到应答
    }
    // 写入命令字节
    if (soft_i2c_write_byte(cmd) != 0) {
        soft_i2c_stop();
        return ESP_FAIL;
    }
    soft_i2c_stop();
    return ESP_OK;
}

/**
 * @brief 初始化BH1750
 */
esp_err_t bh1750_init(void)
{
    // 调用软件I2C引脚初始化
    soft_i2c_init();

    vTaskDelay(pdMS_TO_TICKS(50));

    if (bh1750_write_cmd(BH1750_POWER_ON) != ESP_OK) {
        ESP_LOGE(TAG, "Power ON failed");
        return ESP_FAIL;
    }

    bh1750_write_cmd(BH1750_RESET);
    bh1750_write_cmd(BH1750_CONT_H_MODE);

    ESP_LOGI(TAG, "BH1750 Software I2C init success");
    return ESP_OK;
}

/**
 * @brief 读取光照值 (适配软件I2C)
 */
esp_err_t bh1750_read_light(float *lux)
{
    uint8_t data[2];
    uint16_t raw;

    soft_i2c_start();
    // 写入设备地址 (读操作：ADDR << 1 | 1)
    if (soft_i2c_write_byte((BH1750_ADDR << 1) | 1) != 0) {
        soft_i2c_stop();
        return ESP_FAIL;
    }

    // 读取高8位，发送 ACK (1)
    data[0] = soft_i2c_read_byte(1);
    // 读取低8位，发送 NACK (0) 结束读取
    data[1] = soft_i2c_read_byte(0);
    soft_i2c_stop();

    raw = (data[0] << 8) | data[1];
    *lux = (float)raw / 1.2f;

    return ESP_OK;
}