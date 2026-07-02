#include "adc_config.h"

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "soil_adc";

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

#define SENSOR_AIR_VOLTAGE_MV 3300
#define SENSOR_WATER_VOLTAGE_MV 2200

static bool example_adc_calibration_init(adc_unit_t unit,
                                         adc_channel_t channel,
                                         adc_atten_t atten,
                                         adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &handle) == ESP_OK) {
        calibrated = true;
    }
#endif

    *out_handle = handle;
    return calibrated;
}

esp_err_t adc_sensor_init(void)
{
    if (adc1_handle) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = EXAMPLE_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ret = adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    do_calibration = example_adc_calibration_init(EXAMPLE_ADC_UNIT,
                                                 EXAMPLE_ADC_CHANNEL,
                                                 EXAMPLE_ADC_ATTEN,
                                                 &adc1_cali_handle);
    ESP_LOGI(TAG, "soil moisture ADC init success, calibrated=%d", do_calibration);
    return ESP_OK;
}

esp_err_t adc_sensor_read(int *out_raw, int *out_voltage)
{
    if (!out_raw || !out_voltage) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!adc1_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = adc_oneshot_read(adc1_handle, EXAMPLE_ADC_CHANNEL, out_raw);
    if (ret != ESP_OK) {
        return ret;
    }

    if (do_calibration) {
        return adc_cali_raw_to_voltage(adc1_cali_handle, *out_raw, out_voltage);
    }

    *out_voltage = *out_raw;
    return ESP_OK;
}

esp_err_t get_soil_moisture_percent(int *out_moisture)
{
    if (!out_moisture) {
        return ESP_ERR_INVALID_ARG;
    }

    int raw = 0;
    int voltage = 0;
    esp_err_t ret = adc_sensor_read(&raw, &voltage);
    if (ret != ESP_OK) {
        return ret;
    }

    int percentage = 0;
    if (voltage >= SENSOR_AIR_VOLTAGE_MV) {
        percentage = 0;
    } else if (voltage <= SENSOR_WATER_VOLTAGE_MV) {
        percentage = 100;
    } else {
        percentage = (SENSOR_AIR_VOLTAGE_MV - voltage) * 100 /
                     (SENSOR_AIR_VOLTAGE_MV - SENSOR_WATER_VOLTAGE_MV);
    }

    *out_moisture = percentage;
    return ESP_OK;
}

void adc_sensor_deinit(void)
{
    if (adc1_handle) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
        adc1_handle = NULL;
    }

    if (do_calibration) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle));
#endif
        adc1_cali_handle = NULL;
        do_calibration = false;
    }
}
