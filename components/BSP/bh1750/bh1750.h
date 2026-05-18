#ifndef __BH1750_H
#define __BH1750_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * 注意：软件I2C不再需要 BH1750_I2C_PORT 定义，
 * 引脚定义已迁移至 soft_i2c.h 或直接在此定义。
 * 建议在此保留硬件参数，方便 soft_i2c 使用。
 */

/* BH1750 I2C 从机地址 (ADDR引脚接地为0x23, 接高电平为0x5C) */
#define BH1750_ADDR             0x23

/* BH1750 指令集 */
#define BH1750_POWER_DOWN       0x00    /*!< 关闭电源，不接受测量 */
#define BH1750_POWER_ON         0x01    /*!< 等待测量指令 */
#define BH1750_RESET            0x07    /*!< 重置数据寄存器 (仅在Power On状态有效) */
#define BH1750_CONT_H_MODE      0x10    /*!< 连续高分辨率模式 (1lx精度, 采样时间约120ms) */
#define BH1750_CONT_H_MODE2     0x11    /*!< 连续高分辨率模式2 (0.5lx精度, 采样时间约120ms) */
#define BH1750_CONT_L_MODE      0x13    /*!< 连续低分辨率模式 (4lx精度, 采样时间约16ms) */
#define BH1750_ONCE_H_MODE      0x20    /*!< 单次高分辨率模式 (测量后自动掉电) */

/**
 * @brief 初始化BH1750传感器
 * @note  内部会调用 soft_i2c_init() 配置 GPIO 9 和 10
 * @return
 *     - ESP_OK: 初始化成功
 *     - ESP_FAIL: 器件未响应
 */
esp_err_t bh1750_init(void);

/**
 * @brief 读取当前光照强度值
 * 
 * @param lux 指向存储结果的浮点数指针 (单位: Lux)
 * @return
 *     - ESP_OK: 读取成功
 *     - ESP_FAIL: I2C 通讯错误或器件未响应
 */
esp_err_t bh1750_read_light(float *lux);

#ifdef __cplusplus
}
#endif

#endif /* __BH1750_H */