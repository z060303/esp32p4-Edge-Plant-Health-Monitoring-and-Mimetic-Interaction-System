#include "soft_i2c.h"

// 内部辅助宏：设置引脚电平
#define SCL_H() gpio_set_level(SOFT_I2C_SCL_PIN, 1)
#define SCL_L() gpio_set_level(SOFT_I2C_SCL_PIN, 0)
#define SDA_H() gpio_set_level(SOFT_I2C_SDA_PIN, 1)
#define SDA_L() gpio_set_level(SOFT_I2C_SDA_PIN, 0)
#define SDA_READ() gpio_get_level(SOFT_I2C_SDA_PIN)

/**
 * @brief 切换SDA引脚模式
 */
static void sda_mode_output(void) {
    gpio_set_direction(SOFT_I2C_SDA_PIN, GPIO_MODE_OUTPUT_OD); // 使用开漏模式
}

static void sda_mode_input(void) {
    gpio_set_direction(SOFT_I2C_SDA_PIN, GPIO_MODE_INPUT);
}

void soft_i2c_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SOFT_I2C_SCL_PIN) | (1ULL << SOFT_I2C_SDA_PIN),
        .mode = GPIO_MODE_OUTPUT_OD, // 开漏输出，方便上拉电平
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 初始状态：总线空闲（SCL/SDA 均为高）
    SDA_H();
    SCL_H();
}

void soft_i2c_start(void) {
    sda_mode_output();
    SDA_H();
    SCL_H();
    I2C_DELAY_US();
    SDA_L(); // SCL高电平时，SDA由高变低
    I2C_DELAY_US();
    SCL_L(); // 钳住总线，准备发送或接收
}

void soft_i2c_stop(void) {
    sda_mode_output();
    SCL_L();
    SDA_L();
    I2C_DELAY_US();
    SCL_H();
    I2C_DELAY_US();
    SDA_H(); // SCL高电平时，SDA由低变高
    I2C_DELAY_US();
}

uint8_t soft_i2c_write_byte(uint8_t data) {
    sda_mode_output();
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) SDA_H();
        else SDA_L();
        data <<= 1;
        I2C_DELAY_US();
        SCL_H();
        I2C_DELAY_US();
        SCL_L();
    }
    
    // 等待 ACK
    sda_mode_input();
    SCL_H();
    I2C_DELAY_US();
    uint8_t ack = SDA_READ();
    SCL_L();
    return ack;
}

uint8_t soft_i2c_read_byte(uint8_t ack) {
    uint8_t data = 0;
    sda_mode_input();
    for (int i = 0; i < 8; i++) {
        SCL_H();
        I2C_DELAY_US();
        data <<= 1;
        if (SDA_READ()) data |= 0x01;
        SCL_L();
        I2C_DELAY_US();
    }
    
    // 发送 ACK / NACK
    sda_mode_output();
    if (ack) SDA_L();
    else SDA_H();
    SCL_H();
    I2C_DELAY_US();
    SCL_L();
    return data;
}