#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    float soil_moisture;
    float temperature;
    float air_humidity;
    float light;
    bool has_soil_moisture;
    bool has_air;
    bool has_light;
    uint32_t update_count;
} plant_sensor_reading_t;

esp_err_t plant_sensor_init(void);
void plant_sensor_start(void);
bool plant_sensor_get_latest(plant_sensor_reading_t *out_reading);
