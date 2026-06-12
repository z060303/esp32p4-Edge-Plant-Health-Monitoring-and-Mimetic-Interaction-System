#pragma once

#include <stdbool.h>

typedef enum {
    PLANT_ENGINE_VISION_HEALTHY = 0,
    PLANT_ENGINE_VISION_LOCAL_ISSUE,
    PLANT_ENGINE_VISION_SEVERE,
    PLANT_ENGINE_VISION_UNKNOWN,
} plant_engine_vision_t;

typedef struct {
    float soil_moisture;
    float temperature;
    float air_humidity;
    float light;
    bool touched;
    plant_engine_vision_t vision;
    int confidence_percent;
} plant_engine_input_t;

typedef struct {
    char status[24];
    int confidence_percent;
    char mood[24];
    int comfort_score;
    int health_score;
    char advice[40];
} plant_engine_result_t;

void plant_engine_analyze(const plant_engine_input_t *input, plant_engine_result_t *result);
bool plant_engine_handle_command(const char *command);
