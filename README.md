# ESP32-P4 Plant AI Interactive Monitor

基于 ESP32-P4 的植物健康监测与拟态交互工程，当前工程在原始 `28_mipicamera` 示例基础上，集成了摄像头、植物环境传感、触摸交互、毫米波存在检测、LCD 状态展示与串口命令交互能力。

本工程面向一个桌面植物摆件 / 植物伴侣方向的嵌入式原型，核心目标包括：

- 采集植物周边环境数据
- 提供植物触摸与人体靠近感知
- 通过屏幕展示植物状态、情绪和护理建议
- 通过串口接收上位机视觉分析结果或导出摄像头图像

## 1. 项目概览

当前工程名称：

- `28_mipicamera`

当前软件平台：

- 芯片：`ESP32-P4`
- 框架：`ESP-IDF v5.4.4`
- Flash：`16 MB`
- 控制台输出：`USB Serial JTAG`

主要依赖：

- `espressif/esp_video`
- `espressif/esp_cam_sensor`
- `espressif/esp_sccb_intf`

## 2. 已实现功能

### 2.1 摄像头

- 支持 `OV5645` MIPI CSI 摄像头初始化
- 支持连续采集视频帧
- 支持通过串口命令导出缩放后的 RGB565 图像
- 支持 `SNAP` / `PREVIEW` 串口命令

### 2.2 显示与交互界面

- 支持 LCD 状态界面显示
- 支持植物健康状态、情绪、建议、触摸状态展示
- 支持屏幕测试模式
- 支持串口 `RESULT ...` 命令直接刷新界面
- 支持 `PATTERN` 命令显示测试图案

### 2.3 植物环境传感

- 土壤湿度采集
- 空气温湿度采集
- 光照强度采集
- 周期性传感器轮询
- 根据采样结果计算状态并刷新界面

### 2.4 触摸感知

工程当前存在两套触摸相关逻辑：

1. `plant_touch`

- 基于 `TTP223` 数字触摸模块
- 检测叶片或外部数字触摸输入
- 用于驱动界面中的被触摸状态

2. `touch_sensor`

- 基于 ESP32-P4 原生 `Touch Sensor` 外设
- 固定使用 `GPIO2`
- 支持基线校准、滑动平均、动态基线、防抖判断
- 提供 `touch_sensor_isTouched()` 等接口给其他模块调用

### 2.5 毫米波人体检测

- 已接入 `LD2412`
- 当前软件使用 `OUT` 数字输出方式
- 通过 GPIO 电平判断是否检测到人体
- 提供 `mmwave_isHumanDetected()` 查询接口

### 2.6 串口命令交互

支持以下串口命令：

- `SNAP`
  - 导出一帧缩放图像
- `PREVIEW`
  - 导出一帧更小尺寸的预览图像
- `PATTERN`
  - 显示测试图案
- `RESULT ...`
  - 直接向显示模块发送状态结果
- `ANALYZE ...`
  - 使用植物状态引擎进行分析
- `VISION ...`
  - 输入视觉分类结果参与分析
- `TOUCH ...`
  - 模拟带触摸参与一次状态分析

## 3. 工程目录结构

```text
camera_mipi_headless
├─ components
│  ├─ BSP
│  │  ├─ BH1750
│  │  ├─ LCD
│  │  ├─ LED
│  │  ├─ MMWAVE
│  │  ├─ MYIIC
│  │  ├─ PLANT_I2C
│  │  ├─ SHT30
│  │  ├─ SOFTIIC
│  │  ├─ SOILDSENCER
│  │  └─ TOUCHSENCER
│  └─ Middlewares
├─ main
│  ├─ APP
│  │  ├─ MIPI_CAM
│  │  ├─ PLANT_DISPLAY
│  │  ├─ PLANT_ENGINE
│  │  ├─ PLANT_SENSOR
│  │  └─ PLANT_TOUCH
│  └─ main.c
├─ sdkconfig
├─ partitions-16MiB.csv
└─ README.md
```

