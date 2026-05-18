#ifndef __FREERTOS_H
#define __FREERTOS_H

#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include "nvs_flash.h"
#include "SHT30.h"
#include "bh1750.h"
#include "adc_config.h"
#include "esp_log.h"
#include "spilcd.h"

void SHT30_Task(void *param);
void SoildSenser_Task(void *param);
void BH1750_Task(void *param);
void FREERTOS_Task(void *param);
void LCD_Task(void *param);

#endif /* __FREERTOS_H */