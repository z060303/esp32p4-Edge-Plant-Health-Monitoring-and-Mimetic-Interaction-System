#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Touch 基线校准采样次数 */
#define TOUCH_SAMPLE_COUNT                  100U
/* Touch 滑动平均滤波窗口大小 */
#define TOUCH_FILTER_SIZE                   8U
/* Touch 触发百分比阈值 */
#define TOUCH_TRIGGER_RATIO                 0.01f
/* Touch 动态基线更新系数 */
#define TOUCH_BASELINE_UPDATE_ALPHA         0.005f
/* Touch 触摸/松开确认次数 */
#define TOUCH_CONFIRM_COUNT                 5U
/* Touch 调试开关，1 表示输出实时值，0 表示关闭 */
#define TOUCH_DEBUG                         1
/* Touch 连续读取失败多少次后打印一次告警 */
#define TOUCH_READ_FAIL_LOG_COUNT           25U
/* Touch 周期更新间隔，单位毫秒 */
#define TOUCH_UPDATE_PERIOD_MS              40U
/* Touch 优先尝试的 GPIO，引脚被占用时会自动切换 */
#define TOUCH_PREFERRED_GPIO                2
/* Touch 驱动使用的采样配置数量 */
#define TOUCH_DRIVER_SAMPLE_CFG_NUM         1U
/* Touch 驱动内部阈值，设置为极大值以避免影响本模块判断 */
#define TOUCH_DRIVER_ACTIVE_THRESH          0xFFFFUL

/* 初始化盆底电容板检测模块 */
esp_err_t touch_sensor_init(void);

/* 周期更新盆底电容板检测状态 */
void touch_sensor_update(void);

/* 获取当前是否检测到触摸 */
bool touch_sensor_isTouched(void);

/* 获取当前滤波后的 Touch 实时值 */
uint32_t touch_sensor_getValue(void);

/* 获取当前动态基线值 */
uint32_t touch_sensor_getBaseline(void);
