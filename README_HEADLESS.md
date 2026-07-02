# ATK OV5645 MIPI CSI 无屏验证版

此工程基于正点原子官方 `28_mipicamera` 例程，用于 ESP32-P4 `revision v1.0` 开发板上的摄像头独立验收。

## 修改原因

- 官方例程先初始化 LCD，再初始化摄像头。没有匹配 LCD 时，程序会停在 MIPI DSI 初始化，摄像头代码尚未执行。
- 本板芯片为 ESP32-P4 `revision v1.0`。使用 ESP-IDF 5.5.4 构建时，需要显式启用 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` 和 `CONFIG_ESP32P4_REV_MIN_100=y`。

## 验证范围

无屏版保留官方摄像头初始化路径：

- 使用底板共享 SCCB 总线：SDA `GPIO33`，SCL `GPIO32`
- 检测 OV5645 产品 ID `0x5645`
- 创建 `esp32p4:MIPI-CSI`
- 配置 `1280x960 RGB565`
- 启动视频流并持续读取 V4L2 帧

成功日志应包含：

```text
ov5645: Detected Camera sensor PID=0x5645
app_video: bus:     esp32p4:MIPI-CSI
app_video: width=1280 height=960
mipi_cam: OV5645 CSI init succeeded, starting headless frame capture
mipi_cam: frame=60 index=0 bytes=2457600
```

## 拍照并回传电脑

当前无屏版会持续读取 OV5645 MIPI CSI 帧，并支持把一张照片通过串口回传给电脑：

- 固件端捕获 `1280x960 RGB565` 原始帧
- 回传前在固件中降采样为 `320x240 RGB565`
- 通过串口输出 `PHOTO_BEGIN` / base64 / `PHOTO_END`
- 电脑端脚本接收后转换为 PNG

电脑端依赖：

```powershell
pip install -r ..\requirements-camera.txt
```

刷入本工程固件并让开发板运行后，执行：

```powershell
python ..\tools\capture_mipi_photo.py --port COM8 --output ..\data\captures\mipi_photo.png
```

如果不指定 `--output`，脚本会保存到 `data/captures/mipi_photo_<timestamp>.png`。

固件启动后会在第 `30` 帧自动导出一次；脚本也会持续发送 `SNAP` 命令，请求固件导出下一帧。

## 构建

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. 'C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1'
idf.py build
```

## 直接刷写已生成固件

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. 'C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1'
esptool.py --chip esp32p4 -p COM8 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 '..\docs\evidence\firmware\28_mipicamera_rev1_headless_merged.bin'
```

合并固件 SHA-256：

```text
A31707860E2B48380ABFC7DB8D7A0F39D5B43E1FE392A6190B0CB165A78F4D9F
```

## 已完成验收

- 连续抓帧超过 1000 帧，约 30 FPS
- `esptool` 硬复位复测 `5/5` 通过
- 彻底断电约 10 秒后重新上电，冷启动持续抓帧通过
- 冷启动后再次受控硬复位，重新检测 `PID=0x5645` 并持续抓帧通过