模块职责说明：

- `main/main.c`
  - 应用入口，初始化各模块
- `main/APP/MIPI_CAM`
  - 摄像头初始化、采帧、串口导图
- `main/APP/PLANT_DISPLAY`
  - LCD 绘制与植物状态界面
- `main/APP/PLANT_ENGINE`
  - 状态分析逻辑，将环境与触摸输入转成状态/情绪/建议
- `main/APP/PLANT_SENSOR`
  - 温湿度 / 土壤 / 光照读取与状态更新
- `main/APP/PLANT_TOUCH`
  - TTP223 数字触摸输入
- `components/BSP/MMWAVE`
  - LD2412 毫米波人体检测
- `components/BSP/TOUCHSENCER`
  - ESP32 原生 Touch 外设电容检测

## 4. 当前引脚映射

以下内容依据当前工程代码整理，表示当前软件定义。

### 4.1 基础外设

| 模块 | 功能 | GPIO / 外设 | 说明 |
| --- | --- | --- | --- |
| LED | 板载 LED0 | `GPIO51` | 状态指示 |
| 控制台 | 日志输出 | `USB Serial JTAG` | 非普通 UART 控制台 |

### 4.2 摄像头相关

| 模块 | 功能 | GPIO / 外设 | 说明 |
| --- | --- | --- | --- |
| MIPI 摄像头 | OV5645 SCCB SCL | `GPIO34` | 来自 `CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN` |
| MIPI 摄像头 | OV5645 SCCB SDA | `GPIO31` | 来自 `CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN` |
| MIPI 摄像头 | SCCB I2C 端口 | `I2C0` | 来自 `CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT` |
| MIPI 摄像头 | 图像接口 | `MIPI CSI` | 走板载 / 模组连接器，不是普通 GPIO 并口 |

### 4.3 LCD 相关

#### RGB LCD 识别与控制

| 功能 | GPIO |
| --- | --- |
| LCD ID1 | `GPIO14` |
| LCD ID2 | `GPIO8` |
| LCD ID3 | `GPIO3` |
| 背光 | `GPIO53` |
| 复位 | `GPIO52` |

#### RGB LCD 数据与时序

| 功能 | GPIO |
| --- | --- |
| DE | `GPIO22` |
| PCLK | `GPIO20` |
| R3 | `GPIO18` |
| R4 | `GPIO17` |
| R5 | `GPIO16` |
| R6 | `GPIO15` |
| R7 | `GPIO14` |
| G2 | `GPIO13` |
| G3 | `GPIO12` |
| G4 | `GPIO11` |
| G5 | `GPIO10` |
| G6 | `GPIO9` |
| G7 | `GPIO8` |
| B3 | `GPIO7` |
| B4 | `GPIO6` |
| B5 | `GPIO5` |
| B6 | `GPIO4` |
| B7 | `GPIO3` |

说明：

- `VSYNC` / `HSYNC` 当前为 `GPIO_NUM_NC`
- LCD 显示逻辑在工程中已启用

### 4.4 传感器与植物交互

| 模块 | 功能 | GPIO / 外设 | 说明 |
| --- | --- | --- | --- |
| TTP223 触摸 | 数字触摸输入 | `GPIO48` | `plant_touch` 使用 |
| 原生电容触摸 | Touch 输入 | `GPIO2` | `touch_sensor` 固定使用 |
| 土壤湿度 | ADC 输入 | `GPIO49` | `ADC2 Channel 0` |
| 植物 I2C | SCL | `GPIO28` | `plant_i2c` |
| 植物 I2C | SDA | `GPIO36` | `plant_i2c` |
| 植物 I2C | I2C 端口 | `I2C1` | 用于 SHT30 / 其他植物传感器 |
| 软件 I2C | SCL | `GPIO30` | `BH1750` 当前经软件 I2C |
| 软件 I2C | SDA | `GPIO29` | `BH1750` 当前经软件 I2C |
| 通用 I2C0 | SCL | `GPIO32` | `myiic` 使用 |
| 通用 I2C0 | SDA | `GPIO33` | `myiic` 使用 |

