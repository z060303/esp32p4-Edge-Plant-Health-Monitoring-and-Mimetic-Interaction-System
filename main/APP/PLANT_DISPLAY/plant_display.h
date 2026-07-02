#pragma once

#include <stdbool.h>

void plant_display_init(void);
void plant_display_show_status(const char *status,
                               int confidence_percent,
                               const char *mood,
                               int health_score,
                               const char *advice);
void plant_display_show_status_full(const char *status,
                                    int confidence_percent,
                                    const char *mood,
                                    int health_score,
                                    const char *advice,
                                    bool touched,
                                    float soil_moisture,
                                    float temperature,
                                    float air_humidity,
                                    float light);
void plant_display_show_pattern(void);
void plant_display_show_color_test(int step);
bool plant_display_handle_result_command(const char *command);
