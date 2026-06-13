# 基于 ESP32-P4 的植物健康监测与拟态交互系统

本项目是一个面向比赛展示的植物健康监测与交互原型。当前第一版已经打通了从板端传感器采集、状态判断、屏幕显示、触摸反馈到电脑端 AI 辅助识别的完整演示链路。

项目目前不是单一的裸机例程，也不是 README 旧版本中提到的 `10_spilcd_LVGL` 工程。当前实际可用的板端主工程是：

```text
camera_mipi_headless/
```

同时仓库中保留了电脑端 AI、数据采集、训练和调试脚本，用于配合 ESP32-P4 开发板完成第一版演示。

## 当前完成情况

第一版已经完成：

- ESP32-P4 板端固件可以启动并运行。
- SHT30 温湿度、BH1750 光照、土壤湿度 ADC 等传感器已接入。
- 板端可以根据土壤湿度、温度、空气湿度、光照等数据计算植物健康状态。
- LCD 屏幕可以显示中文植物健康界面、传感器数据、健康状态和触摸反馈。
- TTP223 触摸模块已接入，用于第一版触摸/安抚交互。
- OV5645 MIPI 摄像头链路已在工程中接入，用于图像采集与后续 AI 联动。
- 电脑端已有 YOLO/分类模型相关脚本，用于植物状态识别、训练和串口联动。
- 屏幕刷新闪烁问题已通过后台帧缓冲绘制方式进行优化。

当前仍未完全完成：

- AI 模型还没有完整移植到 ESP32-P4 板端运行。
- 触摸交互目前主要依赖外接 TTP223/传感器输入，后续需要改成更真实的触摸方案。
- 心情系统还需要继续打磨，目前更偏向健康检测和基础反馈。
- 外观结构和比赛展示形态仍需要继续设计。

## 系统链路

当前第一版演示链路如下：

```text
环境传感器数据
  -> ESP32-P4 采集与状态判断
  -> LCD 中文界面显示
  -> 触摸输入触发情绪/反馈变化
  -> 电脑端 AI 可通过串口参与识别结果联动
```

电脑端 AI 与板端固件之间主要通过串口命令传递结果。板端显示模块可以接收类似 `RESULT ...` 的识别结果命令，并更新屏幕上的状态、心情、健康分数和养护建议。

## 主要目录

```text
plant_ai/
├─ camera_mipi_headless/          # 当前 ESP32-P4 板端主工程
│  ├─ components/BSP/             # 板级外设驱动
│  │  ├─ BH1750/                  # 光照传感器
│  │  ├─ LCD/                     # RGB/MIPI LCD 底层驱动
│  │  ├─ LED/                     # LED
│  │  ├─ MYIIC/                   # I2C 基础驱动
│  │  ├─ PLANT_I2C/               # 植物项目 I2C 封装
│  │  ├─ SHT30/                   # 温湿度传感器
│  │  ├─ SOFTIIC/                 # 软件 I2C
│  │  └─ SOILDSENCER/             # 土壤湿度 ADC
│  ├─ main/
│  │  ├─ APP/
│  │  │  ├─ MIPI_CAM/             # MIPI 摄像头采集与串口命令处理
│  │  │  ├─ PLANT_DISPLAY/        # 植物健康 UI 与屏幕显示
│  │  │  ├─ PLANT_ENGINE/         # 植物状态融合与结果处理
│  │  │  ├─ PLANT_SENSOR/         # 传感器采集任务
│  │  │  └─ PLANT_TOUCH/          # 触摸输入任务
│  │  └─ main.c                   # 板端入口
│  ├─ managed_components/         # ESP-IDF 组件依赖
│  ├─ sdkconfig                   # ESP-IDF 配置
│  └─ partitions-16MiB.csv        # 16MB Flash 分区表
├─ src/plant_pet_ai/              # 电脑端植物状态/AI 逻辑
├─ tools/                         # 训练、预测、串口桥接、摄像头调试脚本
├─ models/                        # 已训练/训练中模型文件
├─ dataset*/                      # 数据集目录
└─ docs/                          # 接线、调试、阶段计划和验收记录
```

## 硬件组成

当前第一版使用的核心硬件包括：

- 主控：ESP32-P4 开发板
- 显示：RGB/MIPI LCD 屏幕
- 摄像头：OV5645 MIPI 摄像头
- 温湿度：SHT30
- 光照：BH1750
- 土壤湿度：ADC 土壤湿度传感器
- 触摸：TTP223 触摸模块
- 其他：LED、USB 串口/JTAG

## 板端固件构建与烧录

当前工程使用 ESP-IDF v5.5.4。Windows 下如果项目路径包含中文用户名，部分 ESP-IDF 工具链可能会在链接阶段出现路径编码问题。建议将工程复制到纯英文路径再构建，例如：

```powershell
C:\espbuild\camera_mipi_headless_src
```

加载 ESP-IDF 环境：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. 'C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1'
```

构建：

```powershell
cd C:\espbuild\camera_mipi_headless_src
idf.py build
```

烧录：

```powershell
idf.py -p COM8 flash
```

如果串口号不同，先查看设备管理器或用 ESP-IDF/esptool 识别实际端口。

## 电脑端 AI 与调试脚本

常用脚本位于 `tools/`：

```text
tools/plant_pet_app.py                 # 本地交互演示界面
tools/serial_sensor_bridge.py          # 串口传感器桥接
tools/plant_status_lcd_bridge.py       # AI 结果与 LCD 串口联动
tools/predict_plant_status_yolo.py     # YOLO 植物状态预测
tools/train_plant_status_classifier.py # 植物状态分类训练
tools/train_leaf_classifier.py         # 叶片分类训练
tools/capture_mipi_photo.py            # 从板端抓取 MIPI 摄像头图片
tools/preview_mipi_camera.py           # 摄像头预览/调试
```

基础依赖：

```powershell
pip install -r requirements.txt
```

训练相关依赖：

```powershell
pip install -r requirements-train.txt
```

摄像头调试依赖：

```powershell
pip install -r requirements-camera.txt
```

## 当前分支说明

当前用于屏幕闪烁优化的分支：

```text
plant_ai_01
```

该分支主要改动：

- 在 LCD 底层增加后台帧缓冲绘制接口。
- 将植物健康 UI 改为先在后台缓冲中绘制完整画面，再一次性提交到屏幕。
- 减少传感器刷新或触摸刷新时的整屏闪烁。

## 后续计划

第二版重点方向：

- 将目前的传感器/模块触摸交互升级为更真实的触摸方案。
- 推进电脑端 AI 模型向 ESP32-P4 板端迁移。
- 完善心情系统，让健康状态、触摸反馈和拟态表情更自然。
- 优化设备外观结构，适配比赛展示。
- 增强系统稳定性，包括摄像头、串口、传感器异常情况处理。
- 整理演示流程，让观众能快速理解“环境监测 + 植物健康判断 + 拟态交互”的完整价值。

## 当前项目定位

一句话概括：

```text
这是一个已经跑通第一版完整链路的 ESP32-P4 植物健康监测与拟态交互原型，当前重点是稳定板端展示效果，并为第二版真实触摸和端侧 AI 做准备。
```