### 4.5 毫米波雷达

| 模块 | 功能 | GPIO / 外设 | 说明 |
| --- | --- | --- | --- |
| LD2412 | `OUT` 输入 | `GPIO36` | 当前软件使用数字 OUT 方式 |
| LD2412 | `TX/RX` | 未启用 | 当前代码未使用 UART 模式 |

### 4.6 当前引脚注意事项

当前代码中有一个需要特别注意的地方：

- `GPIO36` 同时被定义为
  - `plant_i2c` 的 `SDA`
  - `LD2412` 的 `OUT`

这表示从当前软件定义看，这两个功能在硬件上存在复用冲突风险。如果后续实际接线同时启用这两项，需要重新规划硬件连接或重新调整软件引脚分配。

## 5. 接线图说明表

下面这张表更适合实际接线、联调和画原理图时使用。

### 5.1 核心外设接线表

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| 板载 LED | LED0 | `GPIO51` | LED | 板载红灯，当前可被毫米波逻辑用于状态指示 |
| 控制台 | USB 调试 | `USB Serial JTAG` | PC USB 口 | 用于日志输出、串口命令输入 |
| TTP223 触摸模块 | SIG | `GPIO48` | TTP223 OUT | 高电平有效 |
| TTP223 触摸模块 | VCC | `3V3` | TTP223 VCC | 建议 3.3V 供电 |
| TTP223 触摸模块 | GND | `GND` | TTP223 GND | 共地 |
| 原生电容触摸板 | 信号端 | `GPIO2` | 铜箔电容板 | 当前固定使用 ESP32 原生 Touch 通道 |
| 土壤湿度传感器 | AO | `GPIO49` | 传感器模拟输出 | ADC 采样输入 |
| 土壤湿度传感器 | VCC | `3V3` 或模块额定电压 | 传感器 VCC | 取决于传感器模块规格 |
| 土壤湿度传感器 | GND | `GND` | 传感器 GND | 共地 |
| LD2412 毫米波 | OUT | `GPIO36` | LD2412 OUT | 当前软件只使用 OUT 数字输出 |
| LD2412 毫米波 | 5V | `5V` | LD2412 5V | 按模块规格供电 |
| LD2412 毫米波 | GND | `GND` | LD2412 GND | 共地 |

### 5.2 植物环境传感器接线表

#### 植物 I2C 总线

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| 植物 I2C 总线 | SCL | `GPIO28` | SCL | `I2C1` |
| 植物 I2C 总线 | SDA | `GPIO36` | SDA | `I2C1` |
| 植物 I2C 总线 | VCC | `3V3` | 传感器 VCC | 传感器侧供电 |
| 植物 I2C 总线 | GND | `GND` | 传感器 GND | 共地 |

适用外设：

- `SHT30` 温湿度传感器
- 其他挂载到 `plant_i2c` 的 I2C 设备

#### 软件 I2C 总线

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| BH1750 光照传感器 | SCL | `GPIO30` | BH1750 SCL | 软件 I2C |
| BH1750 光照传感器 | SDA | `GPIO29` | BH1750 SDA | 软件 I2C |
| BH1750 光照传感器 | VCC | `3V3` | BH1750 VCC | 常见模块支持 3.3V |
| BH1750 光照传感器 | GND | `GND` | BH1750 GND | 共地 |

#### 通用 I2C0 总线

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| 通用 I2C0 | SCL | `GPIO32` | I2C SCL | `myiic` 使用 |
| 通用 I2C0 | SDA | `GPIO33` | I2C SDA | `myiic` 使用 |

