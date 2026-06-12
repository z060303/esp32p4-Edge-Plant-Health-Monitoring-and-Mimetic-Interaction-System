#ifndef ADC_CONFIG_H
#define ADC_CONFIG_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

// 宏定义：使用的 ADC 单元和通道
#define EXAMPLE_ADC_UNIT        ADC_UNIT_2
#define EXAMPLE_ADC_CHANNEL     ADC_CHANNEL_0   // ESP32-P4 GPIO49
#define EXAMPLE_ADC_ATTEN       ADC_ATTEN_DB_12 // 12dB 衰减，量程更宽 (约 0 ~ 3.3V)

/**
 * @brief 初始化 ADC 单次采样模式并配置校准
 * @return esp_err_t ESP_OK 表示成功
 */
esp_err_t adc_sensor_init(void);

/**
 * @brief 读取 ADC 原始数据和转换后的电压值
 * 
 * @param out_raw 存储原始数据的指针
 * @param out_voltage 存储毫伏(mV)电压值的指针
 * @return esp_err_t ESP_OK 表示成功
 */
esp_err_t adc_sensor_read(int *out_raw, int *out_voltage);

/**
 * @brief 读取传感器并转换为土壤湿度百分比 (0-100%)
 * 
 * @param out_moisture 存储湿度百分比的指针
 * @return esp_err_t ESP_OK 表示成功
 */
esp_err_t get_soil_moisture_percent(int *out_moisture);

/**
 * @brief 释放 ADC 资源（如果需要退出时调用）
 */
void adc_sensor_deinit(void);

#endif // ADC_CONFIG_H
