# Plant AI 第一版交接记录

## 第一版目标

第一版定义为：电脑端 AI 识别 + ESP32-P4 板端传感器采集 + RGB 中文界面 + 触摸心情反馈。

本版不要求 AI 完全离线跑在 ESP32-P4 上。模型上板、ESP-DL 量化和端侧推理作为第二版目标。

## 当前工程

- 板端主工程：`camera_mipi_headless`
- RGB 屏 UI：`camera_mipi_headless/main/APP/PLANT_DISPLAY`
- 环境传感器：`camera_mipi_headless/main/APP/PLANT_SENSOR`
- 触摸输入：`camera_mipi_headless/main/APP/PLANT_TOUCH`
- 叶片状态融合引擎：`camera_mipi_headless/main/APP/PLANT_ENGINE`
- 当前电脑端模型：`yolo11n-cls.pt` 以及 `models/yolo_plant_status/` 下训练权重

## 最终接线

| 模块 | 信号 | ESP32-P4 引脚 | 说明 |
|---|---|---:|---|
| RGB LCD | RGB 数据/时钟/DE | GPIO3-GPIO18、GPIO20、GPIO22 | 屏幕专用，不要接其它模块 |
| SHT30 | SCL | GPIO28 | 硬件 I2C，地址通常为 0x44 |
| SHT30 | SDA | GPIO36 | 硬件 I2C |
| BH1750 | SCL | GPIO30 | 软件 I2C，代码探测 0x23/0x5C |
| BH1750 | SDA | GPIO29 | 软件 I2C |
| 土壤湿度 | AO | GPIO49 | ADC2 CH0，模拟输入 |
| TTP223 触摸 | SIG/OUT | GPIO48 | 触摸高电平有效 |
| TTP223 触摸 | VCC | 3.3V | 不接 5V |
| TTP223 触摸 | GND | GND | 与开发板共地 |

注意：用户排针上看似空闲的 GPIO3-GPIO18、GPIO20、GPIO22，在当前 RGB 屏固件中已经被 LCD 占用，不能作为触摸或传感器输入。

## 当前参数

### 土壤湿度

文件：`camera_mipi_headless/components/BSP/SOILDSENCER/adc_config.c`

```c
SENSOR_AIR_VOLTAGE_MV = 3300
SENSOR_WATER_VOLTAGE_MV = 2200
```

当前室内盆土读数约为 `75%-81%`，按中等偏湿处理，不再判定为极干。

### 光照阈值

文件：

- `camera_mipi_headless/main/APP/PLANT_SENSOR/plant_sensor.c`
- `camera_mipi_headless/main/APP/PLANT_ENGINE/plant_engine.c`

```c
LIGHT_DARK_THRESHOLD = 25.0f
```

室内约 `33 lx` 时不再提示光照不足，状态为健康。

### 触摸响应

文件：`camera_mipi_headless/main/APP/PLANT_TOUCH/plant_touch.c`

```c
PLANT_TOUCH_GPIO = GPIO_NUM_48
PLANT_TOUCH_ACTIVE_LEVEL = 1
PLANT_TOUCH_HOLD_MS = 4000
```

触摸后 UI 心情区域显示“开心 / 被安抚”，并保持约 4 秒。

## 第一版已完成

- RGB 屏中文植物主题 UI
- 传感器实时刷新到屏幕
- SHT30 温湿度读取
- BH1750 光照读取
- 土壤湿度 ADC 读取和室内标定
- TTP223 触摸输入和心情反馈
- 电脑端 YOLO11n-cls 叶片健康模型准备
- 室内光照阈值调低，避免误报警

## 验收步骤

1. 上电后确认 RGB 屏显示中文植物界面。
2. 串口日志应出现传感器读数，例如：

```text
SENSOR soil=76 temp=25.4 hum=64.2 light=33.3 valid=111
UI status=HEALTHY mood=HAPPY advice=KEEP touch=0
```

3. 触摸 TTP223，串口应出现：

```text
touch GPIO48 level=1 active=1
```

4. 屏幕心情区域应变为“开心 / 被安抚”。
5. 电脑端 AI 完成叶片分类后，通过串口命令把状态回传到板端 UI。

## 第二版计划

- 将 YOLO11n-cls 导出 ONNX
- 用 ESP-PPQ/ESP-DL 量化为 `.espdl`
- 板端完成 224x224 图像缩放、颜色转换和推理
- 优化摄像头缓存、LCD 缓存和推理内存
- 将叶片健康结果进一步融合进心情系统