### 5.3 摄像头接线说明表

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| OV5645 MIPI 摄像头 | SCCB SCL | `GPIO34` | 摄像头 SCL | 控制总线，`I2C0` |
| OV5645 MIPI 摄像头 | SCCB SDA | `GPIO31` | 摄像头 SDA | 控制总线，`I2C0` |
| OV5645 MIPI 摄像头 | MIPI CSI Lane/CLK | MIPI CSI 接口 | 摄像头 MIPI 接口 | 通过板载座子连接，不是普通跳线 GPIO |
| OV5645 MIPI 摄像头 | 电源 | 按模组接口定义 | 摄像头电源端 | 由模组或开发板接口供电 |
| OV5645 MIPI 摄像头 | 地 | `GND` | 摄像头 GND | 共地 |

### 5.4 LCD 接线说明表

#### LCD 控制与识别

| 模块 | 信号 | ESP32-P4 侧 | 外设侧 | 备注 |
| --- | --- | --- | --- | --- |
| LCD | ID1 | `GPIO14` | LCD ID1 | 屏幕识别 |
| LCD | ID2 | `GPIO8` | LCD ID2 | 屏幕识别 |
| LCD | ID3 | `GPIO3` | LCD ID3 | 屏幕识别 |
| LCD | BL | `GPIO53` | LCD 背光 | 背光控制 |
| LCD | RST | `GPIO52` | LCD 复位 | 复位控制 |

#### RGB 并行数据信号

| 信号 | ESP32-P4 引脚 | 说明 |
| --- | --- | --- |
| DE | `GPIO22` | 数据使能 |
| PCLK | `GPIO20` | 像素时钟 |
| R3 | `GPIO18` | 红色数据 |
| R4 | `GPIO17` | 红色数据 |
| R5 | `GPIO16` | 红色数据 |
| R6 | `GPIO15` | 红色数据 |
| R7 | `GPIO14` | 红色数据 |
| G2 | `GPIO13` | 绿色数据 |
| G3 | `GPIO12` | 绿色数据 |
| G4 | `GPIO11` | 绿色数据 |
| G5 | `GPIO10` | 绿色数据 |
| G6 | `GPIO9` | 绿色数据 |
| G7 | `GPIO8` | 绿色数据 |
| B3 | `GPIO7` | 蓝色数据 |
| B4 | `GPIO6` | 蓝色数据 |
| B5 | `GPIO5` | 蓝色数据 |
| B6 | `GPIO4` | 蓝色数据 |
| B7 | `GPIO3` | 蓝色数据 |

### 5.5 LD2412 专项接线表

当前软件只使用 `OUT` 检测方式，未启用 `TX/RX` 串口模式。

| LD2412 引脚 | 接到哪里 | 当前是否使用 | 说明 |
| --- | --- | --- | --- |
| OUT | `GPIO36` | 是 | 人体存在数字输出 |
| TX | 悬空 / 预留 | 否 | 当前代码未启用 UART 解析 |
| RX | 悬空 / 预留 | 否 | 当前代码未启用 UART 解析 |
| 5V | `5V` | 是 | 模块供电 |
| 3V3 | 不接 | 否 | 当前按 5V 供电理解 |
| GND | `GND` | 是 | 共地 |

### 5.6 接线建议

- 所有传感器和模块必须共地。
- `USB Serial JTAG` 仅负责日志和命令通信，不等于额外可用 UART。
- 若同时使用 `plant_i2c` 和 `LD2412 OUT`，请优先处理 `GPIO36` 冲突。
- 若原生电容触摸板走线较长，建议缩短信号线，避免外部干扰。
- 若光照、温湿度、土壤传感器读数异常，优先检查供电、电平和共地。

## 6. 运行流程

系统启动后，大致流程如下：

1. 初始化 NVS
2. 初始化 `USB Serial JTAG` 控制台
3. 初始化 LED
4. 初始化 `myiic`
5. 初始化毫米波模块
6. 初始化显示模块
7. 初始化 `plant_touch` 并启动触摸任务
8. 初始化植物传感器并启动传感器任务
9. 初始化原生电容触摸模块并启动周期更新定时器
10. 初始化 MIPI 摄像头
11. 主循环保持运行

## 7. 状态分析逻辑

`PLANT_ENGINE` 模块支持把环境数据和触摸输入映射为更高层的植物状态：

