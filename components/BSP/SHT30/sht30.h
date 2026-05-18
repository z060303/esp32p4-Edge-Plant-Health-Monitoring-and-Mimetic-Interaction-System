#ifndef __SHT30_H
#define __SHT30_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#define SHT30_ADDR            0x44

#define I2C_MASTER_SCL_IO     8
#define I2C_MASTER_SDA_IO     7

#define I2C_MASTER_FREQ_HZ    100000

esp_err_t sht30_init(void);

esp_err_t sht30_read_data(float *temperature, float *humidity);

#endif