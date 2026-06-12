# ESP32-P4 外设项目接线解读

来源压缩包：
`C:/Users/猪/Desktop/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-main.zip`

解压保存位置：
`C:/Users/猪/Documents/Codex/2026-05-01/esp32-p4-ai-ai-esp32-p4/plant_ai/external_peripheral_project/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-main`

## 项目用途

这是一个 ESP-IDF v5.4.4 的 ESP32-P4 外设工程，当前重点是：

- SHT30 读取空气温湿度
- BH1750 读取光照强度
- 土壤湿度传感器通过 ADC 读取
- SPI LCD 显示温度、湿度、土壤湿度和光照
- FreeRTOS 多任务并行采集和显示

摄像头和端侧 AI 在 README 里还是 TODO，当前工程没有完整摄像头接入代码。

## 核心接线表

| 模块 | 信号 | ESP32-P4 GPIO | 代码来源 | 备注 |
|---|---:|---:|---|---|
| SHT30 | SDA | GPIO7 | `components/BSP/SHT30/sht30.h` | 硬件 I2C，I2C_NUM_1 |
| SHT30 | SCL | GPIO8 | `components/BSP/SHT30/sht30.h` | 地址 `0x44`，100 kHz |
| BH1750 | SDA | GPIO9 | `components/BSP/SOFTIIC/soft_i2c.h` | 软件 I2C |
| BH1750 | SCL | GPIO10 | `components/BSP/SOFTIIC/soft_i2c.h` | 地址 `0x23`，ADDR 接地 |
| 土壤湿度传感器 | AO/Analog | GPIO16 | `components/BSP/SOILDSENCER/adc_config.h` | ADC1_CH0，12 dB 衰减 |
| SPI LCD | CS | GPIO28 | `components/BSP/SPILCD/spilcd.h` | SPI2_HOST |
| SPI LCD | MOSI/SDA | GPIO29 | `components/BSP/SPILCD/spilcd.h` | 无 MISO |
| SPI LCD | SCLK/SCL | GPIO30 | `components/BSP/SPILCD/spilcd.h` | 20 MHz |
| SPI LCD | DC | GPIO31 | `components/BSP/SPILCD/spilcd.h` | ST7789 |
| 板载/状态 LED | LED0 | GPIO51 | `components/BSP/LED/led.h` | 当前 main 未主动初始化 LED |
| XL9555 | INT | GPIO36 | `components/BSP/XL9555/xl9555.h` | 扩展 IO 中断 |
| MYIIC/XL9555 总线 | SDA | GPIO33 | `components/BSP/MYIIC/myiic.h` | I2C_NUM_0，400 kHz |
| MYIIC/XL9555 总线 | SCL | GPIO32 | `components/BSP/MYIIC/myiic.h` | 供 XL9555 等扩展 IO 使用 |

## 电源和地线原则

- 所有外设必须和 ESP32-P4 共地：模块 GND 接开发板 GND。
- SHT30、BH1750 通常接 3.3V。
- 土壤湿度传感器优先接 3.3V，模拟输出进 GPIO16，避免超过 ADC 输入范围。
- LCD 供电按屏幕模块标注接 3.3V 或 5V；信号电平应为 3.3V。

## 当前代码启动流程

`main/main.c` 中 `app_main()` 顺序：

1. 初始化 NVS
2. `sht30_init()`
3. `adc_sensor_init()`
4. `bh1750_init()`
5. `spilcd_init()`
6. 创建 `FREERTOS_Task`

`main/APP/FreeRTOS/FreeRTOS.c` 中创建 4 个任务：

- `SHT30_Task`：每 500 ms 读空气温湿度
- `SoildSenser_Task`：每 500 ms 读土壤湿度
- `BH1750_Task`：每 500 ms 读光照
- `LCD_Task`：每 200 ms 刷新屏幕显示

## 注意事项

1. `components/BSP/bh1750/bh1750.c` 第 34 行看起来把 `soft_i2c_init();` 写在了 `//` 注释后面，可能导致 BH1750 的 GPIO9/GPIO10 没有初始化。实物联调前建议改成单独一行调用：

```c
soft_i2c_init();
```

2. SHT30 和 BH1750 分别用了两组 I2C：

- SHT30：硬件 I2C GPIO7/GPIO8
- BH1750：软件 I2C GPIO9/GPIO10

不要把两个模块都接到同一组线上，除非同步改代码。

3. 土壤湿度换算校准值在 `adc_config.c`：

```c
SENSOR_AIR_VOLTAGE_MV = 2740
SENSOR_WATER_VOLTAGE_MV = 1040
```

实际传感器差异很大，后续需要干燥/浸水重新校准。

4. README 里提到 Camera/Edge AI 仍是 TODO，当前外设工程主要是传感器和 LCD 显示，不是最终 AI 部署工程。
