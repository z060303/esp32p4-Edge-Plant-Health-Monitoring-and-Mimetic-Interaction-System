#include "sht30.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SHT30";

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t sht30_handle;

/**
 * @brief 初始化I2C和SHT30
 */
esp_err_t sht30_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT30_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &sht30_handle));

    ESP_LOGI(TAG, "SHT30 init success");

    return ESP_OK;
}

/**
 * @brief 读取温湿度
 */
esp_err_t sht30_read_data(float *temperature, float *humidity)
{
    uint8_t cmd[2] = {0x24, 0x00};

    /*
        0x2400:
        高重复率测量
        无时钟拉伸
        数据手册 Table9
    */

    ESP_ERROR_CHECK(
        i2c_master_transmit(
            sht30_handle,
            cmd,
            2,
            -1
        )
    );

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6];

    ESP_ERROR_CHECK(
        i2c_master_receive(
            sht30_handle,
            data,
            6,
            -1
        )
    );

    /*
        数据格式：

        data[0] Temp MSB
        data[1] Temp LSB
        data[2] Temp CRC

        data[3] Humi MSB
        data[4] Humi LSB
        data[5] Humi CRC
    */

    uint16_t raw_temp;
    uint16_t raw_humi;

    raw_temp = (data[0] << 8) | data[1];
    raw_humi = (data[3] << 8) | data[4];

    /*
        温度转换公式
        T = -45 + 175 * raw / 65535

        数据手册4.13
    */

    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);

    /*
        湿度转换公式
        RH = 100 * raw / 65535
    */

    *humidity = 100.0f * ((float)raw_humi / 65535.0f);

    return ESP_OK;
}