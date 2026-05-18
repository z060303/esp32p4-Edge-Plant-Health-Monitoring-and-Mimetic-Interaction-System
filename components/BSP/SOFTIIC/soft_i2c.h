#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#include "driver/gpio.h"
#include "esp_rom_sys.h"

// 引脚定义
#define SOFT_I2C_SCL_PIN    GPIO_NUM_10
#define SOFT_I2C_SDA_PIN    GPIO_NUM_9

// 延时控制：约 10us 对应 100KHz (由于逻辑损耗，实际频率会稍低)
#define I2C_DELAY_US()      esp_rom_delay_us(5)

/**
 * @brief 初始化软件I2C引脚
 */
void soft_i2c_init(void);

/**
 * @brief I2C 起始信号
 */
void soft_i2c_start(void);

/**
 * @brief I2C 停止信号
 */
void soft_i2c_stop(void);

/**
 * @brief 发送一个字节并检查应答
 * @return 0: 收到应答 (ACK), 1: 无应答 (NACK)
 */
uint8_t soft_i2c_write_byte(uint8_t data);

/**
 * @brief 读取一个字节
 * @param ack 1: 发送 ACK, 0: 发送 NACK
 */
uint8_t soft_i2c_read_byte(uint8_t ack);

#endif