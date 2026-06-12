#ifndef PLANT_I2C_H
#define PLANT_I2C_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLANT_I2C_PORT      I2C_NUM_1
#define PLANT_I2C_SCL_IO    28
#define PLANT_I2C_SDA_IO    36
#define PLANT_I2C_FREQ_HZ   100000

esp_err_t plant_i2c_get_bus(i2c_master_bus_handle_t *out_bus);

#ifdef __cplusplus
}
#endif

#endif
