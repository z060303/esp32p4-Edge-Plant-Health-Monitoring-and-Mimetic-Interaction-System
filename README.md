🌱 基于 ESP32-P4 的智能环境监测与端侧 AI 系统
📖 项目简介
本项目基于乐鑫最新一代高性能双核微控制器 ESP32-P4 (400MHz) 开发，旨在构建一个集成了多传感器数据采集、精美图形用户界面（GUI）以及端侧机器视觉（Edge AI）的智能系统。

项目经历了从“裸机前后台轮询”到“FreeRTOS 多任务并行”的架构升级。在保障各类 I2C/ADC 传感器稳定读取的同时，通过 SPI DMA 驱动屏幕，并在高帧率下运行 LVGL 图形库。
/*未来将引入基于硬件 PPA 加速的 YOLO 微型目标检测模型。*/

🛠️ 硬件架构
主控芯片: ESP32-P4 (双核 RISC-V, 400MHz, 內置 PPA 加速器)

显示设备: SPI LCD 屏幕 (ST7789/ILI9341等，RGB565 16-bit)

传感器组:

SHT30 (I2C): 环境温湿度监测

BH1750 (I2C): 环境光照强度监测

土壤湿度传感器 (ADC1_CH0 / GPIO16): 电容式/电阻式土壤湿度采集

外设扩展: 独立按键 (KEY)、状态指示灯 (LED)

🧩 软件与系统架构
开发框架: ESP-IDF v5.4.4

实时操作系统: FreeRTOS (采用双核任务绑定与互斥锁同步)

图形库: LVGL v8.4 (局部双缓冲 + DMA 内存传输)

核心任务分配:

Core 0: 运行 FreeRTOS 系统心跳、低频传感器数据采集任务（ADC/I2C），以及 LVGL 界面刷新 (lv_timer_handler)。

Core 1: 预留给端侧ai                    /*预留给高负载运算，如摄像头数据捕获与 YOLO 目标检测模型推理。*/

📂 目录结构
Plaintext
📦 10_spilcd_LVGL
 ┣ 📂 components
 ┃ ┗ 📂 BSP                 # 板级支持包 (底层硬件驱动)
 ┃   ┣ 📂 BH1750            # 光照传感器驱动
 ┃   ┣ 📂 KEY               # 按键驱动
 ┃   ┣ 📂 LCD               # 屏幕底层驱动 (esp_lcd)
 ┃   ┣ 📂 LED               # LED 驱动
 ┃   ┣ 📂 MYIIC             # 硬件/软件 I2C 核心驱动
 ┃   ┣ 📂 SHT30             # 温湿度传感器驱动
 ┃   ┣ 📂 SOILDSENCER       # 土壤湿度 ADC 驱动及校准算法
 ┃   ┗ 📂 XL9555            # IO 扩展芯片驱动
 ┣ 📂 main
 ┃ ┣ 📂 APP                 # 应用逻辑层
 ┃ ┃ ┣ 📂 FreeRTOS
 ┃ ┃ ┃ ┣ 📜 freertos_demo.c   # FreeRTOS 多任务调度与共享数据管理
 ┃ ┃ ┃ ┗ 📜 freertos_demo.h
 ┃ ┃ ┣ 📂 LVGL
 ┃ ┃ ┃ ┣ 📜 lvgl_demo.c       # LVGL 移植层与 UI 绘制逻辑 (包含图片取模)
 ┃ ┃ ┃ ┗ 📜 lvgl_demo.h
 ┃ ┣ 📜 main.c              # 程序入口 (NVS 初始化及启动 RTOS 任务)
 ┃ ┗ 📜 CMakeLists.txt      # 核心构建脚本
 ┣ 📜 partitions-16MiB.csv  # 16MB Flash 分区表
 ┗ 📜 sdkconfig             # ESP-IDF 编译配置文件
🚀 功能特性
高精度传感器采集: 结合 ESP-IDF v5 驱动，加入曲线拟合校准（Curve Fitting Calibration），实现极高精度的电压到湿度百分比转换。

丝滑的 UI 体验: 使用 esp_lcd 框架对接 LVGL V8，开启 MALLOC_CAP_DMA 内存分配，突破 SPI 屏幕刷新瓶颈。

高内聚低耦合: 采用合理的 CMakeLists.txt 组件化设计，驱动层 (BSP) 与应用层 (APP) 严格分离。

多任务并发: 引入 FreeRTOS，解决慢速 I2C 阻塞导致的屏幕掉帧问题，实现采集与显示完美并行。

⚙️ 快速开始
确保已配置好 ESP-IDF v5.4.4 开发环境。

克隆/下载本仓库后，在 VS Code 中打开。

设定目标芯片为 ESP32-P4：

Bash
idf.py set-target esp32p4
编译并烧录项目：

Bash
idf.py build
idf.py -p COMx flash monitor
🎯 未来规划 (TODO)
[ ] 接入摄像头 (Camera): 引入 DVP/MIPI 摄像头，实现视频流的实时采集与 LCD 预览。

[ ] 端侧 AI 集成 :

借助 ESP-DL 库，部署量化版 YOLO-Fastest V2 或 Micro YOLO。

调用 ESP32-P4 内置的 PPA 硬件加速器进行图像缩放预处理。

Note: 该项目正在积极开发中。在配置 LVGL 与 ESP_LCD 框架时，请确保 lv_conf.h 中的色彩深度 (LV_COLOR_DEPTH) 设置为 16，并开启相应的编译优化（-O3 -ffast-math）以榨干 P4 的全部性能。
