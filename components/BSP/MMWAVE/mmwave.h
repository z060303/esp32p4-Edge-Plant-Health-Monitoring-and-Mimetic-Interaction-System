#pragma once

#include <stdbool.h>
#include "led.h"
#include "esp_err.h"

/* 初始化毫米波 OUT 输入引脚 */
esp_err_t mmwave_init(void);

/* 轮询毫米波 OUT 电平并更新人体状态 */
void mmwave_update(void);

/* 获取当前人体检测状态 */
bool mmwave_isHumanDetected(void);

/* 直接读取毫米波 OUT 引脚状态 */
bool mmwave_gpio_detect(void);
