#include "FreeRTOS.h"

typedef struct {
    float temperature;  // 空气温度
    float humidity;     // 空气湿度
    int moisture;       // 土壤湿度(%)
    float lux;          // 光照强度
} sensor_data_t;

static QueueHandle_t sensor_data_queue = NULL;
static const char *TAG = "SENSOR_APP";

static sensor_data_t data = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .moisture = 0,
    .lux = 0.0f
};

/** 
 * @brief       SHT30 任务函数 
 * @param       param : 传入参数(未用到) 
 * @目标        每 1 秒读取一次空气温湿度
 */
void SHT30_Task(void *param)
{
    const TickType_t xDelay = pdMS_TO_TICKS(500);
    while (1)
    {
        sht30_read_data(&data.temperature, &data.humidity);
        ESP_LOGI(TAG, "空气温湿度: Temp=%.2f, Hum=%.2f", data.temperature, data.humidity);

        xQueueOverwrite(sensor_data_queue, &data);
        vTaskDelay(xDelay);
    }
}

/**
 * @brief       土壤湿度传感器 任务函数 
 * @param       param : 传入参数(未用到) 
 * @目标        每 500 毫秒读取一次土壤湿度传感器数据，并转换为百分比
 */
void SoildSenser_Task(void *param)
{
    const TickType_t xDelay = pdMS_TO_TICKS(500);
    while (1)
    {
        // 读取土壤湿度传感器数据
        get_soil_moisture_percent(&data.moisture);
        ESP_LOGI(TAG, "土壤湿度: %d%%", data.moisture);

        xQueueOverwrite(sensor_data_queue, &data);

        vTaskDelay(xDelay);
    }
}
/**
 * @brief       BH1750 任务函数
 * @param       param : 传入参数(未用到)
 * @目标        每 500 毫秒读取一次 BH1750 光照传感器数据
 */
void BH1750_Task(void *param)
{
    const TickType_t xDelay = pdMS_TO_TICKS(500);
    while (1)
    {
        // 读取 BH1750 光照传感器数据
        bh1750_read_light(&data.lux);
        ESP_LOGI(TAG, "光照强度: %.2f lux", data.lux);

        xQueueOverwrite(sensor_data_queue, &data);

        vTaskDelay(xDelay);
    }
}

void LCD_Task(void *param)
{
    sensor_data_t current_data;
    char buf[64];

    // 只在启动时清屏一次
    spilcd_clear(BLACK);

    while (1)
    {
        // 等待数据（超时100ms，不卡死）
        if(xQueueReceive(sensor_data_queue, &current_data, pdMS_TO_TICKS(100)) == pdPASS)
        {
            // 显示温度
            sprintf(buf, "Temp: %.2f C", current_data.temperature);
            spilcd_show_string(10, 10, 240, 24, 16, buf, RED);

            // 显示湿度
            sprintf(buf, "Humi: %.2f %%", current_data.humidity);
            spilcd_show_string(10, 35, 240, 24, 16, buf, GREEN);

            // 显示土壤湿度
            sprintf(buf, "Soil: %d %%", current_data.moisture);
            spilcd_show_string(10, 60, 240, 24, 16, buf, BLUE);

            // 显示光照
            sprintf(buf, "Lux: %.2f", current_data.lux);
            spilcd_show_string(10, 85, 240, 24, 16, buf, YELLOW);
        }

        // 固定 200ms 刷新一次
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void FREERTOS_Task(void *param)
{
    sensor_data_queue = xQueueCreate(1, sizeof(sensor_data_t));

    // 创建 SHT30 任务
    xTaskCreate(SHT30_Task, "SHT30_Task", 4096, NULL, 5, NULL);
    
    // 创建土壤湿度传感器任务
    xTaskCreate(SoildSenser_Task, "SoildSenser_Task", 4096, NULL, 5, NULL);
    
    // 创建 BH1750 任务
    xTaskCreate(BH1750_Task, "BH1750_Task", 4096, NULL, 5, NULL);

    // 创建 LCD 显示任务
    xTaskCreate(LCD_Task, "LCD_Task", 16384, NULL, 6, NULL);

    // 删除当前任务，释放资源
    vTaskDelete(NULL);
}


