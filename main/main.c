#include "nvs_flash.h" 
#include <stdio.h> 
#include "spilcd.h"
#include "FreeRTOS.h"

/** 
 * @brief       程序入口 
 * @param       无 
 * @retval      无 
 */ 
void app_main(void) 
{ 
    esp_err_t ret;
    ret = nvs_flash_init();     /* 初始化NVS */ 
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    { 
        ESP_ERROR_CHECK(nvs_flash_erase()); 
        ESP_ERROR_CHECK(nvs_flash_init()); 
    } 
    
    sht30_init();
    adc_sensor_init();
    bh1750_init();
    spilcd_init();
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待传感器和 LCD 初始化完成

    xTaskCreate(FREERTOS_Task, "FREERTOS_Task", 4096, NULL, 5, NULL);
    // spilcd_fill(0, 0, 100, 100, RED); 

}