- `status`
  - `HEALTHY`
  - `LOCAL_ISSUE`
  - `SEVERE`
  - `UNKNOWN`
- `mood`
  - 例如 `HAPPY`、`DRY`、`GLOOMY`、`COMFORTED`
- `advice`
  - 例如 `WATER`、`SHADE`、`COOL`、`WATCH`

输入来源包括：

- 土壤湿度
- 温度
- 空气湿度
- 光照
- 触摸状态
- 串口输入的视觉分析结果

## 8. 串口使用说明

当前工程的日志和命令输入使用：

- `USB Serial JTAG`

不是普通 UART 控制台，因此调试时请确认串口监视器连接的是正确的 USB 端口。

### 8.1 常用命令

#### 图像导出

```text
SNAP
PREVIEW
```

#### 显示控制

```text
PATTERN
RESULT status=HEALTHY conf=95 mood=HAPPY health=92 advice=KEEP touch=0 soil=55 temp=25 hum=60 light=500
```

#### 状态分析

```text
ANALYZE soil=30 temp=28 hum=45 light=300 touch=0 conf=85
VISION status=LOCAL_ISSUE conf=80
TOUCH soil=45 temp=26 hum=55 light=500 conf=90
```

## 9. 构建与烧录

### 9.1 ESP-IDF 构建

```powershell
idf.py set-target esp32p4
idf.py build
```

### 9.2 烧录

```powershell
idf.py -p COMx flash
```

### 9.3 监视日志

```powershell
idf.py -p COMx monitor
```

如果你的环境已经正确导入 ESP-IDF，以上命令即可直接使用。

## 10. 关键日志参考

### 10.1 摄像头

```text
OV5645 CSI init succeeded, starting headless frame capture
photo export ready; send SNAP or PREVIEW over serial
frame=30 index=0 bytes=...
```

### 10.2 植物触摸

```text
plant_touch: TTP223 touch input ready: SIG=GPIO48 active=HIGH
plant_touch: touch GPIO48 level=0 active=0 stack_free=...
```

### 10.3 原生电容触摸

```text
[Touch] Touch GPIO selected: GPIO2 CH1
[Touch] touch_sensor_init success
[Touch] Baseline = ...
```

### 10.4 毫米波

```text
[MMWave] OUT input ready: GPIO36 active=HIGH
[MMWave] Human detected
[MMWave] Human lost
```

### 10.5 传感器

```text
SENSOR soil=... temp=... hum=... light=... valid=...
```

## 11. 当前已知说明

- 当前控制台走 `USB Serial JTAG`
- LCD 显示已启用
- `plant_touch` 使用 `GPIO48` 的 TTP223 数字触摸方案
- `touch_sensor` 使用 `GPIO2` 的 ESP32 原生 Touch 方案
- 毫米波当前使用 `LD2412 OUT` 数字输出方式
- `GPIO36` 在当前代码中存在复用冲突风险
- 如果外设未接好，传感器初始化阶段可能出现超时或探测失败日志

## 12. 适用场景

本工程适合用于：

- 植物陪伴类交互装置原型
- 植物健康可视化展示
- ESP32-P4 多外设融合演示
- 摄像头 + 传感器 + 屏幕 + 交互输入综合项目

## 13. 后续可扩展方向

- 接入真正的植物病害视觉模型推理结果
- 将毫米波与电容触摸做融合状态决策
- 增加 Wi-Fi / 云端数据同步
- 增加历史数据记录与趋势显示
- 增加更多植物种类的个性化规则

---

如果你正在接手这个工程，建议优先阅读：

1. `main/main.c`
2. `main/APP/PLANT_SENSOR/plant_sensor.c`
3. `main/APP/PLANT_DISPLAY/plant_display.c`
4. `main/APP/MIPI_CAM/mipi_cam.c`
5. `components/BSP/MMWAVE/mmwave.c`
6. `components/BSP/TOUCHSENCER/touch_sensor.c`
