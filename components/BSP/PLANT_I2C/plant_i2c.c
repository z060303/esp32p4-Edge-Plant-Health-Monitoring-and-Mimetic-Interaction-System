#include "plant_i2c.h"

#include "esp_log.h"

static const char *TAG = "plant_i2c";
static i2c_master_bus_handle_t plant_i2c_bus;

esp_err_t plant_i2c_get_bus(i2c_master_bus_handle_t *out_bus)
{
    if (!out_bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!plant_i2c_bus) {
        i2c_master_bus_config_t bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = PLANT_I2C_PORT,
            .scl_io_num = PLANT_I2C_SCL_IO,
            .sda_io_num = PLANT_I2C_SDA_IO,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };

        esp_err_t ret = i2c_new_master_bus(&bus_config, &plant_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "plant I2C bus init success: port=%d scl=%d sda=%d",
                 PLANT_I2C_PORT, PLANT_I2C_SCL_IO, PLANT_I2C_SDA_IO);
    }

    *out_bus = plant_i2c_bus;
    return ESP_OK;
}
