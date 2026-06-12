#include "plant_sensor.h"

#include "adc_config.h"
#include "bh1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "plant_display.h"
#include "sht30.h"
#include "plant_touch.h"

static const char *TAG = "plant_sensor";

static SemaphoreHandle_t sensor_lock;
static TaskHandle_t sensor_task_handle;

static bool sht30_ready;
static bool soil_ready;
static bool bh1750_ready;

static plant_sensor_reading_t latest = {
    .soil_moisture = 45.0f,
    .temperature = 26.0f,
    .air_humidity = 55.0f,
    .light = 500.0f,
};

static void publish_reading(const plant_sensor_reading_t *reading)
{
    if (!sensor_lock) {
        return;
    }

    xSemaphoreTake(sensor_lock, portMAX_DELAY);
    latest = *reading;
    xSemaphoreGive(sensor_lock);
}

static int clamp_score(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static void show_sensor_reading_on_display(const plant_sensor_reading_t *reading)
{
    int health = 100;
    const char *status = "HEALTHY";
    const char *mood = "HAPPY";
    const char *advice = "KEEP";
    const bool touched = plant_touch_is_active();

    if (!reading->has_air && !reading->has_soil_moisture && !reading->has_light) {
        plant_display_show_status_full("UNKNOWN",
                                       0,
                                       "WAIT",
                                       0,
                                       "CHECK",
                                       false,
                                       -1.0f,
                                       -100.0f,
                                       -1.0f,
                                       -1.0f);
        return;
    }

    if (reading->has_soil_moisture) {
        if (reading->soil_moisture < 25.0f) {
            health -= 25;
            status = "LOCAL_ISSUE";
            mood = "DRY";
            advice = "WATER";
        } else if (reading->soil_moisture > 85.0f) {
            health -= 30;
            status = "LOCAL_ISSUE";
            mood = "STICKY";
            advice = "DRAIN";
        }
    }

    if (reading->has_light) {
        if (reading->light < 25.0f) {
            health -= 20;
            status = "LOCAL_ISSUE";
            mood = "GLOOMY";
            advice = "LIGHT";
        } else if (reading->light > 1200.0f) {
            health -= 20;
            status = "LOCAL_ISSUE";
            mood = "DIZZY";
            advice = "SHADE";
        }
    }

    if (reading->has_air) {
        if (reading->temperature > 34.0f) {
            health -= 25;
            status = "LOCAL_ISSUE";
            mood = "WARM";
            advice = "COOL";
        } else if (reading->temperature < 12.0f) {
            health -= 25;
            status = "LOCAL_ISSUE";
            mood = "SHIVERING";
            advice = "WARM";
        }

        if (reading->air_humidity < 35.0f) {
            health -= 20;
            status = "LOCAL_ISSUE";
            mood = "DRY";
            advice = "HUMIDIFY";
        } else if (reading->air_humidity > 80.0f) {
            health -= 20;
            status = "LOCAL_ISSUE";
            mood = "STICKY";
            advice = "VENTILATE";
        }
    }

    health = clamp_score(health);
    if (health < 45) {
        status = "SEVERE";
    }

    if (touched) {
        mood = health < 45 ? "COMFORTED" : "HAPPY";
        advice = health < 45 ? "WATCH" : "COMFORT";
        if (health < 100) {
            health += 5;
        }
        health = clamp_score(health);
    }

    plant_display_show_status_full(status,
                                   reading->has_air || reading->has_soil_moisture || reading->has_light ? 100 : 0,
                                   mood,
                                   health,
                                   advice,
                                   touched,
                                   reading->has_soil_moisture ? reading->soil_moisture : -1.0f,
                                   reading->has_air ? reading->temperature : -100.0f,
                                   reading->has_air ? reading->air_humidity : -1.0f,
                                   reading->has_light ? reading->light : -1.0f);
}

static void plant_sensor_task(void *arg)
{
    (void)arg;
    plant_sensor_reading_t reading = latest;

    while (1) {
        float temperature = 0.0f;
        float humidity = 0.0f;
        int soil = 0;
        float light = 0.0f;

        if (sht30_ready) {
            esp_err_t ret = sht30_read_data(&temperature, &humidity);
            if (ret == ESP_OK) {
                reading.temperature = temperature;
                reading.air_humidity = humidity;
                reading.has_air = true;
            } else {
                reading.has_air = false;
                ESP_LOGW(TAG, "SHT30 read failed: %s", esp_err_to_name(ret));
            }
        }

        if (soil_ready) {
            esp_err_t ret = get_soil_moisture_percent(&soil);
            if (ret == ESP_OK) {
                reading.soil_moisture = (float)soil;
                reading.has_soil_moisture = true;
            } else {
                reading.has_soil_moisture = false;
                ESP_LOGW(TAG, "soil ADC read failed: %s", esp_err_to_name(ret));
            }
        }

        if (bh1750_ready) {
            esp_err_t ret = bh1750_read_light(&light);
            if (ret == ESP_OK) {
                reading.light = light;
                reading.has_light = true;
            } else {
                reading.has_light = false;
                ESP_LOGW(TAG, "BH1750 read failed: %s", esp_err_to_name(ret));
            }
        }

        reading.update_count++;
        publish_reading(&reading);

        ESP_LOGI(TAG,
                 "SENSOR soil=%.0f temp=%.1f hum=%.1f light=%.1f valid=%d%d%d",
                 reading.soil_moisture,
                 reading.temperature,
                 reading.air_humidity,
                 reading.light,
                 reading.has_soil_moisture,
                 reading.has_air,
                 reading.has_light);

        show_sensor_reading_on_display(&reading);
        for (int i = 0; i < 20; i++) {
            if (plant_touch_was_touched()) {
                show_sensor_reading_on_display(&reading);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

esp_err_t plant_sensor_init(void)
{
    if (!sensor_lock) {
        sensor_lock = xSemaphoreCreateMutex();
        if (!sensor_lock) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t ret = sht30_init();
    sht30_ready = ret == ESP_OK;
    if (!sht30_ready) {
        ESP_LOGW(TAG, "SHT30 init failed: %s", esp_err_to_name(ret));
    }

    ret = adc_sensor_init();
    soil_ready = ret == ESP_OK;
    if (!soil_ready) {
        ESP_LOGW(TAG, "soil ADC init failed: %s", esp_err_to_name(ret));
    }

    ret = bh1750_init();
    bh1750_ready = ret == ESP_OK;
    if (!bh1750_ready) {
        ESP_LOGW(TAG, "BH1750 init failed: %s", esp_err_to_name(ret));
    }

    return (sht30_ready || soil_ready || bh1750_ready) ? ESP_OK : ESP_FAIL;
}

void plant_sensor_start(void)
{
    if (sensor_task_handle) {
        return;
    }

    xTaskCreatePinnedToCore(plant_sensor_task,
                            "plant_sensor",
                            4096,
                            NULL,
                            4,
                            &sensor_task_handle,
                            0);
}

bool plant_sensor_get_latest(plant_sensor_reading_t *out_reading)
{
    if (!out_reading || !sensor_lock) {
        return false;
    }

    xSemaphoreTake(sensor_lock, portMAX_DELAY);
    *out_reading = latest;
    bool has_update = latest.update_count > 0;
    xSemaphoreGive(sensor_lock);
    return has_update;
}
