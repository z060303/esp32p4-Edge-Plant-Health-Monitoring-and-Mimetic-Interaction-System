#include "adc_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADC_CONFIG";

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

/* ================== 传感器校准值（根据传感器实际调整）================== */
// 将传感器完全暴露在空气中
#define SENSOR_AIR_VOLTAGE_MV    2740  // 0% 湿度时的电压 (完全干燥)

// 将传感器插入水杯中
#define SENSOR_WATER_VOLTAGE_MV  1040  // 100% 湿度时的电压 (完全湿润)
/* ========================================================================= */

// (内部函数) 初始化校准
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    return calibrated;
}

esp_err_t adc_sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = EXAMPLE_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC_CHANNEL, &config));

    do_calibration = example_adc_calibration_init(EXAMPLE_ADC_UNIT, EXAMPLE_ADC_CHANNEL, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);
    ESP_LOGI(TAG, "ADC及土壤湿度传感器初始化完成");
    return ESP_OK;
}

esp_err_t adc_sensor_read(int *out_raw, int *out_voltage)
{
    if (adc1_handle == NULL) return ESP_ERR_INVALID_STATE;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC_CHANNEL, out_raw));

    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, *out_raw, out_voltage));
    } else {
        *out_voltage = *out_raw; 
    }
    return ESP_OK;
}

esp_err_t get_soil_moisture_percent(int *out_moisture)
{
    int raw = 0;
    int voltage = 0;

    esp_err_t ret = adc_sensor_read(&raw, &voltage);
    if (ret != ESP_OK) return ret;

    int percentage = 0;

    if (voltage >= SENSOR_AIR_VOLTAGE_MV) {
        percentage = 0;
    } 

    else if (voltage <= SENSOR_WATER_VOLTAGE_MV) {
        percentage = 100;
    } 

    else {
        percentage = (SENSOR_AIR_VOLTAGE_MV - voltage) * 100 / (SENSOR_AIR_VOLTAGE_MV - SENSOR_WATER_VOLTAGE_MV);
    }

    *out_moisture = percentage;
    return ESP_OK;
}

void adc_sensor_deinit(void)
{
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle));
#endif
    }
}